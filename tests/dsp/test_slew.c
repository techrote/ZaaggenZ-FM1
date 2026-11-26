/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void advance(zg_engine *e,unsigned frames)
{
    int16_t out[32];
    while(frames) {unsigned n=frames>32?32:frames;zg_engine_render(e,out,n);frames-=n;}
}
static void sub_patch(zg_engine *e)
{
    zg_patch p;
    zg_engine_init(e,44100);p=e->patch;
    p.bypass=1;p.motion=0;p.separation=0;p.attack=0;p.sustain=127;
    p.release=127;p.gain=127;p.level[ZG_SUB]=127;
    zg_engine_set_patch(e,&p);
    /* Isolate the clean low role without waiting for other roles' gain slews. */
    e->mute_mask=(uint8_t)(((1u<<ZG_LAYER_COUNT)-1u)^(1u<<ZG_SUB));
    zg_engine_note_on(e,36,127,0);advance(e,8192);
}
static void check_level_step(int master)
{
    zg_engine e,reference;zg_patch p;int16_t actual[32],unchanged[32],tail[1024];
    sub_patch(&e);
    /* Perform a full-range level edit at the sine peak, the worst place for an
     * unsmoothed control step. Compare actual output, not implementation fields. */
    e.sub_phase=0x40000000u;reference=e;p=e.patch;
    if(master)p.gain=0;else p.level[ZG_SUB]=0;
    zg_engine_set_patch(&e,&p);
    zg_engine_render(&reference,unchanged,32);zg_engine_render(&e,actual,32);
    assert(abs(unchanged[0])>8000);
    assert(abs((int)actual[0]-(int)unchanged[0])<200);
    advance(&e,12000);zg_engine_render(&e,tail,1024);
    for(unsigned i=0;i<1024;i++)assert(abs(tail[i])<4);
}
static void check_structural_switches(void)
{
    zg_engine base,e;int16_t out[32];zg_patch p;
    zg_engine_init(&base,44100);zg_engine_note_on(&base,36,120,0);advance(&base,10000);
    for(unsigned action=0;action<3;action++) {
        e=base;p=e.patch;int before=e.last_output;
        if(action==0)p.family=(uint8_t)((p.family+1)%ZG_FAMILY_COUNT);
        else if(action==1)p.mode=(uint8_t)((p.mode+1)%3);
        else p.bypass=(uint8_t)!p.bypass;
        zg_engine_set_patch(&e,&p);zg_engine_render(&e,out,32);
        assert(abs((int)out[0]-before)<=2);
    }
}
static void check_full_long_release(void)
{
    zg_engine e;int16_t at_one_second[1024],finished[1024];uint64_t energy=0;
    sub_patch(&e);zg_engine_note_off(&e,36);advance(&e,44100);
    zg_engine_render(&e,at_one_second,1024);
    for(unsigned i=0;i<1024;i++)energy+=(uint64_t)((int32_t)at_one_second[i]*at_one_second[i]);
    assert(energy>1000000u); /* A long role release must still be audible at 1 s. */
    advance(&e,44100*4);zg_engine_render(&e,finished,1024);
    assert(!zg_engine_active(&e));
    for(unsigned i=0;i<1024;i++)assert(abs(finished[i])<4);
}
int main(void)
{
    check_level_step(0);check_level_step(1);check_structural_switches();check_full_long_release();
    puts("Slew PASS: full-range layer/master edits at waveform peaks, source/family/bypass transition continuity, audible long release and silent completion");
    return 0;
}
