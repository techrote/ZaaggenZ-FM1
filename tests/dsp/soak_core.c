/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#define INSTANCES 4u
#define RATE 44100u
static double wall_seconds(void)
{
#ifdef _WIN32
    LARGE_INTEGER value,frequency;QueryPerformanceCounter(&value);QueryPerformanceFrequency(&frequency);
    return (double)value.QuadPart/(double)frequency.QuadPart;
#else
    struct timespec t;timespec_get(&t,TIME_UTC);return (double)t.tv_sec+(double)t.tv_nsec/1000000000.;
#endif
}
static uint32_t next_random(uint32_t *state)
{*state=*state*1664525u+1013904223u;return *state;}
static void fail(const char *what,unsigned instrument,uint64_t frame)
{fprintf(stderr,"Soak failed: %s, instrument=%u, frame=%llu\n",what,instrument,(unsigned long long)frame);exit(1);}
int main(int argc,char **argv)
{
    unsigned seconds=1800;
    if(argc==3&&!strcmp(argv[1],"--seconds")) {
        char *end;long n=strtol(argv[2],&end,10);
        if(!*argv[2]||*end||n<1||n>3600){fputs("--seconds must be 1..3600\n",stderr);return 2;}
        seconds=(unsigned)n;
    } else if(argc!=1){fputs("usage: soak-core [--seconds 1..3600]\n",stderr);return 2;}
    zg_engine engine[INSTANCES];
    int16_t block[ZG_BLOCK];
    uint64_t hash[INSTANCES],sum_squares[INSTANCES]={0},clipped[INSTANCES]={0};
    uint32_t random[INSTANCES],next_event[INSTANCES],off_event[INSTANCES],events[INSTANCES]={0};
    unsigned peak[INSTANCES]={0},preset_mask[2]={0},mode_mask=0,tuning_mask=0;
    const uint32_t musical_frames=seconds*RATE;
    const uint32_t final_frames=RATE*5;
    double began=wall_seconds();
    for(unsigned t=0;t<INSTANCES;t++) {
        zg_engine_init(&engine[t],RATE);zg_engine_set_preset(&engine[t],t*7);
        hash[t]=1469598103934665603ull;random[t]=0x534f414bu+t*331u;
        next_event[t]=t*211u;off_event[t]=UINT32_MAX;
    }
    for(uint32_t at=0;at<musical_frames+final_frames;at+=ZG_BLOCK) {
        unsigned frames=musical_frames+final_frames-at;
        if(frames>ZG_BLOCK)frames=ZG_BLOCK;
        for(unsigned t=0;t<INSTANCES;t++) {
            zg_engine *e=&engine[t];
            if(at<musical_frames) {
                if(at>=next_event[t]) {
                    uint32_t bits=next_random(&random[t]);unsigned event=events[t]++;
                    if(event%8u==0) {
                        unsigned preset=(event/8u+t*7u)%zg_preset_count;
                        zg_engine_set_preset(e,preset);preset_mask[preset/32u]|=1u<<(preset%32u);
                        zg_patch p=e->patch;
                        p.mode=(uint8_t)((event/8u+t)%3u);p.source=(uint8_t)((event/8u+t)%zg_source_count);
                        p.quality=(uint8_t)((event/8u)&1u);p.tuning=(uint8_t)((event/8u+t)%4u);
                        p.roll=(uint8_t)((event/8u+t)%7u);
                        zg_engine_set_patch(e,&p);mode_mask|=1u<<p.mode;tuning_mask|=1u<<p.tuning;
                    }
                    zg_engine_set_macro(e,ZG_GRIT,bits&127u);
                    zg_engine_set_macro(e,ZG_MOTION,(bits>>8)&127u);
                    zg_engine_set_macro(e,ZG_VOWEL,(bits>>16)&127u);
                    zg_engine_pitch_bend(e,(int)(bits%401u)-200);
                    zg_engine_note_on(e,26u+(bits%35u),50u+((bits>>20)%78u),(event%5u)==0u);
                    uint32_t duration=7000u+((bits>>12)%11000u);
                    next_event[t]=at+duration;off_event[t]=at+duration*3u/4u;
                } else if(at>=off_event[t]&&e->gate) {
                    zg_engine_note_off(e,e->note);off_event[t]=UINT32_MAX;
                }
            } else if(e->gate)zg_engine_all_notes_off(e);
            zg_engine_render(e,block,frames);
            for(unsigned i=0;i<frames;i++) {
                int32_t x=block[i];unsigned magnitude=(unsigned)(x<0?-x:x);
                if(magnitude>32760u)fail("output exceeded explicit PCM policy",t,(uint64_t)at+i);
                if(magnitude>peak[t])peak[t]=magnitude;
                if(magnitude==32760u)clipped[t]++;
                sum_squares[t]+=(uint64_t)(x*x);
                hash[t]^=(uint16_t)block[i];hash[t]*=1099511628211ull;
                if(at>=musical_frames+final_frames-1024u&&magnitude>16u)
                    fail("release failed to reach silence",t,(uint64_t)at+i);
            }
        }
    }
    for(unsigned t=0;t<INSTANCES;t++) {
        if(zg_engine_active(&engine[t]))fail("active after complete release",t,musical_frames+final_frames);
        if(!sum_squares[t])fail("silent instrument",t,0);
    }
    if(seconds>=60u&&(preset_mask[0]!=UINT32_MAX||preset_mask[1]!=15u||mode_mask!=7u||tuning_mask!=15u))
        fail("incomplete scenario coverage",0,0);
    printf("{\"method\":\"actual-c-four-instance-soak-v1\",\"simulated_seconds_per_instance\":%u,\"release_seconds\":5,\"instances\":4,\"total_rendered_samples\":%llu,\"elapsed_wall_seconds\":%.6f,\"hardware_qualified\":false,\"preset_mask\":[%u,%u],\"mode_mask\":%u,\"tuning_mask\":%u,\"tracks\":[",seconds,(unsigned long long)(musical_frames+final_frames)*INSTANCES,wall_seconds()-began,preset_mask[0],preset_mask[1],mode_mask,tuning_mask);
    for(unsigned t=0;t<INSTANCES;t++)printf("%s{\"instrument\":%u,\"events\":%u,\"peak\":%u,\"clipped_samples\":%llu,\"sum_squares\":%llu,\"pcm_fnv64\":\"%016llx\"}",t?",":"",t,events[t],peak[t],(unsigned long long)clipped[t],(unsigned long long)sum_squares[t],(unsigned long long)hash[t]);
    puts("]}");return 0;
}
