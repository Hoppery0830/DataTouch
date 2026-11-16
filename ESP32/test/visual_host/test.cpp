#include "visual_result.h"
#include "remote_protocol.h"
#include "Preferences.h"
#include <cassert>
#include <vector>
uint32_t fake_now=0;
static std::vector<uint8_t> last;
void remoteWireSend(const uint8_t *p,size_t n){last.assign(p,p+n);}
static void receive(uint32_t seq,bool accepted=true) {
 uint8_t p[12]={};RC_Write32(p,seq);RC_Write32(p+4,2);
 float v=0.5f;memcpy(p+8,&v,4);
 visualFrame(p,12);visualStore("dev");visualTick();
 assert(last.size()==14 && last[2]==0x23 && (last[12]==0)==accepted);
}
int main(){
 visualBegin();
 flash_fail=true;receive(1,false);flash_fail=false;receive(1);
 String device,token;
 assert(visualNext(device,token).length() && token=="0000000100000002");
 receive(1); // retransmission does not create another entry
 visualUploaded(device,"wrong");
 for(int i=0;i<4;i++){fake_now+=40000;assert(visualNext(device,token).length());}
 fake_now+=40000;assert(!visualNext(device,token).length()); // bounded retry
 visualBegin(); // power cycle reloads pending result; retries resume this boot
 assert(visualNext(device,token).length());
 visualUploaded(device,token);receive(1);
 fake_now+=40000;assert(!visualNext(device,token).length());
 // Eight unsent results occupy all slots; ninth is explicitly rejected.
 for(uint32_t i=2;i<=9;i++)receive(i);
 receive(10,false);
 puts("PASS ESP32 visual outbox: durable receipt, duplicate, finite retry, reboot, full queue");
}
