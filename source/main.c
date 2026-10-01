#include <3ds.h>
#include <citro2d.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "audio.h"
#include "playlist.h"
#include "lyrics.h"
#include "cover.h"

#define MUSIC_DIR   "sdmc:/music"

#define TOP_W 400
#define BOT_W 320
#define SCR_H 240

#define C_SKY_TOP C2D_Color32(0x2B,0x10,0x55,0xFF)
#define C_SKY_MID C2D_Color32(0x7A,0x1F,0x8B,0xFF)
#define C_SKY_BOT C2D_Color32(0xE8,0x4A,0x5F,0xFF)
#define C_PINK    C2D_Color32(0xFF,0x4D,0x9D,0xFF)
#define C_CYAN    C2D_Color32(0x2E,0xE6,0xD6,0xFF)
#define C_ORANGE  C2D_Color32(0xFF,0x9F,0x45,0xFF)
#define C_YELLOW  C2D_Color32(0xFF,0xD9,0x3D,0xFF)
#define C_CREAM   C2D_Color32(0xFF,0xE9,0xC7,0xFF)
#define C_INK     C2D_Color32(0x15,0x10,0x1F,0xFF)
#define C_INK2    C2D_Color32(0x24,0x1B,0x33,0xFF)
#define C_DIM     C2D_Color32(0xB8,0xA8,0xC8,0xFF)
#define C_DIM2    C2D_Color32(0x88,0x80,0x9A,0xFF)
#define C_TAPE    C2D_Color32(0x3A,0x2A,0x22,0xFF)
#define C_SCAN    C2D_Color32(0x00,0x00,0x00,0x44)
#define C_OFF     C2D_Color32(0x40,0x38,0x52,0xFF)
#define C_HDR     C2D_Color32(0x0C,0x09,0x12,0xFF)
#define C_ALBROW  C2D_Color32(0x2C,0x1F,0x44,0xFF)
#define C_PANEL   C2D_Color32(0x14,0x0E,0x20,0xC8)   // inserto J-card translucido (legibilidad sobre el degradado)

typedef enum { REPEAT_OFF=0, REPEAT_ALL, REPEAT_ONE } RepeatMode;

static C2D_TextBuf g_text;
static float g_reelAng = 0.0f;

// ---- layout pantalla inferior ----
#define LIST_TOP 24
#define LINE_H   15
#define BAR_Y    202
#define BAR_H    38
#define VIS_ROWS ((BAR_Y - LIST_TOP) / LINE_H)
#define NBTN     6
#define BTN_W    (BOT_W / NBTN)

static void txt(float x,float y,float z,float s,u32 c,const char* str){
    C2D_Text t; C2D_TextParse(&t,g_text,str); C2D_TextOptimize(&t);
    C2D_DrawText(&t,C2D_WithColor,x,y,z,s,s,c);
}
static void txtRight(float xr,float y,float z,float s,u32 c,const char* str){
    C2D_Text t; C2D_TextParse(&t,g_text,str); C2D_TextOptimize(&t);
    float w,h; C2D_TextGetDimensions(&t,s,s,&w,&h);
    C2D_DrawText(&t,C2D_WithColor,xr-w,y,z,s,s,c);
}
static void txtCtr(float cx,float y,float z,float s,u32 c,const char* str){
    C2D_Text t; C2D_TextParse(&t,g_text,str); C2D_TextOptimize(&t);
    float w,h; C2D_TextGetDimensions(&t,s,s,&w,&h);
    C2D_DrawText(&t,C2D_WithColor,cx-w/2,y,z,s,s,c);
}
// texto con sombra de tinta: legible sobre el degradado+scanlines de la pantalla superior
static void txtSh(float x,float y,float z,float s,u32 c,const char* str){
    C2D_Text t; C2D_TextParse(&t,g_text,str); C2D_TextOptimize(&t);
    C2D_DrawText(&t,C2D_WithColor,x+1,y+1,z,s,s,C_INK);
    C2D_DrawText(&t,C2D_WithColor,x,  y,  z,s,s,c);
}
static void clip(char* d,size_t n,const char* s,size_t mx){
    size_t l=strlen(s);
    if(l<=mx){snprintf(d,n,"%s",s);return;}
    if(mx>3)snprintf(d,n,"%.*s...",(int)(mx-3),s); else snprintf(d,n,"%.*s",(int)mx,s);
}
static void fmtTime(char* d,size_t n,uint64_t fr,uint32_t r){
    if(!r){snprintf(d,n,"0:00");return;}
    uint32_t s=(uint32_t)(fr/r); snprintf(d,n,"%u:%02u",(unsigned)(s/60),(unsigned)(s%60));
}
static void scanlines(float w){ for(float y=0;y<SCR_H;y+=3.0f) C2D_DrawRectSolid(0,y,0.9f,w,1.0f,C_SCAN); }

static void reel(float cx,float cy,float r,float ang){
    C2D_DrawCircleSolid(cx,cy,0.5f,r,C_TAPE);
    C2D_DrawCircleSolid(cx,cy,0.55f,r*0.45f,C_CREAM);
    for(int i=0;i<6;i++){ float a=ang+i*(float)(M_PI/3.0);
        C2D_DrawLine(cx+cosf(a)*r*0.18f,cy+sinf(a)*r*0.18f,C_INK,
                     cx+cosf(a)*r*0.95f,cy+sinf(a)*r*0.95f,C_INK,1.8f,0.6f); }
    C2D_DrawCircleSolid(cx,cy,0.6f,r*0.16f,C_INK);
}

// ---- iconos dibujados (la fuente del sistema no tiene ▶ ◀ ■ ♪) ----
static void icoPlay(float cx,float cy,float r,u32 c){
    C2D_DrawTriangle(cx-r*0.7f,cy-r,c, cx-r*0.7f,cy+r,c, cx+r*0.9f,cy,c, 0.45f);
}
static void icoPause(float cx,float cy,float r,u32 c){
    C2D_DrawRectSolid(cx-r*0.65f,cy-r,0.45f,r*0.5f,2*r,c);
    C2D_DrawRectSolid(cx+r*0.18f,cy-r,0.45f,r*0.5f,2*r,c);
}
static void icoStop(float cx,float cy,float r,u32 c){
    C2D_DrawRectSolid(cx-r*0.8f,cy-r*0.8f,0.45f,r*1.6f,r*1.6f,c);
}
static void icoNext(float cx,float cy,float r,u32 c){
    C2D_DrawTriangle(cx-r,cy-r,c, cx-r,cy+r,c, cx-r*0.05f,cy,c, 0.45f);
    C2D_DrawTriangle(cx-r*0.05f,cy-r,c, cx-r*0.05f,cy+r,c, cx+r*0.9f,cy,c, 0.45f);
}
static void icoPrev(float cx,float cy,float r,u32 c){
    C2D_DrawTriangle(cx+r,cy-r,c, cx+r,cy+r,c, cx+r*0.05f,cy,c, 0.45f);
    C2D_DrawTriangle(cx+r*0.05f,cy-r,c, cx+r*0.05f,cy+r,c, cx-r*0.9f,cy,c, 0.45f);
}
// disco de vinilo girando (placeholder / motivo del logo)
static void drawVinyl(float cx,float cy,float R,float ang){
    C2D_DrawCircleSolid(cx,cy,0.30f,R,        C2D_Color32(0x0c,0x09,0x12,0xFF));
    C2D_DrawCircleSolid(cx,cy,0.31f,R*0.82f,  C2D_Color32(0x22,0x1a,0x30,0xFF));
    C2D_DrawCircleSolid(cx,cy,0.32f,R*0.74f,  C2D_Color32(0x0c,0x09,0x12,0xFF));
    C2D_DrawCircleSolid(cx,cy,0.33f,R*0.60f,  C2D_Color32(0x22,0x1a,0x30,0xFF));
    C2D_DrawCircleSolid(cx,cy,0.34f,R*0.52f,  C2D_Color32(0x0c,0x09,0x12,0xFF));
    C2D_DrawCircleSolid(cx,cy,0.35f,R*0.40f,  C_CYAN);
    C2D_DrawCircleSolid(cx,cy,0.36f,R*0.34f,  C_PINK);
    C2D_DrawLine(cx,cy,C_CREAM, cx+cosf(ang)*R*0.34f, cy+sinf(ang)*R*0.34f, C_CREAM, 1.6f, 0.37f);
    C2D_DrawCircleSolid(cx,cy,0.38f,R*0.06f,  C_INK);
}

static bool startTrack(int idx){
    const Track* t=playlistGet(idx); if(!t) return false;
    if(!audioPlayFile(t->path)) return false;
    lyricsLoad(t->path);
    const Album* al=albumGet(t->album);
    if(al && al->coverPath[0]) coverLoad(al->coverPath); else coverClear();
    return true;
}
static int autoNext(int cur,int found,bool sh,RepeatMode rp){
    if(found<=0) return -1;
    if(rp==REPEAT_ONE) return cur;
    if(sh){ if(found==1) return rp==REPEAT_ALL?cur:-1; int n; do{n=rand()%found;}while(n==cur); return n; }
    if(cur+1<found) return cur+1;
    return rp==REPEAT_ALL?0:-1;
}
static int manualNext(int cur,int found,bool sh){
    if(found<=0) return -1;
    if(sh&&found>1){int n;do{n=rand()%found;}while(n==cur);return n;}
    return cur+1<found?cur+1:0;
}
static int manualPrev(int cur,int found,bool sh){
    if(found<=0) return -1;
    if(sh&&found>1){int n;do{n=rand()%found;}while(n==cur);return n;}
    return cur>0?cur-1:found-1;
}

// ===================== TOP =====================
static void renderTop(int playing,AudioState st,uint64_t cur,uint64_t total,uint32_t rate){
    C2D_DrawRectangle(0,0,0.0f,TOP_W,SCR_H/2,C_SKY_TOP,C_SKY_TOP,C_SKY_MID,C_SKY_MID);
    C2D_DrawRectangle(0,SCR_H/2,0.0f,TOP_W,SCR_H/2,C_SKY_MID,C_SKY_MID,C_SKY_BOT,C_SKY_BOT);
    // banda-etiqueta tipo cassette para el encabezado (el titulo no peleaba con el degradado+scanlines)
    C2D_DrawRectSolid(0,0,0.15f,TOP_W,23,C_HDR);
    C2D_DrawRectSolid(0,23,0.16f,TOP_W,2,C_PINK);
    txtSh(12,6,0.8f,0.6f,C_CYAN,"P A N D A F L O W");
    txtRight(TOP_W-10,9,0.8f,0.42f,C_CREAM,"// mixtape");

    float cvX=16,cvY=42,cvS=110;
    C2D_DrawRectSolid(cvX-3,cvY-3,0.2f,cvS+6,cvS+6,C_PINK);
    if(coverHas()) coverDraw(cvX,cvY,0.25f,cvS);
    else drawVinyl(cvX+cvS/2, cvY+cvS/2, cvS/2-2, g_reelAng);   // vinilo girando

    float ix=140; char tmp[256];
    // inserto tipo J-card detras de la info: el texto neon ahora descansa sobre fondo solido, no sobre el degradado
    C2D_DrawRectSolid(ix-6,40,0.18f,TOP_W-6-(ix-6),152,C_PANEL);
    C2D_DrawRectSolid(ix-6,40,0.19f,2,152,C_CYAN);
    const Track* t=(playing>=0)?playlistGet(playing):NULL;
    if(t){
        clip(tmp,sizeof tmp,t->title[0]?t->title:t->file,26); txt(ix,44,0.3f,0.56f,C_CREAM,tmp);
        if(t->artist[0]){clip(tmp,sizeof tmp,t->artist,30); txt(ix,68,0.3f,0.46f,C_PINK,tmp);}
        const Album* al=albumGet(t->album);
        if(al){clip(tmp,sizeof tmp,al->name,32); txt(ix,86,0.3f,0.42f,C_DIM,tmp);}
    } else txt(ix,60,0.3f,0.5f,C_DIM,"-- choose a tape --");

    char tc[16],tt[16]; fmtTime(tc,sizeof tc,cur,rate); fmtTime(tt,sizeof tt,total,rate);
    float barX=ix+18,barY=150,barW=TOP_W-10-barX-18,barH=7;
    txt(ix,132,0.3f,0.42f,C_CREAM,tc); txtRight(TOP_W-10,132,0.3f,0.42f,C_CREAM,tt);
    C2D_DrawRectSolid(barX,barY,0.3f,barW,barH,C_INK2);
    if(total>0){ float f=(float)cur/(float)total; if(f>1)f=1; C2D_DrawRectSolid(barX,barY,0.31f,barW*f,barH,C_CYAN); }
    reel(ix+8,barY+barH/2,8,g_reelAng); reel(TOP_W-18,barY+barH/2,8,g_reelAng);

    if(st==AUDIO_PLAYING){ icoPlay(ix+5,178,5,C_YELLOW); txt(ix+16,172,0.3f,0.46f,C_YELLOW,"PLAY"); }
    else if(st==AUDIO_PAUSED){ icoPause(ix+5,178,5,C_YELLOW); txt(ix+16,172,0.3f,0.46f,C_YELLOW,"PAUSE"); }
    else { icoStop(ix+5,178,4,C_DIM); txt(ix+16,172,0.3f,0.46f,C_DIM,"STOP"); }
    if(audioKeepingAwake()) txt(ix+90,174,0.3f,0.4f,C_CYAN,"lid: still playing");
    if(lyricsAvailable()) txtSh(16,162,0.3f,0.4f,C_PINK,"SEL: lyrics");
    C2D_DrawRectSolid(10,198,0.28f,182,34,C_PANEL);
    txt(16,200,0.3f,0.4f,C_CREAM,"A play   Y pause  X stop");
    txt(16,216,0.3f,0.4f,C_CREAM,"B back   L/R volume");

    // volume indicator (bottom-right corner)
    char vbuf[16]; int vol=audioGetVolume();
    snprintf(vbuf,sizeof vbuf,"VOL %d%%",vol);
    txtRight(TOP_W-10,196,0.3f,0.4f,C_CYAN,vbuf);
    float vbW=86,vbX=TOP_W-10-vbW,vbY=214,vbH=6;
    C2D_DrawRectSolid(vbX,vbY,0.3f,vbW,vbH,C_INK2);
    float vf=(float)vol/(float)AUDIO_VOL_MAX; if(vf>1)vf=1;
    u32 vc=(vol>100)?C_ORANGE:C_CYAN;   // naranjo cuando hay amplificacion
    C2D_DrawRectSolid(vbX,vbY,0.31f,vbW*vf,vbH,vc);
    if(vol>100){ float tx=vbX+vbW*(100.0f/AUDIO_VOL_MAX);
        C2D_DrawRectSolid(tx,vbY-1,0.32f,1.0f,vbH+2,C_CREAM); }  // marca el 100%
    scanlines(TOP_W);
}

// ===================== BARRA DE TRANSPORTE (táctil) =====================
static void renderTransport(AudioState st,bool sh,RepeatMode rp){
    C2D_DrawRectSolid(0,BAR_Y,0.4f,BOT_W,BAR_H,C_HDR);
    C2D_DrawRectSolid(0,BAR_Y,0.41f,BOT_W,2,C_PINK);
    for(int i=0;i<NBTN;i++){
        float x=i*BTN_W, cx=x+BTN_W/2.0f, cy=BAR_Y+BAR_H/2.0f+1;
        u32 bg=C_INK2;
        if(i==4&&sh) bg=C_PINK;
        if(i==5&&rp!=REPEAT_OFF) bg=C_CYAN;
        C2D_DrawRectSolid(x+2,BAR_Y+5,0.42f,BTN_W-4,BAR_H-9,bg);
        switch(i){
            case 0: icoPrev(cx,cy,7,C_CREAM); break;
            case 1: if(st==AUDIO_PLAYING) icoPause(cx,cy,7,C_YELLOW); else icoPlay(cx,cy,7,C_YELLOW); break;
            case 2: icoNext(cx,cy,7,C_CREAM); break;
            case 3: icoStop(cx,cy,6,C_CREAM); break;
            case 4: txtCtr(cx,cy-6,0.45f,0.46f,sh?C_INK:C_DIM,"SHUF"); break;
            default: txtCtr(cx,cy-6,0.45f,0.46f,(rp==REPEAT_OFF)?C_DIM:C_INK,
                            rp==REPEAT_ONE?"RPT1":rp==REPEAT_ALL?"RPT*":"RPT"); break;
        }
    }
}

// ===================== LISTA: ÁLBUMES =====================
static void renderAlbums(int sel,int scroll,int playing){
    C2D_DrawRectSolid(0,0,0.1f,BOT_W,20,C_HDR);
    C2D_DrawRectSolid(0,20,0.1f,BOT_W,2,C_PINK);
    char h[64]; snprintf(h,sizeof h,"ALBUMS (%d)",albumCount());
    txt(8,4,0.2f,0.5f,C_CYAN,h);

    int playAlbum = (playing>=0 && playlistGet(playing)) ? playlistGet(playing)->album : -1;
    int n=albumCount(); char tmp[256],line[300];
    if(n<=0){
        txt(10,70,0.2f,0.5f,C_YELLOW,"No music found.");
        txt(10,92,0.2f,0.42f,C_CREAM,"Put album folders in");
        txt(10,108,0.2f,0.42f,C_CREAM,"sdmc:/music (mp3/flac/wav/m4a)");
        return;
    }
    int end=scroll+VIS_ROWS; if(end>n)end=n;
    for(int i=scroll;i<end;i++){
        float y=LIST_TOP+(i-scroll)*LINE_H;
        const Album* al=albumGet(i);
        if(i==sel){ C2D_DrawRectSolid(0,y,0.13f,BOT_W,LINE_H,C2D_Color32(0xFF,0x4D,0x9D,0x33));
                    C2D_DrawRectSolid(0,y,0.14f,3,LINE_H,C_PINK); }
        // iconito de carpeta dibujado
        C2D_DrawRectSolid(8,y+4,0.2f,12,8,C_ORANGE);
        C2D_DrawRectSolid(8,y+3,0.2f,6,3,C_ORANGE);
        clip(tmp,sizeof tmp,al->name,36);
        snprintf(line,sizeof line,"%s",tmp);
        u32 c=(i==playAlbum)?C_CYAN:(i==sel?C_CREAM:C_DIM);
        if(i==playAlbum) icoPlay(28,y+LINE_H/2.0f,4,C_CYAN);
        txt(34,y+1,0.2f,0.44f,c,line);
        char cnt[16]; snprintf(cnt,sizeof cnt,"%d",al->count);
        txtRight(BOT_W-8,y+1,0.2f,0.4f,C_DIM2,cnt);
    }
}

// ===================== LISTA: TRACKS DE UN ÁLBUM =====================
static void renderTracks(int alb,int sel,int scroll,int playing){
    const Album* al=albumGet(alb);
    C2D_DrawRectSolid(0,0,0.1f,BOT_W,20,C_HDR);
    C2D_DrawRectSolid(0,20,0.1f,BOT_W,2,C_CYAN);
    icoPrev(12,10,5,C_PINK);                            // flecha atras (tap)
    char tmp[256]; clip(tmp,sizeof tmp,al?al->name:"",40); txt(24,4,0.2f,0.45f,C_CYAN,tmp);
    if(!al) return;

    int first=al->firstTrack, n=al->count;
    char line[300];
    int end=scroll+VIS_ROWS; if(end>n)end=n;
    for(int r=scroll;r<end;r++){
        int ti=first+r; float y=LIST_TOP+(r-scroll)*LINE_H;
        bool s=(ti==sel), pl=(ti==playing);
        if(s){ C2D_DrawRectSolid(0,y,0.13f,BOT_W,LINE_H,C2D_Color32(0xFF,0x4D,0x9D,0x33));
               C2D_DrawRectSolid(0,y,0.14f,3,LINE_H,C_PINK); }
        const Track* t=playlistGet(ti);
        clip(tmp,sizeof tmp,t->title[0]?t->title:t->file,42);
        snprintf(line,sizeof line,"%02d  %s",r+1,tmp);
        u32 c=pl?C_CYAN:(s?C_CREAM:C_DIM);
        txt(12,y+1,0.2f,0.42f,c,line);
        if(pl) icoPlay(6,y+LINE_H/2.0f,4,C_CYAN);
    }
}

// ===================== LETRAS =====================
static void renderLyrics(uint32_t curMs,int manual){
    C2D_DrawRectSolid(0,0,0.1f,BOT_W,20,C_HDR);
    C2D_DrawRectSolid(0,20,0.1f,BOT_W,2,C_CYAN);
    txt(8,4,0.2f,0.5f,C_PINK,"LYRICS");
    txtRight(BOT_W-8,5,0.2f,0.4f,C_CREAM,lyricsSynced()?"synced":"text");
    const int LH=16, top=26, vis=(BAR_Y-top)/LH;
    if(!lyricsAvailable()){
        txt(10,64,0.2f,0.46f,C_YELLOW,"No lyrics for this song.");
        txt(10,88,0.2f,0.42f,C_CREAM,"Place a file next to the audio");
        txt(10,104,0.2f,0.42f,C_CREAM,"<same name>.lrc  or  .txt");
        return;
    }
    int n=lyricsCount(), active=lyricsSynced()?lyricsActiveIndex(curMs):-1;
    int first=lyricsSynced()?((active<0?0:active)-vis/2):manual;
    if(first>n-vis)first=n-vis; if(first<0)first=0;
    char tmp[200];
    for(int i=first;i<first+vis&&i<n;i++){
        float y=top+(i-first)*LH;
        u32 c=(i==active)?C_CYAN:(lyricsSynced()?C_DIM2:C_CREAM);
        clip(tmp,sizeof tmp,lyricsLine(i),44);
        txt(i==active?6:10,y,0.2f,i==active?0.5f:0.44f,c,tmp);
    }
}

int main(int argc,char** argv){
    romfsInit();
    osSetSpeedupEnable(true);   // New 3DS: 804 MHz + L2 (clave para decodificar FLAC sin cortes)
    gfxInitDefault();
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    C2D_Init(C2D_DEFAULT_MAX_OBJECTS);
    C2D_Prepare();
    srand((unsigned)svcGetSystemTick());

    C3D_RenderTarget* top=C2D_CreateScreenTarget(GFX_TOP,GFX_LEFT);
    C3D_RenderTarget* bot=C2D_CreateScreenTarget(GFX_BOTTOM,GFX_LEFT);
    g_text=C2D_TextBufNew(8192);

    irrstInit();   // Optional C-stick volume control on New 3DS

    bool audioOk=audioInit();
    int found=playlistScan(MUSIC_DIR);
    int volHold=0;   // anti-rebote/repeticion del C-stick para el volumen

    int playing=-1;
    int browseAlbum=-1;            // -1 = vista de albumes; >=0 = tracks de ese album
    int albSel=0, albScroll=0;
    int trkSel=0, trkScroll=0;
    bool showLyrics=false; int lyScroll=0;
    bool shuffle=false; RepeatMode repeat=REPEAT_OFF;

    while(aptMainLoop()){
        hidScanInput();
        u32 kDown=hidKeysDown();
        if(kDown & KEY_START) break;

        if(kDown & KEY_SELECT){ showLyrics=!showLyrics; lyScroll=0; }

        // ---------- toques ----------
        if(kDown & KEY_TOUCH){
            touchPosition tp; hidTouchRead(&tp);
            if(tp.py>=BAR_Y){                                   // barra de transporte
                int b=tp.px/BTN_W; if(b>=NBTN)b=NBTN-1;
                switch(b){
                    case 0: if(playing>=0){int p=manualPrev(playing,found,shuffle); if(p>=0&&startTrack(p))playing=p;} break;
                    case 1:
                        if(audioGetState()==AUDIO_STOPPED){
                            int idx = (browseAlbum>=0)?trkSel:(albumGet(albSel)?albumGet(albSel)->firstTrack:0);
                            if(audioOk&&startTrack(idx)) playing=idx;
                        } else audioTogglePause();
                        break;
                    case 2: if(playing>=0){int nx=manualNext(playing,found,shuffle); if(nx>=0&&startTrack(nx))playing=nx;} break;
                    case 3: audioStop(); playing=-1; break;
                    case 4: shuffle=!shuffle; break;
                    case 5: repeat=(RepeatMode)((repeat+1)%3); break;
                }
            } else if(!showLyrics && tp.py<20 && browseAlbum>=0 && tp.px<60){
                browseAlbum=-1;                                  // tap flecha atras
            } else if(!showLyrics && tp.py>=LIST_TOP && tp.py<BAR_Y){
                int row=(tp.py-LIST_TOP)/LINE_H;
                if(browseAlbum<0){                               // tocar album -> abrir
                    int a=albScroll+row;
                    if(a>=0&&a<albumCount()){ browseAlbum=a; trkSel=albumGet(a)->firstTrack; trkScroll=0; }
                } else {                                         // tocar track -> reproducir
                    const Album* al=albumGet(browseAlbum);
                    int ti=al->firstTrack+trkScroll+row;
                    if(ti>=al->firstTrack && ti<al->firstTrack+al->count && audioOk && startTrack(ti)) { playing=ti; trkSel=ti; }
                }
            }
        }

        // ---------- cruceta / botones ----------
        if(showLyrics){
            if(kDown&KEY_DOWN)lyScroll++; if(kDown&KEY_UP)lyScroll--;
            if(kDown&KEY_RIGHT)lyScroll+=6; if(kDown&KEY_LEFT)lyScroll-=6;
            if(lyScroll<0)lyScroll=0;
        } else if(browseAlbum<0){                                // ALBUMES
            int n=albumCount();
            if(n>0){
                if(kDown&KEY_DOWN)albSel++; if(kDown&KEY_UP)albSel--;
                if(albSel<0)albSel=0; if(albSel>=n)albSel=n-1;
                if(albSel<albScroll)albScroll=albSel;
                if(albSel>=albScroll+VIS_ROWS)albScroll=albSel-VIS_ROWS+1;
                if((kDown&KEY_A)||(kDown&KEY_RIGHT)){ browseAlbum=albSel; trkSel=albumGet(albSel)->firstTrack; trkScroll=0; }
            }
        } else {                                                 // TRACKS
            const Album* al=albumGet(browseAlbum);
            int lo=al->firstTrack, hi=al->firstTrack+al->count-1;
            if(kDown&KEY_DOWN)trkSel++; if(kDown&KEY_UP)trkSel--;
            if(trkSel<lo)trkSel=lo; if(trkSel>hi)trkSel=hi;
            int rel=trkSel-lo;
            if(rel<trkScroll)trkScroll=rel;
            if(rel>=trkScroll+VIS_ROWS)trkScroll=rel-VIS_ROWS+1;
            if(kDown&KEY_A){ if(audioOk&&startTrack(trkSel))playing=trkSel; }
            if((kDown&KEY_B)||(kDown&KEY_LEFT)) browseAlbum=-1;
        }

        if(kDown&KEY_Y) audioTogglePause();
        if(kDown&KEY_X){ audioStop(); playing=-1; }

        if(kDown&KEY_R) audioSetVolume(audioGetVolume()+5);
        else if(kDown&KEY_L) audioSetVolume(audioGetVolume()-5);

        // Optional volume control with the New 3DS C-stick.
        irrstScanInput();
        circlePosition cs; hidCstickRead(&cs);
        if(cs.dy>40 || cs.dy<-40){
            if(volHold<=0){ audioSetVolume(audioGetVolume()+(cs.dy>0?+5:-5)); volHold=3; }
            else volHold--;
        } else volHold=0;
        if(audioConsumeTrackEnded()){
            int nx=autoNext(playing,found,shuffle,repeat);
            if(nx>=0&&startTrack(nx))playing=nx; else playing=-1;
        }

        AudioState st=audioGetState();
        if(st==AUDIO_PLAYING) g_reelAng+=0.10f;
        uint64_t cur=0,total=0; uint32_t rate=0;
        audioGetProgress(&cur,&total,&rate);
        uint32_t curMs=rate?(uint32_t)(cur*1000ULL/rate):0;

        C2D_TextBufClear(g_text);
        C3D_FrameBegin(C3D_FRAME_SYNCDRAW);
        C2D_TargetClear(top,C_SKY_TOP); C2D_SceneBegin(top);
        renderTop(playing,st,cur,total,rate);
        C2D_TargetClear(bot,C_INK); C2D_SceneBegin(bot);
        C2D_DrawRectangle(0,0,0.0f,BOT_W,SCR_H,C_INK,C_INK,C_INK2,C_INK2);
        if(showLyrics) renderLyrics(curMs,lyScroll);
        else if(browseAlbum<0) renderAlbums(albSel,albScroll,playing);
        else renderTracks(browseAlbum,trkSel,trkScroll,playing);
        renderTransport(st,shuffle,repeat);
        scanlines(BOT_W);
        C3D_FrameEnd(0);
    }

    audioExit(); coverClear(); irrstExit();
    C2D_TextBufDelete(g_text);
    C2D_Fini(); C3D_Fini(); gfxExit(); romfsExit();
    return 0;
}
