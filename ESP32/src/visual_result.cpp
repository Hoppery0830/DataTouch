/** @file visual_result.cpp
 * @brief 八条 NVS 结果槽；持久化后确认 F4，云端确认后释放。满队列拒绝，不覆盖未上传结果。
 */
#include "visual_result.h"
#include "remote_link.h"
#include "remote_protocol.h"
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <math.h>
namespace {
struct Incoming { uint8_t bytes[12]; };
struct Receipt { uint8_t bytes[9]; };
struct Entry {
    uint8_t wire[12];
    char device[65];
    uint8_t state; // 0 empty, 1 awaiting database confirmation, 2 delivered
};
struct Store { uint32_t magic; Entry rows[8]; } records{};
QueueHandle_t incoming=nullptr, receipts=nullptr;
Preferences nvs;
bool storage=false;
uint8_t attempts[8]{};
uint32_t nextTry[8]{};
bool persist() { return storage && nvs.putBytes("results",&records,sizeof(records))==sizeof(records); }
String tokenOf(const Entry &e) {
    char b[17];snprintf(b,sizeof(b),"%08lx%08lx",(unsigned long)RC_Read32(e.wire),(unsigned long)RC_Read32(e.wire+4));return String(b);
}
}
void visualBegin()
{
    memset(attempts,0,sizeof(attempts));memset(nextTry,0,sizeof(nextTry));
    incoming=xQueueCreate(8,sizeof(Incoming));receipts=xQueueCreate(8,sizeof(Receipt));
    storage=nvs.begin("vision-v1",false);
    if(storage && nvs.getBytesLength("results")==sizeof(records)) nvs.getBytes("results",&records,sizeof(records));
    if(records.magic!=0x56533131) { memset(&records,0,sizeof(records));records.magic=0x56533131; }
}
void visualFrame(const uint8_t *p,uint8_t len)
{
    if(len!=12 || !incoming) return;
    Incoming msg;memcpy(msg.bytes,p,12);(void)xQueueSend(incoming,&msg,0);
}
void visualTick()
{
    Receipt msg; if(!receipts || xQueueReceive(receipts,&msg,0)!=pdTRUE)return;
    uint8_t wire[14];remoteWireSend(wire,RC_Build(wire,0x23,msg.bytes,9));
}
void visualStore(const String &deviceId)
{
    if(!incoming || !receipts || !storage || deviceId.isEmpty() || deviceId.length()>64)return;
    Incoming msg;
    for(unsigned budget=0;budget<8 && xQueueReceive(incoming,&msg,0)==pdTRUE;budget++) {
        float value;uint32_t bits=RC_Read32(msg.bytes+8);memcpy(&value,&bits,4);
        Receipt ack{};memcpy(ack.bytes,msg.bytes,8);ack.bytes[8]=1;
        int found=-1,slot=-1;
        for(int i=0;i<8;i++) {
            if(records.rows[i].state && !strcmp(records.rows[i].device,deviceId.c_str()) &&
                !memcmp(records.rows[i].wire,msg.bytes,8))found=i;
            if(records.rows[i].state==0 && slot<0)slot=i;
        }
        if(found>=0) {
            if(!memcmp(records.rows[found].wire,msg.bytes,12))ack.bytes[8]=0;
        } else if(isfinite(value) && value>=0 && value<=1e6f) {
            if(slot<0)for(int i=0;i<8;i++)if(records.rows[i].state==2){slot=i;break;}
            if(slot>=0) {
                Entry old=records.rows[slot];
                Entry e{};memcpy(e.wire,msg.bytes,12);strlcpy(e.device,deviceId.c_str(),sizeof(e.device));e.state=1;
                records.rows[slot]=e;
                if(persist()){ack.bytes[8]=0;attempts[slot]=0;nextTry[slot]=0;}
                else records.rows[slot]=old;
            }
        }
        (void)xQueueSend(receipts,&ack,0);
        if(ack.bytes[8])Serial.println("[VISION] storage full/failed or conflicting result; no acceptance");
    }
}
String visualNext(String &deviceId,String &token)
{
    uint32_t now=millis();
    for(int i=0;i<8;i++) {
        Entry &e=records.rows[i];
        if(e.state!=1 || attempts[i]>=5 || (attempts[i] && (int32_t)(now-nextTry[i])<0))continue;
        attempts[i]++;nextTry[i]=now+2000U*(1U<<(attempts[i]-1));
        if(attempts[i]==5)Serial.println("[VISION] final upload attempt this boot; unconfirmed result remains in NVS");
        float value;uint32_t bits=RC_Read32(e.wire+8);memcpy(&value,&bits,4);
        deviceId=String(e.device);token=tokenOf(e);
        char payload[180];
        snprintf(payload,sizeof(payload),"{\"token\":\"%s\",\"value\":%.9g,\"algorithm\":\"gunay-full-curve-v1\"}",token.c_str(),(double)value);
        return String(payload);
    }
    return String();
}
void visualUploaded(const String &deviceId,const String &token)
{
    for(int i=0;i<8;i++) {
        Entry &e=records.rows[i];
        if(e.state==1 && deviceId==e.device && token==tokenOf(e)) {
            e.state=2;if(!persist())e.state=1;
            else Serial.printf("[VISION] database confirmed experiment=%s\n",token.c_str());
            return;
        }
    }
}
