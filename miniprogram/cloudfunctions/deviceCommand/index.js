// cloudfunctions/deviceCommand/index.js
// ============================================================
// deviceCommand —— 设备侧「远程控制命令」拉取 / 确认入口
//   ESP32 → HTTPS → 微信云开发「HTTP 访问服务」→ 本云函数
//
// 与 sendCommand 的分工：
//   sendCommand（小程序调用）只写 commands（state=pending）
//   本函数（设备调用）只做两件事：
//     action = "poll" → 先把过期 pending 置 expired，再把未过期的 pending 取走并置 sent
//     action = "ack"  → 把设备已执行的命令置 done（回填 doneAt/result）
//
// 调用方式（两种都支持）：
//   1) HTTP 访问服务：POST https://<环境ID>.service.tcloudbase.com/deviceCommand
//      body 为 JSON 字符串，被包在 event.body 里（event.isBase64Encoded 一般不出现）
//   2) 云开发控制台「云函数测试」/ wx.cloud.callFunction：event 直接就是参数对象
//
// 入库协议（event，JSON）：
//   { "payload": { "action": "poll", "token": "REPLACE_ME", "deviceId": "esp32-dev01" } }
//   { "payload": { "action": "ack",  "token": "REPLACE_ME", "deviceId": "esp32-dev01",
//                  "id": "<commands._id>", "result": "ok" } }
//   （为方便联调，也接受把 action/token/deviceId… 直接放在最外层，不带 payload 包装）
//
// 出参：
//   poll 成功 { ok:true, commands:[{id, cmd}], serverTime: <ms>, insecure: <bool> }
//   ack  成功 { ok:true, insecure: <bool> }
//   失败      { ok:false, code:<number>, message:"<可读原因>" }
//     code 约定：
//       4011 token 校验失败
//       4001 请求体解析失败
//       4002 缺少 deviceId
//       4003 缺少 id（ack）
//       4004 未知 action
//       4040 命令不存在或不属于该设备（防越权）
//       5002 集合 commands 不存在
//       5000 其它数据库错误
// ============================================================

const cloud = require("wx-server-sdk");

// 使用当前云环境（云函数被哪个环境调用就读写哪个环境）
cloud.init({ env: cloud.DYNAMIC_CURRENT_ENV });

const db = cloud.database();
const _ = db.command;

/** 命令集合名 */
const COLLECTION = "commands";

// ------------------------------------------------------------
// ⚠️ 共享密钥（占位）
//   ★ 必须与 cloudfunctions/deviceUpload/index.js 里的 SHARED_TOKEN 保持完全一致！
//     （两处是同一条设备链路的同一个密钥；改一处就要同时改另一处）
//   请改成你自己的随机长字符串（建议 32 位以上），并与 ESP32 端保持一致。
//   ⚠️ 上线请改并用 HMAC：见方案 §5.3 —— 每台设备独立 deviceSecret，
//      请求 sign = HMAC_SHA256(derivedKey, deviceId|action|ts|nonce|bodyHash)，
//      云端只存 devices.secretHash，并校验 |now-ts| <= 300s 与 nonce 防重放。
// ------------------------------------------------------------
const SHARED_TOKEN = "REPLACE_ME";

/**
 * ★ 命令白名单 —— 安全核心
 *   只处理这些命令；即使库里被塞进奇怪的值，也绝不会下发给设备。
 *   与 sendCommand 的白名单保持一致。
 *   mode_* 用于选择电机运动模式（PRESS/RUB/SLIDE），下发后由 ESP32 转 FRAME_ACTION 给 STM32。
 */
const ALLOWED_CMDS = [
  "arm", "start", "stop", "reset",
  "mode_press", "mode_rub", "mode_slide",
];

/** 单次 poll 最多返回几条命令（防止设备一次性被灌一堆命令） */
const MAX_BATCH = 4;

/** deviceId 长度上限 */
const DEVICE_ID_MAX_LEN = 64;

/** 统一失败返回 */
function fail(code, message) {
  return { ok: false, code, message };
}

/**
 * 常量时间字符串比较，避免 token 比较被计时侧信道利用
 * （与 deviceUpload 中的实现保持一致）
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
 * · 控制台测试 / callFunction：event 本身就是参数
 * · HTTP 访问服务：event.body 是 JSON 字符串（需 JSON.parse），
 *   也可能已经是对象；另外带上 event.headers 便于将来读 X-Sign / X-Timestamp
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
      return Object.assign({}, parsed, { __headers: ev.headers || {} });
    } catch (e) {
      // body 解析失败时，回退用外层 event（可能本身就是 JSON 对象）
      return Object.assign({}, ev, { __headers: ev.headers || {} });
    }
  }
  // HTTP 触发：body 已经是对象
  if (ev.body && typeof ev.body === "object") {
    return Object.assign({}, ev.body, { __headers: ev.headers || {} });
  }
  return ev;
}

/**
 * 取出真正的业务参数：优先 payload，其次最外层（方便联调）
 * @param {object} ev normalizeEvent 的结果
 */
function extractPayload(ev) {
  const p = ev && ev.payload;
  if (p && typeof p === "object") return p;
  if (typeof p === "string" && p) {
    try {
      return JSON.parse(p);
    } catch (e) {
      /* 落到外层兜底 */
    }
  }
  return ev || {};
}

/** 从请求头里兜底取 token（兼容设备把 token 放在 header 的情况） */
function headerToken(ev) {
  const h = (ev && ev.__headers) || {};
  const keys = ["x-token", "X-Token", "token", "Token"];
  for (let i = 0; i < keys.length; i++) {
    if (h[keys[i]] !== undefined && h[keys[i]] !== null) return h[keys[i]];
  }
  return "";
}

/** 把任意时间形态转成毫秒时间戳（仅用于日志，取不到返回 0） */
function tsOf(v) {
  if (!v) return 0;
  if (v instanceof Date) return v.getTime() || 0;
  const t = new Date(v).getTime();
  return isFinite(t) ? t : 0;
}

/** 集合不存在的错误识别（给出可操作提示） */
function isCollectionMissing(e) {
  const msg = (e && (e.errMsg || e.message)) || "";
  return msg.indexOf("collection not exists") >= 0 || (e && e.errCode === -502005);
}

/**
 * 主入口
 */
exports.main = async (event, context) => {
  const ev = normalizeEvent(event);

  if (ev.__parseError) {
    return fail(4001, "请求体解析失败：" + ev.__parseError);
  }

  // ---------- 1) 共享密钥校验（先鉴权，再做任何数据库操作） ----------
  const raw = extractPayload(ev);
  const token = raw.token || ev.token || headerToken(ev);
  if (!safeEqual(token, SHARED_TOKEN)) {
    return fail(4011, "token 校验失败：请让 payload 带上与云函数 SHARED_TOKEN 一致的 token");
  }
  // 沿用占位符 REPLACE_ME 时返回 insecure 标记，提醒尽快替换
  const insecure = SHARED_TOKEN === "REPLACE_ME";
  if (insecure) {
    console.warn("[deviceCommand] ⚠️ 正在使用占位密钥 REPLACE_ME，仅可用于联调，上线请改并用 HMAC");
  }

  // ---------- 2) 解析 payload ----------
  const payload = extractPayload(ev);
  const action = String(payload.action || "").trim().toLowerCase();

  // ★ 安全约束：查询一律强制带 deviceId，设备只能看/改自己的命令（防越权）
  const deviceId = String(payload.deviceId || "").trim();
  if (!deviceId) {
    return fail(4002, "缺少 deviceId");
  }
  if (deviceId.length > DEVICE_ID_MAX_LEN) {
    return fail(4002, "deviceId 过长（最多 " + DEVICE_ID_MAX_LEN + " 个字符）");
  }

  const now = new Date();

  try {
    // ==========================================================
    // action = poll：设备轮询拉取待执行命令
    // ==========================================================
    if (action === "poll") {
      // Live status is separate from command history; no sensor/score records are fabricated.
      if (payload.status && /^[A-Za-z0-9_-]{1,64}$/.test(deviceId)) {
        const st = payload.status;
        const valid = [st.state,st.mode,st.stage,st.online,st.fault].every(Number.isInteger) &&
          st.state >= 0 && st.state <= 6 && (st.mode === 255 || (st.mode >= 0 && st.mode <= 2)) &&
          st.stage >= 0 && st.stage <= 7 && st.online >= 0 && st.online <= 7 && st.fault >= 0 && st.fault <= 7;
        if (valid) try { await db.collection("device_status").doc(deviceId).set({data:{
          deviceId, alive: st.alive === true, state: st.state, mode: st.mode,
          stage: st.stage, online: st.online, fault: st.fault, updatedAt: now
        }}); } catch(e) { console.warn("device_status 写入失败（命令通道继续）：", e.message); }
      }
      // Missing final receipt is an unknown outcome, never a reason to replay START.
      await db.collection(COLLECTION).where({deviceId, state: _.in(["sent","accepted"]),
        sentAt: _.lt(new Date(now.getTime()-200000))}).update({data:{state:"unknown",result:"timeout",doneAt:now}});

      // ---- 2.1 过期清理：该设备下「还挂着 pending 但已超时」的记录置为 expired ----
      // 说明：这是「惰性清理」——只在设备来拉取时顺手做，不依赖定时触发器。
      //       先清理再查询，避免把已过期的命令误发给设备。
      let expiredCount = 0;
      try {
        const expiredRes = await db
          .collection(COLLECTION)
          .where({
            deviceId,                                  // ← 强制带 deviceId
            state: "pending",
            expiresAt: _.lt(now),                      // ← 已过期
          })
          .update({ data: { state: "expired" } });
        expiredCount = (expiredRes && expiredRes.stats && expiredRes.stats.updated) || 0;
      } catch (e) {
        // 过期清理失败不阻断本次下发（例如个别历史文档 expiresAt 类型异常）
        console.warn("[deviceCommand] 过期清理失败（忽略，继续取命令）：", e);
      }

      // ---- 2.2 取未过期的 pending，按 createdAt 升序（先来先执行），最多 MAX_BATCH 条 ----
      const priority = await db.collection(COLLECTION).where({deviceId,state:"pending",
        cmd:_.in(["stop","reset"]),expiresAt:_.gte(now)}).orderBy("createdAt","asc").limit(1).get();
      const got = priority.data && priority.data.length ? priority : await db
        .collection(COLLECTION)
        .where({
          deviceId,                                    // ← 强制带 deviceId
          state: "pending",
          expiresAt: _.gte(now),                       // ← 未过期
        })
        .orderBy("createdAt", "asc")
        .limit(MAX_BATCH)
        .get();

      const docs = (got && got.data) || [];

      // ---- 2.3 逐条白名单过滤 + 置为 sent（sentAt = now） ----
      // 二次白名单：万一库里有脏数据（非 4 个安全命令），直接跳过不发给设备
      const commands = [];
      const skipped = [];
      for (let i = 0; i < docs.length; i++) {
        const doc = docs[i];
        const cmd = String(doc.cmd || "").trim().toLowerCase();
        if (ALLOWED_CMDS.indexOf(cmd) < 0) {
          skipped.push(doc._id);
          continue;
        }
        try {
          // 再次带 deviceId 条件更新，确保「只能改自己设备的命令」
          const claim = await db.collection(COLLECTION)
            .where({ _id: doc._id, deviceId, state: "pending" })
            .update({ data: { state: "sent", sentAt: now } });
          if (!claim.stats || !claim.stats.updated) continue;
          if (cmd === "stop" || cmd === "reset") {
            await db.collection(COLLECTION).where({deviceId,state:"pending",createdAt:_.lte(doc.createdAt)})
              .update({data:{state:"cancelled",result:"cancelled",doneAt:now}});
          }
          commands.push({ id: doc._id, cmd, ts: tsOf(doc.createdAt) });
        } catch (e) {
          console.error("[deviceCommand] 置 sent 失败，跳过该条：", doc._id, e);
        }
      }

      if (skipped.length) {
        console.warn("[deviceCommand] 跳过非白名单命令（疑似脏数据）：", skipped.join(", "));
      }

      console.log(
        "[deviceCommand] poll",
        JSON.stringify({
          deviceId,
          expired: expiredCount,
          sent: commands.length,
          scanned: docs.length,
        })
      );

      return {
        ok: true,
        commands,
        serverTime: Date.now(),   // 毫秒时间戳，便于设备校准/判断延迟
        insecure,
      };
    }

    // ==========================================================
    // action = ack：设备确认已执行
    // ==========================================================
    if (action === "ack") {
      const id = String(payload.id || "").trim();
      if (!id) {
        return fail(4003, "缺少 id：ack 需要带上要确认的命令 id");
      }
      // result 可选：设备回填的执行结果（如 "ok" / "busy" / 错误码）
      const result = payload.result === undefined || payload.result === null
        ? ""
        : String(payload.result).slice(0, 500);

      // ★ 条件里同时带 deviceId：防止 A 设备确认（甚至篡改）B 设备的命令
      const knownResults = ["accepted","ok","busy","invalid","not_ready","fault","cancelled","timeout","offline","unknown","expired"];
      if (!knownResults.includes(result)) return fail(4001,"未知回执结果");
      const state = result === "accepted" ? "accepted" : result === "ok" ? "done" :
        result === "cancelled" ? "cancelled" : result === "timeout" ? "unknown" : "failed";
      const data = {state,result};
      if (state === "accepted") data.acceptedAt=now; else data.doneAt=now;
      const upd = await db
        .collection(COLLECTION)
        .where({ _id: id, deviceId, state: _.in(["sent","accepted"]) })
        .update({ data });

      const updated = (upd && upd.stats && upd.stats.updated) || 0;
      if (!updated) {
        // 记录不存在，或存在但不属于这个 deviceId
        const old = await db.collection(COLLECTION).where({_id:id,deviceId}).get();
        if (old.data && old.data.length) return {ok:true,insecure}; // terminal result cannot regress
        return fail(4040, "命令不存在或不属于该设备：" + id);
      }

      console.log("[deviceCommand] ack", JSON.stringify({ deviceId, id, result }));
      return { ok: true, insecure };
    }

    // ==========================================================
    // 其它 action
    // ==========================================================
    return fail(4004, "未知 action：" + (payload.action === undefined ? "(未提供)" : String(payload.action)) + "；只支持 poll / ack");
  } catch (e) {
    console.error("[deviceCommand] 数据库操作失败：", e);
    if (isCollectionMissing(e)) {
      return fail(5002, "集合 commands 不存在：请在云开发控制台 → 数据库 → 新建集合 commands");
    }
    const msg = (e && (e.errMsg || e.message)) || "未知错误";
    return fail(5000, "数据库操作失败：" + msg);
  }
};
