const fs=require('fs'),vm=require('vm'),assert=require('assert');
const codec=require('../miniprogram/utils/telemetry');
let page,resolve;const ctx=new Proxy({}, {get:()=>()=>{}});let draws=0;
ctx.draw=()=>draws++;
const wx={getStorageSync:()=>'',cloud:{callFunction:()=>new Promise(r=>resolve=r)},stopPullDownRefresh(){},createCanvasContext:()=>ctx};
vm.runInNewContext(fs.readFileSync(__dirname+'/../miniprogram/pages/telemetry/telemetry.js','utf8'),{
 require:()=>codec,wx,Page:p=>page=p,setInterval:()=>1,clearInterval(){},console
});
page.setData=function(v){Object.assign(this.data,v);};
page.createSelectorQuery=()=>({select(){return this;},boundingClientRect(fn){fn({width:350});return this;},exec(){}});
(async()=>{
 page.onLoad({deviceId:'dev'});page.onShow();page.onReady();
 const sample=[1,100,127,0,1,100,...Array(22).fill(0.5)];
 resolve({result:{ok:true,serverTime:Date.now(),telemetry:{fresh:true,sampleAt:new Date(),samples:[sample]}}});
 await new Promise(r=>setImmediate(r));assert(page.data.fresh);assert.equal(page._rows.length,1);assert(page.data.metrics.length>10);
 for(let i=0;i<codec.GROUPS.length;i++)page.onGroup({detail:{value:String(i)}});
 page.refresh();page.onHide();resolve({result:{ok:true,telemetry:null}});await new Promise(r=>setImmediate(r));assert.equal(page._rows.length,1);
 page.onShow();resolve({result:{ok:true,serverTime:Date.now(),telemetry:{fresh:false,sampleAt:new Date(0),samples:[sample]}}});await new Promise(r=>setImmediate(r));assert(!page.data.fresh);assert.equal(page.data.forceText,'无新鲜数据');
 console.log('PASS telemetry page: load/render groups, stale state, hidden-page response ignored');
})().catch(e=>{console.error(e);process.exitCode=1;});
