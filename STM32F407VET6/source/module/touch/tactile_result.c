/** @file tactile_result.c
 * @brief 实验统计来自 100 ms 块均值；不冒充原始采样峰值或舒适度评分。
 */
#include "tactile_result.h"
#include <math.h>
#include <string.h>
static void put32(uint8_t *p, uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
bool Tactile_Begin(tactile_result_t *s,uint32_t seq,uint32_t epoch,uint8_t mode,uint32_t now)
{
    if(s->active || s->pending || mode>2)return false;
    memset(s,0,sizeof(*s));s->active=true;s->seq=seq;s->epoch=epoch;s->mode=mode;s->start=now;
    return true;
}
void Tactile_Stage(tactile_result_t *s,uint8_t stage,uint32_t now)
{
    if(s->stage!=stage){s->stage=stage;s->phase_ms=now;}
}
void Tactile_Sample(tactile_result_t *s,const touch_telemetry_t *t,uint32_t now)
{
    const uint8_t phases[3]={1,2,4}; /* PRESS, RUB, SLIDE; excludes every return stage. */
    if(!s->active || s->stage!=phases[s->mode] || !t->seq || t->seq==s->last_seq ||
       now-t->ms>150 || (int32_t)(t->ms-s->phase_ms)<110)return;
    if(s->last_seq && t->seq!=s->last_seq+1)s->quality|=8;
    s->last_seq=t->seq;s->frames++;
    if(t->flags&32)s->quality|=16;
    if(t->flags&1){
        s->fn++;for(unsigned i=0;i<6;i++)s->sum[i]+=t->v[i];
        float ft=t->v[6],normal=t->v[7];s->ft+=ft;s->normal+=normal;
        double d=ft-s->mean;s->mean+=d/s->fn;s->m2+=d*(ft-s->mean);
        if(s->fn==1 || ft<s->lo)s->lo=ft;
        if(s->fn==1 || ft>s->hi)s->hi=ft;
        if((t->flags&8) && normal>=0.2f){
            s->cn++;s->contact_ft+=ft;s->contact_normal+=normal;
            if(ft/normal>s->mu_peak)s->mu_peak=ft/normal;
        }
    } else s->quality|=8;
    if((t->flags&2) && !(t->flags&32)){
        float v=t->v[12];s->an++;s->rms_sq+=(double)v*v;
        if(v>s->rms_peak)s->rms_peak=v;
    } else s->quality|=8;
}
void Tactile_End(tactile_result_t *s,uint8_t outcome,uint32_t now)
{
    if(!s->active)return;
    s->active=false;s->pending=true;s->attempts=0;
    uint8_t *p=s->wire;put32(p,s->seq);put32(p+4,s->epoch);
    p[8]=1;p[9]=s->mode;p[10]=outcome;
    p[11]=s->quality|(s->fn?1:0)|(s->an?2:0)|(s->cn?4:0)|(s->frames?0:32);
    put32(p+12,now-s->start);put32(p+16,s->fn);put32(p+20,s->an);put32(p+24,s->cn);put32(p+28,s->frames);
    float v[14]={0};
    if(s->fn){for(unsigned i=0;i<6;i++)v[i]=(float)(s->sum[i]/s->fn);
        v[6]=(float)(s->normal/s->fn);v[7]=(float)(s->ft/s->fn);
        v[8]=(float)sqrt(s->m2/s->fn);v[9]=s->hi-s->lo;}
    if(s->cn){v[10]=(float)(s->contact_ft/s->contact_normal);v[11]=s->mu_peak;}
    if(s->an){v[12]=(float)sqrt(s->rms_sq/s->an);v[13]=s->rms_peak;}
    for(unsigned i=0;i<14;i++){uint32_t bits;memcpy(&bits,&v[i],4);put32(p+32+4*i,bits);}
}
bool Tactile_Take(tactile_result_t *s,uint32_t now,uint8_t out[TACTILE_RESULT_SIZE])
{
    if(!s->pending || s->attempts>=10 || (s->attempts && now-s->sent_ms<1000))return false;
    memcpy(out,s->wire,TACTILE_RESULT_SIZE);s->attempts++;s->sent_ms=now;return true;
}
void Tactile_Ack(tactile_result_t *s,const uint8_t ack[9])
{
    if(s->pending && !ack[8] && !memcmp(s->wire,ack,8))s->pending=false;
}
