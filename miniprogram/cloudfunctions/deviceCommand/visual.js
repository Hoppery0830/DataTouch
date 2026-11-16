/**
 * @file visual.js
 * @brief 每个设备/实验令牌仅创建一条不可变视觉记录，重复提交返回同一确认。
 */
const crypto = require("crypto");
function validate(v) {
  return !!v && /^[a-f0-9]{16}$/.test(v.token) &&
    typeof v.value === "number" && Number.isFinite(v.value) &&
    v.value >= 0 && v.value <= 1e6 && v.algorithm === "gunay-full-curve-v1";
}
async function save(db, deviceId, v) {
  if (!/^[A-Za-z0-9_-]{1,64}$/.test(deviceId) || !validate(v))
    throw new Error("invalid visual result");
  const id = crypto.createHash("sha256").update(deviceId + ":" + v.token).digest("hex").slice(0,32);
  const data = {
    _id:id, deviceId, experimentToken:v.token, uCurve:v.value,
    algorithm:v.algorithm, commandSeq:parseInt(v.token.slice(0,8),16),
    exposureStartUptimeMs:parseInt(v.token.slice(8),16),
    receivedAt:new Date(), status:"complete"
  };
  const collection = db.collection("visual_results");
  try {
    await collection.add({data});
  } catch (error) {
    // Includes the concurrent retry case: acknowledge only a matching stored record.
    const got = await collection.doc(id).get();
    const old = got && got.data;
    if (!old || old.deviceId !== deviceId || old.experimentToken !== v.token ||
        old.uCurve !== v.value || old.algorithm !== v.algorithm)
      throw error;
  }
  return {ok:true, visualAck:v.token, id};
}
module.exports = {validate,save};
