/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static unsigned checks;
#define CHECK(x) do {assert(x);checks++;} while(0)
static void render(zg_engine*e,unsigned samples)
{ int16_t b[32];while(samples){unsigned n=samples>32?32:samples;zg_engine_render(e,b,n);samples-=n;} }
static uint64_t fingerprint(zg_engine*e,unsigned frames)
{int16_t b[32];uint64_t h=1469598103934665603ull;while(frames){unsigned n=frames>32?32:frames;zg_engine_render(e,b,n);for(unsigned i=0;i<n;i++){h^=(uint16_t)b[i];h*=1099511628211ull;}frames-=n;}return h;}
static void deterministic_checkpoint(void)
{
    zg_engine a,b;int16_t x[256],y[256];zg_engine_init(&a,44100);zg_engine_set_preset(&a,18);
    zg_engine_note_on(&a,36,110,0);render(&a,8192);b=a;
    zg_engine_render(&a,x,256);zg_engine_render(&b,y,256);
    CHECK(!memcmp(x,y,sizeof x));CHECK(!memcmp(&a,&b,sizeof a));
    zg_engine_note_off(&a,36);b=a;zg_engine_render(&a,x,256);zg_engine_render(&b,y,256);
    CHECK(!memcmp(x,y,sizeof x));CHECK(a.native.phase!=0);
}
static void phase_release_and_glide(void)
{
    zg_engine e;zg_engine_init(&e,44100);e.patch.mode=0;e.patch.attack=1;e.patch.sustain=127;e.patch.release=127;
    zg_engine_note_on(&e,36,127,0);render(&e,8192);uint32_t phase=e.native.phase;
    zg_engine_note_on(&e,48,127,1);CHECK(e.native.phase==phase);CHECK(e.pitch!=e.target_pitch);
    uint32_t before=e.pitch;render(&e,32);CHECK(e.pitch>before&&e.pitch<e.target_pitch);
    zg_engine_note_off(&e,36);CHECK(e.gate);zg_engine_note_off(&e,48);CHECK(!e.gate);
    render(&e,44100);CHECK(e.envelope>0);CHECK(zg_engine_active(&e));
    render(&e,44100*4);CHECK(!zg_engine_active(&e));CHECK(e.envelope==0&&e.layer_envelope==0);
    /* Host glide uses the same smoothing, while zero host amplitude is exact. */
    zg_engine_init(&e,44100);e.patch.glide=100;zg_engine_note_on(&e,36,100,0);
    int16_t b[32];uint32_t lo=e.pitch,hi=zg_pitch_increment(48,0,44100);
    zg_engine_render_host(&e,b,32,hi,24000,24000,1);CHECK(e.pitch>lo&&e.pitch<hi);
    zg_engine_render_host(&e,b,32,hi,0,0,1);for(unsigned i=0;i<32;i++)CHECK(b[i]==0);
}
static void layer_ownership(void)
{
    zg_engine a,b;int16_t x[32],y[32];int32_t sx[32*ZG_STEM_COUNT],sy[32*ZG_STEM_COUNT];
    zg_engine_init(&a,44100);zg_engine_note_on(&a,36,100,0);render(&a,2000);b=a;b.mute_mask=1u<<ZG_BODY;
    zg_engine_render_stems(&a,x,sx,32);zg_engine_render_stems(&b,y,sy,32);
    CHECK(a.body_phase==b.body_phase);CHECK(a.sub_phase==b.sub_phase);
    for(unsigned i=0;i<32;i++) {
        CHECK(sx[i*7+5]==sy[i*7+5]);
        CHECK(sx[i*7+6]-sy[i*7+6]==sx[i*7+2]);
        CHECK(sx[i*7+6]==sx[i*7+5]+sx[i*7+2]+sx[i*7+3]+sx[i*7+4]);
    }
    /* Raw source mute occurs before the shared nonlinear character graph. */
    b=a;b.mute_mask=3;zg_engine_render_stems(&b,y,sy,32);
    if(b.patch.mode==ZG_NATIVE)CHECK(b.native.phase!=a.native.phase);
    else CHECK(b.source.position_q16!=a.source.position_q16); /* active muted source still advances */
    /* Pedal SUB remains fixed while source note moves. */
    zg_engine_init(&a,44100);a.patch.sub_mode=1;zg_engine_note_on(&a,36,100,0);uint32_t sub=a.sub_pitch;
    zg_engine_note_on(&a,48,100,1);CHECK(a.sub_pitch==sub);
    a.patch.sub_mode=0;zg_engine_note_on(&a,48,100,1);CHECK(a.sub_pitch!=sub);
    a.bypass_character=1;uint32_t motion=a.character.motion_phase;render(&a,32);CHECK(a.character.motion_phase!=motion);
}
static void pitches_and_presets(void)
{
    CHECK(zg_preset_count==36);CHECK(sizeof(zg_engine)<=416);
    CHECK(zg_pitch_increment(69,0,44100)>42852000u&&zg_pitch_increment(69,0,44100)<42853000u);
    for(unsigned t=0;t<4;t++)for(int n=1;n<100;n++)CHECK(zg_pitch_increment(n,t,44100)>=zg_pitch_increment(n-1,t,44100));
    uint64_t hashes[36];
    for(unsigned p=0;p<zg_preset_count;p++){
        zg_engine e;zg_engine_init(&e,44100);zg_engine_set_preset(&e,p);zg_engine_note_on(&e,36,110,0);
        hashes[p]=fingerprint(&e,8192);CHECK(hashes[p]!=0);
        for(unsigned j=0;j<p;j++)CHECK(hashes[p]!=hashes[j]);
    }
}
static void extremes_and_continuity(void)
{
    zg_engine e;zg_engine_init(&e,44100);int16_t b[32];unsigned samples=0;
    /* 4096 preset transitions, all sources/modes/families, valid extremes and
     * envelope changes. This is a bounded regression, not a wall-clock soak. */
    for(unsigned k=0;k<4096;k++) {
        zg_engine_set_preset(&e,k%36);e.patch.mode=(uint8_t)(k%3);
        zg_engine_set_macro(&e,ZG_GRIT,k&1?127:0);zg_engine_set_macro(&e,ZG_MOTION,k&2?127:0);
        zg_engine_set_macro(&e,ZG_VOWEL,k&4?127:0);e.patch.quality=(uint8_t)(k&1);
        zg_engine_note_on(&e,k%128,1+(k%127),k&1);
        for(unsigned j=0;j<8;j++){zg_engine_render(&e,b,32);samples+=32;for(unsigned i=0;i<32;i++)CHECK(b[i]>=-32760&&b[i]<=32760);}
        zg_engine_note_off(&e,k%128);
    }
    CHECK(samples==1048576);
    zg_engine_all_notes_off(&e);render(&e,44100*5);CHECK(!zg_engine_active(&e));
    /* Host amplitude and velocity never overflow, even out-of-range host data. */
    zg_engine_render_host(&e,b,32,UINT32_MAX,INT32_MIN,INT32_MAX,1);
    for(unsigned i=0;i<32;i++)CHECK(b[i]>=-32760&&b[i]<=32760);
}
int main(void)
{
    deterministic_checkpoint();phase_release_and_glide();layer_ownership();pitches_and_presets();extremes_and_continuity();
    printf("Engine PASS: %u assertions; deterministic checkpoints, 2s release, phase, glide, raw/source bus ownership, pedal/moving SUB, 36 differentiated presets, 1048576 stress frames; state %u bytes\n",checks,(unsigned)sizeof(zg_engine));return 0;
}
