/* SPDX-License-Identifier: MIT */
#include "zg/dsp.h"
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t hash_sample(uint32_t h,int32_t x)
{
    unsigned k;
    for(k=0;k<4;k++){h^=(uint32_t)x&255u;h*=16777619u;x>>=8;}
    return h;
}
static uint32_t run(unsigned family, unsigned quality, uint32_t rate,
                    uint32_t inc, uint32_t seed, unsigned extreme)
{
    zg_native_state ns;zg_character_state cs;zg_dsp_coeffs c;
    zg_dsp_params p={24576,28000,22000,0,0};
    uint32_t h=2166136261u,i;int32_t x,y,peak=0;
    p.family=(uint8_t)family;p.quality=(uint8_t)quality;
    if(extreme)p.grit=p.motion=p.vowel=65535;
    zg_native_init(&ns,seed);zg_character_init(&cs);
    zg_dsp_prepare(&c,&p,rate,inc,324639u);
    for(i=0;i<rate*2u;i++) {
        x=zg_native_sample(&ns,&c,inc);
        if(extreme && i%5u==0u)x=(i&1u)?INT_MAX:INT_MIN;
        y=zg_character_sample(&cs,&c,x);
        assert(y>=-32767 && y<=32767);
        if(y<0)y=-y;
        if(y>peak)peak=y;
        h=hash_sample(h,y);
    }
    assert(peak>100);
    return h;
}
int main(void)
{
    unsigned f,q;uint32_t hashes[6];
    assert(sizeof(zg_native_state)==28);
    assert(sizeof(zg_character_state)<=216);
    assert(sizeof(zg_dsp_coeffs)<=52);
    assert(zg_sine(0)==0);
    assert(zg_sine(0x40000000u)==32767);
    assert(zg_sine(0x80000000u)==0);
    assert(zg_sine(0xc0000000u)==-32767);
    assert(zg_tanh_q15(INT_MIN)<-32000);
    assert(zg_tanh_q15(INT_MAX)>32000);
    assert(zg_tanh_q15(0)==0);
    for(f=0;f<6;f++) {
        hashes[f]=run(f,0,44100,4674794u,55,0);
        assert(hashes[f]==run(f,0,44100,4674794u,55,0));
        for(q=0;q<f;q++)assert(hashes[f]!=hashes[q]);
        for(q=0;q<2;q++) {
            (void)run(f,q,8000,429496729u,UINT_MAX,1);
            (void)run(f,q,48000,8947848u,0,1);
        }
    }
    {
        zg_native_state s;zg_character_state t;zg_dsp_coeffs c;
        zg_dsp_params p={0,0,0,0,0};uint32_t oldphase;
        zg_native_init(&s,1);zg_character_init(&t);
        zg_dsp_prepare(&c,&p,44100,1u,0);
        assert(c.harmonics==16);
        (void)zg_native_sample(&s,&c,1234567u);
        oldphase=s.phase;zg_native_trigger(&s,1);assert(s.phase==oldphase);
        zg_native_trigger(&s,0);assert(s.phase==0);
        /* A >27-hour held note must not wrap its age into a fresh attack. */
        s.age=UINT32_MAX-1u;s.sweep=0;
        zg_native_state settled=s;settled.age=UINT32_MAX;
        for(unsigned sample=0;sample<256;sample++)
            assert(zg_native_sample(&s,&c,1234567u)==zg_native_sample(&settled,&c,1234567u));
        assert(s.age==UINT32_MAX);
        t.delay[1]=1234;t.form1_band=456;zg_character_trigger(&t);
        assert(t.delay[1]==1234 && t.form1_band==456);
    }
    printf("DSP PASS: native=%zu character=%zu coeffs=%zu, six differentiated deterministic families, extreme inputs, both shaper modes, phase/history continuation\n",sizeof(zg_native_state),sizeof(zg_character_state),sizeof(zg_dsp_coeffs));
    return 0;
}
