const {test}=require("node:test");
const assert=require("node:assert/strict");
const {save,validate}=require("../cloudfunctions/deviceCommand/visual");
function database(){
 const rows=new Map();
 return {rows,collection:()=>({
  async add({data}){if(rows.has(data._id))throw Error("duplicate");rows.set(data._id,data);},
  doc:id=>({async get(){return {data:rows.get(id)};}})
 })};
}
const v={token:"1234567800000100",value:0.581635,algorithm:"gunay-full-curve-v1"};
test("lost database acknowledgement: retries preserve one immutable experiment",async()=>{
 const db=database();
 const replies=await Promise.all([save(db,"esp32-dev01",v),save(db,"esp32-dev01",v)]);
 assert.equal(db.rows.size,1);assert.equal(replies[0].id,replies[1].id);
 assert.equal(replies[0].visualAck,v.token);
 const original=[...db.rows.values()][0];
 await save(db,"esp32-dev01",v);
 assert.equal([...db.rows.values()][0].receivedAt,original.receivedAt);
 await assert.rejects(save(db,"esp32-dev01",{...v,value:0.8}));
 assert.equal([...db.rows.values()][0].uCurve,v.value);
 await save(db,"esp32-dev02",v);assert.equal(db.rows.size,2);
});
test("invalid values and database failure do not acknowledge a result",async()=>{
 for(const value of [NaN,Infinity,-1,1e7,"0.5"])assert.equal(validate({...v,value}),false);
 assert.equal(validate({...v,token:"wrong"}),false);
 assert.equal(validate({...v,algorithm:"unapproved"}),false);
 const db={collection:()=>({add:async()=>{throw Error("offline");},doc:()=>({get:async()=>{throw Error("offline");}})})};
 await assert.rejects(save(db,"esp32-dev01",v));
});
test("visual page loads stored values and preserves explicit error state",async()=>{
 let page;global.Page=p=>page=p;
 global.wx={cloud:{callFunction:async()=>({result:{ok:true,rows:[{_id:"1",uCurve:0.5,experimentToken:v.token,receivedAt:Date.now()}]}})},stopPullDownRefresh(){}};
 require("../miniprogram/pages/vision/vision");
 page.setData=patch=>Object.assign(page.data,patch);
 await page.load(true);
 assert.equal(page.data.rows[0].valueText,"0.500000");
 global.wx.cloud.callFunction=async()=>{throw Error("network failed");};
 await page.load(true);
 assert.equal(page.data.error,"network failed");assert.equal(page.data.loading,false);
 delete global.Page;delete global.wx;
});
