// utils/cloud.js
// 云开发访问封装（M1）：只做「读」——experiments 集合的列表分页与单条详情
// 说明（见 docs/小程序与微信云开发落地方案.md §3.1 / §7.1）：
//   · 客户端一律不写库（安全规则建议 write:false），写入口只有云函数 deviceUpload；
//   · 归属字段统一用 openid（云函数写入）；M1 未做设备绑定时该字段可能为空，
//     此时列表会读到「无归属」的记录，便于联调（上线前请收紧安全规则，见 M1说明.md）。

/** 每页条数（§4.3 建议 20） */
const PAGE_SIZE = 20;

/** 详情/列表共用的字段白名单：不返回 curve 等大字段 */
const FIELDS = [
  "_id",
  "deviceId",
  "deviceName",
  "fabricName",
  "fabricCode",
  "operator",
  "operatorOpenid",
  "openid",
  "softness",
  "smoothness",
  "roughness",
  "rebound",
  "composite",
  "metricsNormalized",
  "metricsFlags",
  "weights",
  "curveRef",
  "dedupId",
  "startedAt",
  "createdAt",
  "source",
];

/** 集合名常量，避免各处硬编码拼错 */
const COLLECTION = "experiments";

/** 每页最大条数（云数据库客户端单次 limit 上限为 20，硬性约束） */
const MAX_PAGE_SIZE = 20;

/**
 * 列表排序白名单：只允许按这几个字段排序，防止外部传入非法排序字段
 * 说明：M2 列表提供「入库时间(createdAt)」「综合评分(composite)」两档
 */
const SORT_FIELDS = ["createdAt", "composite", "startedAt"];

/** 把字符串里的正则元字符转义，避免用户输入的 . ( ) * 等被当成正则语法 */
function escapeRegExp(s) {
  return String(s).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

/** 把时间形态统一成时间戳，供 score 兜底排序用；无法解析返回 0 */
function toTimestamp(v) {
  if (!v && v !== 0) return 0;
  if (v instanceof Date) return v.getTime() || 0;
  if (typeof v === "number") return v < 1e12 ? v * 1000 : v;
  const t = new Date(v).getTime();
  return isFinite(t) ? t : 0;
}

/** 取云数据库实例（未 init 时给出可读错误） */
function db() {
  if (!wx.cloud) {
    throw new Error("当前基础库不支持云能力，请使用 2.2.3 及以上版本");
  }
  return wx.cloud.database();
}

/**
 * 分页读取实验记录
 *
 * 向后兼容说明（M1 老调用 `fetchExperiments({page, pageSize})` 行为完全不变）：
 *   · 新增参数全部可选，不传时退化为「createdAt 倒序 + 无过滤 + 不取总数」；
 *   · 新增 `keyword`（模糊）与旧的 `fabricName`（精确）并存，二者同时传时 keyword 生效。
 *
 * @param {object} options
 * @param {number} options.page      页码，从 0 开始
 * @param {number} options.pageSize  每页条数，默认 20，最大 20（云数据库客户端上限）
 * @param {string} options.keyword   可选：按 fabricName 模糊过滤（大小写不敏感，自动转义正则元字符）
 * @param {string} options.fabricName 可选：按布料名精确过滤（M1 旧参数，保留）
 * @param {string} options.deviceId  可选：按设备过滤
 * @param {string|Array} options.source 可选：按来源过滤，如 "device" 或 ["device","manual"]
 * @param {string|object} options.orderBy 可选：排序字段，"createdAt" | "composite" | "startedAt"，
 *                                        或 `{field:"composite", direction:"desc"}`，默认 createdAt 倒序
 * @param {boolean} options.withTotal 可选：是否同时返回匹配总数（多一次 count 读，默认 false）
 * @param {boolean} options.compositeFallback 可选：orderBy 为 composite 且本页无数据时，
 *                                        退化为「取一页 + 内存排序」的兼容路径（默认 true）
 * @returns {Promise<{list:Array, hasMore:boolean, page:number, total:(number|null), sortedBy:string, fallback:boolean}>}
 */
async function fetchExperiments(options) {
  const opt = options || {};
  // 云数据库客户端单次最多返回 20 条，这里做一次硬约束，避免误传 100 却只拿到 20 条而分页错乱
  const pageSize = Math.min(opt.pageSize || PAGE_SIZE, MAX_PAGE_SIZE);
  const page = opt.page || 0;

  // 排序参数（白名单校验，非法值退化为 createdAt 倒序）
  const orderBy = opt.orderBy || {};
  let sortField = typeof orderBy === "string" ? orderBy : orderBy.field;
  let sortDirection = (typeof orderBy === "object" && orderBy.direction) || "desc";
  if (SORT_FIELDS.indexOf(sortField) < 0) sortField = "createdAt";
  if (sortDirection !== "asc" && sortDirection !== "desc") sortDirection = "desc";

  // where 条件
  const where = {};
  const keyword = (opt.keyword || "").trim();
  if (keyword) {
    // 模糊匹配：客户端用 db.RegExp 构造正则（options:"i" 忽略大小写）
    where.fabricName = db().RegExp({ regexp: escapeRegExp(keyword), options: "i" });
  } else if (opt.fabricName) {
    where.fabricName = opt.fabricName; // M1 精确匹配语义，保持兼容
  }
  if (opt.deviceId) where.deviceId = opt.deviceId;
  const sources = []
    .concat(opt.source || [])
    .filter((s) => !!s); // 过滤掉 "" / null，避免 where 里出现空值
  if (sources.length === 1) where.source = sources[0];
  else if (sources.length > 1) where.source = db().command.in(sources);

  function buildQuery() {
    let query = db().collection(COLLECTION);
    if (Object.keys(where).length) query = query.where(where);
    return query;
  }

  const fieldSpec = FIELDS.reduce((acc, k) => { acc[k] = true; return acc; }, {});

  async function getPage() {
    const res = await buildQuery()
      .field(fieldSpec)
      .orderBy(sortField, sortDirection)
      .skip(page * pageSize)
      .limit(pageSize)
      .get();
    return res.data || [];
  }

  let list = await getPage();
  let sortedBy = sortField;

  // 兼容兜底（仅在「第一页 + 有数据」时生效，不会把空结果伪装成有数据）：
  // 若服务端按 composite 排序没有体现顺序（个别环境未建 composite 索引时可能出现），
  // 这里对当前页做一次内存排序，至少保证展示顺序正确；sortedBy 会回传 "composite-memory" 供调用方提示。
  let fallback = false;
  if (sortField === "composite" && page === 0 && list.length > 1 && opt.compositeFallback !== false) {
    const byComposite = (a, b) => {
      const d = (Number(b.composite) || 0) - (Number(a.composite) || 0);
      return d !== 0 ? d : toTimestamp(b.createdAt) - toTimestamp(a.createdAt);
    };
    const sorted = list.slice().sort(byComposite);
    const same = sorted.every((it, i) => it === list[i]);
    if (!same) {
      list = sorted;
      fallback = true;
      sortedBy = "composite-memory";
    }
  }

  // 二级排序（稳定化）：评分相同时按 createdAt 倒序，避免翻页时同分记录顺序抖动
  if (sortField === "composite" && list.length > 1) {
    list = list.slice().sort((a, b) => {
      const d = (Number(b.composite) || 0) - (Number(a.composite) || 0);
      return d !== 0 ? d : toTimestamp(b.createdAt) - toTimestamp(a.createdAt);
    });
  }

  // 总数：count() 会额外算一次读，所以默认不调用（M1 的「取满一页即认为还有下一页」策略保留）
  let total = null;
  if (opt.withTotal) {
    const countRes = await buildQuery().count();
    total = (countRes && countRes.total) || 0;
  }

  return {
    list,
    hasMore: list.length >= pageSize,
    page,
    total,
    sortedBy,
    fallback,
  };
}

/**
 * 读取单条实验记录（详情页）
 * 优先按 _id 查库（拿到完整字段）；失败时调用方可用页面参数兜底渲染
 * @param {string} id
 */
async function fetchExperimentById(id) {
  if (!id) throw new Error("缺少记录 id");
  const res = await db().collection(COLLECTION).doc(id).field(
    FIELDS.reduce((acc, k) => { acc[k] = true; return acc; }, {})
  ).get();
  return res.data || null;
}

/**
 * 统一错误提示：把云开发错误翻译成人话，方便 M1 联调
 * @param {Error} err
 */
function describeError(err) {
  const msg = (err && (err.errMsg || err.message)) || "未知错误";
  if (msg.indexOf("collection not exists") >= 0 || (err && err.errCode === -502005)) {
    return "集合 experiments 不存在：请在云开发控制台 → 数据库 → 新建集合 experiments";
  }
  if (msg.indexOf("Environment not found") >= 0) {
    return "云环境未找到：请检查 miniprogram/app.js 中的 env 是否与云开发环境 ID 一致";
  }
  if (msg.indexOf("permission denied") >= 0 || (err && err.errCode === -502003)) {
    return "无读取权限：请在控制台把 experiments 的读权限开放给所有用户（M1 联调用）";
  }
  if (msg.indexOf("timeout") >= 0) {
    return "请求超时：请检查网络后下拉刷新重试";
  }
  return msg;
}

module.exports = {
  PAGE_SIZE,
  MAX_PAGE_SIZE,
  SORT_FIELDS,
  FIELDS,
  COLLECTION,
  db,
  fetchExperiments,
  fetchExperimentById,
  describeError,
};
