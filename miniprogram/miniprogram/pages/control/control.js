// Remote motor control: confirmed state comes from STM32, never command history.
const fmt = require("../../utils/fmt.js");
const COLLECTION="commands", STORAGE_KEY="lastDeviceId", DEFAULT_DEVICE_ID="esp32-dev01";
const CMDS = [
  { cmd: "arm", label: "ARM", desc: "准备参与电机", cls: "info" },
  { cmd: "start", label: "START", desc: "执行所选动作一次", cls: "ok" },
  { cmd: "stop", label: "STOP", desc: "停止当前运动", cls: "warn" },
  { cmd: "reset", label: "RESET", desc: "停止并清理会话", cls: "danger" },
];

/** 命令英文名 → 中文展示名 */
const CMD_LABEL = {
  arm: "ARM（待命）",
  start: "START（开始）",
  stop: "STOP（停止）",
  reset: "RESET（复位）",
  // 模式命令：云端 cmd 是 mode_press / mode_rub / mode_slide（只选模式，不启动）
  mode_press: "选择模式：按压",
  mode_rub: "选择模式：揉搓",
  mode_slide: "选择模式：滑动",
};

/**
 * 3 种运动模式（顺序即展示顺序）
 * · key  : "press" | "rub" | "slide"，仅用于页面内部标识
 * · cmd  : 下发给云函数的 cmd（白名单内的 mode_* 字符串）
 * · name : 中文名（与下面 MODE_NAME 映射保持一致，改一处要同步另一处）
 * · en   : 英文后缀，展示成「按压 PRESS」
 * · act  : 该模式对应的 STM32 动作（说明文字用）
 * · cls  : 配色类名，复用本页既有 cmd-info / cmd-ok / cmd-warn 三色
 */
const MODES = [
  { key: "press", cmd: "mode_press", name: "按压", en: "PRESS", act: "Z 轴下压并回位", cls: "info" },
  { key: "rub", cmd: "mode_rub", name: "揉搓", en: "RUB", act: "Z + Yaw 往复", cls: "ok" },
  { key: "slide", cmd: "mode_slide", name: "滑动", en: "SLIDE", act: "Z + X 平移", cls: "warn" },
];

/** 模式区说明文案（保证与 ESP32 / STM32 端定义一致，改动作前先同步文档） */
const MODE_NOTE_ROWS = [
  { name: "按压", desc: "Z 轴下压再回位" },
  { name: "揉搓", desc: "Z + Yaw 往复运动" },
  { name: "滑动", desc: "Z + X 方向平移" },
];
const MODE_NOTE_FOOT = "用法：先选模式（只选中，不启动），再 ARM，待准备完成后点 START。命令 60 秒内有效。";

/** mode_* → 模式中文名（"当前模式"派生用；与 MODES[].name 保持一致） */
const MODE_NAME = {
  mode_press: "按压",
  mode_rub: "揉搓",
  mode_slide: "滑动",
};


const STATE_TEXT={pending:"待发送",sent:"已下发，等待确认",accepted:"STM32 已接受",done:"已完成",failed:"失败",cancelled:"已取消",unknown:"结果未知",expired:"已过期"};
const MOTOR_STATES=["空闲","准备完成，可 START","动作运行中","动作完成","故障","正在准备电机","正在停止"];
const STAGES=["无","Z 压入","Yaw 揉搓","Yaw 回中","X 滑动","Z 返回","X 返回","动作结束"];
const RESULTS={ok:"已完成",accepted:"STM32 已接受，等待完成",busy:"设备忙",invalid:"参数非法",not_ready:"尚未 ARM 或准备未完成",fault:"电机或通信故障",cancelled:"已取消",timeout:"确认超时，执行结果未知",offline:"STM32 离线",unknown:"未知命令",expired:"已过期"};
Page({
 data:{deviceId:"",devicePlaceholder:DEFAULT_DEVICE_ID,cmds:CMDS,sendingCmd:"",modes:MODES,
  modeNoteRows:MODE_NOTE_ROWS,modeNoteFoot:MODE_NOTE_FOOT,currentModeCmd:"",currentModeText:"未知",
  modeHintText:"等待 STM32 状态",list:[],loading:false,inited:false,errMsg:"",envId:"",serverTimeText:"",hintText:"",
  motorAlive:false,motorState:-1,motorStateText:"未连接",motorStageText:"无",faultText:"",statusTimeText:""},
 _reqSeq:0,_statusSeq:0,_sendSeq:0,_visible:false,
 onLoad(){const app=getApp();let saved="";try{saved=wx.getStorageSync(STORAGE_KEY)||"";}catch(e){}
  this.setData({deviceId:String(saved||DEFAULT_DEVICE_ID),envId:(app.globalData&&app.globalData.env)||""});},
 onShow(){this._visible=true;this.refresh();this._timer=setInterval(()=>{if(!this.data.loading)this.refresh();},2000);},
 onHide(){this._visible=false;clearInterval(this._timer);this._reqSeq++;this._statusSeq++;this.setData({loading:false});},
 onUnload(){this.onHide();clearTimeout(this._debounceTimer);},
 onPullDownRefresh(){this.refresh();},
 refresh(){this.loadCommands();this.loadStatus();},
 onDeviceInput(e){const value=(e.detail.value||"").trim();this.setData({deviceId:value,motorAlive:false,currentModeCmd:"",currentModeText:"未知"});
  this._reqSeq++;this._statusSeq++;clearTimeout(this._debounceTimer);this._debounceTimer=setTimeout(()=>this.commitDeviceId(value),500);},
 onDeviceConfirm(e){clearTimeout(this._debounceTimer);this.commitDeviceId((e.detail.value||this.data.deviceId||"").trim());},
 commitDeviceId(id){this.setData({deviceId:id});try{wx.setStorageSync(STORAGE_KEY,id);}catch(e){}this.refresh();},
 loadStatus(){const deviceId=this.data.deviceId,seq=++this._statusSeq;
  if(!deviceId)return;
  wx.cloud.callFunction({name:"sendCommand",data:{deviceId,action:"status"}}).then(res=>{
   if(!this._visible || seq!==this._statusSeq)return;
   const result=res.result||{};if(!result.ok)throw new Error(result.message||"状态查询失败");
   const st=result.status,alive=!!(st&&st.alive),mode=alive&&MODES[st.mode];
   this.setData({motorAlive:alive,motorState:st?st.state:-1,motorStateText:alive?(MOTOR_STATES[st.state]||"未知"):"离线或状态已过期",
    motorStageText:alive?(STAGES[st.stage]||"未知"):"无",faultText:alive&&st.fault?"故障轴："+["X","Z","Yaw"].filter((n,i)=>st.fault&(1<<i)).join("、"):"",
    statusTimeText:st?fmt.formatTime(st.updatedAt,true):"",currentModeCmd:mode?mode.cmd:"",currentModeText:mode?mode.name:"未知",
    modeHintText:mode?"STM32 已确认模式："+mode.name:"尚未取得 STM32 确认的模式"});
  }).catch(e=>{if(this._visible&&seq===this._statusSeq)this.setData({motorAlive:false,motorStateText:e.message||e.errMsg||"状态查询失败",currentModeCmd:"",currentModeText:"未知"});});
 },
 loadCommands(){const deviceId=this.data.deviceId,seq=++this._reqSeq;
  if(!deviceId){this.setData({list:[],loading:false});wx.stopPullDownRefresh();return Promise.resolve();}
  this.setData({loading:true,errMsg:""});
  return wx.cloud.database().collection(COLLECTION).where({deviceId}).orderBy("createdAt","desc").limit(10).get().then(res=>{
   if(!this._visible||seq!==this._reqSeq)return;
   this.setData({list:(res.data||[]).map(item=>this.formatItem(item)),inited:true,hintText:"暂无命令记录",serverTimeText:fmt.formatTime(new Date(),true)});
  }).catch(e=>{if(this._visible&&seq===this._reqSeq)this.setData({errMsg:e.errMsg||e.message||"读取失败"});})
  .then(()=>{if(seq===this._reqSeq){this.setData({loading:false});wx.stopPullDownRefresh();}});
 },
 formatItem(item){const state=item.state||"pending",cmd=item.cmd||"";return {_id:item._id,idText:String(item._id||"").slice(-6),
  cmdLabel:CMD_LABEL[cmd]||cmd,stateText:STATE_TEXT[state]||state,stateClass:"st-"+state,isMode:cmd.indexOf("mode_")===0,
  timeText:fmt.formatTime(item.createdAt,true),auxText:RESULTS[item.result]|| (state==="pending"?"等待设备轮询；60 秒内有效":state==="sent"?"等待 STM32 回执":"")};},
 onModeTap(e){this.onSendCommand(e);},
 onSendCommand(e){const cmd=e.currentTarget.dataset.cmd;if(!cmd)return;
  const urgent=cmd==="stop"||cmd==="reset";
  if(this.data.sendingCmd&&!urgent)return;
  const deviceId=this.data.deviceId;if(!deviceId){wx.showToast({title:"请填写设备号",icon:"none"});return;}
  if(!urgent&&!this.data.motorAlive){wx.showToast({title:"等待 STM32 在线",icon:"none"});return;}
  if(cmd==="arm"&&!this.data.currentModeCmd){wx.showToast({title:"先选择模式并等待确认",icon:"none"});return;}
  if(cmd==="start"&&this.data.motorState!==1){wx.showToast({title:"先 ARM，等待准备完成",icon:"none"});return;}
  const seq=++this._sendSeq;this.setData({sendingCmd:cmd});
  const timer=setTimeout(()=>{if(this._visible&&seq===this._sendSeq){this.setData({sendingCmd:""});wx.showToast({title:"请求结果未知，请刷新确认",icon:"none"});}},10000);
  wx.cloud.callFunction({name:"sendCommand",data:{deviceId,cmd}}).then(res=>{
   if(!this._visible||seq!==this._sendSeq)return;
   const r=res.result||{};if(!r.ok)throw new Error(r.message||"下发失败");
   wx.showToast({title:"命令已入队，等待 STM32 确认",icon:"none"});this.refresh();
  }).catch(e=>{if(this._visible&&seq===this._sendSeq)wx.showToast({title:e.message||e.errMsg||"下发失败",icon:"none"});})
  .then(()=>{clearTimeout(timer);if(seq===this._sendSeq)this.setData({sendingCmd:""});});
 },
 onRetry(){this.refresh();}
});
