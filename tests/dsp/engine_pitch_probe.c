/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Emit actual engine PCM while isolating one persistent layer. */
int main(int argc,char **argv)
{
#ifdef _WIN32
    if (_setmode(_fileno(stdout), _O_BINARY) == -1) return 4;
#endif
    zg_engine e;zg_patch p;int16_t out[ZG_BLOCK];unsigned at,k;
    unsigned layer=argc>1?(unsigned)atoi(argv[1]):ZG_SUB;
    unsigned note=argc>2?(unsigned)atoi(argv[2]):36;
    unsigned tuning=argc>3?(unsigned)atoi(argv[3]):0;
    unsigned pedal=argc>4?(unsigned)atoi(argv[4]):0;
    if(layer>=ZG_LAYER_COUNT || note>127 || tuning>=ZG_TUNING_COUNT)return 2;
    zg_engine_init(&e,44100);p=e.patch;
    for(k=0;k<ZG_LAYER_COUNT;k++)p.level[k]=0;
    p.level[layer]=100;p.gain=100;p.sustain=127;p.attack=0;
    p.motion=0;p.separation=0;p.roll=0;p.sub_mode=(uint8_t)pedal;
    p.body_degree=0;p.aux_degree=7;p.tuning=(uint8_t)tuning;p.bypass=1;
    zg_engine_set_patch(&e,&p);
    /* Mute assembly participation immediately. The engine deliberately slews
     * level changes, so merely setting other level targets to zero leaves their
     * prior preset contributions in the first few milliseconds. */
    e.mute_mask=(uint8_t)(((1u<<ZG_LAYER_COUNT)-1u) ^ (1u<<layer));
    zg_engine_note_on(&e,note,127,0);
    for(at=0;at<88200u;at+=ZG_BLOCK) {
        unsigned n=88200u-at;if(n>ZG_BLOCK)n=ZG_BLOCK;
        zg_engine_render(&e,out,n);
        if(fwrite(out,2,n,stdout)!=n)return 3;
    }
    return 0;
}
