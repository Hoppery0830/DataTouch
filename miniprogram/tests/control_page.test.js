const fs=require('fs'),vm=require('vm'),path=require('path'),assert=require('assert');
let page,calls=[],response={ok:true,status:{alive:true,state:1,mode:2,stage:0,online:7,fault:0,updatedAt:new Date()}};
const wx={cloud:{callFunction:q=>{calls.push(q);return Promise.resolve({result:response});}},showToast(){},stopPullDownRefresh(){}};
vm.runInNewContext(fs.readFileSync(path.join(__dirname,'../miniprogram/pages/control/control.js'),'utf8'),{
 require:()=>({formatTime:()=>''}),wx,Page:p=>page=p,setTimeout:()=>1,clearTimeout(){},setInterval:()=>1,clearInterval(){},getApp:()=>({globalData:{}})
});
page.setData=function(p){Object.assign(this.data,p)};page.data.deviceId='dev';page._visible=true;
page.refresh=()=>{};
(async()=>{
 await page.loadStatus();await new Promise(r=>setImmediate(r));
 assert.equal(page.data.currentModeCmd,'mode_slide');assert.equal(page.data.motorState,1);
 const event=cmd=>({currentTarget:{dataset:{cmd}}});
 page.onSendCommand(event('start'));assert.equal(calls[calls.length-1].data.cmd,'start');
 await new Promise(r=>setImmediate(r));page.data.motorState=0;let n=calls.length;page.onSendCommand(event('start'));assert.equal(calls.length,n);
 page.data.motorAlive=false;page.data.sendingCmd='arm';page.onSendCommand(event('stop'));assert.equal(calls[calls.length-1].data.cmd,'stop');
 response={ok:true,status:{alive:false,state:1,mode:2,stage:0,updatedAt:new Date()}};
 await page.loadStatus();await new Promise(r=>setImmediate(r));assert.equal(page.data.currentModeCmd,'');assert(!page.data.motorAlive);
 assert.equal(page.formatItem({state:'failed',result:'busy'}).stateText,'失败');
 console.log('PASS mini program: authoritative mode, ARMED gate, STOP bypasses busy/offline UI, stale mode cleared, failure label');
})().catch(e=>{console.error(e);process.exitCode=1;});
