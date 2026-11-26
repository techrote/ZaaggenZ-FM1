/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define INC_CEILING 0x73333333u
static uint32_t bent_reference(uint32_t increment,uint32_t ratio_q20)
{
    uint64_t value=(uint64_t)increment*ratio_q20/1048576u;
    return value>INC_CEILING?INC_CEILING:(uint32_t)value;
}
int main(void)
{
    static const uint32_t rates[]={8000,12000,22050,44100,48000};
    unsigned cases=0,bend_cases=0;
    for(unsigned rate_index=0;rate_index<sizeof(rates)/sizeof(rates[0]);rate_index++) {
        uint32_t rate=rates[rate_index];
        for(unsigned tuning=0;tuning<ZG_TUNING_COUNT;tuning++) {
            zg_engine engine;zg_patch p;uint32_t previous=0;
            zg_engine_init(&engine,rate);p=engine.patch;p.tuning=(uint8_t)tuning;p.glide=0;
            zg_engine_set_patch(&engine,&p);
            for(unsigned key=0;key<128;key++) {
                uint32_t increment=zg_pitch_increment((int)key,tuning,rate);
                assert(increment>0 && increment<=INC_CEILING);
                assert(increment>=previous);
                /* Rate conversion must agree with the nominal fixed pitch to
                 * within one phase unit, saturating before uint32 conversion. */
                uint32_t nominal=zg_pitch_increment((int)key,tuning,44100);
                uint64_t converted=(uint64_t)nominal*44100u/rate;
                uint32_t expected=converted>INC_CEILING?INC_CEILING:(uint32_t)converted;
                assert(increment==expected);
                /* The source player must follow every declared tuning degree,
                 * including the sub-6 Hz bottom of 13 equal tritave divisions.
                 * Checking the oscillator increment alone misses a separate
                 * image-playback clamp. */
                zg_source_state source;
                zg_source_reset(&source,0);
                zg_source_prepare(&source,increment,rate);
                uint32_t image_step=(uint32_t)(((uint64_t)increment*14700u+524288u)>>20);
                assert(source.step_q16==image_step);
                zg_engine_pitch_bend(&engine,0);zg_engine_note_on(&engine,key,100,0);
                assert(engine.target_pitch==increment);
                zg_engine_pitch_bend(&engine,-200);
                uint32_t below=engine.target_pitch;
                zg_engine_pitch_bend(&engine,200);
                uint32_t above=engine.target_pitch;
                assert(below>0 && below<=increment && above>=increment && above<=INC_CEILING);
                /* Independent musical ratios, rounded Q20: +/-2 semitones. */
                assert(below==bent_reference(increment,934175u));
                assert(above==bent_reference(increment,1176987u));
                zg_engine_pitch_bend(&engine,-500);assert(engine.target_pitch==below);
                zg_engine_pitch_bend(&engine,500);assert(engine.target_pitch==above);
                zg_engine_pitch_bend(&engine,0);assert(engine.target_pitch==increment);
                previous=increment;cases++;bend_cases+=5;
            }
        }
    }
    printf("Pitch range PASS: %u oscillator/image key/rate/tuning combinations, %u bend/reset boundaries; monotonic and bounded at 8/12/22.05/44.1/48 kHz\n",cases,bend_cases);
    return 0;
}
