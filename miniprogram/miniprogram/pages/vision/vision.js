/** @file vision.js
 * @brief 按实验显示已入库的独立视觉特征；不将 U_curve 当作舒适度分数。
 */
const fmt = require("../../utils/fmt.js");
Page({
  data:{deviceId:"",rows:[],loading:false,error:"",more:false},
  _request:0,
  onLoad(options) { this.setData({deviceId:options.deviceId||"esp32-dev01"}); this.load(true); },
  onUnload() { this._request++; },
  onPullDownRefresh() { this.load(true); },
  onReachBottom() { if(this.data.more&&!this.data.loading)this.load(false); },
  retry() { this.load(true); },
  load(reset) {
    const request=++this._request;
    const offset=reset?0:this.data.rows.length;
    this.setData({loading:true,error:""});
    return wx.cloud.callFunction({name:"sendCommand",data:{
      action:"visualResults",deviceId:this.data.deviceId,offset
    }}).then(response=>{
      if(request!==this._request)return;
      const result=response.result||{};
      if(!result.ok)throw new Error(result.message||"读取视觉结果失败");
      const rows=(result.rows||[]).map(row=>({
        ...row, valueText:Number(row.uCurve).toFixed(6),
        timeText:fmt.formatTime(row.receivedAt,true)
      }));
      this.setData({rows:reset?rows:this.data.rows.concat(rows),more:rows.length===20});
    }).catch(error=>{
      if(request===this._request)this.setData({error:error.message||error.errMsg||"读取失败"});
    }).finally(()=>{
      if(request===this._request){this.setData({loading:false});wx.stopPullDownRefresh();}
    });
  }
});
