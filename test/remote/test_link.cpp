#include "remote_link.h"
#include "cloud_command.h"
#include <cassert>
#include <vector>
#include <utility>
#include <cstdio>
SerialStub Serial;
static uint32_t now=100;
uint32_t millis(){return now;}
static std::vector<std::vector<uint8_t>> sent;
static std::vector<std::pair<std::string,std::string>> receipts;
void remoteWireSend(const uint8_t *p,size_t n){sent.emplace_back(p,p+n);}
void remoteApplyStatus(const remote_status_t &,bool){}
void cloudCommandAckResult(const char *id,const char *r){receipts.emplace_back(id,r);}
static uint32_t lastSeq(){assert(sent.back()[2]==RC_REQUEST);return RC_Read32(sent.back().data()+4);}
static void ack(uint32_t seq,uint8_t value){uint8_t p[5];RC_Write32(p,seq);p[4]=value;remoteLinkFrame(RC_REPLY,p,5);}
int main(){
 remoteLinkBegin();remoteLinkSubmit("start","offline");assert(receipts.back().second=="offline");
 uint8_t status[]={RC_IDLE,255,0,7,0};remoteLinkFrame(RC_STATUS,status,5);
 remoteLinkSubmit("mode_slide","mode");remoteLinkTick();uint32_t select=lastSeq();
 assert(sent.back()[8]==RC_SELECT && sent.back()[9]==2);
 now+=501;remoteLinkTick();assert(lastSeq()==select); // same sequence retry
 ack(select,RC_COMPLETE);assert(receipts.back().second=="ok");
 remoteLinkSubmit("arm","arm");remoteLinkTick();uint32_t arm=lastSeq();ack(arm,RC_ACCEPTED);
 assert(receipts.back().second=="accepted");
 remoteLinkSubmit("start","queued_start");remoteLinkSubmit("stop","stop");
 assert(sent.back()[8]==RC_STOP);
 assert(receipts[receipts.size()-2]==std::make_pair(std::string("queued_start"),std::string("cancelled")));
 assert(receipts.back()==std::make_pair(std::string("arm"),std::string("cancelled")));
 uint32_t stop=lastSeq();ack(arm,RC_COMPLETE); // stale completion cannot complete STOP
 ack(stop,RC_COMPLETE);assert(receipts.back()==std::make_pair(std::string("stop"),std::string("ok")));
 now+=10;remoteLinkFrame(RC_STATUS,status,5);
 remoteLinkSubmit("start","run");remoteLinkTick();uint32_t start=lastSeq();ack(start,RC_ACCEPTED);
 remoteLinkSubmit("mode_rub","busy");assert(receipts.back().second=="busy");
 now+=2100;remoteLinkTick();assert(sent.back()[8]==RC_STOP);
 assert(receipts.back()==std::make_pair(std::string("run"),std::string("timeout")));
 for(auto &f:sent)assert(RC_Crc(f.data(),f.size()-1)==f.back());
 puts("PASS ESP32 transport: offline, framing, same-seq retry, accepted/final, STOP bypass/cancel, stale ACK, busy, timeout stop");
}
