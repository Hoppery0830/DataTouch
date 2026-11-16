// Fixed wire schema; never store arbitrary device-supplied object properties.
/**
 * @brief 校验设备上传的遥测批次及每行的字段格式。
 * @param input 待校验的设备遥测。
 * @return 校验通过的批次，或 null。
 */
function validate(input){
 if(!input || input.version!==1 || typeof input.session!=="string" || !/^[a-f0-9]{8}-\d{1,10}$/.test(input.session))return null;
 if(!Number.isInteger(input.ageMs) || input.ageMs<0 || input.ageMs>0xffffffff || !Array.isArray(input.samples) || !input.samples.length || input.samples.length>60)return null;
 let lastSeq=-1,lastMs=-1;
 const samples=[];
 for(const row of input.samples){
  if(!Array.isArray(row)||row.length!==28)return null;
  if(!row.every(v=>typeof v==='number'&&Number.isFinite(v)))return null;
  if(!row.slice(0,6).every(v=>Number.isInteger(v)&&v>=0&&v<=0xffffffff))return null;
  if(row[0]<=lastSeq||row[1]<lastMs||row[2]>127||row[3]>1||row.slice(6).some(v=>Math.abs(v)>1e6))return null;
  lastSeq=row[0];lastMs=row[1];samples.push(row.slice());
 }
 return {version:1,session:input.session,ageMs:input.ageMs,samples};
}
/**
 * @brief 合并新旧遥测记录并限制设备实时窗口的长度。
 * @param previous 已存储的设备遥测文档。
 * @param input 已校验的新批次。
 * @param deviceId 设备 ID。
 * @param now 当前云端时间。
 * @return 要写回设备遥测集合的文档。
 */
function merge(previous,input,deviceId,now){
 const same=previous && previous.session===input.session;
 const rows=same && Array.isArray(previous.samples)?previous.samples:[];
 const map=new Map(rows.map(r=>[r[0],r]));for(const r of input.samples)map.set(r[0],r);
 const samples=[...map.values()].sort((a,b)=>a[0]-b[0]).slice(-600);
 const latest=samples[samples.length-1],incoming=input.samples[input.samples.length-1];
 const isNew=!same || !rows.length || incoming[0]>rows[rows.length-1][0];
 return {deviceId,version:1,session:input.session,samples,
  sampleAt:isNew?new Date(now.getTime()-input.ageMs):previous.sampleAt,
  updatedAt:now,lastSeq:latest[0]};
}
module.exports={validate,merge};
