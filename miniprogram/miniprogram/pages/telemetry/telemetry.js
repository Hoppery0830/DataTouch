const {GROUPS,validRows,value,series}=require('../../utils/telemetry');
const COLORS=['#2667d9','#e45b40','#239b70','#9254bd'];
Page({
 data:{deviceId:'',groups:GROUPS.map(g=>g.name),group:0,unit:'N',message:'等待数据',fresh:false,
  forceText:'--',adcText:'--',kText:'暂无结果',lightText:'--',latestTime:'',metrics:[],legend:[]},
 onLoad(options){let id=options.deviceId||wx.getStorageSync('lastDeviceId')||'esp32-dev01';this.setData({deviceId:id});this._rows=[];this._generation=0;},
 onShow(){this._visible=true;this.refresh();this._timer=setInterval(()=>this.refresh(),2000);},
 onReady(){this._ready=true;this.draw();},
 onHide(){this._visible=false;this._generation++;this._busy=false;clearInterval(this._timer);},
 onUnload(){this.onHide();},
 onPullDownRefresh(){this.refresh();},
 onGroup(e){const group=Number(e.detail.value);this.setData({group,unit:GROUPS[group].unit});this.draw();},
 refresh(){
  if(this._busy||!this._visible)return;this._busy=true;const generation=this._generation;
  wx.cloud.callFunction({name:'sendCommand',data:{action:'telemetry',deviceId:this.data.deviceId}}).then(res=>{
   if(!this._visible||generation!==this._generation)return;
   const result=res.result||{};if(!result.ok)throw new Error(result.message||'读取失败');
   const t=result.telemetry;
   if(!t){this._rows=[];this.setData({fresh:false,message:'尚无采集数据，请确认固件、云函数与设备号'});this.draw();return;}
   this._rows=validRows(t.samples);const last=this._rows[this._rows.length-1];
   if(!last)return;
   const age=Math.max(0,Math.round((result.serverTime-new Date(t.sampleAt).getTime())/1000));
   const fresh=t.fresh===true;
   const groups=GROUPS.slice(2,8),metrics=[];
   groups.forEach(g=>g.fields.forEach(([label,i,flag])=>{const v=value(last,i,flag);metrics.push({label,value:v===null?'--':v.toFixed(3),unit:g.unit});}));
   metrics.push({label:'2 s 有效接触样本数',value:(last[2]&4)?String(last[27]):'--',unit:'个'});
   let kText='尚无 K230 结果';
   if(last[2]&16)kText=`U_curve = ${last[20].toFixed(6)}；F4 接收序号 #${last[4]}，接收时间 F4 ${(last[5]/1000).toFixed(2)} s`;
   this.setData({fresh,message:fresh?`更新延迟约 ${age} 秒；10 Hz 曲线，最近最多 60 秒`:`数据已过期（${age} 秒），显示历史曲线`,
    forceText:fresh&&(last[2]&1)?'有效':'无新鲜数据',adcText:(last[2]&32)?'ADC 饱和':fresh&&(last[2]&2)?'有效':'无新鲜数据',
    kText,lightText:last[3]?'开启':'关闭',metrics,latestTime:`F4 ${(last[1]/1000).toFixed(2)} s · 样本 #${last[0]}`});
   this.draw();
  }).catch(e=>{if(this._visible&&generation===this._generation)this.setData({fresh:false,message:e.message||e.errMsg||'云端读取失败'});})
   .then(()=>{if(generation===this._generation){this._busy=false;wx.stopPullDownRefresh();}});
 },
 draw(){
  if(!this._ready||!this._visible)return;
  const lines=series(this._rows,GROUPS[this.data.group]);
  this.setData({legend:lines.map((s,i)=>({name:s.name,color:COLORS[i%COLORS.length]}))});
  this.createSelectorQuery().select('#curve').boundingClientRect(rect=>{
   if(!rect||!this._visible)return;
   const ctx=wx.createCanvasContext('curve',this),w=rect.width,h=280,left=52,right=w-12,top=16,bottom=h-32;
   ctx.setFillStyle('#ffffff');ctx.fillRect(0,0,w,h);
   let values=[];lines.forEach(s=>s.points.forEach(p=>{if(p.y!==null)values.push(p.y);}));
   let low=values.length?Math.min(...values):0,high=values.length?Math.max(...values):1;
   const pad=Math.max((high-low)*0.1,Math.abs(high)*0.02,0.001);low-=pad;high+=pad;
   ctx.setFontSize(10);ctx.setStrokeStyle('#e3e8ef');ctx.setFillStyle('#697789');
   for(let i=0;i<=4;i++){const y=top+(bottom-top)*i/4;ctx.beginPath();ctx.moveTo(left,y);ctx.lineTo(right,y);ctx.stroke();ctx.fillText((high-(high-low)*i/4).toPrecision(3),2,y+4);}
   const all=[].concat(...lines.map(s=>s.points)),start=all.length?Math.min(-1,...all.map(p=>p.x)):-60;
   for(let i=0;i<=4;i++){const x=left+(right-left)*i/4;ctx.fillText((start*(1-i/4)).toFixed(0)+'s',x-10,bottom+20);}
   lines.forEach((s,i)=>{ctx.setStrokeStyle(COLORS[i%COLORS.length]);ctx.setFillStyle(COLORS[i%COLORS.length]);ctx.setLineWidth(1.5);ctx.beginPath();let pen=false;
    s.points.forEach(p=>{if(p.y===null){pen=false;return;}const x=left+(p.x-start)/(-start)*(right-left),y=bottom-(p.y-low)/(high-low)*(bottom-top);
     if(this.data.group===8){ctx.moveTo(x+3,y);ctx.arc(x,y,3,0,Math.PI*2);return;}
     if(!pen||p.break)ctx.moveTo(x,y);else ctx.lineTo(x,y);pen=true;
    });ctx.stroke();
   });
   if(!values.length){ctx.setFillStyle('#697789');ctx.fillText('暂无有效数据',left+30,130);}ctx.draw();
  }).exec();
 }
});
