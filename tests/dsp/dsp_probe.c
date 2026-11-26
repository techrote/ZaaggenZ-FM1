/* SPDX-License-Identifier: MIT */
#include "zg/dsp.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc,char **argv)
{
    zg_native_state ns;zg_character_state cs;zg_dsp_coeffs c;
    zg_dsp_params p={24576,28000,22000,0,0};
    unsigned long i, count=96000;int32_t x,y;
    unsigned family=argc>1?(unsigned)atoi(argv[1]):0u;
    int character_only=argc>3?atoi(argv[3]):0;
    uint32_t phase=0,inc=argc>4?(uint32_t)strtoul(argv[4],0,0):4294967u;
    p.family=(uint8_t)family;p.quality=(uint8_t)(argc>2?atoi(argv[2]):0);
    zg_native_init(&ns,123);zg_character_init(&cs);
    zg_dsp_prepare(&c,&p,48000,inc,283348u);
    for(i=0;i<count;i++) {
        phase+=inc;
        x=character_only ? zg_sine(phase)>>1 : zg_native_sample(&ns,&c,inc);
        y=family<6u ? zg_character_sample(&cs,&c,x) : x;
        {int16_t out=(int16_t)y;if(fwrite(&out,2,1,stdout)!=1)return 2;}
    }
    return ferror(stdout)?1:0;
}
