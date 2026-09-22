// cloudfunctions/sendCommand/index.js
// ============================================================
// sendCommand —— 小程序端「下发远程控制命令」入口
//
// 角色分工（远程控制命令通道）：
//   小程序 pages/control  ──callFunction──▶  sendCommand  ──写入──▶  commands 集合
//   设备 ESP32  ──HTTP──▶ deviceCommand(action=poll)  ──取走并置 sent──▶ commands
//   设备 ESP32  ──HTTP──▶ deviceCommand(action=ack)   ──置 done──────▶ commands
//
// 本函数只负责：校验白名单命令 → 取 openid → 写入一条 state="pending" 的命令。
// 不做任何设备侧交互（设备靠轮询 deviceCommand 拉取），所以返回很快。
//
// 入参（event）：
//   {
//     "deviceId": "esp32-dev01",   // 必填，目标设备号
//     "cmd":      "arm"            // 必填，白名单：
//                                  //   arm | start | stop | reset          —— 动作命令
//                                  //   mode_press | mode_rub | mode_slide  —— 只选模式（不启动）
//   }
//
// 出参：
//   成功 { ok: true,  id: "<commands._id>" }
//   失败 { ok: false, code: <number>, message: "<可读原因>" }
//     code 约定：
//       4001 参数/命令非法（不在白名单）
//       4002 缺少 deviceId
//       5002 集合 commands 不存在（需在控制台新建）
//       5000 其它写库失败
// ============================================================

const cloud = require("wx-server-sdk");

// 使用当前云环境（云函数被哪个环境调用就读写哪个环境）
cloud.init({ env: cloud.DYNAMIC_CURRENT_ENV });

const db = cloud.database();
const _ = db.command;

/** 命令集合名 */
const COLLECTION = "commands";

/**
 * ★ 命令白名单 —— 安全核心
 *   只允许下列「安全命令」被远程下发；任何其它字符串一律拒绝。
 *   · 动作命令：arm / start / stop / reset
 *   · 模式命令：mode_press（按压）/ mode_rub（揉搓）/ mode_slide（滑动）
 *     —— 语义是「只选中 STM32 的动作模式，不启动」；真正执行仍由后续 start 触发。
 *   注意：设备端 deviceCommand 也必须用同一份白名单做二次校验，
 *   不要只依赖小程序端（小程序端可被篡改，云端校验才是权威）。
 *   本次扩展只追加了 3 个 mode_* 字符串，写入的文档字段（cmd 仍是字符串）与
 *   TTL / openid / 返回值等既有逻辑一行未改。
 */
const ALLOWED_CMDS = ["arm", "start", "stop", "reset", "mode_press", "mode_rub", "mode_slide"];

/** 命令有效期（ms）：超过这个时间还没被设备取走，就会被标记为 expired（60 秒） */
const TTL_MS = 60 * 1000;

/** deviceId 长度上限，防止超长字符串写库 */
const DEVICE_ID_MAX_LEN = 64;

/** 统一失败返回 */
function fail(code, message) {
  return { ok: false, code, message };
}

/**
 * 主入口
 * @param {object} event  小程序 callFunction 的 data
 * @param {object} context
 */
exports.main = async (event, context) => {
  const ev = event || {};

  // ---------- 1) 参数校验 ----------
  const deviceId = String(ev.deviceId || "").trim();
  if (!deviceId) {
    return fail(4002, "缺少 deviceId：请填写目标设备号（如 esp32-dev01）");
  }
  if (deviceId.length > DEVICE_ID_MAX_LEN) {
    return fail(4002, "deviceId 过长（最多 " + DEVICE_ID_MAX_LEN + " 个字符）");
  }

  // Read authoritative live state through the existing cloud entry point.
  if (ev.action === "status") {
    try {
      const got = await db.collection("device_status").where({deviceId}).limit(1).get();
      const status = got.data && got.data[0];
      if (!status) return {ok:true,status:null};
      status.alive = status.alive === true && Date.now()-new Date(status.updatedAt).getTime()<15000;
      return {ok:true,status};
    } catch(e) { return fail(5000,"读取 device_status 失败："+(e.message||e.errMsg)); }
  }

  // 命令白名单校验：不在白名单直接拒绝，绝不写库
  const cmd = String(ev.cmd || "").trim().toLowerCase();
  if (ALLOWED_CMDS.indexOf(cmd) < 0) {
    return fail(
      4001,
      "非法的 cmd：" + (ev.cmd === undefined ? "(未提供)" : String(ev.cmd)) +
        "；只允许 " + ALLOWED_CMDS.join(" / ")
    );
  }

  // ---------- 2) 取调用者 openid（用于审计：谁下发的命令） ----------
  // 说明：经小程序 callFunction 调用时 openid 一定存在；
  //       若将来从 HTTP 访问服务调用，可能拿不到 openid，这里退化为空串而不是报错。
  const wxContext = cloud.getWXContext() || {};
  const openid = wxContext.OPENID || wxContext.FROM_OPENID || "";

  // ---------- 3) 写入 commands ----------
  try {
    const doc = {
      deviceId,
      cmd,
      state: "pending",                                  // pending 待发送
      openid,
      createdAt: db.serverDate(),                        // 服务端时间（权威）
      expiresAt: new Date(Date.now() + TTL_MS),          // 当前 + 60s
      // sentAt / doneAt / result 此时还没有，设备取走/执行后再回填（字段可缺省）
    };

    const added = await db.collection(COLLECTION).add({ data: doc });
    console.log("[sendCommand] 已写入命令", { deviceId, cmd, openid, id: added._id });
    return { ok: true, id: added._id };
  } catch (e) {
    console.error("[sendCommand] 写库失败：", e);
    const msg = (e && (e.errMsg || e.message)) || "未知错误";
    if (msg.indexOf("collection not exists") >= 0 || (e && e.errCode === -502005)) {
      return fail(5002, "集合 commands 不存在：请在云开发控制台 → 数据库 → 新建集合 commands");
    }
    return fail(5000, "写库失败：" + msg);
  }
};
