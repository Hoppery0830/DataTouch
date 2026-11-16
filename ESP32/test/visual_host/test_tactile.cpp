#include "tactile_result.h"
#include "remote_protocol.h"
#include "Preferences.h"
#include <cassert>
#include <vector>
uint32_t fake_now=0;
static std::vector<uint8_t> last;
void remoteWireSend(const uint8_t *p,size_t n){last.assign(p,p+n);}
static void receive(uint32_t seq,bool accepted=true) {
 uint8_t p[88]={};RC_Write32(p,seq);RC_Write32(p+4,2);
 p[8]=1;p[9]=0;p[10]=0;p[11]=7;
 RC_Write32(p+12,1000);for(unsigned i=16;i<=28;i+=4)RC_Write32(p+i,2);
 float v=0.5f;for(unsigned i=0;i<14;i++)memcpy(p+32+4*i,&v,4);
 tactileFrame(p,88);tactileStore("dev");tactileTick();
 assert(last.size()==14 && last[2]==0x25 && (last[12]==0)==accepted);
}
int main(){
 tactileBegin();
 flash_fail=true;receive(1,false);flash_fail=false;receive(1);
 String device,token;
 assert(tactileNext(device,token).length() && token=="0000000100000002");
 receive(1); // retransmission does not create another entry
 tactileUploaded(device,"wrong");
 for(int i=0;i<4;i++){fake_now+=40000;assert(tactileNext(device,token).length());}
 fake_now+=40000;assert(!tactileNext(device,token).length()); // bounded retry
 tactileBegin(); // power cycle reloads pending result; retries resume this boot
 assert(tactileNext(device,token).length());
 tactileUploaded(device,token);receive(1);
 fake_now+=40000;assert(!tactileNext(device,token).length());
 // Eight unsent results occupy all slots; ninth is explicitly rejected.
 for(uint32_t i=2;i<=9;i++)receive(i);
 receive(10,false);
 puts("PASS ESP32 tactile outbox: durable receipt, duplicate, finite retry, reboot, full queue");
}
