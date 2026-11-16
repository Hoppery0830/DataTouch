/** @file tactile.js
 * @brief 按设备与实验令牌保存不可变触觉摘要；确认值只在落库后返回。
 */
const crypto=require('crypto');
const COUNT_FIELDS=['durationMs','forceBlocks','adcBlocks','contactBlocks','blocks'];
function validate(v){
 if(!v || typeof v.token!=='string' || !/^[a-f0-9]{16}$/.test(v.token) || v.version!==1)return false;
 if(!Number.isInteger(v.mode)||v.mode<0||v.mode>2||![0,1].includes(v.outcome))return false;
 if(!Number.isInteger(v.quality)||v.quality<0||v.quality>63)return false;
 if(!COUNT_FIELDS.every(k=>Number.isInteger(v[k])&&v[k]>=0&&v[k]<=0xffffffff))return false;
 if(v.forceBlocks>v.blocks||v.adcBlocks>v.blocks||v.contactBlocks>v.forceBlocks)return false;
 if(Boolean(v.quality&1)!==Boolean(v.forceBlocks)||Boolean(v.quality&2)!==Boolean(v.adcBlocks)||Boolean(v.quality&4)!==Boolean(v.contactBlocks)||Boolean(v.quality&32)!==(v.blocks===0))return false;
 return Array.isArray(v.values)&&v.values.length===14&&v.values.every(x=>typeof x==='number'&&Number.isFinite(x)&&Math.abs(x)<=1e6)&&v.values.slice(6).every(x=>x>=0);
}
async function save(db,deviceId,v){
 if(!/^[A-Za-z0-9_-]{1,64}$/.test(deviceId)||!validate(v))throw new Error('invalid tactile result');
 const id=crypto.createHash('sha256').update(deviceId+':'+v.token).digest('hex').slice(0,32);
 const result={version:1,mode:v.mode,outcome:v.outcome,quality:v.quality,values:v.values.slice()};
 COUNT_FIELDS.forEach(k=>result[k]=v[k]);
 const data={_id:id,deviceId,experimentToken:v.token,result,algorithm:'tactile-block-summary-v1',receivedAt:new Date(),status:v.outcome?'interrupted':'complete'};
 const collection=db.collection('tactile_results');
 try {await collection.add({data});}
 catch(error){
  const got=await collection.doc(id).get(),old=got&&got.data;
  if(!old||old.deviceId!==deviceId||old.experimentToken!==v.token||old.algorithm!==data.algorithm||
     !['version','mode','outcome','quality',...COUNT_FIELDS].every(k=>old.result&&old.result[k]===result[k])||
     JSON.stringify(old.result.values)!==JSON.stringify(result.values))throw error;
 }
 return {ok:true,tactileAck:v.token,id};
}
module.exports={validate,save};
