#include "visual_result.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void)
{
    visual_result_t s={0};
    uint8_t data[12]={1,0,0,0,2,0,0,0,0,0,0,0x3f},out[12],ack[9]={1,0,0,0,2};
    assert(Visual_Accept(&s,data,1,2,false)==1);
    assert(Visual_Accept(&s,data,9,2,true)==1);
    assert(!Visual_Accept(&s,data,1,2,true));
    assert(!Visual_Accept(&s,data,1,2,true));
    data[11]=0x40;assert(Visual_Accept(&s,data,1,2,true)==1);data[11]=0x3f;
    data[0]=3;assert(Visual_Accept(&s,data,3,2,true)==2);data[0]=1;
    for(unsigned i=0;i<10;i++){
        assert(Visual_Take(&s,0xffffff00U+i*500,out));
        assert(!memcmp(data,out,12));
        assert(!Visual_Take(&s,0xffffff01U+i*500,out));
    }
    assert(!Visual_Take(&s,6000,out)&&s.pending);
    ack[0]=9;Visual_Ack(&s,ack);assert(s.pending);
    ack[0]=1;ack[8]=1;Visual_Ack(&s,ack);assert(s.pending);
    ack[8]=0;Visual_Ack(&s,ack);assert(!s.pending);
    assert(!Visual_Accept(&s,data,1,2,false)); // late duplicate after cancellation
    assert(!s.pending); // never creates another experiment
    data[0]=3;assert(!Visual_Accept(&s,data,3,2,true));
    data[11]=0x7f;data[10]=0x80;assert(Visual_Accept(&s,data,3,2,true)==1);
    puts("PASS visual result: token, duplicate/conflict, backpressure, finite retry/wrap, durable ACK");
}
