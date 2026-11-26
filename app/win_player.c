/* SPDX-License-Identifier: MIT
 * Native Windows evaluator: Win32 controls + WinMM audio/MIDI, no runtime DLLs
 * beyond the Windows system libraries. Same core and assets as the FM-1 plugin. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "zg/engine.h"

#define AUDIO_FRAMES 512
#define AUDIO_BUFFERS 4
#define ID_PRESET 100
#define ID_MODE 101
#define ID_FAMILY 102
#define ID_SOURCE 103
#define ID_TUNING 104
#define ID_ROLL 105
#define ID_BPM 106
#define ID_PLAY 107
#define ID_STOP 108
#define ID_EXPORT 109
#define ID_MIDI 110
#define ID_BYPASS 111
#define ID_MACRO 200
#define ID_LAYER 210
#define ID_GLIDE 220
#define ID_RELEASE 221

static zg_engine engine;
static CRITICAL_SECTION lock;
static HANDLE audio_thread;
static volatile LONG running=1;
static HWAVEOUT wave;
static HMIDIIN midi;
static WAVEHDR headers[AUDIO_BUFFERS];
static int16_t buffers[AUDIO_BUFFERS][AUDIO_FRAMES];
static HWND window,preset_box,mode_box,family_box,source_box,tuning_box,roll_box,midi_box;
static HWND macros[3],layers[5],bpm_slider,glide_slider,release_slider,bypass_box;
static HFONT font,title_font;
static HBRUSH background,control_bg;
static int sequencer=0,seqstep=0,current_note=36,octave=0;
static uint32_t seqclock=0;
static uint8_t held[128];static uint32_t key_order[128],serial;
static int16_t scope[256];
static char status_text[256]="Ready. Play Z S X D C V G B H N J M, or start the phrase.";
static int self_test=0;

static void note_on(unsigned note,unsigned velocity)
{
    if(note>127)return;
    held[note]=1;key_order[note]=++serial;current_note=(int)note;
    zg_engine_note_on(&engine,note,velocity,engine.gate);
}
static void note_off(unsigned note)
{
    if(note>127)return;held[note]=0;
    if(engine.note!=note)return;
    unsigned best=128;uint32_t age=0;
    for(unsigned n=0;n<128;n++)if(held[n]&&key_order[n]>age){age=key_order[n];best=n;}
    if(best<128){current_note=(int)best;zg_engine_note_on(&engine,best,100,1);}
    else zg_engine_note_off(&engine,note);
}
static void stop_all(void)
{sequencer=0;memset(held,0,sizeof held);zg_engine_all_notes_off(&engine);}
static void render_audio(int16_t*out,unsigned count)
{
    static const int8_t melody[16]={0,0,7,3,0,12,10,7,0,3,5,7,12,10,7,3};
    unsigned offset=0;
    while(offset<count){
        unsigned n=count-offset>32?32:count-offset;
        if(sequencer){
            uint32_t duration=44100u*60u/(engine.patch.bpm*4u);
            if(!seqclock){current_note=36+octave*12+melody[seqstep&15];zg_engine_note_on(&engine,(unsigned)current_note,seqstep%4?98:120,0);seqstep++;}
            if(seqclock>=duration*3/4&&engine.gate)zg_engine_all_notes_off(&engine);
            seqclock+=n;if(seqclock>=duration)seqclock-=duration;
            /* Crossing the boundary must schedule the next hit, even when the
             * 32-sample callback does not divide the musical step exactly. */
            if(seqclock<n)seqclock=0;
        }
        zg_engine_render(&engine,out+offset,n);offset+=n;
    }
    for(unsigned i=0;i<256;i++)scope[i]=out[(i*count)/256];
}
static DWORD WINAPI audio_main(LPVOID unused)
{
    (void)unused;
    while(InterlockedCompareExchange(&running,1,1)){
        int did=0;
        for(unsigned i=0;i<AUDIO_BUFFERS;i++)if(headers[i].dwFlags&WHDR_DONE){
            EnterCriticalSection(&lock);render_audio(buffers[i],AUDIO_FRAMES);LeaveCriticalSection(&lock);
            if(waveOutWrite(wave,&headers[i],sizeof(WAVEHDR))!=MMSYSERR_NOERROR)InterlockedExchange(&running,0);
            did=1;
        }
        if(!did)Sleep(1);
    }
    return 0;
}
static void CALLBACK midi_callback(HMIDIIN handle,UINT msg,DWORD_PTR instance,DWORD_PTR one,DWORD_PTR two)
{
    (void)handle;(void)instance;(void)two;
    if(msg!=MIM_DATA)return;
    unsigned type=(unsigned)one&0xf0u,note=((unsigned)one>>8)&127u,velocity=((unsigned)one>>16)&127u;
    EnterCriticalSection(&lock);
    if(type==0x90&&velocity)note_on(note,velocity);
    else if(type==0x80||type==0x90)note_off(note);
    else if(type==0xb0&&(note==123||note==120))stop_all();
    LeaveCriticalSection(&lock);
}
static void start_audio(void)
{
    WAVEFORMATEX fmt={0};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=1;fmt.nSamplesPerSec=44100;
    fmt.wBitsPerSample=16;fmt.nBlockAlign=2;fmt.nAvgBytesPerSec=88200;
    MMRESULT r=waveOutOpen(&wave,WAVE_MAPPER,&fmt,0,0,CALLBACK_NULL);
    if(r){wave=0;snprintf(status_text,sizeof(status_text),"No Windows audio output (code %u). WAV export and controls still work.",r);return;}
    for(unsigned i=0;i<AUDIO_BUFFERS;i++){
        memset(&headers[i],0,sizeof headers[i]);headers[i].lpData=(LPSTR)buffers[i];headers[i].dwBufferLength=sizeof buffers[i];
        waveOutPrepareHeader(wave,&headers[i],sizeof(WAVEHDR));headers[i].dwFlags|=WHDR_DONE;
    }
    audio_thread=CreateThread(0,0,audio_main,0,0,0);
}
static HWND child(const char*cls,const char*text,DWORD style,int x,int y,int w,int h,int id)
{
    HWND c=CreateWindowExA(0,cls,text,WS_CHILD|WS_VISIBLE|style,x,y,w,h,window,(HMENU)(INT_PTR)id,GetModuleHandleA(0),0);
    SendMessageA(c,WM_SETFONT,(WPARAM)font,TRUE);return c;
}
static HWND combo(int x,int y,int w,int id)
{return child("COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL,x,y,w,350,id);}
static HWND slider(int x,int y,int w,int id)
{
    HWND c=child(TRACKBAR_CLASSA,"",TBS_HORZ|TBS_NOTICKS,x,y,w,32,id);
    SendMessageA(c,TBM_SETRANGE,TRUE,MAKELONG(0,127));return c;
}
static void item(HWND c,const char*s){SendMessageA(c,CB_ADDSTRING,0,(LPARAM)s);}
static void sync_controls(void)
{
    SendMessageA(preset_box,CB_SETCURSEL,engine.preset,0);
    SendMessageA(mode_box,CB_SETCURSEL,engine.patch.mode,0);
    SendMessageA(family_box,CB_SETCURSEL,engine.patch.family,0);
    SendMessageA(source_box,CB_SETCURSEL,engine.patch.source,0);
    SendMessageA(tuning_box,CB_SETCURSEL,engine.patch.tuning,0);
    SendMessageA(roll_box,CB_SETCURSEL,engine.patch.roll,0);
    SendMessageA(macros[0],TBM_SETPOS,TRUE,engine.patch.grit);SendMessageA(macros[1],TBM_SETPOS,TRUE,engine.patch.motion);SendMessageA(macros[2],TBM_SETPOS,TRUE,engine.patch.vowel);
    for(unsigned i=0;i<5;i++)SendMessageA(layers[i],TBM_SETPOS,TRUE,engine.patch.level[i]);
    SendMessageA(bpm_slider,TBM_SETPOS,TRUE,engine.patch.bpm);
    SendMessageA(glide_slider,TBM_SETPOS,TRUE,engine.patch.glide);SendMessageA(release_slider,TBM_SETPOS,TRUE,engine.patch.release);
    SendMessageA(bypass_box,BM_SETCHECK,engine.patch.bypass?BST_CHECKED:BST_UNCHECKED,0);
    InvalidateRect(window,0,FALSE);
}
static void controls(void)
{
    child("STATIC","FACTORY INSTRUMENT",0,26,88,190,20,0);preset_box=combo(26,112,400,ID_PRESET);
    for(unsigned i=0;i<zg_preset_count;i++)item(preset_box,zg_presets[i].name);
    child("STATIC","SOURCE ARCHITECTURE",0,450,88,220,20,0);mode_box=combo(450,112,180,ID_MODE);
    item(mode_box,"Native procedural");item(mode_box,"Source image");item(mode_box,"Hybrid blend");
    child("STATIC","COMPILED SOURCE",0,652,88,220,20,0);source_box=combo(652,112,240,ID_SOURCE);
    for(unsigned i=0;i<zg_source_count;i++)item(source_box,zg_source_get_info(i)->name);
    child("STATIC","CHARACTER",0,26,166,190,20,0);family_box=combo(26,190,210,ID_FAMILY);
    for(unsigned i=0;i<6;i++)item(family_box,zg_family_name(i));
    const char*labels[]={"GRIT","MOTION","VOWEL"};
    for(int i=0;i<3;i++){child("STATIC",labels[i],0,258+i*218,166,190,20,0);macros[i]=slider(252+i*218,189,205,ID_MACRO+i);}
    const char*ln[]={"SYNTHLINE","EXCITER","BODY","AUX","SUB"};
    for(int i=0;i<5;i++){child("STATIC",ln[i],0,26+i*177,252,160,20,0);layers[i]=slider(20+i*177,274,162,ID_LAYER+i);}
    child("STATIC","TUNING",0,26,326,170,20,0);tuning_box=combo(26,350,200,ID_TUNING);
    for(unsigned i=0;i<4;i++)item(tuning_box,zg_tuning_name(i));
    child("STATIC","EXCITER ROLLS",0,250,326,170,20,0);roll_box=combo(250,350,185,ID_ROLL);
    const char*rolls[]={"Off","Quarter","Eighth","Eighth triplets","Sixteenth","Sixteenth triplets","Thirty-second"};
    for(unsigned i=0;i<7;i++)item(roll_box,rolls[i]);
    child("STATIC","TEMPO",0,460,326,160,20,0);bpm_slider=slider(452,347,200,ID_BPM);SendMessageA(bpm_slider,TBM_SETRANGE,TRUE,MAKELONG(40,300));
    child("STATIC","GLIDE",0,680,326,120,20,0);glide_slider=slider(672,347,100,ID_GLIDE);
    child("STATIC","RELEASE",0,790,326,120,20,0);release_slider=slider(782,347,116,ID_RELEASE);
    child("BUTTON","Play phrase",BS_PUSHBUTTON,26,410,150,34,ID_PLAY);child("BUTTON","Stop / panic",BS_PUSHBUTTON,190,410,150,34,ID_STOP);
    child("BUTTON","Export current WAV",BS_PUSHBUTTON,354,410,180,34,ID_EXPORT);
    bypass_box=child("BUTTON","Character bypass",BS_AUTOCHECKBOX,560,416,180,22,ID_BYPASS);
    child("STATIC","MIDI INPUT",0,26,470,120,20,0);midi_box=combo(142,466,390,ID_MIDI);item(midi_box,"Computer keyboard only");
    for(UINT i=0;i<midiInGetNumDevs();i++){MIDIINCAPSA caps;if(!midiInGetDevCapsA(i,&caps,sizeof caps))item(midi_box,caps.szPname);}
    SendMessageA(midi_box,CB_SETCURSEL,0,0);
    child("STATIC","Z row = C2   Q row = C3   [ / ] octave   Space = phrase",0,26,512,850,22,0);
    sync_controls();
}
static void le16(FILE*f,uint16_t n){fputc(n&255,f);fputc(n>>8,f);}
static void le32(FILE*f,uint32_t n){le16(f,(uint16_t)n);le16(f,(uint16_t)(n>>16));}
static int export_wav(void)
{
    zg_engine copy;EnterCriticalSection(&lock);copy=engine;LeaveCriticalSection(&lock);
    zg_engine_init(&copy,44100);EnterCriticalSection(&lock);zg_engine_set_patch(&copy,&engine.patch);LeaveCriticalSection(&lock);
    uint32_t play=3*44100,tail=(2*zg_time_ms(copy.patch.release)+1000)*44100/1000,frames=play+tail;
    FILE*f=fopen("ZaaggenZ-current.wav","wb");if(!f)return 0;
    fwrite("RIFF",1,4,f);le32(f,36+frames*2);fwrite("WAVEfmt ",1,8,f);le32(f,16);le16(f,1);le16(f,1);le32(f,44100);le32(f,88200);le16(f,2);le16(f,16);fwrite("data",1,4,f);le32(f,frames*2);
    zg_engine_note_on(&copy,(unsigned)current_note,110,0);int16_t pcm[32];
    for(uint32_t at=0;at<frames;){unsigned n=frames-at>32?32:frames-at;if(at>=play&&copy.gate)zg_engine_all_notes_off(&copy);zg_engine_render(&copy,pcm,n);for(unsigned i=0;i<n;i++)le16(f,(uint16_t)pcm[i]);at+=n;}
    int ok=fclose(f)==0;return ok;
}
static void capture_window(void)
{
    RECT r;GetClientRect(window,&r);HDC dc=GetDC(window),mem=CreateCompatibleDC(dc);
    BITMAPINFO bi={0};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=r.right;bi.bmiHeader.biHeight=-r.bottom;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    void*pixels;HBITMAP bitmap=CreateDIBSection(dc,&bi,DIB_RGB_COLORS,&pixels,0,0);HGDIOBJ old=SelectObject(mem,bitmap);PrintWindow(window,mem,PW_CLIENTONLY);
    FILE*f=fopen("windows-audition.bmp","wb");if(f){BITMAPFILEHEADER b={0};b.bfType=0x4d42;b.bfOffBits=sizeof b+sizeof(BITMAPINFOHEADER);b.bfSize=b.bfOffBits+(DWORD)r.right*(DWORD)r.bottom*4;fwrite(&b,sizeof b,1,f);fwrite(&bi.bmiHeader,sizeof(BITMAPINFOHEADER),1,f);fwrite(pixels,(size_t)r.right*r.bottom*4,1,f);fclose(f);}
    SelectObject(mem,old);DeleteObject(bitmap);DeleteDC(mem);ReleaseDC(window,dc);
}
static int key_note(WPARAM key)
{
    const char*low="ZSXDCVGBHNJM",*high="Q2W3ER5T6Y7U";const char*p;
    if(key>127)return -1;
    if((p=strchr(low,(int)key)))return 36+octave*12+(int)(p-low);
    if((p=strchr(high,(int)key)))return 48+octave*12+(int)(p-high);
    return -1;
}
static void paint(void)
{
    PAINTSTRUCT ps;HDC dc=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);FillRect(dc,&r,background);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(239,243,245));SelectObject(dc,title_font);TextOutA(dc,26,22,"ZAAGGENZ / FM-1",15);
    SelectObject(dc,font);SetTextColor(dc,RGB(106,221,211));const char*subtitle="NATIVE INSTRUMENT  |  WINDOWS AUDITION";TextOutA(dc,476,33,subtitle,(int)strlen(subtitle));
    EnterCriticalSection(&lock);int16_t points[256];memcpy(points,scope,sizeof points);zg_patch p=engine.patch;int n=current_note;int seq=sequencer;LeaveCriticalSection(&lock);
    RECT box={26,552,892,652};HBRUSH b=CreateSolidBrush(RGB(12,19,24));FillRect(dc,&box,b);DeleteObject(b);
    HPEN line=CreatePen(PS_SOLID,1,RGB(106,221,211));HGDIOBJ old=SelectObject(dc,line);
    for(unsigned i=0;i<256;i++){int x=26+(int)i*866/255,y=602-points[i]*46/32768;if(!i)MoveToEx(dc,x,y,0);else LineTo(dc,x,y);}
    SelectObject(dc,old);DeleteObject(line);
    char info[256];snprintf(info,sizeof info,"%s  |  key %d  |  %u BPM  |  grit %u / motion %u / vowel %u  |  44.1 kHz integer core",seq?"PHRASE PLAYING":"LIVE KEYS",n,p.bpm,p.grit,p.motion,p.vowel);
    SetTextColor(dc,RGB(194,207,214));TextOutA(dc,26,668,info,(int)strlen(info));SetTextColor(dc,RGB(150,165,178));TextOutA(dc,26,699,status_text,(int)strlen(status_text));
    EndPaint(window,&ps);
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM w,LPARAM l)
{
    if(msg==WM_CREATE){window=h;controls();SetTimer(h,1,40,0);if(self_test)SetTimer(h,2,1200,0);return 0;}
    if(msg==WM_PAINT){paint();return 0;}
    if(msg==WM_ERASEBKGND)return 1;
    if(msg==WM_CTLCOLORSTATIC||msg==WM_CTLCOLORBTN){SetTextColor((HDC)w,RGB(206,217,223));SetBkColor((HDC)w,RGB(24,32,40));return (LRESULT)background;}
    if(msg==WM_HSCROLL){HWND c=(HWND)l;int v=(int)SendMessageA(c,TBM_GETPOS,0,0);EnterCriticalSection(&lock);
        for(unsigned i=0;i<3;i++)if(c==macros[i])zg_engine_set_macro(&engine,ZG_GRIT+i,(unsigned)v);
        for(unsigned i=0;i<5;i++)if(c==layers[i])engine.patch.level[i]=(uint8_t)v;
        if(c==bpm_slider)engine.patch.bpm=(uint16_t)v;if(c==glide_slider)engine.patch.glide=(uint8_t)v;if(c==release_slider)engine.patch.release=(uint8_t)v;
        LeaveCriticalSection(&lock);SetFocus(window);return 0;}
    if(msg==WM_COMMAND){unsigned id=LOWORD(w);int selected=0;
        if((id==ID_PRESET||id==ID_MODE||id==ID_FAMILY||id==ID_SOURCE||id==ID_TUNING||id==ID_ROLL||id==ID_MIDI)&&HIWORD(w)!=CBN_SELCHANGE)return 0;
        if(id==ID_MIDI&&HIWORD(w)==CBN_SELCHANGE){if(midi){midiInStop(midi);midiInClose(midi);midi=0;}selected=(int)SendMessageA(midi_box,CB_GETCURSEL,0,0);if(selected>0){MMRESULT r=midiInOpen(&midi,(UINT)(selected-1),(DWORD_PTR)midi_callback,0,CALLBACK_FUNCTION);if(!r)midiInStart(midi);else snprintf(status_text,sizeof(status_text),"MIDI input failed (code %u)",r);}SetFocus(window);return 0;}
        if(id==ID_EXPORT){snprintf(status_text,sizeof(status_text),"%s",export_wav()?"Saved ZaaggenZ-current.wav beside the application, with its release tail.":"Could not write WAV; choose a writable application folder.");return 0;}
        EnterCriticalSection(&lock);
        if(id==ID_PRESET&&HIWORD(w)==CBN_SELCHANGE){selected=(int)SendMessageA(preset_box,CB_GETCURSEL,0,0);zg_engine_set_preset(&engine,(unsigned)selected);sync_controls();}
        if(id==ID_MODE&&HIWORD(w)==CBN_SELCHANGE)engine.patch.mode=(uint8_t)SendMessageA(mode_box,CB_GETCURSEL,0,0);
        if(id==ID_FAMILY&&HIWORD(w)==CBN_SELCHANGE){zg_patch p=engine.patch;p.family=(uint8_t)SendMessageA(family_box,CB_GETCURSEL,0,0);zg_engine_set_patch(&engine,&p);}
        if(id==ID_SOURCE&&HIWORD(w)==CBN_SELCHANGE){zg_patch p=engine.patch;p.source=(uint8_t)SendMessageA(source_box,CB_GETCURSEL,0,0);zg_engine_set_patch(&engine,&p);}
        if(id==ID_TUNING&&HIWORD(w)==CBN_SELCHANGE){zg_patch p=engine.patch;p.tuning=(uint8_t)SendMessageA(tuning_box,CB_GETCURSEL,0,0);zg_engine_set_patch(&engine,&p);}
        if(id==ID_ROLL&&HIWORD(w)==CBN_SELCHANGE)engine.patch.roll=(uint8_t)SendMessageA(roll_box,CB_GETCURSEL,0,0);
        if(id==ID_BYPASS){engine.patch.bypass=(uint8_t)(SendMessageA(bypass_box,BM_GETCHECK,0,0)==BST_CHECKED);engine.bypass_character=engine.patch.bypass;}
        if(id==ID_PLAY){sequencer=!sequencer;seqclock=0;seqstep=0;if(!sequencer)stop_all();}
        if(id==ID_STOP)stop_all();LeaveCriticalSection(&lock);SetFocus(window);return 0;}
    if(msg==WM_KEYDOWN){if(l&(1L<<30))return 0;EnterCriticalSection(&lock);int note=key_note(w);
        if(note>=0&&note<=127)note_on((unsigned)note,110);
        else if(w==VK_SPACE){sequencer=!sequencer;seqclock=0;seqstep=0;if(!sequencer)stop_all();}
        else if(w==VK_ESCAPE)stop_all();else if(w==VK_OEM_4){stop_all();if(octave>-2)octave--;}else if(w==VK_OEM_6){stop_all();if(octave<4)octave++;}
        LeaveCriticalSection(&lock);return 0;}
    if(msg==WM_KEYUP){int note=key_note(w);EnterCriticalSection(&lock);if(note>=0&&note<=127)note_off((unsigned)note);LeaveCriticalSection(&lock);return 0;}
    if(msg==WM_KILLFOCUS){EnterCriticalSection(&lock);if(!sequencer){memset(held,0,sizeof held);zg_engine_all_notes_off(&engine);}LeaveCriticalSection(&lock);}
    if(msg==WM_TIMER){if(w==1){RECT r={26,552,910,735};InvalidateRect(h,&r,FALSE);}else if(w==2){KillTimer(h,2);EnterCriticalSection(&lock);zg_engine_set_preset(&engine,6);zg_engine_note_on(&engine,36,110,0);int16_t probe[512];render_audio(probe,512);sync_controls();LeaveCriticalSection(&lock);UpdateWindow(h);capture_window();int ok=export_wav();FILE*f=fopen("windows-smoke.json","w");if(f){fprintf(f,"{\"native_window_created\":true,\"core_rendered\":true,\"wav_exported\":%s,\"audio_device_opened\":%s,\"hardware_qualified\":false}\n",ok?"true":"false",wave?"true":"false");fclose(f);}PostMessageA(h,WM_CLOSE,0,0);}return 0;}
    if(msg==WM_DESTROY){PostQuitMessage(0);return 0;}
    return DefWindowProcA(h,msg,w,l);
}
int WINAPI WinMain(HINSTANCE instance,HINSTANCE previous,LPSTR command,int show)
{
    (void)previous;self_test=strstr(command,"--smoke")!=0;InitializeCriticalSection(&lock);zg_engine_init(&engine,44100);
    INITCOMMONCONTROLSEX cc={sizeof cc,ICC_BAR_CLASSES};InitCommonControlsEx(&cc);
    font=CreateFontA(-16,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");
    title_font=CreateFontA(-32,0,0,0,FW_BOLD,0,0,0,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,"Segoe UI");background=CreateSolidBrush(RGB(24,32,40));control_bg=CreateSolidBrush(RGB(35,45,55));
    WNDCLASSA wc={0};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName="ZaaggenZFM1Audition";wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=background;
    if(!RegisterClassA(&wc))return 1;
    HWND h=CreateWindowA(wc.lpszClassName,"ZaaggenZ-FM1 - Native Instrument Audition",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,938,780,0,0,instance,0);
    if(!h)return 2;ShowWindow(h,show);UpdateWindow(h);start_audio();MSG m;
    while(GetMessageA(&m,0,0,0)>0){TranslateMessage(&m);DispatchMessageA(&m);}
    InterlockedExchange(&running,0);if(audio_thread){WaitForSingleObject(audio_thread,2000);CloseHandle(audio_thread);}if(midi){midiInStop(midi);midiInClose(midi);}if(wave){waveOutReset(wave);for(unsigned i=0;i<AUDIO_BUFFERS;i++)waveOutUnprepareHeader(wave,&headers[i],sizeof(WAVEHDR));waveOutClose(wave);}DeleteObject(font);DeleteObject(title_font);DeleteObject(background);DeleteObject(control_bg);DeleteCriticalSection(&lock);return 0;
}
