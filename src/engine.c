/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <string.h>
#include <limits.h>
#include "pitch_tables.inc"

/* Keep wide multiplies away from int16 loads on the pi32v2 CPU (Fune erratum).
 * All divisions below are initialization/control-rate, never in sample loops. */
#if defined(__GNUC__)
#define ZG_NOINLINE __attribute__((noinline))
#else
#define ZG_NOINLINE
#endif
static ZG_NOINLINE uint32_t wide_ratio(uint32_t x, uint32_t y, uint32_t z)
{
    if(!z)return 0;
    if(x==z)return y;
    if(y==z)return x;
    uint64_t product=(uint64_t)x*y;
    /* The fixed-point control ratios have exact power-of-two denominators.
     * Keep these shifts explicit: the target compiler otherwise emits its
     * software 64-bit divider even for a constant denominator at the caller. */
    uint64_t quotient=z==65536u?product>>16:z==1048576u?product>>20:product/z;
    /* Saturate before narrowing. High notes at 8/12 kHz must reach the pitch
     * ceiling, never wrap back into the bass register. */
    return quotient>UINT32_MAX?UINT32_MAX:(uint32_t)quotient;
}
static int32_t clamp32(int32_t x, int32_t lo, int32_t hi)
{ return x < lo ? lo : x > hi ? hi : x; }
static uint8_t cap127(uint8_t x) { return x > 127 ? 127 : x; }
static int32_t shr_zero(int32_t x,unsigned shift)
{
    uint32_t magnitude=x<0?0u-(uint32_t)x:(uint32_t)x;
    int32_t result=(int32_t)(magnitude>>shift);
    return x<0?-result:result;
}
static int32_t scale(int32_t x, int32_t q15)
{
    x=clamp32(x,-262143,262143);q15=clamp32(q15,0,32767);
    /* Preserve decoded source headroom without a 64-bit sample multiply. */
    uint32_t magnitude=x<0?0u-(uint32_t)x:(uint32_t)x;
    int32_t result=(int32_t)((magnitude>>15)*(uint32_t)q15+
                            (((magnitude&32767u)*(uint32_t)q15)>>15));
    return x<0?-result:result;
}
static uint32_t min1(uint32_t n) { return n ? n : 1; }

const char *zg_tuning_name(unsigned tuning)
{
    static const char *const names[] = {"12 equal octave", "19 equal octave", "24 equal octave", "13 equal tritave"};
    return names[tuning < ZG_TUNING_COUNT ? tuning : 0];
}
uint32_t zg_time_ms(uint8_t control)
{ uint32_t v=cap127(control); return 1u + v*v*1999u/16129u; }
uint32_t zg_pitch_increment(int key, unsigned tuning, uint32_t sample_rate)
{
    if (!sample_rate) sample_rate=ZG_SAMPLE_RATE;
    if (sample_rate<8000) sample_rate=8000;
    if (sample_rate>48000) sample_rate=48000;
    if (tuning>=ZG_TUNING_COUNT) tuning=0;
    key=clamp32(key,0,127);
    uint32_t inc=PITCH_TABLE[tuning][key];
    if(sample_rate!=44100u)inc=wide_ratio(inc,44100,sample_rate);
    return inc>0x73333333u ? 0x73333333u : inc;
}
static uint32_t pitch_for(const zg_engine *e, int key)
{ return zg_pitch_increment(key,e->patch.tuning,e->rate); }
static uint32_t expressive_pitch(const zg_engine *e,int key)
{
    uint32_t base=pitch_for(e,key);
    if(!e->bend_cents)return base;
    uint32_t value=wide_ratio(base,BEND_RATIO_Q20[clamp32(e->bend_cents,-200,200)+200],1048576);
    return value>0x73333333u?0x73333333u:value;
}
void zg_engine_pitch_bend(zg_engine *e,int cents)
{
    if(!e)return;
    e->bend_cents=(int16_t)clamp32(cents,-200,200);
    e->target_pitch=expressive_pitch(e,e->note);
}
static void sanitize(zg_patch *p)
{
    unsigned i;
    p->family%=ZG_FAMILY_COUNT;
    p->source=(uint8_t)(p->source % (zg_source_count ? zg_source_count : 1));
    p->mode%=3;
    p->grit=cap127(p->grit); p->motion=cap127(p->motion); p->vowel=cap127(p->vowel);
    for(i=0;i<ZG_LAYER_COUNT;i++) p->level[i]=cap127(p->level[i]);
    p->attack=cap127(p->attack); p->decay=cap127(p->decay); p->sustain=cap127(p->sustain);
    p->release=cap127(p->release); p->glide=cap127(p->glide);
    p->roll=p->roll>6?6:p->roll; p->tuning%=ZG_TUNING_COUNT; p->sub_mode%=2;
    p->body_degree=(int8_t)clamp32(p->body_degree,-48,48);
    p->aux_degree=(int8_t)clamp32(p->aux_degree,-48,48);
    p->separation=cap127(p->separation); p->gain=cap127(p->gain);
    p->quality=!!p->quality; p->source_sustain=!!p->source_sustain; p->bypass=!!p->bypass;
    p->bpm=(uint16_t)clamp32(p->bpm,40,300);
}
void zg_engine_set_patch(zg_engine *e, const zg_patch *patch)
{
    if (!e || !patch) return;
    zg_patch next=*patch;sanitize(&next);
    if(!memcmp(&e->patch,&next,sizeof next))return;
    uint8_t old=e->patch.source;
    uint8_t family=e->patch.family;
    int structural=old!=next.source||family!=next.family||e->patch.mode!=next.mode||e->patch.bypass!=next.bypass;
    e->patch=next;
    if(old!=e->patch.source) {
        zg_source_reset(&e->source,e->patch.source);
        zg_source_reset(&e->exciter_source,e->patch.source);
    }
    if(family!=e->patch.family) zg_character_trigger(&e->character);
    e->target_pitch=expressive_pitch(e,e->note);
    e->bypass_character=e->patch.bypass;
    if(structural)e->fade=0;
}
void zg_engine_set_preset(zg_engine *e,unsigned preset)
{
    if(!e || !zg_preset_count) return;
    preset%=zg_preset_count;
    zg_engine_set_patch(e,&zg_presets[preset].patch);
    e->preset=(uint8_t)preset;
}
void zg_engine_init(zg_engine *e,uint32_t rate)
{
    memset(e,0,sizeof(*e));
    e->rate=(uint32_t)clamp32((int32_t)rate,8000,48000);
    e->note=36; e->velocity=100;
    zg_native_init(&e->native,0x5a414147u);
    zg_native_init(&e->exciter_native,0x45584349u);
    zg_character_init(&e->character);
    zg_engine_set_preset(e,0);
    zg_source_reset(&e->source,e->patch.source);
    zg_source_reset(&e->exciter_source,e->patch.source);
    e->pitch=e->target_pitch; e->sub_pitch=pitch_for(e,24);
    e->grit=(uint16_t)(e->patch.grit*258u);
    e->motion=(uint16_t)(e->patch.motion*258u);
    e->vowel=(uint16_t)(e->patch.vowel*258u);
    for(unsigned i=0;i<ZG_LAYER_COUNT;i++)e->level_gain[i]=(uint16_t)(e->patch.level[i]*258u);
    e->output_gain=(uint16_t)(e->patch.gain*128u);
    e->fade=32767;
}
void zg_engine_set_macro(zg_engine *e,unsigned macro,unsigned value)
{
    if(!e) return;
    value=value>127?127:value;
    if(macro==ZG_CHARACTER) {
        unsigned f=value*ZG_FAMILY_COUNT/128;
        if(f!=e->patch.family) { e->patch.family=(uint8_t)f; zg_character_trigger(&e->character);e->fade=0; }
    } else if(macro==ZG_GRIT) e->patch.grit=(uint8_t)value;
    else if(macro==ZG_MOTION) e->patch.motion=(uint8_t)value;
    else if(macro==ZG_VOWEL) e->patch.vowel=(uint8_t)value;
}
static void excite(zg_engine *e)
{
    e->exciter_envelope=32767;
    zg_native_trigger(&e->exciter_native,1);
    zg_source_reset(&e->exciter_source,e->patch.source);
}
void zg_engine_note_on(zg_engine *e,unsigned note,unsigned velocity,int legato)
{
    if(!e) return;
    note=note>127?127:note; velocity=velocity>127?127:velocity;
    if(!velocity) { zg_engine_note_off(e,note);return; }
    int was_active=zg_engine_active(e);
    e->note=(uint8_t)note; e->velocity=(uint8_t)velocity;
    e->target_pitch=expressive_pitch(e,(int)note);
    if(!was_active || !e->patch.glide) e->pitch=e->target_pitch;
    /* Every articulated note renews the short independent exciter. Legato
     * leaves the long melodic SYNTHLINE and its phase/cursor intact. */
    excite(e);
    if(!legato || !e->gate || e->source.ended) {
        e->stage=1; e->rhythm_phase=0;
        zg_native_trigger(&e->native,1);
        zg_character_trigger(&e->character);
        zg_source_reset(&e->source,e->patch.source);
    }
    e->gate=1; e->held=1;
    e->sub_pitch=pitch_for(e,e->patch.sub_mode==ZG_SUB_PEDAL?24:(int)note-12);
}
void zg_engine_note_off(zg_engine *e,unsigned note)
{ if(e && e->note==note) {e->gate=0;e->stage=3;e->held=0;} }
void zg_engine_all_notes_off(zg_engine *e)
{ if(e) {e->gate=0;e->stage=3;e->held=0;} }
int zg_engine_active(const zg_engine *e)
{ return e && (e->gate || e->envelope>0 || e->layer_envelope>0 || e->exciter_envelope>8); }

static int32_t ramp(int32_t x,int32_t target,int32_t step)
{ if(x<target) {x+=step;return x>target?target:x;} x-=step;return x<target?target:x; }

static void block(zg_engine *e,int16_t *out,int32_t *stems,unsigned n,
                  int external,uint32_t host_inc,int32_t amp0,int32_t amp1,int gate)
{
    if(!n) return;
    zg_dsp_coeffs c;
    zg_dsp_params p;
    unsigned i,l;
    /* Envelopes retain eight fractional bits so long releases do not round
     * to the one-Q15-unit-per-sample floor (~743 ms at44.1k). */
    int32_t atk=0,dec=0;
    if(!external) {
        atk=(int32_t)min1((uint32_t)(((uint64_t)32767000u<<8)/(zg_time_ms(e->patch.attack)*e->rate)));
        dec=(int32_t)min1((uint32_t)(((uint64_t)32767000u<<8)/(zg_time_ms(e->patch.decay)*e->rate)));
    }
    int32_t rel=(int32_t)min1((uint32_t)(((uint64_t)32767000u<<8)/(zg_time_ms(e->patch.release)*e->rate)));
    int32_t layer_rel=(rel+1)/2;
    uint32_t beat_inc=(uint32_t)(((uint64_t)e->patch.bpm<<32)/(60u*e->rate));
    static const uint8_t rolls[]={0,1,2,3,4,6,8};
    uint32_t roll_inc=beat_inc*rolls[e->patch.roll];
    uint32_t body_ratio=wide_ratio(pitch_for(e,(int)e->note+e->patch.body_degree),65536,pitch_for(e,e->note));
    uint32_t aux_ratio=wide_ratio(pitch_for(e,(int)e->note+e->patch.aux_degree),65536,pitch_for(e,e->note));
    uint32_t base_inc=external?host_inc:e->pitch;
    if(external && e->patch.tuning) {
        uint32_t equal=zg_pitch_increment(e->note,0,e->rate);
        base_inc=wide_ratio(host_inc,pitch_for(e,e->note),equal);
    }
    if(base_inc>0x73333333u) base_inc=0x73333333u;
    if(external) { e->target_pitch=base_inc;if(!e->patch.glide)e->pitch=base_inc; }
    uint32_t body_inc=wide_ratio(e->pitch,body_ratio,65536);
    uint32_t aux_inc=wide_ratio(e->pitch,aux_ratio,65536);
    if(body_inc>0x73333333u) body_inc=0x73333333u;
    if(aux_inc>0x73333333u) aux_inc=0x73333333u;
    if(external) { e->gate=!!gate; e->envelope=clamp32(amp1,0,32767)*256; }
    int32_t vel=(int32_t)e->velocity*258;
    int32_t sus=e->patch.sustain*258;
    int32_t host_amp=clamp32(amp0,0,32767);
    int32_t amp_step=(clamp32(amp1,0,32767)-host_amp)/(int32_t)n;
    unsigned glide_shift=e->patch.glide?4u+e->patch.glide/14u:0;
    int32_t exc_decay=(int32_t)(32768u-((44100u*96u)/e->rate));
    int32_t target_gain=e->patch.gain*128; /* 127 -> -6.1 dB, declared fixed mix policy */
    int32_t level_targets[ZG_LAYER_COUNT];
    for(l=0;l<ZG_LAYER_COUNT;l++)level_targets[l]=e->patch.level[l]*258;
    e->grit=(uint16_t)ramp(e->grit,e->patch.grit*258,1400);
    e->motion=(uint16_t)ramp(e->motion,e->patch.motion*258,1400);
    e->vowel=(uint16_t)ramp(e->vowel,e->patch.vowel*258,1400);
    int32_t motion_depth=e->motion/3;
    p.grit=e->grit;p.motion=e->motion;p.vowel=e->vowel;p.family=e->patch.family;p.quality=e->patch.quality;
    zg_dsp_prepare(&c,&p,e->rate,e->pitch,beat_inc);
    zg_source_prepare(&e->source,e->pitch,e->rate);
    zg_source_prepare(&e->exciter_source,e->pitch,e->rate);
    for(i=0;i<n;i++) {
        for(l=0;l<ZG_LAYER_COUNT;l++) {
            int32_t d=level_targets[l]-e->level_gain[l],delta=shr_zero(d,7);
            if(!delta&&d)delta=d>0?1:-1;
            e->level_gain[l]=(uint16_t)((int32_t)e->level_gain[l]+delta);
        }
        {int32_t d=target_gain-e->output_gain,delta=shr_zero(d,7);if(!delta&&d)delta=d>0?1:-1;e->output_gain=(uint16_t)((int32_t)e->output_gain+delta);}
        if(!external) {
            if(!e->gate) e->stage=3;
            if(e->stage==1) { e->envelope=ramp(e->envelope,32767*256,atk);if(e->envelope==32767*256)e->stage=2; }
            else if(e->stage==2) e->envelope=ramp(e->envelope,sus*256,dec);
            else if(e->stage==3) {e->envelope=ramp(e->envelope,0,rel);if(!e->envelope)e->stage=0;}
        }
            if(glide_shift) {
                int32_t d=(int32_t)e->target_pitch-(int32_t)e->pitch;
                int32_t delta=d>=0?(int32_t)((uint32_t)d>>glide_shift):-(int32_t)((uint32_t)(-d)>>glide_shift);
                if(!delta && d)delta=d>0?1:-1;
                e->pitch=(uint32_t)((int32_t)e->pitch+delta);
            } else e->pitch=e->target_pitch;
        e->layer_envelope=ramp(e->layer_envelope,e->gate?32767*256:0,e->gate?160*256:layer_rel);
        uint32_t old=e->rhythm_phase;e->rhythm_phase+=roll_inc;
        if(e->gate && roll_inc && e->rhythm_phase<old) {
            uint32_t step=e->exciter_source.step_q16;
            uint8_t mip=e->exciter_source.mip;
            excite(e);e->exciter_source.step_q16=step;e->exciter_source.mip=mip;
        }
        int32_t a=external?host_amp:scale(shr_zero(e->envelope,8),vel);
        int32_t la=external?host_amp:scale(shr_zero(e->layer_envelope,8),vel);
        int32_t native=0,img=0,exnative=0,eximg=0;
        if(e->patch.mode!=ZG_SOURCE_IMAGE) {
            native=zg_native_sample(&e->native,&c,e->pitch);
            if(e->exciter_envelope>8)exnative=zg_native_sample(&e->exciter_native,&c,e->pitch);
        }
        if(e->patch.mode!=ZG_NATIVE) {
            img=zg_source_sample(&e->source,e->gate,e->patch.source_sustain);
            if(e->exciter_envelope>8)eximg=zg_source_sample(&e->exciter_source,0,0);
        }
        int32_t source=e->patch.mode==ZG_NATIVE?native:e->patch.mode==ZG_SOURCE_IMAGE?img:(shr_zero(native,1)+shr_zero(img,1));
        int32_t ex=e->patch.mode==ZG_NATIVE?exnative:e->patch.mode==ZG_SOURCE_IMAGE?eximg:(shr_zero(exnative,1)+shr_zero(eximg,1));
        source=scale(scale(source,a),e->level_gain[ZG_SYNTHLINE]);
        ex=scale(scale(scale(ex,e->exciter_envelope),a),e->level_gain[ZG_EXCITER]);
        e->exciter_envelope=scale(e->exciter_envelope,exc_decay);
        if(e->exciter_envelope<8)e->exciter_envelope=0;
        int32_t input=((e->mute_mask&1)?0:source)+((e->mute_mask&2)?0:ex);
        int32_t source_bus=e->bypass_character?input:zg_character_sample(&e->character,&c,clamp32(input,-65535,65535));
        if(e->bypass_character)e->character.motion_phase+=c.beat_inc;
        int32_t motion=zg_sine(e->character.motion_phase);
        int32_t body=zg_sine(e->body_phase),aux=zg_sine(e->aux_phase),sub=zg_sine(e->sub_phase);
        e->body_phase+=body_inc;e->aux_phase+=aux_inc;
        uint32_t sub_inc=e->patch.sub_mode==ZG_SUB_PEDAL?e->sub_pitch:e->pitch>>1;
        e->sub_phase+=sub_inc;
        /* Opposed role motion is subtractive. Separation only yields BODY/AUX,
         * never rewrites the protected source or boosts the master. */
        body=scale(body,32767-scale(shr_zero(motion+32768,1),motion_depth));
        aux=scale(aux,32767-scale(shr_zero(32767-motion,1),motion_depth));
        e->source_low+=shr_zero(clamp32(source_bus,-65535,65535)-e->source_low,5);
        int32_t detector=e->source_low<0?-e->source_low:e->source_low;
        int32_t yield=32767-scale(detector,e->patch.separation*128);
        yield=clamp32(yield,8192,32767);
        body=scale(scale(scale(body,la),e->level_gain[ZG_BODY]),yield);
        aux=scale(scale(scale(aux,la),e->level_gain[ZG_AUX]),yield);
        sub=scale(scale(sub,la),e->level_gain[ZG_SUB]);
        int32_t roles[ZG_LAYER_COUNT]={source,ex,body,aux,sub};
        int32_t mix=source_bus;
        for(l=ZG_BODY;l<ZG_LAYER_COUNT;l++) if(!(e->mute_mask&(1u<<l)))mix+=roles[l];
        if(stems) {
            for(l=0;l<ZG_LAYER_COUNT;l++)stems[i*ZG_STEM_COUNT+l]=roles[l];
            stems[i*ZG_STEM_COUNT+5]=source_bus;stems[i*ZG_STEM_COUNT+6]=mix;
        }
        /* One final policy: explicit fixed gain, DC removal, hard bounded PCM.
         * No compressor, normalization, automatic compensation or soft master. */
        int32_t mastered=scale(mix,e->output_gain);
        e->master_dc+=shr_zero(mastered*256-e->master_dc,10);
        if(!mastered && e->master_dc>-256 && e->master_dc<256)e->master_dc=0;
        mastered=clamp32(mastered-shr_zero(e->master_dc,8),-32760,32760);
        if(e->fade<32767) {
            mastered=scale(mastered,e->fade)+scale(e->last_output,32767-e->fade);
            e->fade=(uint16_t)(e->fade>32510?32767:e->fade+257);
        }
        if(external && host_amp<=0)mastered=0;
        e->last_output=mastered;out[i]=(int16_t)mastered;
        host_amp+=amp_step;
    }
}
void zg_engine_render_host(zg_engine *e,int16_t*out,unsigned n,uint32_t inc,int32_t a0,int32_t a1,int gate)
{
    if(!e || !out)return;
    if(n>ZG_BLOCK)n=ZG_BLOCK;
    block(e,out,0,n,1,inc,a0,a1,gate);
}
void zg_engine_render_stems(zg_engine *e,int16_t*out,int32_t*stems,size_t n)
{
    if(!e || !out)return;
    while(n) {
        unsigned chunk=n>ZG_BLOCK?ZG_BLOCK:(unsigned)n;
        block(e,out,stems,chunk,0,0,0,0,e->gate);
        out+=chunk;if(stems)stems+=chunk*ZG_STEM_COUNT;n-=chunk;
    }
}
void zg_engine_render(zg_engine *e,int16_t*out,size_t n)
{ zg_engine_render_stems(e,out,0,n); }
