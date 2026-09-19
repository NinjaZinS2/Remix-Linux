#pragma once
// Casca Linux: estado de desenho (texturas), helpers graficos com a "cara"
// do GDI+ e os hooks Platform* que nao dependem da janela. Toda a logica esta
// em app_core.h (compartilhada com o Windows).
#include "app_core.h"
#include "gfx.h"
#if defined(__ANDROID__)
#include "sys_android.h"   // casca Android (docs/ANDROID.md): mesma API sys::, sem zenity/inotify/socket
#else
#include "sys_linux.h"
#endif
#include <ctime>

using gfx::RectF;
using gfx::Img;

static Img* g_coverImg = nullptr;
static Img* g_splashImg = nullptr;
static std::map<std::wstring, Img*> g_thumbCache;

static inline Color ToGdi(COLORREF c, BYTE a=255){ return gfx::Col(c, a); }
static inline Color Argb(int a,int r,int g,int b){ return gfx::ARGB(a,r,g,b); }
// Cor por estilo: no classico a cor original; nos estilos novos o token da paleta.
static inline Color Cs(Color classico,COLORREF novo,BYTE a=255){ return UiClassic()?classico:ToGdi(novo,a); }
static RectF RF(const RECT& r){ return gfx::FromRECT(r); }
static RectFC RC(const RectF& r){ return RectFC{r.X,r.Y,r.Width,r.Height}; }
// Texto: segue a paleta do estilo ativo (app_ui.h). No classico sao as cores originais.
#define C_WHITE (ToGdi(UI().text))
#define C_GRAY  (ToGdi(UI().textDim))
#define C_GRAY2 (ToGdi(UI().textFaint))

static void DrawRoundRect(const RectF& r, float radius, const Color* fill, const Color* pen, float penW=1.f){ gfx::RoundRect(r, radius, fill, pen, penW); }

static void DrawCameraIcon(const RECT& r, Color c){
    Color fillC = gfx::WithA(c, 30);
    RectF body((float)r.left+3,(float)r.top+7,(float)(r.right-r.left-6),(float)(r.bottom-r.top-10));
    gfx::StrokeRoundRect(body,4,1.6f,c);
    gfx::FillEllipse((float)(r.left+(r.right-r.left)/2-4),(float)(r.top+11),8,8,fillC);
    gfx::Line((float)r.left+7,(float)r.top+5,(float)r.left+12,(float)r.top+5,1.6f,c);
}
// Icone de volume (alto-falante + ondas; mudo = alto-falante + X). Segue a cor do tema.
static void DrawVolumeIcon(const RECT& r, Color c, bool muted, int vol){
    float x=(float)r.left,y=(float)r.top,w=(float)(r.right-r.left),h=(float)(r.bottom-r.top);
    float cy=y+h/2;
    gfx::FillRect(x,cy-h*0.16f,w*0.22f,h*0.32f,c);
    Vector2 cone[4]={{x+w*0.20f,cy-h*0.16f},{x+w*0.48f,cy-h*0.42f},{x+w*0.48f,cy+h*0.42f},{x+w*0.20f,cy+h*0.16f}};
    gfx::FillPolygon(cone,4,c);
    if(muted||vol==0){
        gfx::Line(x+w*0.60f,cy-h*0.25f,x+w*0.95f,cy+h*0.25f,2.f,c);
        gfx::Line(x+w*0.60f,cy+h*0.25f,x+w*0.95f,cy-h*0.25f,2.f,c);
    } else {
        gfx::Arc(x+w*0.30f,cy-h*0.30f,w*0.60f,h*0.60f,-40,80,1.8f,c);
        gfx::Arc(x+w*0.30f,cy-h*0.50f,w*1.00f,h*1.00f,-40,80,1.8f,gfx::WithA(c,vol>45?255:80));
    }
}

static void IconPlay(const RectF& r, Color b){
    float x=r.X,y=r.Y,w=r.Width,h=r.Height;
    Vector2 pts[3]={{x+w*.08f,y+h*.02f},{x+w*.94f,y+h*.5f},{x+w*.08f,y+h*.98f}};
    gfx::FillPolygon(pts,3,b);
}
static void IconPause(const RectF& r, Color b){
    float bw=r.Width*.26f;
    gfx::FillRect(r.X+r.Width*.10f,r.Y,bw,r.Height,b);
    gfx::FillRect(r.X+r.Width*.64f,r.Y,bw,r.Height,b);
}
static void IconSkip(const RectF& r, Color b, bool fwd){
    float x=r.X,y=r.Y,w=r.Width,h=r.Height,bar=w*.15f;
    if(fwd){
        gfx::FillRect(x+w*.85f,y,bar,h,b);
        Vector2 pts[3]={{x,y+h*.03f},{x+w*.76f,y+h*.5f},{x,y+h*.97f}};
        gfx::FillPolygon(pts,3,b);
    }else{
        gfx::FillRect(x,y,bar,h,b);
        Vector2 pts[3]={{x+w,y+h*.03f},{x+w*.24f,y+h*.5f},{x+w,y+h*.97f}};
        gfx::FillPolygon(pts,3,b);
    }
}
static void DrawRunnerRect(const RectF& r,float rad,const COLORREF& col,BYTE maxA,float head){
    if(maxA<6||r.Width<24||r.Height<24||!FxOn()) return;
    const int N=26; float span=.16f;
    RectFC rc=RC(r);
    for(int s=N;s>=1;s--){
        float u=head-span*(float)s/N, u2=head-span*(float)(s-1)/N;
        u-=floorf(u); u2-=floorf(u2);
        float x1,y1,x2,y2; PerimeterPoint(rc,rad,u,x1,y1); PerimeterPoint(rc,rad,u2,x2,y2);
        float tt=1.f-(float)s/N;
        BYTE a=(BYTE)(maxA*powf(tt,1.7f));
        gfx::Line(x1,y1,x2,y2,2.2f+tt*1.8f,ToGdi(col,a));
    }
    float hx,hy; PerimeterPoint(rc,rad,head,hx,hy);
    gfx::FillEllipse(hx-2.5f,hy-2.5f,5.f,5.f,ToGdi(col,maxA));
}
static void DrawRunnerCircle(const RectF& r,const COLORREF& col,BYTE maxA,float head){
    if(maxA<6||r.Width<30||!FxOn()) return;
    const int N=26; float spanDeg=95.f;
    for(int s=N;s>=1;s--){
        float a1=head*360.f-spanDeg*(float)s/N;
        float tt=1.f-(float)s/N;
        BYTE a=(BYTE)(maxA*powf(tt,1.7f));
        gfx::Arc(r.X,r.Y,r.Width,r.Height,a1,-spanDeg/(float)N-1.5f,2.4f+tt*2.0f,ToGdi(col,a));
    }
}
static void DrawParticles(const RectF& area,const COLORREF& col,float t,float amul,const RectF* clipEllipse=nullptr){
    if(!FxOn()) return;
    const int NP=34;
    float spd=g_cfg.particlesSpeed/100.0f;
    for(int k=0;k<NP;k++){
        float sp=(10.f+(float)(k%13))*spd;
        float cyc=area.Height+30.f;
        float yy=area.Y+area.Height-fmodf(t*sp+(float)k*37.7f,cyc);
        float wob=sinf(t*.9f+(float)k*2.1f)*(6.f+(float)(k%5)*2.f);
        float xx=area.X+8.f+fmodf((float)k*53.3f,std::max(8.f,area.Width-16.f))+wob;
        float fadeTop=std::min(1.f,(yy-area.Y)/40.f);
        float fadeBot=std::min(1.f,(area.Y+area.Height-yy)/30.f);
        float al=std::min(.95f,.78f*amul)*std::max(0.f,std::min(1.f,fadeTop*fadeBot));
        if(al<=.02f) continue;
        int bi=(int)(al*5.999f); if(bi<0)bi=0; if(bi>5)bi=5;
        float rad=1.8f+(float)(k%4)*.7f;
        if(clipEllipse){
            float ex=(xx-(clipEllipse->X+clipEllipse->Width/2))/(clipEllipse->Width/2);
            float ey=(yy-(clipEllipse->Y+clipEllipse->Height/2))/(clipEllipse->Height/2);
            if(ex*ex+ey*ey>1.f) continue;
        } else {
            if(xx<area.X-rad||xx>area.X+area.Width+rad||yy<area.Y-rad||yy>area.Y+area.Height+rad) continue;
        }
        gfx::FillEllipse(xx-rad,yy-rad,rad*2.f,rad*2.f,ToGdi(col,(BYTE)(bi*51)));
    }
}
static void DrawMarqueeText(const std::wstring& s,float px,bool bold,const RectF& rc,Color brk,bool centerWhenFit,float speedPx,DWORD nowMs){
    float mw=gfx::TextWidth(s,px,bold);
    gfx::PushClip(RectF(rc.X,rc.Y-2,rc.Width,rc.Height+4));
    if(mw<=rc.Width+1.f){
        float x=centerWhenFit?rc.X+(rc.Width-mw)/2.f:rc.X;
        gfx::Text(s,x,rc.Y,px,brk,bold);
    } else {
        float total=mw+S(60);
        float off=fmodf((nowMs/1000.0f)*speedPx,total);
        gfx::Text(s,rc.X-off,rc.Y,px,brk,bold);
        gfx::Text(s,rc.X-off+total,rc.Y,px,brk,bold);
    }
    gfx::PopClip();
}
static Img* GetThumb(const std::wstring& path){
    if(path.empty()) return nullptr;
    auto it=g_thumbCache.find(path); if(it!=g_thumbCache.end()) return it->second;
    Img* img=gfx::LoadImg(path); if(!img->ok){ gfx::FreeImg(img); img=nullptr; }
    g_thumbCache[path]=img; return img;
}
static Img* g_wallImg=nullptr;
static std::wstring g_wallLoadedPath;
static Img* GetWallpaperImage(){
    if(g_cfg.bgWallpaper.empty()){ if(g_wallImg){gfx::FreeImg(g_wallImg);g_wallImg=nullptr;g_wallLoadedPath.clear();} return nullptr; }
    if(!g_wallImg||g_wallLoadedPath!=g_cfg.bgWallpaper){
        if(g_wallImg){gfx::FreeImg(g_wallImg);g_wallImg=nullptr;}
        g_wallImg=gfx::LoadImg(g_cfg.bgWallpaper);
        if(!g_wallImg->ok){gfx::FreeImg(g_wallImg);g_wallImg=nullptr;}
        g_wallLoadedPath=g_cfg.bgWallpaper;
    }
    return g_wallImg;
}
static void DrawAppBackground(int w,int h,const Color& fallback){
    Img* wp=GetWallpaperImage();
    bool useBlur=(!wp&&g_cfg.coverBlurBg&&g_coverImg&&g_coverImg->ok);
    gfx::FillRect(0,0,(float)w,(float)h,fallback);
    if(!wp&&!useBlur) return;
    if(wp){
        float iw=(float)wp->w,ih=(float)wp->h;
        float sc=std::max((float)w/std::max(1.f,iw),(float)h/std::max(1.f,ih));
        float dw=iw*sc,dh=ih*sc;
        gfx::DrawImg(wp,RectF(((float)w-dw)/2.f,((float)h-dh)/2.f,dw,dh));
        gfx::FillRect(0,0,(float)w,(float)h,Argb(170,3,4,12));
    } else {
        Texture2D& t=gfx::TinyTex(g_coverImg);
        if(t.id) DrawTexturePro(t,Rectangle{0,0,(float)t.width,(float)t.height},Rectangle{-8,-8,(float)w+16,(float)h+16},Vector2{0,0},0,WHITE);
        gfx::FillRect(0,0,(float)w,(float)h,Argb(150,3,4,12));
    }
}

// ------------------------------------------------ hooks (sem janela) -------
void AppPost(int type,const std::wstring& s,int n){ sys::Post(type,s,n); }
void PlatformPickFolderAsync(){ sys::PickAsync(sys::DLG_FOLDER,L"Escolha a pasta com suas musicas",EV_PICK_FOLDER); }
void PlatformPickImageAsync(int evType,int ctx){ sys::PickAsync(sys::DLG_IMAGE,L"Escolha a imagem",evType,ctx); }
void PlatformPickFolderFor(int evType,int ctx){ sys::PickAsync(sys::DLG_FOLDER,L"Escolha a pasta",evType,ctx); }
void PlatformPickAudioFilesAsync(int evType,int ctx){ sys::PickAsync(sys::DLG_AUDIO_MULTI,L"Escolha as músicas",evType,ctx); }
void PlatformPickProgramAsync(int evType){ sys::PickAsync(sys::DLG_ANYFILE,L"Escolha o programa de linha de comando",evType,0); }
std::wstring PlatformClipboardText(){ const char* c=GetClipboardText(); return c?Utf8ToWide(c):L""; }
bool PlatformSetClipboardText(const std::wstring& t){ SetClipboardText(WideToUtf8(t).c_str()); return true; }
bool PlatformHttpGet(const std::string& url,std::string& body){ long st=0; body=sys::HttpGet(url,nullptr,&st); return !body.empty()&&(st==0||(st>=200&&st<400)); }
bool PlatformHttpGetNoRedirect(const std::string& url,std::string& body){ long st=0; body=sys::HttpGet(url,nullptr,&st,false); if(st<200||st>=300) body.clear(); return !body.empty(); }
void PlatformRedraw(){}
void PlatformLoadCover(const std::wstring& path){
    if(g_coverImg){ gfx::FreeImg(g_coverImg); g_coverImg=nullptr; }
    if(!path.empty()){ g_coverImg=gfx::LoadImg(path); if(!g_coverImg->ok){ gfx::FreeImg(g_coverImg); g_coverImg=nullptr; } }
}
void PlatformEvictThumb(const std::wstring& path){
    auto it=g_thumbCache.find(path); if(it==g_thumbCache.end()) return;
    if(it->second) gfx::FreeImg(it->second);
    g_thumbCache.erase(it);
}
void PlatformClearThumbs(){ for(auto&kv:g_thumbCache) if(kv.second) gfx::FreeImg(kv.second); g_thumbCache.clear(); }
bool PlatformHaveFfmpeg(){ return sys::HaveFfmpeg(); }
std::wstring PlatformTranscodeToWav(const std::wstring& src){ return sys::TranscodeToWav(src); }
void PlatformOpenFolder(const std::wstring& path){
    std::string cmd="(xdg-open "+sys::ShQuote(WideToUtf8(path))+" >/dev/null 2>&1 &)";
    int rc=system(cmd.c_str()); (void)rc;
}
// Lixeira: gio trash (GNOME/KDE/... via GLib) ou, sem ele, a especificacao XDG Trash a mao.
bool PlatformTrash(const std::wstring& path){
    std::string u8=WideToUtf8(path);
    if(sys::HaveCmd("gio")){
        int rc=system(("gio trash "+sys::ShQuote(u8)+" >/dev/null 2>&1").c_str());
        if(rc==0) return true;
    }
    namespace fs=std::filesystem;
    const char* xd=std::getenv("XDG_DATA_HOME"); const char* home=std::getenv("HOME");
    std::string base=(xd&&*xd)?std::string(xd):(std::string(home?home:"/tmp")+"/.local/share");
    fs::path trash=fs::path(base)/"Trash";
    std::error_code ec;
    fs::create_directories(trash/"files",ec); fs::create_directories(trash/"info",ec);
    fs::path src(u8); std::string name=src.filename().string();
    fs::path dst=trash/"files"/name;
    int k=1; while(fs::exists(dst,ec)){ dst=trash/"files"/(src.stem().string()+"."+std::to_string(k++)+src.extension().string()); }
    fs::rename(src,dst,ec);
    if(ec){ ec.clear(); fs::copy_file(src,dst,ec); if(ec) return false; fs::remove(src,ec); }
    time_t now=time(nullptr); char stamp[64]; strftime(stamp,sizeof stamp,"%Y-%m-%dT%H:%M:%S",localtime(&now));
    std::string info="[Trash Info]\nPath="+u8+"\nDeletionDate="+stamp+"\n";
    sys::WriteFileBytes((trash/"info"/(dst.filename().string()+".trashinfo")).wstring(),info);
    return true;
}
static sys::FolderWatch g_watch;
void PlatformWatchStart(const std::wstring& dir){
    g_watch.Stop();
    std::wstring abs=Config::FromPortable(dir);
    if(abs.empty()||!Config::DirExists(abs)) return;
    g_watch.Start(abs);
}
void PlatformWatchStop(){ g_watch.Stop(); }
bool PlatformWatchTake(unsigned long long quietMs){
    if(g_watch.pending.load()<=0) return false;
    if(GetTickCount64()-g_watch.lastChangeMs.load()<quietMs) return false;
    return g_watch.pending.exchange(0)>0;
}
// Log de diagnostico (erros tratados nas threads): no Linux vai para o stderr com REMIX_DEBUG=1.
void PlatformLog(const char* msg){ if(std::getenv("REMIX_DEBUG")) fprintf(stderr,"[remix] %s\n",msg); }
