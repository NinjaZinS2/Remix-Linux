#pragma once
// Atalhos GLOBAIS no Linux via X11: XGrabKey numa conexao propria com o servidor X.
// Funciona em sessao X11 com qualquer programa na frente. Em sessao Wayland (via
// XWayland) o servidor so entrega as teclas enquanto um app X tem foco; la a saida
// e o atalho do sistema chamando "remix --cmd <acao>" (documentado no README).
#include "app_core.h"
#include <dlfcn.h>
#include <cstdlib>

namespace hkx {
typedef struct _XDisplay Display; typedef unsigned long Window; typedef unsigned long KeySym; typedef unsigned char XKeyCode; typedef int XBool;
struct XKeyEventL { int type; unsigned long serial; XBool send_event; Display* display; Window window; Window root; Window subwindow; unsigned long time; int x,y; int x_root,y_root; unsigned int state; unsigned int keycode; XBool same_screen; };
union XEventL { int type; XKeyEventL xkey; long pad[24]; };
typedef int (*XErrHandler)(Display*,void*);
struct Api {
    void* lib=nullptr;
    Display* (*OpenDisplay)(const char*)=nullptr; int (*CloseDisplay)(Display*)=nullptr;
    Window (*RootWindow)(Display*,int)=nullptr; int (*DefaultScreen)(Display*)=nullptr;
    int (*GrabKey)(Display*,int,unsigned,Window,XBool,int,int)=nullptr; int (*UngrabKey)(Display*,int,unsigned,Window)=nullptr;
    XKeyCode (*KeysymToKeycode)(Display*,KeySym)=nullptr; KeySym (*StringToKeysym)(const char*)=nullptr;
    int (*Pending)(Display*)=nullptr; int (*NextEvent)(Display*,XEventL*)=nullptr; int (*Sync)(Display*,XBool)=nullptr;
    XErrHandler (*SetErrorHandler)(XErrHandler)=nullptr;
};
static Api A; static Display* g_dpy=nullptr; static Window g_root=0; static bool g_on=false, g_failed=false;
struct Grab { int action; unsigned keycode; unsigned mods; };
static std::vector<Grab> g_grabs;
static int IgnoreXError(Display*,void*){ return 0; }   // BadAccess = outra app ja pegou a tecla
static const char* KeysymName(int kc){
    static char buf[4];
    if(kc>='A'&&kc<='Z'){ buf[0]=(char)(kc-'A'+'a'); buf[1]=0; return buf; }
    if(kc>='0'&&kc<='9'){ buf[0]=(char)kc; buf[1]=0; return buf; }
    if(kc>=KC_F1&&kc<=KC_F12){ static char fb[4]; snprintf(fb,4,"F%d",kc-KC_F1+1); return fb; }
    if(kc>=KC_KP0&&kc<=KC_KP9){ static char kb[6]; snprintf(kb,6,"KP_%d",kc-KC_KP0); return kb; }
    switch(kc){
    case KC_SPACE: return "space"; case KC_LEFT: return "Left"; case KC_RIGHT: return "Right"; case KC_UP: return "Up"; case KC_DOWN: return "Down";
    case KC_ENTER: return "Return"; case KC_ESC: return "Escape"; case KC_BACKSPACE: return "BackSpace"; case KC_DELETE: return "Delete"; case KC_TAB: return "Tab";
    case KC_INSERT: return "Insert"; case KC_HOME: return "Home"; case KC_END: return "End"; case KC_PAGEUP: return "Prior"; case KC_PAGEDOWN: return "Next";
    case KC_EQUAL: return "equal"; case KC_MINUS: return "minus"; case KC_COMMA: return "comma"; case KC_PERIOD: return "period"; case KC_SEMICOLON: return "semicolon";
    case KC_APOSTROPHE: return "apostrophe"; case KC_SLASH: return "slash"; case KC_BACKSLASH: return "backslash"; case KC_LBRACKET: return "bracketleft"; case KC_RBRACKET: return "bracketright"; case KC_GRAVE: return "grave";
    case KC_KPADD: return "KP_Add"; case KC_KPSUB: return "KP_Subtract"; case KC_KPMUL: return "KP_Multiply"; case KC_KPDIV: return "KP_Divide"; case KC_KPDECIMAL: return "KP_Decimal"; case KC_KPENTER: return "KP_Enter";
    case KC_MEDIA_PLAY: return "XF86AudioPlay"; case KC_MEDIA_NEXT: return "XF86AudioNext"; case KC_MEDIA_PREV: return "XF86AudioPrev"; case KC_MEDIA_STOP: return "XF86AudioStop";
    case KC_VOL_UP: return "XF86AudioRaiseVolume"; case KC_VOL_DOWN: return "XF86AudioLowerVolume"; case KC_VOL_MUTE: return "XF86AudioMute";
    case KC_PAUSE: return "Pause"; case KC_PRINT: return "Print"; case KC_SCROLL: return "Scroll_Lock"; case KC_NUMLOCK: return "Num_Lock"; case KC_CAPSLOCK: return "Caps_Lock";
    default: return nullptr;
    }
}
static bool Start(){
    if(g_on) return true; if(g_failed) return false;
    if(!std::getenv("DISPLAY")){ g_failed=true; return false; }
    if(!A.lib){
        A.lib=dlopen("libX11.so.6",RTLD_NOW|RTLD_LOCAL); if(!A.lib){ g_failed=true; return false; }
        auto L=[&](auto& fn,const char* n){ *(void**)&fn=dlsym(A.lib,n); return fn!=nullptr; };
        bool ok=L(A.OpenDisplay,"XOpenDisplay")&&L(A.CloseDisplay,"XCloseDisplay")&&L(A.RootWindow,"XRootWindow")&&L(A.DefaultScreen,"XDefaultScreen")&&L(A.GrabKey,"XGrabKey")&&L(A.UngrabKey,"XUngrabKey")&&L(A.KeysymToKeycode,"XKeysymToKeycode")&&L(A.StringToKeysym,"XStringToKeysym")&&L(A.Pending,"XPending")&&L(A.NextEvent,"XNextEvent")&&L(A.Sync,"XSync")&&L(A.SetErrorHandler,"XSetErrorHandler");
        if(!ok){ dlclose(A.lib); A.lib=nullptr; g_failed=true; return false; }
    }
    g_dpy=A.OpenDisplay(nullptr); if(!g_dpy){ g_failed=true; return false; }
    g_root=A.RootWindow(g_dpy,A.DefaultScreen(g_dpy));
    A.SetErrorHandler(IgnoreXError);
    g_on=true; return true;
}
static const unsigned IGNORE_MASKS[4]={0u,2u,16u,18u};   // -, CapsLock, NumLock, ambos
static void UngrabAll(){ if(!g_on) return; for(auto& g:g_grabs) for(unsigned m:IGNORE_MASKS) A.UngrabKey(g_dpy,(int)g.keycode,g.mods|m,g_root); g_grabs.clear(); A.Sync(g_dpy,0); }
static void Update(){   // re-registra conforme g_cfg.hk[*].global
    bool any=false; for(int a=0;a<HK_COUNT;a++) if(g_cfg.hk[a].global&&g_cfg.hk[a].key) any=true;
    if(!g_on){ if(!any||!Start()) return; }
    UngrabAll();
    for(int a=0;a<HK_COUNT;a++){
        const Hotkey& h=g_cfg.hk[a]; if(!h.global||!h.key) continue;
        const char* ks=KeysymName(h.key); if(!ks) continue;
        KeySym sym=A.StringToKeysym(ks); if(!sym) continue;
        unsigned code=A.KeysymToKeycode(g_dpy,sym); if(!code) continue;
        unsigned mods=(h.mods&KM_CTRL?4u:0u)|(h.mods&KM_SHIFT?1u:0u)|(h.mods&KM_ALT?8u:0u);
        for(unsigned m:IGNORE_MASKS) A.GrabKey(g_dpy,(int)code,mods|m,g_root,0,1,1);
        g_grabs.push_back({a,code,mods});
    }
    A.Sync(g_dpy,0);
    if(std::getenv("REMIX_DEBUG")) fprintf(stderr,"[remix] atalhos globais X11: %zu registrados\n",g_grabs.size());
}
static void Poll(){
    if(!g_on) return;
    while(A.Pending(g_dpy)>0){
        XEventL ev; A.NextEvent(g_dpy,&ev);
        if(ev.type!=2) continue;   // KeyPress
        unsigned st=ev.xkey.state&~(2u|16u);
        for(auto& g:g_grabs) if(g.keycode==ev.xkey.keycode&&g.mods==st){ RunHotkeyAction(g.action); break; }
    }
}
static void Stop(){ if(!g_on) return; UngrabAll(); A.CloseDisplay(g_dpy); g_dpy=nullptr; g_on=false; }
} // namespace hkx
