// Fixed wire schema; never store arbitrary device-supplied object properties.
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
