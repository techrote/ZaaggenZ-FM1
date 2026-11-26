/* SPDX-License-Identifier: MIT */
#include "zg/engine.h"
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

/* Real engine source bus and isolated persistent sum, signed int32 Q15. */
int main(int argc,char **argv)
{
#ifdef _WIN32
    if(_setmode(_fileno(stdout),_O_BINARY)==-1)return 4;
#endif
    zg_engine e;zg_patch p;int16_t pcm[ZG_BLOCK];int32_t stems[ZG_BLOCK*ZG_STEM_COUNT];
    unsigned family=argc>1?(unsigned)atoi(argv[1]):0;
    unsigned mode=argc>2?(unsigned)atoi(argv[2]):1;
    unsigned macro=argc>3?(unsigned)atoi(argv[3]):99;
    unsigned value=argc>4?(unsigned)atoi(argv[4]):0;
    unsigned source=argc>5?(unsigned)atoi(argv[5]):family+1;
    if(family>=6 || mode>2 || source>=zg_source_count)return 2;
    zg_engine_init(&e,44100);p=e.patch;p.family=(uint8_t)family;p.mode=(uint8_t)mode;p.source=(uint8_t)source;
    p.source_sustain=1;p.roll=0;
    if(macro==1)p.grit=(uint8_t)value;else if(macro==2)p.motion=(uint8_t)value;else if(macro==3)p.vowel=(uint8_t)value;
    zg_engine_set_patch(&e,&p);zg_engine_note_on(&e,36,110,0);
    for(unsigned at=0;at<44100u*3;at+=ZG_BLOCK) {
        unsigned n=44100u*3-at;if(n>ZG_BLOCK)n=ZG_BLOCK;
        zg_engine_render_stems(&e,pcm,stems,n);
        for(unsigned i=0;i<n;i++) {
            int32_t row[3]={stems[i*7],stems[i*7+5],stems[i*7+2]+stems[i*7+3]+stems[i*7+4]};
            if(fwrite(row,sizeof row,1,stdout)!=1)return 3;
        }
    }
    return 0;
}
