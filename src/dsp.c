/* SPDX-License-Identifier: MIT */
#include "zg/dsp.h"
#include <string.h>
#include "dsp_tables.inc"

/* Bounds are explicit so each product fits signed 32 bits on the pi32v2. */
static int32_t limit(int32_t x, int32_t lo, int32_t hi)
{ return x < lo ? lo : (x > hi ? hi : x); }
int32_t zg_mul_q15(int32_t x, int32_t y) { return (x * y) >> 15; }
static int32_t qmul(int32_t x, int32_t y) { return (x * y) >> 15; }
static int32_t unit(int32_t x) { return limit(x, -32767, 32767); }

int32_t zg_sine(uint32_t phase)
{
    uint32_t i = phase >> 22;
    int32_t a = zg_sine_table[i];
    int32_t delta = (int32_t)zg_sine_table[i+1] - a;
    return a + ((delta * (int32_t)((phase >> 7) & 32767u)) >> 15);
}
int32_t zg_tanh_q15(int32_t x)
{
    uint32_t a, i, frac;
    int32_t y;
    /* Clamp before negating, including INT32_MIN. */
    x = limit(x, -131071, 131071);
    a = (uint32_t)(x < 0 ? -x : x); i = a >> 9; frac = a & 511u;
    y = zg_tanh_table[i] + (((int32_t)zg_tanh_table[i+1] - zg_tanh_table[i]) * (int32_t)frac >> 9);
    return x < 0 ? -y : y;
}
static int32_t onepole(int32_t *state, int32_t x, uint16_t k)
{
    /* Both input/state are at most 32767, so delta * coefficient fits. */
    *state += qmul(x - *state, k);
    return *state;
}
static uint32_t random32(uint32_t *x)
{
    uint32_t a=*x; a ^= a<<13; a ^= a>>17; a ^= a<<5; *x=a; return a;
}
static uint32_t hz_inc(uint32_t hz, uint32_t rate)
{
    /* Control-rate only; split division avoids an unsigned 64-bit multiply. */
    return (UINT32_MAX / rate) * hz;
}
static uint16_t pole_coefficient(uint32_t hz, uint32_t rate)
{
    /* Bilinear one-pole g/(1+g), g = 2*pi*f/Fs. A bounded causal split,
     * deliberately not advertised as the offline Butterworth filtfilt. */
    uint32_t g=(hz*205887u)/rate;
    return (uint16_t)((g*32768u)/(32768u+g));
}
static uint16_t form_coefficient(uint32_t hz, uint32_t rate)
{
    /* Chamberlin SVF stability bound; formants capped to 0.12 Fs. */
    uint32_t top=(rate*12u)/100u;
    if (hz>top) hz=top;
    return (uint16_t)limit(zg_sine(hz_inc(hz,rate)>>1)*2,1,24576);
}

void zg_native_init(zg_native_state *s, uint32_t seed)
{
    memset(s,0,sizeof(*s)); s->noise=seed ? seed : 0x71ca93a5u;
    zg_native_trigger(s,0);
}
void zg_native_trigger(zg_native_state *s, int preserve_phase)
{
    if (!preserve_phase) { s->phase=0; s->rough_phase=0; s->previous=0; s->dc=0; }
    s->age=0; s->sweep=32767;
}
void zg_character_init(zg_character_state *s) { memset(s,0,sizeof(*s)); }
void zg_character_trigger(zg_character_state *s) { s->bloom=0; }

void zg_dsp_prepare(zg_dsp_coeffs *c, const zg_dsp_params *p,
                    uint32_t rate, uint32_t increment, uint32_t beat_inc)
{
    static const uint16_t low_hz[6]={500,650,500,420,350,650};
    static const uint16_t edge_hz[6]={900,1200,800,1000,1200,700};
    static const uint8_t tap48[4]={11,23,43,79};
    unsigned j;
    if (rate<8000u) rate=8000u;
    if (rate>48000u) rate=48000u;
    c->p=*p;
    if (c->p.family>=ZG_FAMILY_COUNT) c->p.family=0;
    if (c->p.grit>32767u) c->p.grit=32767u;
    if (c->p.motion>32767u) c->p.motion=32767u;
    if (c->p.vowel>32767u) c->p.vowel=32767u;
    c->p.quality=(uint8_t)(c->p.quality!=0);
    c->rate=rate; c->beat_inc=beat_inc;
    c->ring_inc=hz_inc(c->p.family==ZG_UPPER_CHOP ? 310u : (c->p.family==ZG_SPLIT_MAUL ? 220u : 270u),rate);
    c->rough_inc=hz_inc(31u,rate);
    {
        /* VOWEL is a spectral-color control in all six families; Snarl also
         * moves its explicit resonances. Midpoint retains authored split. */
        uint32_t colour=16384u+c->p.vowel;
        c->low_coef=pole_coefficient((low_hz[c->p.family]*colour)>>15,rate);
        c->edge_coef=pole_coefficient((edge_hz[c->p.family]*colour)>>15,rate);
    }
    c->form1_coef=form_coefficient(650u + ((uint32_t)c->p.vowel*1100u>>15),rate);
    c->form2_coef=form_coefficient(2300u + ((uint32_t)c->p.vowel*1700u>>15),rate);
    c->sweep_decay=(uint16_t)limit((int32_t)(762046u/rate),1,32767);
    c->bloom_step=(uint16_t)limit((int32_t)(1310720u/rate),1,32767);
    c->attack_frames=(uint16_t)(rate/350u);
    c->attack_step=(uint16_t)(32768u/c->attack_frames);
    /* Up to 16 native partials below 0.42Fs; retain the fundamental alone
     * up to the engine pitch ceiling (0.45Fs). Waveshaping images are measured
     * separately and are not labeled bandlimited. */
    {
        uint32_t count=increment ? 0x6b851eb8u/increment : 16u;
        if(count>16u)count=16u;
        if(!count)count=1u;
        c->harmonics=(uint8_t)count;
    }
    for(j=0;j<4;j++) {
        uint32_t d=(tap48[j]*rate+24000u)/48000u;
        c->delays[j]=(uint8_t)(d ? (d<ZG_COMB_SAMPLES?d:ZG_COMB_SAMPLES-1) : 1);
    }
}

static int32_t fold(int32_t x)
{
    /* Triangle fold, period 4 in Q15, without remainder or division. */
    uint32_t u=((uint32_t)x+32768u)&131071u;
    return u<65536u ? (int32_t)u-32768 : 98304-(int32_t)u;
}
int32_t zg_native_sample(zg_native_state *s, const zg_dsp_coeffs *c,
                         uint32_t increment)
{
    const int16_t *w=zg_weights[c->p.family];
    uint32_t ph, delta, h, rnd;
    int32_t x=0, grit=c->p.grit, noise, pre, driven, soft, hard, shaped;
    int32_t rough, env, wet, foldmix;
    /* Native pitch motion is a causal reverse micro-cadence. Hybrid playback
     * never goes through this function, preserving its post-topology source. */
    delta=(increment>>15)*(uint32_t)(s->sweep>>3);
    s->phase += increment-delta;
    s->rough_phase += c->rough_inc;
    ph=s->phase;
    rough=zg_sine(s->rough_phase);
    for(h=1;h<=c->harmonics;h++) {
        int32_t partial=qmul(zg_sine(ph),w[h-1]);
        if (h>=3u) partial += qmul(qmul(partial,rough),grit>>3);
        x+=partial;
        ph+=s->phase;
    }
    /* Small, separate deterministic stochastic residue. */
    rnd=random32(&s->noise);
    noise=((int32_t)(rnd>>16)-32768);
    x += qmul(noise, c->p.family==ZG_CRUSHED_TEETH ? (grit>>5) : (grit>>7));
    /* Recovered source amplitude is inside the nonlinear topology. This compact
     * two-stage decay makes a held native voice bloom then settle to a tone;
     * the engine retains independent note-off/release amplitude ownership. */
    env=8192+(s->sweep*3>>2);
    if (s->age<c->attack_frames) env=qmul(env,(int32_t)(s->age*c->attack_step));
    x=qmul(unit(x),env);
    pre=x-qmul(s->previous,1024+(grit>>2)); s->previous=x;
    driven=(pre*(768+(grit>>4)))>>8;
    soft=zg_tanh_q15(driven+3500+(grit>>4))-zg_tanh_q15(3500+(grit>>4));
    hard=limit(driven,-26000,26000);
    shaped=(soft+hard)>>1;
    foldmix=12000+(grit>>1);
    shaped += qmul(fold(shaped)-shaped,foldmix);
    wet=19000+(grit>>2);
    x=pre+qmul(limit(shaped-pre,-65535,65535),wet);
    /* DC removal is causal; no per-note peak or RMS normalization. */
    s->dc += (x-s->dc)>>9;
    x-=s->dc;
    s->sweep-=qmul(s->sweep,c->sweep_decay);
    if (s->sweep>0 && qmul(s->sweep,c->sweep_decay)==0) s->sweep--;
    if(s->age!=UINT32_MAX) s->age++;
    return limit(x,-32767,32767);
}

static int32_t bandpass(int32_t x, int32_t *low, int32_t *band,
                         uint16_t f)
{
    int32_t high;
    *low=unit(*low+qmul(*band,f));
    high=unit(x-*low-qmul(*band,6000));
    *band=unit(*band+qmul(high,f));
    return *band;
}
static int32_t saturate(int32_t x, int32_t drive, uint8_t quality,
                         int32_t *previous)
{
    int32_t prior=*previous;
    int32_t a=zg_tanh_q15((limit(x,-65535,65535)*drive)>>8);
    *previous=x;
    if (quality) {
        /* Linear midpoint at 2x plus [1/4,1/2,1/4] reconstruction. This is a
         * local shaper antialias mode, not full-path oversampling. */
        int32_t mid=(x+prior)>>1;
        int32_t b=zg_tanh_q15((limit(mid,-65535,65535)*drive)>>8);
        int32_t old=zg_tanh_q15((limit(prior,-65535,65535)*drive)>>8);
        a=(a+2*b+old)>>2;
    }
    return a;
}

int32_t zg_character_sample(zg_character_state *s, const zg_dsp_coeffs *c,
                            int32_t source)
{
    int32_t x=unit(source), low, upper, z, edge, y=0;
    int32_t grit=c->p.grit, motion=c->p.motion;
    int32_t ring, phase_mod, bloom, gate;
    uint32_t phase;
    uint8_t f=c->p.family;
    low=onepole(&s->low,x,c->low_coef); upper=x-low;
    phase=s->motion_phase;
    s->motion_phase+=c->beat_inc;
    phase_mod=zg_sine(phase);
    /* FM via increment modulation, avoiding the noncausal f(t)*t phase law. */
    s->ring_phase+=c->ring_inc + (uint32_t)((int32_t)(c->ring_inc>>15)*(phase_mod>>3));
    ring=zg_sine(s->ring_phase);
    s->bloom=(uint16_t)(s->bloom+qmul(32767-(int32_t)s->bloom,c->bloom_step));
    bloom=s->bloom;
    switch(f) {
    case ZG_BLOOM_BARK:
        z=x+qmul(limit(upper,-65535,65535),28000)*2;
        z=saturate(z+2300,640+(grit>>3),c->p.quality,&s->previous)-zg_tanh_q15((2300*(640+(grit>>3)))>>8);
        z=unit(z);
        edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,5000)+qmul(z,9000+(bloom>>2))+qmul(limit(edge,-65535,65535),16000+(bloom>>3));
        break;
    case ZG_FORMANT_SNARL: {
        int32_t a=bandpass(x,&s->form1_low,&s->form1_band,c->form1_coef);
        int32_t b=bandpass(x,&s->form2_low,&s->form2_band,c->form2_coef);
        int32_t cross=limit((int32_t)c->p.vowel+(qmul(zg_sine(phase>>2),motion)>>2),0,32767);
        z=a+qmul(b-a,cross);
        z=qmul(x,5000)+limit(upper*2+z*4,-60000,60000);
        z=saturate(z+700,700+(grit>>3),c->p.quality,&s->previous)-zg_tanh_q15((700*(700+(grit>>3)))>>8);
        z=unit(z); edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,2600)+qmul(z,14000)+qmul(limit(edge,-65535,65535),12000);
        break;
    }
    case ZG_UPPER_CHOP:
        /* Eight subdivisions per quarter note; the low foundation stays on. */
        gate=((phase*8u)&0x80000000u) ? 32767-qmul(motion,31780) : 32767;
        z=qmul(low,16000)+qmul(limit(upper*2+qmul(qmul(x,ring),24000),-65535,65535),gate);
        z=saturate(z,600+(grit>>3),c->p.quality,&s->previous);
        edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,3000)+qmul(z,11000)+qmul(limit(edge,-65535,65535),17500);
        break;
    case ZG_SPLIT_MAUL: {
        int low_on=((phase*2u)&0x80000000u)==0;
        int32_t a=saturate(low,650+(grit>>3),c->p.quality,&s->previous);
        int32_t b=saturate(upper+qmul(qmul(x,ring),9800),800+(grit>>3),c->p.quality,&s->form1_low);
        int32_t duck=32767-qmul(motion,28000);
        z=qmul(a,low_on?32767:duck)+qmul(b,low_on?duck:32767);
        z=unit(z>>1); edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,2000)+qmul(z,24000)+qmul(limit(edge,-65535,65535),10000);
        break;
    }
    case ZG_CRUSHED_TEETH: {
        z=saturate(x+qmul(limit(upper,-65535,65535),30000),1100+(grit>>3),c->p.quality,&s->previous);
        if(!s->hold_count) {
            int32_t shift=9+(grit>>13); /* 6..3 effective magnitude bits */
            int32_t step=1<<shift;
            /* Power-of-two quantizer; rounded symmetric and division-free. */
            int32_t a=z<0?-z:z;
            a=((a+(step>>1))>>shift)*step;
            s->held=(int16_t)unit(z<0?-a:a);
            s->hold_count=(uint8_t)(1u+(grit>>14)+(motion>>14));
        } else s->hold_count--;
        z=s->held; edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,1300)+qmul(z,13000)+qmul(limit(edge,-65535,65535),15000);
        break;
    }
    case ZG_HARMONIC_RIP: {
        static const int16_t gains[4]={31130,-29491,24576,-21299};
        unsigned j;
        z=x;
        for(j=0;j<4;j++) {
            int i=(int)s->position-(int)c->delays[j];
            if(i<0)i+=ZG_COMB_SAMPLES;
            z+=qmul(s->delay[i],gains[j]);
        }
        s->delay[s->position]=(int16_t)x;
        s->position=(uint8_t)(s->position+1u);
        if(s->position==ZG_COMB_SAMPLES)s->position=0;
        z=limit(z,-65535,65535);
        z+=qmul(qmul(z,ring),8000+(motion>>1));
        z=saturate(z,680+(grit>>3),c->p.quality,&s->previous);
        edge=z-onepole(&s->edge_low,z,c->edge_coef);
        y=qmul(x,2000)+qmul(z,12000)+qmul(limit(edge,-65535,65535),18000);
        break;
    }
    default: y=x; break;
    }
    /* One fixed character-stage trim (1/2), then causal DC block. This is not
     * final-output limiting; root engine owns the single output policy. */
    y>>=1;
    s->dc+=(y-s->dc)>>10;
    return limit(y-s->dc,-32767,32767);
}
const char *zg_family_name(unsigned f)
{
    static const char * const names[6]={"Bloom Bark","Formant Snarl","Upper Chop","Split Maul","Crushed Teeth","Harmonic Rip"};
    return f<6u?names[f]:"Unknown";
}
