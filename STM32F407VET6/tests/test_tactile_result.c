#include "tactile_result.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static float val(const uint8_t *w,unsigned i){float v;memcpy(&v,w+32+4*i,4);return v;}
static touch_telemetry_t sample(uint32_t seq,uint32_t ms,float ft){touch_telemetry_t t={0};t.seq=seq;t.ms=ms;t.flags=11;t.v[6]=ft;t.v[7]=2;t.v[12]=3;return t;}
int main(void){
 tactile_result_t s={0};uint8_t wire[88],ack[9]={0};
 assert(Tactile_Begin(&s,42,500,1,100));
 Tactile_Stage(&s,1,100);touch_telemetry_t t=sample(1,300,100);Tactile_Sample(&s,&t,300);assert(!s.frames);
 Tactile_Stage(&s,2,400);t=sample(2,500,100);Tactile_Sample(&s,&t,500);assert(!s.frames);
 t=sample(3,600,2);Tactile_Sample(&s,&t,600);Tactile_Sample(&s,&t,600);assert(s.frames==1);
 t=sample(4,700,4);Tactile_Sample(&s,&t,700);
 Tactile_Stage(&s,3,710);t=sample(5,900,100);Tactile_Sample(&s,&t,900);assert(s.frames==2);
 Tactile_End(&s,0,1000);assert(s.pending&&s.wire[11]==7);assert(!Tactile_Begin(&s,43,501,0,1100));
 assert(fabsf(val(s.wire,7)-3)<1e-5 && fabsf(val(s.wire,8)-1)<1e-5 && fabsf(val(s.wire,10)-1.5f)<1e-5);
 assert(Tactile_Take(&s,1000,wire));assert(!Tactile_Take(&s,1500,wire));
 for(unsigned i=1;i<10;i++)assert(Tactile_Take(&s,1000+1000*i,wire));
 assert(!Tactile_Take(&s,12000,wire));
 memcpy(ack,wire,8);ack[8]=1;Tactile_Ack(&s,ack);assert(s.pending);
 ack[8]=0;ack[0]++;Tactile_Ack(&s,ack);assert(s.pending);ack[0]--;Tactile_Ack(&s,ack);assert(!s.pending);
 assert(Tactile_Begin(&s,43,501,0,13000));Tactile_End(&s,1,13020);assert(s.wire[10]==1&&(s.wire[11]&32));
 memcpy(ack,s.wire,8);Tactile_Ack(&s,ack);
 assert(Tactile_Begin(&s,44,502,2,14000));Tactile_Stage(&s,4,14000);
 t=sample(10,14200,5);t.flags=32;Tactile_Sample(&s,&t,14200);Tactile_End(&s,0,14300);
 assert(s.wire[11]==24 && !s.fn && !s.an);
 puts("PASS tactile: stage boundaries, return exclusion, duplicate, statistics, missing/saturation, interrupted, bounded retry and exact ACK");
}
