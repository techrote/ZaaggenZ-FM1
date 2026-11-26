/* SPDX-License-Identifier: MIT
 * Offline renderer for the identical fixed-point engine used in the FM-1 plugin. */
#include "zg/engine.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

static void u16(FILE*f,uint16_t v){fputc(v&255,f);fputc(v>>8,f);}
static void u32(FILE*f,uint32_t v){u16(f,(uint16_t)v);u16(f,(uint16_t)(v>>16));}
static void wav_header(FILE*f,uint32_t rate,uint32_t frames)
{
    fwrite("RIFF",1,4,f);u32(f,36+frames*2);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,1);u16(f,1);u32(f,rate);u32(f,rate*2);u16(f,2);u16(f,16);
    fwrite("data",1,4,f);u32(f,frames*2);
}
static uint64_t hash_pcm(uint64_t h,int16_t x)
{h^=(uint8_t)x;h*=1099511628211ull;h^=(uint8_t)((uint16_t)x>>8);return h*1099511628211ull;}
static void help(void)
{
 puts("ZaaggenZ-FM1 actual native-engine renderer\n"
      "  --list                         list 36 factory instruments\n"
      "  --out FILE.wav                 render mono 16-bit PCM\n"
      "  --preset 0..35 --seconds 1..120 --note 0..127 --rate 8000..48000\n"
      "  --mode native|image|hybrid      architecture override\n"
      "  --source 0..6 --family 0..5 --grit 0..127 --motion 0..127 --vowel 0..127\n"
      "  --pattern hold|riff|roll|glide --bpm 40..300 --quality 0|1 --bypass\n"
      "  --stems PREFIX                 write seven pre-master 32-bit float WAVs\n"
      "  --benchmark                    render without writing PCM; print JSON metrics\n"
      "--seconds is the musical duration; the complete bounded release tail is appended.\n"
      "All reported timing is this host CPU, never an FM-1 hardware claim.");
}
static void float_wav_header(FILE*f,uint32_t rate,uint32_t frames)
{
    fwrite("RIFF",1,4,f);u32(f,36+frames*4);fwrite("WAVEfmt ",1,8,f);u32(f,16);
    u16(f,3);u16(f,1);u32(f,rate);u32(f,rate*4);u16(f,4);u16(f,32);fwrite("data",1,4,f);u32(f,frames*4);
}
static int number(const char*s,int lo,int hi,const char*name)
{ char*end;long v=strtol(s,&end,10);if(!*s||*end||v<lo||v>hi){fprintf(stderr,"Invalid %s: %s\n",name,s);exit(2);}return (int)v; }
int main(int argc,char**argv)
{
    const char*outpath="audition.wav",*pattern="riff",*stem_prefix=0;
    unsigned preset=0,rate=44100,seconds=6,note=36,mode=99,source=99,family=99,quality=99,bpm=0;
    int grit=-1,motion=-1,vowel=-1,bypass=0,benchmark=0;
    for(int i=1;i<argc;i++) {
        const char*a=argv[i];
        if(!strcmp(a,"--help")){help();return 0;}
        if(!strcmp(a,"--list")){for(unsigned j=0;j<zg_preset_count;j++)printf("%u\t%s\t%s\n",j,zg_presets[j].name,zg_presets[j].description);return 0;}
        if(!strcmp(a,"--bypass")){bypass=1;continue;}
        if(!strcmp(a,"--benchmark")){benchmark=1;continue;}
        if(i+1>=argc){fprintf(stderr,"Missing value for %s\n",a);return 2;}
        const char*v=argv[++i];
        if(!strcmp(a,"--out"))outpath=v;
        else if(!strcmp(a,"--stems"))stem_prefix=v;
        else if(!strcmp(a,"--pattern")){pattern=v;if(strcmp(v,"hold")&&strcmp(v,"riff")&&strcmp(v,"roll")&&strcmp(v,"glide")){fprintf(stderr,"Unknown pattern\n");return 2;}}
        else if(!strcmp(a,"--preset"))preset=(unsigned)number(v,0,(int)zg_preset_count-1,a);
        else if(!strcmp(a,"--seconds"))seconds=(unsigned)number(v,1,120,a);
        else if(!strcmp(a,"--note"))note=(unsigned)number(v,0,127,a);
        else if(!strcmp(a,"--rate"))rate=(unsigned)number(v,8000,48000,a);
        else if(!strcmp(a,"--source"))source=(unsigned)number(v,0,(int)zg_source_count-1,a);
        else if(!strcmp(a,"--family"))family=(unsigned)number(v,0,5,a);
        else if(!strcmp(a,"--quality"))quality=(unsigned)number(v,0,1,a);
        else if(!strcmp(a,"--bpm"))bpm=(unsigned)number(v,40,300,a);
        else if(!strcmp(a,"--grit"))grit=number(v,0,127,a);
        else if(!strcmp(a,"--motion"))motion=number(v,0,127,a);
        else if(!strcmp(a,"--vowel"))vowel=number(v,0,127,a);
        else if(!strcmp(a,"--mode")){mode=!strcmp(v,"native")?0:!strcmp(v,"image")?1:!strcmp(v,"hybrid")?2:99;if(mode==99){fprintf(stderr,"Unknown mode\n");return 2;}}
        else {fprintf(stderr,"Unknown argument: %s\n",a);return 2;}
    }
    zg_engine e;zg_engine_init(&e,rate);zg_engine_set_preset(&e,preset);
    zg_patch p=e.patch;
    if(mode!=99)p.mode=(uint8_t)mode;
    if(source!=99)p.source=(uint8_t)source;
    if(family!=99)p.family=(uint8_t)family;
    if(quality!=99)p.quality=(uint8_t)quality;
    if(bpm)p.bpm=(uint16_t)bpm;
    if(grit>=0)p.grit=(uint8_t)grit;
    if(motion>=0)p.motion=(uint8_t)motion;
    if(vowel>=0)p.vowel=(uint8_t)vowel;
    if(bypass)p.bypass=1;
    if(!strcmp(pattern,"roll"))p.roll=6;
    if(!strcmp(pattern,"glide"))p.glide=80;
    zg_engine_set_patch(&e,&p);
    uint32_t play_frames=seconds*rate;
    uint32_t tail=(2*zg_time_ms(e.patch.release)+180)*rate/1000;
    zg_source_state tail_source;zg_source_reset(&tail_source,e.patch.source);
    zg_source_prepare(&tail_source,zg_pitch_increment((int)note,e.patch.tuning,rate),rate);
    const zg_source_info*info=zg_source_get_info(e.patch.source);
    uint32_t source_tail=(uint32_t)(((uint64_t)info->frames[0]<<16)/tail_source.step_q16)+rate/5;
    if(source_tail>tail)tail=source_tail;
    uint32_t frames=play_frames+tail;
    FILE*f=benchmark?0:fopen(outpath,"wb");
    if(!benchmark&&!f){perror(outpath);return 1;}
    if(f)wav_header(f,rate,frames);
    FILE*sf[ZG_STEM_COUNT]={0};
    const char*sn[]={"synthline_raw","exciter_raw","body","aux","sub","source_bus","pre_master"};
    if(stem_prefix)for(unsigned j=0;j<ZG_STEM_COUNT;j++){
        char path[1024];snprintf(path,sizeof(path),"%s_%s.wav",stem_prefix,sn[j]);sf[j]=fopen(path,"wb");
        if(!sf[j]){perror(path);return 1;}float_wav_header(sf[j],rate,frames);
    }
    uint32_t step=rate*60/(e.patch.bpm*4),next=0,off=0,at=0;
    unsigned seq=0,current_note=note;
    static const int8_t notes[]={0,0,7,3,0,12,10,7,0,3,5,7,12,10,7,3};
    int16_t pcm[ZG_BLOCK];int32_t stems[ZG_BLOCK*ZG_STEM_COUNT];
    uint64_t hash=1469598103934665603ull;double sum=0,max_block_ms=0;unsigned peak=0,clipped=0;
    clock_t begin=clock();unsigned blocks=0;
    while(at<frames) {
        if(!strcmp(pattern,"hold")) {
            if(!at)zg_engine_note_on(&e,note,110,0);
            if(at>=play_frames && e.gate)zg_engine_note_off(&e,note);
        } else if(at<play_frames) {
            if(at>=next) {
                current_note=note+(unsigned)notes[seq%16];if(current_note>127)current_note=127;
                zg_engine_note_on(&e,current_note,seq%4==0?120:94,!strcmp(pattern,"glide"));
                off=next+step*3/4;next+=step;seq++;
            } else if(at>=off && e.gate && strcmp(pattern,"glide"))zg_engine_note_off(&e,current_note);
        } else if(e.gate)zg_engine_all_notes_off(&e);
        unsigned n=frames-at>ZG_BLOCK?ZG_BLOCK:frames-at;
        clock_t tick=clock();zg_engine_render_stems(&e,pcm,stem_prefix?stems:0,n);
        double ms=1000.0*(clock()-tick)/CLOCKS_PER_SEC;if(ms>max_block_ms)max_block_ms=ms;blocks++;
        for(unsigned j=0;j<n;j++) {
            int x=pcm[j];unsigned ax=(unsigned)(x<0?-x:x);if(ax>peak)peak=ax;if(ax>=32760)clipped++;
            sum+=(double)x*x;hash=hash_pcm(hash,pcm[j]);if(f)u16(f,(uint16_t)pcm[j]);
            if(stem_prefix)for(unsigned k=0;k<ZG_STEM_COUNT;k++){
                float q=(float)stems[j*ZG_STEM_COUNT+k]/32768.0f;uint32_t bits;memcpy(&bits,&q,4);u32(sf[k],bits);
            }
        }
        at+=n;
    }
    double elapsed=(double)(clock()-begin)/CLOCKS_PER_SEC;
    if(f&&fclose(f)){perror("write");return 1;}
    for(unsigned j=0;j<ZG_STEM_COUNT;j++)if(sf[j])fclose(sf[j]);
    printf("{\"preset\":%u,\"mode\":%u,\"frames\":%u,\"rate\":%u,\"peak\":%u,\"rms\":%.8f,\"clipped_frames\":%u,\"pcm_fnv64\":\"%016llx\",\"engine_bytes\":%u,\"host_cpu_seconds\":%.6f,\"host_max_block_ms\":%.6f,\"host_mean_block_us\":%.4f,\"hardware_qualified\":false}\n",preset,e.patch.mode,frames,rate,peak,sqrt(sum/frames)/32768.0,clipped,(unsigned long long)hash,(unsigned)sizeof(e),elapsed,max_block_ms,elapsed*1e6/blocks);
    return 0;
}
