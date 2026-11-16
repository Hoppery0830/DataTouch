const {test}=require('node:test');const assert=require('node:assert/strict');
const {save,validate}=require('../cloudfunctions/deviceCommand/tactile');
const v={token:'1234567800000100',version:1,mode:1,outcome:0,quality:7,durationMs:2000,forceBlocks:10,adcBlocks:10,contactBlocks:8,blocks:10,values:Array(14).fill(0.5)};
function database(){const rows=new Map();return {rows,collection:()=>({async add({data}){if(rows.has(data._id))throw Error('duplicate');rows.set(data._id,data);},doc:id=>({async get(){return {data:rows.get(id)};}})})};}
test('tactile: lost ACK, concurrent duplicate, conflict, device separation',async()=>{
 const db=database();const replies=await Promise.all([save(db,'dev',v),save(db,'dev',v)]);assert.equal(db.rows.size,1);assert.equal(replies[0].tactileAck,v.token);
 const at=[...db.rows.values()][0].receivedAt;await save(db,'dev',v);assert.equal([...db.rows.values()][0].receivedAt,at);
 await assert.rejects(save(db,'dev',{...v,outcome:1}));await save(db,'other',v);assert.equal(db.rows.size,2);
});
test('tactile: invalid quality/counts/values and database failure never acknowledge',async()=>{
 assert(validate(v));for(const patch of [{mode:3},{quality:0},{contactBlocks:11},{values:[NaN]},{values:Array(14).fill(Infinity)},{token:'invalid'}])assert(!validate({...v,...patch}));
 const db={collection:()=>({add:async()=>{throw Error('offline');},doc:()=>({get:async()=>{throw Error('offline');}})})};await assert.rejects(save(db,'dev',v));
});
test('tactile page: saved interrupted result, missing values, matching vision, query failure',async()=>{
 let page;global.Page=p=>page=p;
 global.wx={cloud:{callFunction:async()=>({result:{ok:true,rows:[{_id:'1',experimentToken:v.token,receivedAt:Date.now(),result:{...v,quality:32,outcome:1,forceBlocks:0,adcBlocks:0,contactBlocks:0,blocks:0},visual:{uCurve:0.3}}]}})},stopPullDownRefresh(){}};
 require('../miniprogram/pages/tactile/tactile');page.setData=p=>Object.assign(page.data,p);await page.load(true);
 assert.equal(page.data.rows[0].metrics[0].value,'--');assert.equal(page.data.rows[0].outcomeText,'实验中断');assert.equal(page.data.rows[0].visualText,'0.300000');
 global.wx.cloud.callFunction=async()=>{throw Error('network');};await page.load(true);assert.equal(page.data.error,'network');assert(!page.data.loading);delete global.Page;delete global.wx;
});
