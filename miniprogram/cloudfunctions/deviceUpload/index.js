// cloudfunctions/deviceUpload/index.js
// ============================================================
// deviceUpload —— M1 设备上云入口（ESP32-S3 → 云函数 → 云数据库 experiments）
//
// 设计依据：docs/小程序与微信云开发落地方案.md
//   §3.1 experiments 表结构
//   §4.1 deviceUpload（M1 只做最小子集）
//   §5.3 鉴权（正式版为 HMAC-SHA256，M1 先用「共享密钥 token」占位）
//
// 调用方式（两种都支持）：
//   1) HTTP 云函数：ESP32 直接 HTTPS POST，body 为 JSON 字符串
//   2) wx.cloud.callFunction({ name:'deviceUpload', data:{...} })
//      —— 小程序端用 python/Postman 联调时也可用云开发控制台的「云函数测试」
//
// 入参（event，JSON）：
//   {
//     "token":      "REPLACE_ME",        // 共享密钥（M1 占位；上线请改并用 HMAC）
//     "deviceId":   "YD32S3-3C6108A1",
//     "fabricName": "纯棉平纹40支",
//     "operator":   "张三",               // 可选
//     "softness":   0.62,                // ★ STM32 归一化后的 [0,1]
//     "smoothness": 0.71,
//     "roughness":  0.24,                // 越小越细腻
//     "rebound":    0.55,
//     "composite":  72.4,                // 0-100
//     "startedAt":  1756376463000,       // 可选，毫秒时间戳 / ISO 字符串
//     "dedupId":    "YD32S3-3C6108A1-000042-0007",
//     "weights":    {...},               // 可选，权重快照
//     "curveRef":   "cloud://.../curves/xxx.json", // 可选，抽稀曲线在云存储的引用
//     "metricsNormalized": true          // 可选，默认 true
//   }
//
// 出参：
//   成功 { ok: true,  id: "<_id>", dedup: false, insecure: false }
//        dedup=true    → 命中幂等键，未重复写入（重复上报安全）
//        insecure=true → 仍在使用占位密钥 REPLACE_ME，仅可联调，上线请改并用 HMAC
//   失败 { ok: false, code: 4011, message: "..." }
// ============================================================

const cloud = require("wx-server-sdk");

// 使用当前云环境（云函数被哪个环境调用就读写哪个环境）
cloud.init({ env: cloud.DYNAMIC_CURRENT_ENV });

const db = cloud.database();
const COLLECTION = "experiments";

// ------------------------------------------------------------
// ⚠️ 共享密钥（M1 占位）
//   请改成你自己的随机长字符串（建议 32 位以上），并与设备端保持一致。
//   ⚠️ 上线请改并用 HMAC：见方案 §5.3 —— 每台设备独立 deviceSecret，
//      上报 sign = HMAC_SHA256(derivedKey, deviceId|dedupId|ts|nonce|bodyHash)，
//      云端只存 devices.secretHash，并校验 |now-ts| <= 300s 与 nonce 防重放。
// ------------------------------------------------------------
const SHARED_TOKEN = "REPLACE_ME";

// 四维权重（仅用于在 composite 缺省时由四维折算，权威评分仍以 STM32 为准）
const DEFAULT_WEIGHTS = {
  softness: 0.3,
  smoothness: 0.25,
  roughness: 0.2,
  rebound: 0.25,
};

/** 统一失败返回 */
function fail(code, message) {
  return { ok: false, code, message };
}

/**
 * 常量时间字符串比较，避免 token 比较被计时侧信道利用
 * （M1 只是基础防护，正式请用 crypto.timingSafeEqual + HMAC）
 */
function safeEqual(a, b) {
  const sa = String(a === undefined || a === null ? "" : a);
  const sb = String(b === undefined || b === null ? "" : b);
  if (sa.length !== sb.length) return false;
  let diff = 0;
  for (let i = 0; i < sa.length; i++) {
    diff |= sa.charCodeAt(i) ^ sb.charCodeAt(i);
  }
  return diff === 0;
}

/**
 * 兼容多种调用形态，取出真正的参数对象
 * · callFunction / 控制台测试：event 本身就是参数
 * · HTTP 云函数：body 可能是 JSON 字符串，也可能包在 event.body 里
 *   （微信云开发 HTTP 访问服务：event.body + event.headers + event.isBase64Encoded）
 */
function normalizeEvent(event) {
  let ev = event || {};
  if (typeof ev === "string") {
    try {
      ev = JSON.parse(ev);
    } catch (e) {
      return { __parseError: "event 不是合法 JSON 字符串" };
    }
  }
  // HTTP 触发：body 是字符串
  if (typeof ev.body === "string" && ev.body) {
    try {
      const parsed = JSON.parse(ev.body);
      // 把 headers 一并透传，便于将来从 header 读 X-Sign / X-Timestamp
      return Object.assign({}, parsed, { __headers: ev.headers || {} });
    } catch (e) {
      // body 解析失败时，回退用外层 event（可能本身就是 JSON 对象）
      return ev;
    }
  }
  // HTTP 触发：body 已经是对象
  if (ev.body && typeof ev.body === "object") {
    return Object.assign({}, ev.body, { __headers: ev.headers || {} });
  }
  return ev;
}

/** 数字解析：非法返回 null */
function num(v) {
  if (v === undefined || v === null || v === "") return null;
  const n = Number(v);
  return isFinite(n) ? n : null;
}

/** 归一化值 clamp 到 [0,1]；兼容误传 0-100 的情况（>1 时按 /100 处理） */
function normalized01(v, fieldName) {
  let n = num(v);
  if (n === null) throw new Error(`缺少或非法字段：${fieldName}`);
  if (n > 1.0000001 && n <= 100) n = n / 100;
  if (n < 0) n = 0;
  if (n > 1) n = 1;
  return Math.round(n * 10000) / 10000; // 保留 4 位，避免浮点噪声入库
}

/** 时间解析：毫秒数 / 秒数 / ISO 字符串 / Date 都接受；失败返回 null */
function toDate(v) {
  if (v === undefined || v === null || v === "") return null;
  if (v instanceof Date) return isNaN(v.getTime()) ? null : v;
  if (typeof v === "number") {
    const ms = v < 1e12 ? v * 1000 : v; // 秒级时间戳兜底
    const d = new Date(ms);
    return isNaN(d.getTime()) ? null : d;
  }
  if (typeof v === "string") {
    const s = v.trim();
    if (!s) return null;
    if (/^\d+$/.test(s)) return toDate(Number(s));
    const d = new Date(s);
    return isNaN(d.getTime()) ? null : d;
  }
  return null;
}

/**
 * 主入口
 */
exports.main = async (event, context) => {
  const ev = normalizeEvent(event);

  if (ev.__parseError) {
    return fail(4001, "请求体解析失败：" + ev.__parseError);
  }

  // ---------- 1) 共享密钥校验（M1） ----------
  const token = ev.token || (ev.__headers && (ev.__headers["x-token"] || ev.__headers["X-Token"]));
  if (!safeEqual(token, SHARED_TOKEN)) {
    return fail(4011, "token 校验失败：请让请求体带上与云函数 SHARED_TOKEN 一致的 token");
  }
  // M1 允许沿用占位符 REPLACE_ME 以便联调；此时返回 insecure 标记，提醒尽快替换
  // ⚠️ 上线必须把 SHARED_TOKEN 改成真实随机密钥，并改用 HMAC 验签（§5.3）
  const insecure = SHARED_TOKEN === "REPLACE_ME";
  if (insecure) {
    console.warn("[deviceUpload] ⚠️ 正在使用占位密钥 REPLACE_ME，仅可用于 M1 联调，上线请改并用 HMAC");
  }

  // ---------- 2) 结构校验 ----------
  const deviceId = String(ev.deviceId || "").trim();
  if (!deviceId) return fail(4002, "缺少 deviceId");

  const fabricName = String(ev.fabricName || "").trim() || "未命名布料";

  let softness;
  let smoothness;
  let roughness;
  let rebound;
  try {
    // 四维：STM32 归一化后的 [0,1]
    softness = normalized01(ev.softness, "softness");
    smoothness = normalized01(ev.smoothness, "smoothness");
    roughness = normalized01(ev.roughness, "roughness");
    rebound = normalized01(ev.rebound, "rebound");
  } catch (e) {
    return fail(4003, e.message);
  }

  // composite ∈ [0,100]；缺省时按权重从四维折算（roughness 取反），入库时标注来源
  let composite = num(ev.composite);
  if (composite === null) {
    const w = Object.assign({}, DEFAULT_WEIGHTS, ev.weights || {});
    composite =
      (w.softness * softness +
        w.smoothness * smoothness +
        w.roughness * (1 - roughness) +
        w.rebound * rebound) *
      100;
  }
  if (composite < 0 || composite > 100) {
    return fail(4004, "composite 必须在 0-100 之间");
  }
  composite = Math.round(composite * 100) / 100;

  // ---------- 3) 幂等键 ----------
  // 设备应自带 dedupId（§5.4：deviceId-bootCount-seq）；缺失时按「设备+开始时间+评分」生成稳定值
  const startedAtDate = toDate(ev.startedAt);
  const dedupId =
    String(ev.dedupId || "").trim() ||
    [deviceId, startedAtDate ? startedAtDate.getTime() : "t0", composite].join("-");

  try {
    // 先查：命中则直接返回，保证重复上报不产生新记录（网络抖动/half-success 也安全）
    const existed = await db
      .collection(COLLECTION)
      .where({ dedupId })
      .field({ _id: true })
      .limit(1)
      .get();
    if (existed.data && existed.data.length) {
      return { ok: true, id: existed.data[0]._id, dedup: true, insecure };
    }

    // ---------- 4) 组装文档（字段与 §3.1 experiments 对齐，M1 只含核心字段） ----------
    const now = new Date();
    const doc = {
      deviceId,
      fabricName,
      operator: String(ev.operator || "").trim(), // 操作员（M1 用字符串；§3.1 的 operatorOpenid 留待绑定后回填）
      softness,
      smoothness,
      roughness,
      rebound,
      composite,
      metricsNormalized: ev.metricsNormalized !== false, // 权威链路恒为 true（STM32 已归一化）
      weights: Object.assign({}, DEFAULT_WEIGHTS, ev.weights || {}),
      curveRef: String(ev.curveRef || ""),               // 抽稀曲线引用（云存储 fileID），M1 可为空
      dedupId,
      startedAt: startedAtDate || now,                    // 缺失时退化为入库时刻（§5.6 退化策略）
      createdAt: now,                                     // 服务端时间，最可信
      source: "device",
    };

    const added = await db.collection(COLLECTION).add({ data: doc });
    return { ok: true, id: added._id, dedup: false, insecure };
  } catch (e) {
    console.error("[deviceUpload] 写库失败：", e);
    const msg = (e && (e.errMsg || e.message)) || "未知错误";
    if (msg.indexOf("collection not exists") >= 0) {
      return fail(5002, "集合 experiments 不存在：请在云开发控制台 → 数据库 → 新建集合 experiments");
    }
    return fail(5000, "写库失败：" + msg);
  }
};
