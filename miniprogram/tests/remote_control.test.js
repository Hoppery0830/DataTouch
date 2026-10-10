// In-memory SDK harness: exercises actual cloud handlers without cloud access.
const fs=require('fs'),vm=require('vm'),path=require('path'),assert=require('assert');
const root=path.resolve(__dirname,'..');let store={commands:[],device_status:[]},next=0,failStatus=false;
const ops={};for(const op of ['lt','lte','gte','in'])ops[op]=value=>({op,value});
function matches(doc,filter){return Object.entries(filter).every(([k,v])=>{
 if(v&&v.op){const x=doc[k];switch(v.op){case 'lt':return x<v.value;case 'lte':return x<=v.value;case 'gte':return x>=v.value;case 'in':return v.value.includes(x);}}
 return doc[k]===v;
});}
function query(name,filter={}){let order='',direction=1,max=Infinity;
 const api={where(f){return query(name,f);},orderBy(k,d){order=k;direction=d==='asc'?1:-1;return api;},limit(n){max=n;return api;},
  async get(){let data=(store[name]||[]).filter(x=>matches(x,filter));if(order)data.sort((a,b)=>(a[order]>b[order]?1:a[order]<b[order]?-1:0)*direction);return {data:data.slice(0,max).map(x=>({...x}))};},
  async update({data}){let n=0;for(const doc of store[name]||[])if(matches(doc,filter)){Object.assign(doc,data);n++;}return {stats:{updated:n}};},
  async add({data}){const id=String(++next);store[name].push({...data,_id:id});return {_id:id};},
  doc(id){return {async set({data}){if(failStatus)throw Error('missing collection');store[name]=store[name].filter(x=>x._id!==id);store[name].push({...data,_id:id});}};}
 };return api;
}
const db={command:ops,collection:query,serverDate:()=>new Date()};
const cloud={init(){},DYNAMIC_CURRENT_ENV:'test',database:()=>db,getWXContext:()=>({OPENID:'test-user'})};
function handler(name){const module={exports:{}};const context={exports:module.exports,require:n=>{assert.equal(n,'wx-server-sdk');return cloud;},console:{log(){},warn(){},error(){}},Date};
 vm.runInNewContext(fs.readFileSync(path.join(root,'cloudfunctions',name,'index.js'),'utf8'),context);return context.exports.main;}
const send=handler('sendCommand'),device=handler('deviceCommand');
const poll=(extra={})=>device({action:'poll',token:'REPLACE_ME',deviceId:'dev',...extra});
const ack=(id,result,deviceId='dev')=>device({action:'ack',token:'REPLACE_ME',deviceId,id,result});
(async()=>{
 assert.equal((await send({deviceId:'dev',cmd:'unsafe'})).ok,false);
 assert.equal((await device({action:'poll',deviceId:'dev',token:'wrong'})).ok,false);
 for(const cmd of ['mode_press','arm','start','arm','start'])assert((await send({deviceId:'dev',cmd})).ok);
 let p=await poll();assert.equal(p.commands.length,4);assert.equal(store.commands.filter(x=>x.state==='pending').length,1);
 const first=p.commands[0].id;assert((await ack(first,'accepted')).ok);assert.equal(store.commands[0].state,'accepted');
 await ack(first,'busy');assert.equal(store.commands[0].state,'failed');
 await ack(first,'accepted');assert.equal(store.commands[0].state,'failed'); // no regression
 assert.equal((await ack(first,'ok','other')).ok,false);
 const second=p.commands[1].id;await ack(second,'ok');assert.equal(store.commands[1].state,'done');
 const status={alive:true,state:1,mode:2,stage:0,online:7,fault:0};
 await poll({status});let live=await send({deviceId:'dev',action:'status'});assert(live.ok&&live.status.alive&&live.status.mode===2);
 store.device_status[0].updatedAt=new Date(Date.now()-16000);live=await send({deviceId:'dev',action:'status'});assert(!live.status.alive);
 store.commands=[];
 await send({deviceId:'dev',cmd:'start'});await send({deviceId:'dev',cmd:'stop'});
 p=await poll();assert.equal(p.commands.length,1);assert.equal(p.commands[0].cmd,'stop');assert.equal(store.commands[0].state,'cancelled');
 // Snapshot write failure must never block STOP delivery.
 await send({deviceId:'dev',cmd:'stop'});failStatus=true;p=await poll({status});assert.equal(p.commands[0].cmd,'stop');failStatus=false;
 // Sent commands are never replayed after reboot/lost ACK.
 p=await poll();assert.equal(p.commands.length,0);
 store.commands.push({_id:'old',deviceId:'dev',cmd:'start',state:'sent',sentAt:new Date(Date.now()-210000)});
 await poll();assert.equal(store.commands.find(x=>x._id==='old').state,'unknown');
 console.log('PASS cloud: validation, max batch=4, claims, STOP priority/cancel, accepted/final monotonic receipts, device isolation, live/stale status, status failure, no START replay');
})().catch(e=>{console.error(e);process.exitCode=1;});
