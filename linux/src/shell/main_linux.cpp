// Remix Player — casca Linux (raylib + miniaudio).
// A logica esta em app_core.h/app_input.h (compartilhada com o Windows); aqui
// ficam a janela, o loop, os hooks que precisam da janela e a CLI.
#include "app_draw2.h"
#include "app_input.h"
#include "mpris_linux.h"
#include "hotkeys_x11.h"
#define GLFW_INCLUDE_NONE
#include "glfw/include/GLFW/glfw3.h"
#include <clocale>
#include <cstdio>
#include <cstring>

// ------------------------------------------------ capas (impl. Linux) -----
static bool ImageHasTransparency(Image& img){
    if(img.format!=PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 && img.format!=PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA &&
       img.format!=PIXELFORMAT_UNCOMPRESSED_R5G5B5A1 && img.format!=PIXELFORMAT_UNCOMPRESSED_R4G4B4A4) return false;
    ImageFormat(&img,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    Color* px=(Color*)img.data; size_t n=(size_t)img.width*img.height;
    for(size_t i=0;i<n;i++) if(px[i].a<255) return true;
    return false;
}
static bool SaveResizedLinux(Image img,const std::wstring& dest,bool png){
    int w=img.width,h=img.height; if(w<=0||h<=0) return false;
    double sc=(double)COVER_MAX_DIM/(double)(w>h?w:h);
    int nw=std::max(1,(int)std::floor(w*sc)), nh=std::max(1,(int)std::floor(h*sc));
    Image copy=ImageCopy(img);
    ImageFormat(&copy,PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    ImageResize(&copy,nw,nh);
    if(!png) ImageFormat(&copy,PIXELFORMAT_UNCOMPRESSED_R8G8B8);
    bool ok=ExportImage(copy,WideToUtf8(dest).c_str());
    UnloadImage(copy);
    return ok;
}
std::wstring ShrinkCoverInto(const std::wstring& dir,const wchar_t* name,const std::wstring& imgPath){
    Image img=LoadImage(WideToUtf8(imgPath).c_str());
    if(!img.data) return L"";
    int w=img.width,h=img.height;
    if(w<=0||h<=0||(w<=COVER_MAX_DIM&&h<=COVER_MAX_DIM)){ UnloadImage(img); return L""; }
    bool alpha=ImageHasTransparency(img);
    std::wstring dest=Config::Join(dir,std::wstring(name)+(alpha?L".png":L".jpg"));
    bool ok=SaveResizedLinux(img,dest,alpha);
    UnloadImage(img);
    return ok?dest:L"";
}
int MigrateOversizedCovers(){
    namespace fs=std::filesystem;
    std::wstring dir=Config::CoversDir();
    std::error_code ec;
    if(!fs::exists(fs::path(dir),ec)||ec) return 0;
    fs::path bakDir=fs::path(dir)/"_originais";
    std::vector<fs::path> files;
    for(fs::directory_iterator it(fs::path(dir),fs::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec) break;
        if(!it->is_regular_file()) continue;
        auto ext=it->path().extension().wstring(); for(auto&c:ext) c=towlower(c);
        if(ext==L".png"||ext==L".jpg"||ext==L".jpeg") files.push_back(it->path());
    }
    int changed=0;
    for(auto& p:files){
        Image img=LoadImage(p.string().c_str());
        if(!img.data) continue;
        if(img.width>COVER_MAX_DIM||img.height>COVER_MAX_DIM){
            auto lext=p.extension().wstring(); for(auto&c:lext) c=towlower(c);
            fs::create_directories(bakDir,ec);
            fs::path orig=bakDir/p.filename();
            ec.clear(); fs::rename(p,orig,ec);
            if(!ec){
                if(SaveResizedLinux(img,p.wstring(),lext==L".png")) ++changed;
                else { ec.clear(); fs::rename(orig,p,ec); }
            }
        }
        UnloadImage(img);
    }
    return changed;
}

// ------------------------------------------------ hooks com janela ---------
static bool g_quit=false;
static std::wstring g_shotPath; static bool g_shotPending=false;
static void ApplyMinSize(){ if(g_cfg.displayMode==L"vertical") SetWindowMinSize(340,560); else SetWindowMinSize(720,540); }
static void WorkArea(int& aw,int& ah){
    int m=GetCurrentMonitor(); int mw=GetMonitorWidth(m), mh=GetMonitorHeight(m);
    if(mw<=0||mh<=0){ aw=1500; ah=812; return; }
    aw=mw-40; ah=mh-100;
}
static void ClampToWork(int& w,int& h){
    int aw,ah; WorkArea(aw,ah);
    if(w>aw)w=std::max(MIN_WIN_W,aw);
    if(h>ah)h=std::max(MIN_WIN_H,ah);
}
static void NormalSize(int& w,int& h){
    int aw,ah; WorkArea(aw,ah);
    if(g_cfg.winW>=720&&g_cfg.winH>=540){ w=g_cfg.winW; h=g_cfg.winH; }
    else { w=std::min(1500,(int)(aw*0.92f)); h=std::min(812,(int)(ah*0.90f)); if(w<720)w=720; if(h<540)h=540; }
    ClampToWork(w,h);
}
void PlatformResizeForMode(){
    int w,h;
    if(g_cfg.displayMode==L"vertical"){ w=460; h=700; ClampToWork(w,h); }
    else NormalSize(w,h);
    g_listScroll=0; g_setScroll=0;
    SetWindowSize(w,h);
    ApplyMinSize();
    g_winW=w; g_winH=h;
    BuildLayout();
}
void PlatformMinimize(){ MinimizeWindow(); }
void PlatformClose(){ g_quit=true; }
void PlatformRestoreAndFocus(){ if(IsWindowState(FLAG_WINDOW_HIDDEN)) ClearWindowState(FLAG_WINDOW_HIDDEN); RestoreWindow(); SetWindowFocused(); BuildLayout(); }
// Segundo plano: a janela some, o loop continua (eventos, MPRIS, autoplay).
void PlatformHide(){
    SetWindowState(FLAG_WINDOW_HIDDEN);
    static bool told=false;
    if(!told){ told=true; std::thread([]{ int rc=system("notify-send -a 'Remix Player' -i remix 'Remix continua tocando em segundo plano' 'Abra o Remix de novo para mostrar a janela, ou use os controles de mídia do sistema.' >/dev/null 2>&1"); (void)rc; }).detach(); }
    if(std::getenv("REMIX_DEBUG")) fprintf(stderr,"[remix] janela escondida (segundo plano)\n");
}
// GLFW marca "fechar" quando o usuario clica no X; para esconder em vez de sair,
// limpamos a marca (funcoes do GLFW dentro da libraylib.a).
void PlatformSetWindowSize(int w,int h){ SetWindowSize(w,h); g_winW=w; g_winH=h; BuildLayout(); }
static void ResetCloseRequest(){ if(GLFWwindow* w=glfwGetCurrentContext()) glfwSetWindowShouldClose(w,0); }
bool PlatformScreenshot(const std::wstring& png){ g_shotPath=png; g_shotPending=true; return true; }

static void Render(){
    int w=g_winW,h=g_winH;
    if(g_showSplash){DrawSplash(w,h);return;}
    if(g_cfg.displayMode==L"vertical")DrawVertical(w,h);else DrawNormal(w,h);
    if(g_imgMenuOpen)DrawImgMenu(w,h);
    if(OU().open)DrawOnline(w,h);   // os menus (ex.: escolher playlist) ficam por cima
    if(host::PU().open)DrawHostPanel(w,h);
    if(g_fxp.open)DrawFxPanel(w,h);
    if(g_spadP.open)DrawSpadPanel(w,h);
    if(g_dcP.open)DrawDcPanel(w,h);
    if(g_folderMenuOpen)DrawFolderMenu(w,h);
    if(g_ctxOpen)DrawCtxMenu(w,h);
    if(g_editArtist)DrawArtistEditor(w,h);
    if(WP().open){LayoutWebPick(w,h);DrawWebPick(w,h);}
    if(g_confirmOpen)DrawConfirm(w,h);
    DrawActivity(w,h);
    DrawStatusToast(w,h);
}
// raylib KEY_* -> KeyCode (app_keys.h)
static int MapKC(int k){
    if(k>=KEY_A&&k<=KEY_Z) return 'A'+(k-KEY_A);
    if(k>=KEY_ZERO&&k<=KEY_NINE) return '0'+(k-KEY_ZERO);
    if(k>=KEY_F1&&k<=KEY_F12) return KC_F1+(k-KEY_F1);
    if(k>=KEY_KP_0&&k<=KEY_KP_9) return KC_KP0+(k-KEY_KP_0);
    switch(k){
    case KEY_SPACE: return KC_SPACE; case KEY_LEFT: return KC_LEFT; case KEY_RIGHT: return KC_RIGHT; case KEY_UP: return KC_UP; case KEY_DOWN: return KC_DOWN;
    case KEY_ENTER: return KC_ENTER; case KEY_KP_ENTER: return KC_KPENTER; case KEY_ESCAPE: return KC_ESC; case KEY_BACKSPACE: return KC_BACKSPACE; case KEY_DELETE: return KC_DELETE;
    case KEY_TAB: return KC_TAB; case KEY_INSERT: return KC_INSERT; case KEY_HOME: return KC_HOME; case KEY_END: return KC_END; case KEY_PAGE_UP: return KC_PAGEUP; case KEY_PAGE_DOWN: return KC_PAGEDOWN;
    case KEY_EQUAL: return KC_EQUAL; case KEY_MINUS: return KC_MINUS; case KEY_COMMA: return KC_COMMA; case KEY_PERIOD: return KC_PERIOD; case KEY_SEMICOLON: return KC_SEMICOLON;
    case KEY_APOSTROPHE: return KC_APOSTROPHE; case KEY_SLASH: return KC_SLASH; case KEY_BACKSLASH: return KC_BACKSLASH; case KEY_LEFT_BRACKET: return KC_LBRACKET; case KEY_RIGHT_BRACKET: return KC_RBRACKET; case KEY_GRAVE: return KC_GRAVE;
    case KEY_KP_ADD: return KC_KPADD; case KEY_KP_SUBTRACT: return KC_KPSUB; case KEY_KP_MULTIPLY: return KC_KPMUL; case KEY_KP_DIVIDE: return KC_KPDIV; case KEY_KP_DECIMAL: return KC_KPDECIMAL;
    case KEY_PAUSE: return KC_PAUSE; case KEY_PRINT_SCREEN: return KC_PRINT; case KEY_SCROLL_LOCK: return KC_SCROLL; case KEY_NUM_LOCK: return KC_NUMLOCK; case KEY_CAPS_LOCK: return KC_CAPSLOCK;
    default: return KC_NONE;
    }
}
void PlatformUpdateGlobalHotkeys(){ hkx::Update(); }
static void Usage(){
    printf("Remix Player (Linux)\n"
           "uso: remix [opcoes] [arquivo de audio]\n"
           "  --play-file <arquivo>   toca o arquivo (reusa a instancia aberta)\n"
           "  --home <pasta>          usa a pasta como config/biblioteca (modo portatil)\n"
           "  --wayland               Wayland nativo (padrao: X11/XWayland)\n"
           "  --no-splash --settings --autoplay --list --mode <square|cd|vertical>\n"
           "  --screenshot <png> [--shot-delay ms]   --after <ms>:<acao>   --exit-after <ms>\n"
           "  --cmd <acao>            manda um comando para a instancia aberta (playpause, next, prev,\n"
           "                          volup, voldown, mute, shuffle, repeat, show, hide, quit) - use nos atalhos do sistema\n");
}

int main(int argc,char** argv){
    if(!std::setlocale(LC_CTYPE,"C.UTF-8")) std::setlocale(LC_CTYPE,"");
    sys::InitProcess();
    std::wstring cmd, shotPath; std::string cmdName; int shotDelay=400, exitAfter=0; bool noSplash=false, openSettings=false, autoplay=false, listMode=false, wantWayland=std::getenv("REMIX_WAYLAND")&&std::string(std::getenv("REMIX_WAYLAND"))=="1";
    std::string modeOpt;
    for(int i=1;i<argc;i++){
        std::string a=argv[i];
        auto next=[&](std::string& out){ if(i+1<argc){ out=argv[++i]; return true; } return false; };
        std::string v;
        if((a=="--play-file"||a=="--play")&&next(v)) cmd=Utf8ToWide(v);
        else if(a=="--home"&&next(v)) setenv("REMIX_HOME",v.c_str(),1);
        else if(a=="--wayland") wantWayland=true;
        else if(a=="--x11") wantWayland=false;
        else if(a=="--no-splash") noSplash=true;
        else if(a=="--settings") openSettings=true;
        else if(a=="--autoplay") autoplay=true;
        else if(a=="--list") listMode=true;
        else if(a=="--mode"&&next(v)) modeOpt=v;
        else if(a=="--screenshot"&&next(v)) shotPath=Utf8ToWide(v);
        else if(a=="--shot-delay"&&next(v)) shotDelay=atoi(v.c_str());
        else if(a=="--exit-after"&&next(v)) exitAfter=atoi(v.c_str());
        else if(a=="--cmd"&&next(v)) cmdName=v;
        else if(a=="--after"&&next(v)){ size_t c=v.find(':'); if(c!=std::string::npos) g_timed.push_back({atoi(v.substr(0,c).c_str()),v.substr(c+1)}); }
        else if(a=="-h"||a=="--help"){ Usage(); return 0; }
        else if(!a.empty()&&a[0]!='-') cmd=Utf8ToWide(a);
    }
    if(!cmd.empty()){ std::error_code ec; auto ap=std::filesystem::absolute(std::filesystem::path(cmd),ec); if(!ec) cmd=ap.wstring(); }

    if(!sys::AcquireSingleInstance()){ if(!cmdName.empty()) sys::SendToExisting(L"CMD|"+Utf8ToWide(cmdName)); else sys::SendToExisting(L"PLAYFILE|"+cmd); return 0; }
    sys::StartIpcServer(EV_COMMAND);

    g_cfg.Load();
    if(modeOpt=="square"){g_cfg.displayMode=L"normal";g_cfg.artShape=L"square";}
    else if(modeOpt=="cd"){g_cfg.displayMode=L"normal";g_cfg.artShape=L"cd";}
    else if(modeOpt=="vertical"){g_cfg.displayMode=L"vertical";}
    sys::CleanupCache(72);
    CoreInit();
    if(!Player::Available()) fprintf(stderr,"[remix] aviso: nenhum dispositivo de audio disponivel\n");
    g_customChrome=false; // decoracao do sistema

    if(!wantWayland&&std::getenv("DISPLAY")) glfwInitHint(GLFW_PLATFORM,GLFW_PLATFORM_X11);
    SetTraceLogLevel(std::getenv("REMIX_DEBUG")?LOG_INFO:LOG_ERROR);
    unsigned flags=FLAG_WINDOW_RESIZABLE|FLAG_VSYNC_HINT|FLAG_WINDOW_ALWAYS_RUN;   // ALWAYS_RUN: minimizado continua tocando/avancando
    if(!(std::getenv("REMIX_NO_MSAA")&&std::string(std::getenv("REMIX_NO_MSAA"))=="1")&&!g_cfg.perfMode) flags|=FLAG_MSAA_4X_HINT;
    SetConfigFlags(flags);
    int startW=g_cfg.displayMode==L"vertical"?460:1500,startH=g_cfg.displayMode==L"vertical"?700:812;
    if(g_cfg.displayMode!=L"vertical"&&g_cfg.winW>=720&&g_cfg.winH>=540){startW=g_cfg.winW;startH=g_cfg.winH;}
    InitWindow(startW,startH,"Remix Player");
    if(!IsWindowReady()){ fprintf(stderr,"[remix] nao foi possivel abrir a janela (X11/Wayland/OpenGL 3.3).\n"); return 1; }
    { int w=startW,h=startH; if(g_cfg.displayMode==L"vertical") ClampToWork(w,h); else NormalSize(w,h); if(w!=startW||h!=startH) SetWindowSize(w,h); startW=w; startH=h; }
    SetExitKey(0); SetTargetFPS(g_cfg.perfMode?30:60); ApplyMinSize();
    {
        Image ic=LoadImage(WideToUtf8(Config::Join(Config::Join(Config::AssetDir(),L"branding"),L"icon.png")).c_str());
        if(ic.data){ SetWindowIcon(ic); UnloadImage(ic); }
    }
    gfx::InitFonts(Config::AssetDir());
    g_splashImg=gfx::LoadImg(Config::Join(Config::Join(Config::AssetDir(),L"branding"),L"splash.png"));
    if(!g_splashImg->ok){ gfx::FreeImg(g_splashImg); g_splashImg=nullptr; }
    g_splashStart=GetTickCount64();
    if(noSplash) g_showSplash=false;
    else Player::PlayOneShot(Config::Join(Config::Join(Config::AssetDir(),L"branding"),L"open.wav"));
    std::thread([]{ int n=MigrateOversizedCovers(); if(n>0) sys::Post(EV_THUMBS_INVALIDATE,L"",n); }).detach();

    LoadFolderAndPlaylist(); PlatformWatchStart(g_cfg.musicFolder); if(g_current>=0)PlayIndex(g_current,false);
    RestoreOpenPlaylist(); PlatformUpdateGlobalHotkeys();
    if(!cmdName.empty()) HandleCommand(L"CMD|"+Utf8ToWide(cmdName));
    if(!cmd.empty()) g_pendingPlay=cmd;
    g_winW=GetScreenWidth(); g_winH=GetScreenHeight(); if(g_winW<=0){g_winW=startW;g_winH=startH;} BuildLayout();
    if(g_cfg.musicFolder.empty()) StartAutoScan();
    if(!g_pendingPlay.empty()) HandleCommand(L"PLAYFILE|"+g_pendingPlay);
    if(openSettings) g_showSettings=true;
    if(listMode){ g_cfg.listMode=1; BuildLayout(); }
    if(autoplay&&g_player.loaded) g_player.Play();

    float wheelAcc=0; bool shotDone=shotPath.empty(); ULONGLONG shotT0=0; ULONGLONG t0=GetTickCount64();
    bool lastPerf=g_cfg.perfMode; ULONGLONG lastHiddenTick=0;
    int lastFps=-1; ULONGLONG mexeuEm=GetTickCount64(); Vector2 mouseAnt={0,0};
    while(!g_quit){
        if(g_cfg.sysMedia){ if(!mpris::Active()) mpris::Start(); } else if(mpris::Active()) mpris::Stop();
        if(WindowShouldClose()){ ResetCloseRequest(); RequestClose(); if(g_quit) break; }
        float dt;
        if(g_hiddenToBg){ ULONGLONG now=GetTickCount64(); dt=lastHiddenTick?(float)(now-lastHiddenTick)/1000.f:0.05f; lastHiddenTick=now; }
        else { lastHiddenTick=0; dt=GetFrameTime(); }
        if(dt>0.1f) dt=0.1f; if(dt<=0) dt=1.f/60.f;
        if(!g_hiddenToBg&&IsWindowResized()){
            g_winW=GetScreenWidth(); g_winH=GetScreenHeight(); BuildLayout();
            if(g_cfg.displayMode!=L"vertical"&&!IsWindowMinimized()&&g_winW>=720&&g_winH>=540){ g_cfg.winW=g_winW; g_cfg.winH=g_winH; }
        } else if(!g_hiddenToBg){
            // rede de seguranca: se o compositor aplicou outro tamanho sem evento (troca de modo,
            // limites minimos), refaz o layout para o tamanho real em vez de esperar um redimensionamento
            int cw=GetScreenWidth(), ch=GetScreenHeight();
            if(cw>0&&ch>0&&(cw!=g_winW||ch!=g_winH)){ g_winW=cw; g_winH=ch; BuildLayout(); }
        }
        if(lastPerf!=g_cfg.perfMode){ lastPerf=g_cfg.perfMode; lastFps=-1; }
        // Quantos quadros por segundo a tela realmente precisa agora. Um player passa a maior
        // parte do tempo sem nada mudando (ou atras de outra janela): desenhar 60x por segundo
        // nessas horas so queima CPU -- era o que fazia o Remix pesar enquanto a pessoa jogava.
        if(!g_hiddenToBg){
            Vector2 mp0=GetMousePosition();
            if(mp0.x!=mouseAnt.x||mp0.y!=mouseAnt.y){ mouseAnt=mp0; mexeuEm=GetTickCount64(); }
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)||IsMouseButtonDown(MOUSE_BUTTON_RIGHT)||GetMouseWheelMove()!=0.f) mexeuEm=GetTickCount64();
            int teto=g_cfg.perfMode?30:60;
            bool foco=IsWindowFocused()&&!IsWindowMinimized();
            bool tocando=g_player.playing;
            bool mexendo=GetTickCount64()-mexeuEm<1500;         // mouse/clique/roda nos ultimos 1,5 s
            int alvo;
            if(!foco)              alvo=tocando?10:5;           // atras de um jogo, por exemplo
            else if(g_showSplash)  alvo=teto;
            else if(mexendo)       alvo=teto;                    // interagindo: fluido
            else if(tocando)       alvo=g_cfg.perfMode?20:30;    // so a capa/onda/barra andando
            else                   alvo=10;                      // parado e sem ninguem mexendo
            if(alvo!=lastFps){ lastFps=alvo; SetTargetFPS(alvo); }
        }
        sys::Ev ev; while(sys::Poll(ev)) HandleEvent(ev.type,ev.s,ev.n);
        if(g_cfg.sysMedia) mpris::Poll();
        hkx::Poll();
        Tick(dt);
        RunTimedActions(GetTickCount64()-t0);
        if(exitAfter>0&&GetTickCount64()-t0>=(ULONGLONG)exitAfter) break;
        if(g_quit) break;
        if(g_hiddenToBg){ PollInputEvents(); WaitTime(0.05); continue; }   // 2o plano: sem desenhar
        Vector2 mp=GetMousePosition(); int mx=(int)mp.x,my=(int)mp.y; g_mouseX=mx; g_mouseY=my;
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) OnLButtonDown(mx,my);
        else if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)&&g_dragSeek!=-1) OnMouseDrag(mx);
        if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) OnLButtonUp();
        if(IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) OnRButtonDown(mx,my);
        wheelAcc+=GetMouseWheelMove(); int steps=(int)wheelAcc; if(steps!=0){ wheelAcc-=steps; OnWheel(steps); }
        for(int c=GetCharPressed();c;c=GetCharPressed()){ OnChar(c); mexeuEm=GetTickCount64(); }
        {
            int mods=(IsKeyDown(KEY_LEFT_CONTROL)||IsKeyDown(KEY_RIGHT_CONTROL)?KM_CTRL:0)|(IsKeyDown(KEY_LEFT_SHIFT)||IsKeyDown(KEY_RIGHT_SHIFT)?KM_SHIFT:0)|(IsKeyDown(KEY_LEFT_ALT)||IsKeyDown(KEY_RIGHT_ALT)?KM_ALT:0);
            for(int k=GetKeyPressed();k;k=GetKeyPressed()){ int kc=MapKC(k); if(kc){ OnKeyEvent(kc,mods); mexeuEm=GetTickCount64(); } }
            if(IsKeyPressedRepeat(KEY_BACKSPACE)){ OnKeyEvent(KC_BACKSPACE,mods); mexeuEm=GetTickCount64(); }
        }
        BeginDrawing();
        Render();
        bool takeShot=false;
        if(!shotDone&&!g_showSplash){
            if(!shotT0) shotT0=GetTickCount64();
            if(GetTickCount64()-shotT0>=(ULONGLONG)shotDelay){ g_shotPath=shotPath; takeShot=true; shotDone=true; }
        }
        if(g_shotPending&&!g_showSplash){ takeShot=true; g_shotPending=false; }
        if(takeShot){
            rlDrawRenderBatchActive();
            Image im=LoadImageFromScreen(); ExportImage(im,WideToUtf8(g_shotPath).c_str()); UnloadImage(im);
            fprintf(stderr,"[remix] screenshot salvo em %s\n",WideToUtf8(g_shotPath).c_str());
        }
        EndDrawing();
    }
    mpris::Stop(); hkx::Stop();
    CoreShutdown();
    if(g_coverImg)gfx::FreeImg(g_coverImg);
    if(g_splashImg)gfx::FreeImg(g_splashImg);
    PlatformClearThumbs();
    gfx::UnloadFonts();
    CloseWindow();
    sys::CleanupIpc();
    return 0;
}
