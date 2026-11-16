/** @file tactile.js
 * @brief 每次实验的触觉摘要与同令牌视觉指标，缺失值不当成零展示。
 */
const fmt=require('../../utils/fmt');
const FIELDS=[['平均 Fx','N',1],['平均 Fy','N',1],['平均 Fz','N',1],['平均 Mx','N·m',1],['平均 My','N·m',1],['平均 Mz','N·m',1],['平均法向力','N',1],['平均切向力','N',1],['切向力块均值标准差','N',1],['切向力块均值峰峰值','N',1],['有效接触摩擦均值比','',4],['摩擦比块峰值','',4],['振动 RMS','mg',2],['100 ms 振动 RMS 峰值','mg',2]];
Page({
 data:{deviceId:'',rows:[],loading:false,error:'',more:false},
 _request:0,
 onLoad(o){this.setData({deviceId:o.deviceId||'esp32-dev01'});this.load(true);},
 onUnload(){this._request++;},
 onPullDownRefresh(){this.load(true);},
 onReachBottom(){if(this.data.more&&!this.data.loading)this.load(false);},
 retry(){this.load(true);},
 load(reset){
  const request=++this._request,offset=reset?0:this.data.rows.length;
  this.setData({loading:true,error:''});
  return wx.cloud.callFunction({name:'sendCommand',data:{action:'tactileResults',deviceId:this.data.deviceId,offset}}).then(res=>{
   if(request!==this._request)return;
   const result=res.result||{};if(!result.ok)throw new Error(result.message||'读取失败');
   const rows=(result.rows||[]).map(row=>{
    const r=row.result,q=r.quality;
    const warnings=[];
    if(q&8)warnings.push('部分数据缺失');if(q&16)warnings.push('振动采集饱和');
    if(q&32)warnings.push('有效阶段不足或无采样');if(!(q&1))warnings.push('无有效力数据');if(!(q&2))warnings.push('无有效振动数据');if(!(q&4))warnings.push('无有效接触');
    return {...row,modeText:['按压 · 压入阶段','揉搓 · 揉搓阶段','滑动 · 滑动阶段'][r.mode],
     outcomeText:r.outcome?'实验中断':'动作完成',qualityText:warnings.join('；')||'采样有效',
     timeText:fmt.formatTime(row.receivedAt,true),durationText:(r.durationMs/1000).toFixed(2),
     visualText:row.visual?Number(row.visual.uCurve).toFixed(6):'尚未收到同一实验的视觉结果',
     metrics:FIELDS.map(([label,unit,flag],i)=>({label,unit,value:(q&flag)&&Number.isFinite(r.values[i])?r.values[i].toFixed(3):'--'}))};
   });
   this.setData({rows:reset?rows:this.data.rows.concat(rows),more:rows.length===20});
  }).catch(e=>{if(request===this._request)this.setData({error:e.message||e.errMsg||'读取失败'});})
  .finally(()=>{if(request===this._request){this.setData({loading:false});wx.stopPullDownRefresh();}});
 }
});
