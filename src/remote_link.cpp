// Reliable bounded control transport. Retransmit only the SAME sequence;
// terminal timeout is uncertain, never generates a replacement START.
#include "remote_link.h"
#include "cloud_command.h"
#include <esp_system.h>
#include <string.h>
namespace {
struct Command {uint32_t seq;uint8_t op,mode;char id[CLOUD_CMD_ID_LEN];};
Command pending{},queue[8];unsigned count=0;
bool active=false,accepted=false,seen=false;
uint32_t serial=0,sent=0,began=0,lastRx=0,lastBeat=0;
remote_status_t status{RC_IDLE,255,0,0,0};
void report(const Command &c,const char *result){
 if(c.id[0])cloudCommandAckResult(c.id,result);
 Serial.printf("[STM32] seq=%lu op=%u result=%s\n",(unsigned long)c.seq,c.op,result);
}
void transmit(){uint8_t p[6],b[16];RC_Write32(p,pending.seq);p[4]=pending.op;p[5]=pending.mode;
 remoteWireSend(b,RC_Build(b,RC_REQUEST,p,6));sent=millis();}
void begin(const Command &c){pending=c;active=true;accepted=false;began=millis();transmit();}
void clear(){for(unsigned i=0;i<count;i++)report(queue[i],"cancelled");count=0;}
const char *resultName(uint8_t r){switch(r){case RC_ACCEPTED:return "accepted";case RC_COMPLETE:return "ok";case RC_BUSY:return "busy";case RC_INVALID:return "invalid";case RC_NOT_READY:return "not_ready";case RC_CANCELLED:return "cancelled";default:return "fault";}}
}
void remoteLinkBegin(){serial=esp_random();if(!serial)serial=1;lastBeat=millis()-500;}
void remoteLinkSubmit(const String &cmd,const char *id){
 Command c{};c.seq=++serial;if(!c.seq)c.seq=++serial;
 if(id)strlcpy(c.id,id,sizeof(c.id));
 if(cmd=="mode_press" || cmd=="mode_rub" || cmd=="mode_slide"){
  c.op=RC_SELECT;c.mode=cmd=="mode_press"?0:cmd=="mode_rub"?1:2;
 }else if(cmd=="arm")c.op=RC_ARM;
 else if(cmd=="start")c.op=RC_START;
 else if(cmd=="stop")c.op=RC_STOP;
 else if(cmd=="reset")c.op=RC_RESET;
 else{report(c,"unknown");return;}
 if(c.op==RC_STOP || c.op==RC_RESET){
  clear();if(active)report(pending,"cancelled");begin(c);return;
 }
 if(!seen || millis()-lastRx>2000){report(c,"offline");return;}
 if(active && pending.op==RC_START){report(c,"busy");return;}
 if(count==8){report(c,"busy");return;}
 queue[count++]=c;
}
void remoteLinkFrame(uint8_t type,const uint8_t *p,uint8_t len){
 if(type==RC_STATUS && len==5){
  status={p[0],p[1],p[2],p[3],p[4]};lastRx=millis();seen=true;
  remoteApplyStatus(status,true);
 }else if(type==RC_REPLY && len==5){
  lastRx=millis();
  if(!active || RC_Read32(p)!=pending.seq)return;
  if(p[4]>RC_CANCELLED)return;
  if(p[4]==RC_ACCEPTED){if(!accepted)report(pending,"accepted");accepted=true;}
  else{report(pending,resultName(p[4]));active=false;if(p[4]!=RC_COMPLETE)clear();}
 }
}
void remoteLinkTick(){
 uint32_t now=millis();
 if(now-lastBeat>=500){uint8_t b[8];remoteWireSend(b,RC_Build(b,RC_HEARTBEAT,nullptr,0));lastBeat=now;}
 bool alive=seen && now-lastRx<=2000;
 if(!alive && seen){seen=false;remoteApplyStatus(status,false);}
 if(active){
  uint32_t limit=accepted?(pending.op==RC_START?180000U:20000U):2000U;
  if(now-began>limit || (!alive && accepted)){
   bool moving=pending.op==RC_START || pending.op==RC_ARM;
   report(pending,"timeout");active=false;clear();
   if(moving){Command stop{};stop.seq=++serial;stop.op=RC_STOP;begin(stop);}
  }else if(now-sent>=500){
   // Poll the cached result with the original request; duplicate START never restarts.
   transmit();
  }
 }
 if(!active && count){Command c=queue[0];for(unsigned i=1;i<count;i++)queue[i-1]=queue[i];count--;
  if(alive)begin(c);else report(c,"offline");
 }
}
