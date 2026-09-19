#pragma once
// MPRIS2 (org.mpris.MediaPlayer2) usando a libdbus-1 carregada com dlopen.
// Faz o Remix aparecer nos controles de midia do KDE/GNOME/XFCE/etc. (applet
// do player, teclas de midia, playerctl) — inclusive com a janela escondida em
// segundo plano: "Raise" traz a janela de volta e "Quit" encerra.
// Sem libdbus ou sem bus de sessao, simplesmente nao liga (REMIX_NO_MPRIS=1 desliga).
#include "app_core.h"
#include <dlfcn.h>
#include <unistd.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace mpris {

typedef struct DBusConnection DBusConnection;
typedef struct DBusMessage DBusMessage;
struct DBusMessageIter { alignas(8) unsigned char opaque[128]; };   // o real tem 72 bytes; maior e seguro
struct DBusError { const char* name; const char* message; unsigned int flags; void* pad; };
typedef unsigned int dbus_bool_t;
enum { BUS_SESSION=0 /*DBusBusType: SESSION=0, SYSTEM=1*/, NAME_FLAG_DO_NOT_QUEUE=4, REPLY_PRIMARY_OWNER=1, MSG_METHOD_CALL=1 };
enum { T_BOOLEAN='b', T_INT64='x', T_DOUBLE='d', T_STRING='s', T_OBJECT_PATH='o', T_ARRAY='a', T_VARIANT='v', T_DICT_ENTRY='e' };

struct Api {
    void* lib=nullptr;
    void (*error_init)(DBusError*)=nullptr;
    dbus_bool_t (*error_is_set)(const DBusError*)=nullptr;
    void (*error_free)(DBusError*)=nullptr;
    DBusConnection* (*bus_get)(int,DBusError*)=nullptr;
    void (*set_exit_on_disconnect)(DBusConnection*,dbus_bool_t)=nullptr;
    int (*bus_request_name)(DBusConnection*,const char*,unsigned,DBusError*)=nullptr;
    int (*bus_release_name)(DBusConnection*,const char*,DBusError*)=nullptr;
    dbus_bool_t (*read_write)(DBusConnection*,int)=nullptr;
    DBusMessage* (*pop_message)(DBusConnection*)=nullptr;
    dbus_bool_t (*send)(DBusConnection*,DBusMessage*,unsigned*)=nullptr;
    void (*flush)(DBusConnection*)=nullptr;
    void (*conn_unref)(DBusConnection*)=nullptr;
    int (*msg_get_type)(DBusMessage*)=nullptr;
    const char* (*msg_get_interface)(DBusMessage*)=nullptr;
    const char* (*msg_get_member)(DBusMessage*)=nullptr;
    const char* (*msg_get_path)(DBusMessage*)=nullptr;
    dbus_bool_t (*msg_get_no_reply)(DBusMessage*)=nullptr;
    DBusMessage* (*msg_new_method_return)(DBusMessage*)=nullptr;
    DBusMessage* (*msg_new_error)(DBusMessage*,const char*,const char*)=nullptr;
    DBusMessage* (*msg_new_signal)(const char*,const char*,const char*)=nullptr;
    void (*msg_unref)(DBusMessage*)=nullptr;
    dbus_bool_t (*iter_init)(DBusMessage*,DBusMessageIter*)=nullptr;
    void (*iter_init_append)(DBusMessage*,DBusMessageIter*)=nullptr;
    dbus_bool_t (*iter_append_basic)(DBusMessageIter*,int,const void*)=nullptr;
    dbus_bool_t (*iter_open_container)(DBusMessageIter*,int,const char*,DBusMessageIter*)=nullptr;
    dbus_bool_t (*iter_close_container)(DBusMessageIter*,DBusMessageIter*)=nullptr;
    int (*iter_get_arg_type)(DBusMessageIter*)=nullptr;
    void (*iter_get_basic)(DBusMessageIter*,void*)=nullptr;
    dbus_bool_t (*iter_next)(DBusMessageIter*)=nullptr;
    void (*iter_recurse)(DBusMessageIter*,DBusMessageIter*)=nullptr;
};
static Api A;
static DBusConnection* g_conn=nullptr;
static bool g_on=false, g_failed=false;
static std::string g_busName;
static const char* OBJ_PATH="/org/mpris/MediaPlayer2";
static const char* IF_ROOT="org.mpris.MediaPlayer2";
static const char* IF_PLAYER="org.mpris.MediaPlayer2.Player";
static const char* IF_PROPS="org.freedesktop.DBus.Properties";
// ultimo estado anunciado (para o sinal PropertiesChanged)
static std::string s_lastStatus; static std::wstring s_lastTrackPath; static int s_lastVol=-1; static int s_lastShuffle=-1, s_lastRepeat=-1; static ULONGLONG s_lastCheck=0;

static bool LoadApi(){
    if(A.lib) return true;
    A.lib=dlopen("libdbus-1.so.3",RTLD_NOW|RTLD_LOCAL);
    if(!A.lib) return false;
    bool ok=true;
    auto L=[&](auto& fn,const char* name){ *(void**)&fn=dlsym(A.lib,name); if(!fn) ok=false; };
    L(A.error_init,"dbus_error_init"); L(A.error_is_set,"dbus_error_is_set"); L(A.error_free,"dbus_error_free");
    L(A.bus_get,"dbus_bus_get"); L(A.set_exit_on_disconnect,"dbus_connection_set_exit_on_disconnect");
    L(A.bus_request_name,"dbus_bus_request_name"); L(A.bus_release_name,"dbus_bus_release_name");
    L(A.read_write,"dbus_connection_read_write"); L(A.pop_message,"dbus_connection_pop_message");
    L(A.send,"dbus_connection_send"); L(A.flush,"dbus_connection_flush"); L(A.conn_unref,"dbus_connection_unref");
    L(A.msg_get_type,"dbus_message_get_type"); L(A.msg_get_interface,"dbus_message_get_interface");
    L(A.msg_get_member,"dbus_message_get_member"); L(A.msg_get_path,"dbus_message_get_path"); L(A.msg_get_no_reply,"dbus_message_get_no_reply");
    L(A.msg_new_method_return,"dbus_message_new_method_return"); L(A.msg_new_error,"dbus_message_new_error");
    L(A.msg_new_signal,"dbus_message_new_signal"); L(A.msg_unref,"dbus_message_unref");
    L(A.iter_init,"dbus_message_iter_init"); L(A.iter_init_append,"dbus_message_iter_init_append");
    L(A.iter_append_basic,"dbus_message_iter_append_basic"); L(A.iter_open_container,"dbus_message_iter_open_container");
    L(A.iter_close_container,"dbus_message_iter_close_container"); L(A.iter_get_arg_type,"dbus_message_iter_get_arg_type");
    L(A.iter_get_basic,"dbus_message_iter_get_basic"); L(A.iter_next,"dbus_message_iter_next"); L(A.iter_recurse,"dbus_message_iter_recurse");
    if(!ok){ dlclose(A.lib); A.lib=nullptr; }
    return ok;
}

// ---- helpers para montar variantes -------------------------------------------
static void VStr(DBusMessageIter& p,const char* v){ DBusMessageIter s; A.iter_open_container(&p,T_VARIANT,"s",&s); A.iter_append_basic(&s,T_STRING,&v); A.iter_close_container(&p,&s); }
static void VPath(DBusMessageIter& p,const char* v){ DBusMessageIter s; A.iter_open_container(&p,T_VARIANT,"o",&s); A.iter_append_basic(&s,T_OBJECT_PATH,&v); A.iter_close_container(&p,&s); }
static void VBool(DBusMessageIter& p,bool b){ dbus_bool_t v=b?1:0; DBusMessageIter s; A.iter_open_container(&p,T_VARIANT,"b",&s); A.iter_append_basic(&s,T_BOOLEAN,&v); A.iter_close_container(&p,&s); }
static void VDouble(DBusMessageIter& p,double d){ DBusMessageIter s; A.iter_open_container(&p,T_VARIANT,"d",&s); A.iter_append_basic(&s,T_DOUBLE,&d); A.iter_close_container(&p,&s); }
static void VInt64(DBusMessageIter& p,long long x){ DBusMessageIter s; A.iter_open_container(&p,T_VARIANT,"x",&s); A.iter_append_basic(&s,T_INT64,&x); A.iter_close_container(&p,&s); }
static void VStrArray(DBusMessageIter& p,const std::vector<std::string>& v){
    DBusMessageIter s,arr; A.iter_open_container(&p,T_VARIANT,"as",&s); A.iter_open_container(&s,T_ARRAY,"s",&arr);
    for(auto& e:v){ const char* c=e.c_str(); A.iter_append_basic(&arr,T_STRING,&c); }
    A.iter_close_container(&s,&arr); A.iter_close_container(&p,&s);
}
static std::string FileUrl(const std::wstring& path){
    std::string u8=WideToUtf8(path), out="file://";
    for(unsigned char c:u8){
        if(isalnum(c)||c=='/'||c=='-'||c=='_'||c=='.'||c=='~') out.push_back((char)c);
        else { char b[4]; snprintf(b,4,"%%%02X",c); out+=b; }
    }
    return out;
}
static std::string UrlToPath(const std::string& url){
    std::string s=url; if(s.rfind("file://",0)==0) s=s.substr(7);
    std::string out; for(size_t i=0;i<s.size();++i){ if(s[i]=='%'&&i+2<s.size()){ out.push_back((char)strtol(s.substr(i+1,2).c_str(),nullptr,16)); i+=2; } else out.push_back(s[i]); }
    return out;
}
static std::string Status(){ return g_player.playing?"Playing":(g_player.loaded?"Paused":"Stopped"); }
static bool HasTrack(){ return g_current>=0&&g_current<(int)g_tracks.size(); }
static int VolPercent(){ return g_muted?0:g_cfg.volume; }

// escreve, dentro de 'p', a variante com o valor da propriedade; false = desconhecida
static bool WriteValue(DBusMessageIter& p,const std::string& iface,const std::string& name){
    if(iface==IF_ROOT){
        if(name=="CanQuit"||name=="CanRaise") { VBool(p,true); return true; }
        if(name=="HasTrackList"||name=="Fullscreen"||name=="CanSetFullscreen") { VBool(p,false); return true; }
        if(name=="Identity") { VStr(p,"Remix Player"); return true; }
        if(name=="DesktopEntry") { VStr(p,"remix"); return true; }
        if(name=="SupportedUriSchemes") { VStrArray(p,{"file"}); return true; }
        if(name=="SupportedMimeTypes") { VStrArray(p,{"audio/mpeg","audio/x-wav","audio/wav","audio/flac","audio/ogg"}); return true; }
        return false;
    }
    if(iface==IF_PLAYER){
        if(name=="PlaybackStatus") { VStr(p,Status().c_str()); return true; }
        if(name=="LoopStatus") { VStr(p,g_cfg.repeat?"Track":"None"); return true; }
        if(name=="Rate"||name=="MinimumRate"||name=="MaximumRate") { VDouble(p,1.0); return true; }
        if(name=="Shuffle") { VBool(p,g_cfg.shuffle); return true; }
        if(name=="Volume") { VDouble(p,VolPercent()/100.0); return true; }
        if(name=="Position") { VInt64(p,(long long)(g_player.loaded?g_player.GetPositionMs():0)*1000LL); return true; }
        if(name=="CanGoNext"||name=="CanGoPrevious"||name=="CanPlay"||name=="CanPause"||name=="CanSeek"||name=="CanControl") { VBool(p,name=="CanPlay"?!g_tracks.empty():true); return true; }
        if(name=="Metadata"){
            DBusMessageIter v,d;
            A.iter_open_container(&p,T_VARIANT,"a{sv}",&v); A.iter_open_container(&v,T_ARRAY,"{sv}",&d);
            auto entry=[&](const char* key,auto fn){ DBusMessageIter e; A.iter_open_container(&d,T_DICT_ENTRY,nullptr,&e); A.iter_append_basic(&e,T_STRING,&key); fn(e); A.iter_close_container(&d,&e); };
            if(HasTrack()){
                const Track& t=g_tracks[(size_t)g_current];
                std::string tid="/org/remix/track/"+std::to_string(g_current);
                entry("mpris:trackid",[&](DBusMessageIter& e){ VPath(e,tid.c_str()); });
                long long len=(long long)(g_player.loaded?g_player.GetLengthMs():0)*1000LL;
                entry("mpris:length",[&](DBusMessageIter& e){ VInt64(e,len); });
                std::string title=WideToUtf8(t.title), artist=WideToUtf8(t.artist), url=FileUrl(t.path);
                entry("xesam:title",[&](DBusMessageIter& e){ VStr(e,title.c_str()); });
                entry("xesam:artist",[&](DBusMessageIter& e){ VStrArray(e,{artist}); });
                entry("xesam:url",[&](DBusMessageIter& e){ VStr(e,url.c_str()); });
                if(!t.coverPath.empty()){ std::string art=FileUrl(t.coverPath); entry("mpris:artUrl",[&](DBusMessageIter& e){ VStr(e,art.c_str()); }); }
            } else {
                entry("mpris:trackid",[&](DBusMessageIter& e){ VPath(e,"/org/mpris/MediaPlayer2/TrackList/NoTrack"); });
            }
            A.iter_close_container(&v,&d); A.iter_close_container(&p,&v);
            return true;
        }
        return false;
    }
    return false;
}
static const char* ROOT_PROPS[]={"CanQuit","CanRaise","HasTrackList","Fullscreen","CanSetFullscreen","Identity","DesktopEntry","SupportedUriSchemes","SupportedMimeTypes",nullptr};
static const char* PLAYER_PROPS[]={"PlaybackStatus","LoopStatus","Rate","Shuffle","Metadata","Volume","Position","MinimumRate","MaximumRate","CanGoNext","CanGoPrevious","CanPlay","CanPause","CanSeek","CanControl",nullptr};

static const char* INTROSPECT_XML=
"<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\" \"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
"<node><interface name=\"org.freedesktop.DBus.Introspectable\"><method name=\"Introspect\"><arg name=\"data\" type=\"s\" direction=\"out\"/></method></interface>"
"<interface name=\"org.freedesktop.DBus.Properties\"><method name=\"Get\"><arg type=\"s\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/><arg type=\"v\" direction=\"out\"/></method>"
"<method name=\"GetAll\"><arg type=\"s\" direction=\"in\"/><arg type=\"a{sv}\" direction=\"out\"/></method><method name=\"Set\"><arg type=\"s\" direction=\"in\"/><arg type=\"s\" direction=\"in\"/><arg type=\"v\" direction=\"in\"/></method>"
"<signal name=\"PropertiesChanged\"><arg type=\"s\"/><arg type=\"a{sv}\"/><arg type=\"as\"/></signal></interface>"
"<interface name=\"org.mpris.MediaPlayer2\"><method name=\"Raise\"/><method name=\"Quit\"/><property name=\"CanQuit\" type=\"b\" access=\"read\"/><property name=\"CanRaise\" type=\"b\" access=\"read\"/>"
"<property name=\"HasTrackList\" type=\"b\" access=\"read\"/><property name=\"Identity\" type=\"s\" access=\"read\"/><property name=\"DesktopEntry\" type=\"s\" access=\"read\"/>"
"<property name=\"SupportedUriSchemes\" type=\"as\" access=\"read\"/><property name=\"SupportedMimeTypes\" type=\"as\" access=\"read\"/></interface>"
"<interface name=\"org.mpris.MediaPlayer2.Player\"><method name=\"Next\"/><method name=\"Previous\"/><method name=\"Pause\"/><method name=\"PlayPause\"/><method name=\"Stop\"/><method name=\"Play\"/>"
"<method name=\"Seek\"><arg type=\"x\" direction=\"in\"/></method><method name=\"SetPosition\"><arg type=\"o\" direction=\"in\"/><arg type=\"x\" direction=\"in\"/></method><method name=\"OpenUri\"><arg type=\"s\" direction=\"in\"/></method>"
"<signal name=\"Seeked\"><arg type=\"x\"/></signal>"
"<property name=\"PlaybackStatus\" type=\"s\" access=\"read\"/><property name=\"LoopStatus\" type=\"s\" access=\"readwrite\"/><property name=\"Rate\" type=\"d\" access=\"readwrite\"/><property name=\"Shuffle\" type=\"b\" access=\"readwrite\"/>"
"<property name=\"Metadata\" type=\"a{sv}\" access=\"read\"/><property name=\"Volume\" type=\"d\" access=\"readwrite\"/><property name=\"Position\" type=\"x\" access=\"read\"/><property name=\"MinimumRate\" type=\"d\" access=\"read\"/><property name=\"MaximumRate\" type=\"d\" access=\"read\"/>"
"<property name=\"CanGoNext\" type=\"b\" access=\"read\"/><property name=\"CanGoPrevious\" type=\"b\" access=\"read\"/><property name=\"CanPlay\" type=\"b\" access=\"read\"/><property name=\"CanPause\" type=\"b\" access=\"read\"/><property name=\"CanSeek\" type=\"b\" access=\"read\"/><property name=\"CanControl\" type=\"b\" access=\"read\"/></interface></node>";

static bool ReadString(DBusMessageIter& it,std::string& out){ if(A.iter_get_arg_type(&it)!=T_STRING&&A.iter_get_arg_type(&it)!=T_OBJECT_PATH) return false; const char* s=nullptr; A.iter_get_basic(&it,&s); out=s?s:""; A.iter_next(&it); return true; }
static bool ReadInt64(DBusMessageIter& it,long long& out){ if(A.iter_get_arg_type(&it)!=T_INT64) return false; A.iter_get_basic(&it,&out); A.iter_next(&it); return true; }

static void ApplySet(const std::string& iface,const std::string& name,DBusMessageIter& var){
    if(iface!=IF_PLAYER) return;
    int t=A.iter_get_arg_type(&var);
    if(name=="Volume"&&t==T_DOUBLE){ double d=0; A.iter_get_basic(&var,&d); int pc=(int)(d*100.0+0.5); if(pc<0)pc=0; if(pc>100)pc=100; SetVolumePercent(pc); }
    else if(name=="Shuffle"&&t==T_BOOLEAN){ dbus_bool_t b=0; A.iter_get_basic(&var,&b); g_cfg.shuffle=b?1:0; g_cfg.Save(); }
    else if(name=="LoopStatus"&&t==T_STRING){ const char* s=nullptr; A.iter_get_basic(&var,&s); g_cfg.repeat=(s&&std::string(s)!="None")?1:0; g_cfg.Save(); }
}

static void HandleCall(DBusMessage* m){
    const char* ifc=A.msg_get_interface(m); const char* mem=A.msg_get_member(m); const char* path=A.msg_get_path(m);
    std::string I=ifc?ifc:"", M=mem?mem:"", P=path?path:"";
    DBusMessage* reply=nullptr;
    if(P!=OBJ_PATH){
        reply=A.msg_new_error(m,"org.freedesktop.DBus.Error.UnknownObject","Objeto desconhecido");
    } else if(I=="org.freedesktop.DBus.Introspectable"&&M=="Introspect"){
        reply=A.msg_new_method_return(m); DBusMessageIter it; A.iter_init_append(reply,&it); A.iter_append_basic(&it,T_STRING,&INTROSPECT_XML);
    } else if(I==IF_PROPS){
        DBusMessageIter in; bool has=A.iter_init(m,&in)!=0;
        std::string pi,pn;
        if(M=="Get"&&has&&ReadString(in,pi)&&ReadString(in,pn)){
            reply=A.msg_new_method_return(m); DBusMessageIter it; A.iter_init_append(reply,&it);
            if(!WriteValue(it,pi,pn)){ A.msg_unref(reply); reply=A.msg_new_error(m,"org.freedesktop.DBus.Error.InvalidArgs","Propriedade desconhecida"); }
        } else if(M=="GetAll"&&has&&ReadString(in,pi)){
            reply=A.msg_new_method_return(m); DBusMessageIter it,d; A.iter_init_append(reply,&it);
            A.iter_open_container(&it,T_ARRAY,"{sv}",&d);
            const char** names=(pi==IF_ROOT)?ROOT_PROPS:(pi==IF_PLAYER)?PLAYER_PROPS:nullptr;
            for(const char** n=names;n&&*n;++n){ DBusMessageIter e; A.iter_open_container(&d,T_DICT_ENTRY,nullptr,&e); A.iter_append_basic(&e,T_STRING,n); WriteValue(e,pi,*n); A.iter_close_container(&d,&e); }
            A.iter_close_container(&it,&d);
        } else if(M=="Set"&&has&&ReadString(in,pi)&&ReadString(in,pn)&&A.iter_get_arg_type(&in)==T_VARIANT){
            DBusMessageIter var; A.iter_recurse(&in,&var); ApplySet(pi,pn,var);
            reply=A.msg_new_method_return(m);
        }
    } else if(I==IF_ROOT||(I.empty()&&(M=="Raise"||M=="Quit"))){
        if(M=="Raise") ShowFromBackground(); else if(M=="Quit") QuitApp();
        reply=A.msg_new_method_return(m);
    } else if(I==IF_PLAYER||I.empty()){
        bool known=true;
        if(M=="Next") NextTrack();
        else if(M=="Previous") PrevOrRestart();
        else if(M=="Pause") RemotePause();
        else if(M=="PlayPause") RemotePlayPause();
        else if(M=="Stop") RemoteStop();
        else if(M=="Play") RemotePlay();
        else if(M=="Seek"){ DBusMessageIter in; long long off=0; if(A.iter_init(m,&in)&&ReadInt64(in,off)&&g_player.loaded){ long long pos=(long long)g_player.GetPositionMs()+off/1000; if(pos<0)pos=0; g_player.SeekMs((DWORD)pos); } }
        else if(M=="SetPosition"){ DBusMessageIter in; std::string tid; long long pos=0; if(A.iter_init(m,&in)&&ReadString(in,tid)&&ReadInt64(in,pos)&&g_player.loaded){ if(pos<0)pos=0; g_player.SeekMs((DWORD)(pos/1000)); } }
        else if(M=="OpenUri"){ DBusMessageIter in; std::string uri; if(A.iter_init(m,&in)&&ReadString(in,uri)) PlayFileDirect(Utf8ToWide(UrlToPath(uri)),true); }
        else known=false;
        if(known) reply=A.msg_new_method_return(m);
    }
    if(!reply) reply=A.msg_new_error(m,"org.freedesktop.DBus.Error.UnknownMethod","Metodo desconhecido");
    if(!A.msg_get_no_reply(m)) A.send(g_conn,reply,nullptr);
    A.msg_unref(reply);
}

static void EmitChanged(const std::vector<std::string>& props){
    if(props.empty()) return;
    DBusMessage* sig=A.msg_new_signal(OBJ_PATH,IF_PROPS,"PropertiesChanged"); if(!sig) return;
    DBusMessageIter it,d,inv; A.iter_init_append(sig,&it);
    A.iter_append_basic(&it,T_STRING,&IF_PLAYER);
    A.iter_open_container(&it,T_ARRAY,"{sv}",&d);
    for(auto& n:props){ DBusMessageIter e; const char* k=n.c_str(); A.iter_open_container(&d,T_DICT_ENTRY,nullptr,&e); A.iter_append_basic(&e,T_STRING,&k); WriteValue(e,IF_PLAYER,n); A.iter_close_container(&d,&e); }
    A.iter_close_container(&it,&d);
    A.iter_open_container(&it,T_ARRAY,"s",&inv); A.iter_close_container(&it,&inv);
    A.send(g_conn,sig,nullptr); A.msg_unref(sig);
}
static void CheckChanges(){
    ULONGLONG now=GetTickCount64(); if(now-s_lastCheck<150) return; s_lastCheck=now;
    std::vector<std::string> ch;
    std::string st=Status(); if(st!=s_lastStatus){ s_lastStatus=st; ch.push_back("PlaybackStatus"); }
    std::wstring tp=HasTrack()?g_tracks[(size_t)g_current].path:L""; if(tp!=s_lastTrackPath){ s_lastTrackPath=tp; ch.push_back("Metadata"); }
    int v=VolPercent(); if(v!=s_lastVol){ s_lastVol=v; ch.push_back("Volume"); }
    if((int)g_cfg.shuffle!=s_lastShuffle){ s_lastShuffle=g_cfg.shuffle; ch.push_back("Shuffle"); }
    if((int)g_cfg.repeat!=s_lastRepeat){ s_lastRepeat=g_cfg.repeat; ch.push_back("LoopStatus"); }
    EmitChanged(ch);
}

// ---- API usada pelo main --------------------------------------------------------
static bool Start(){
    if(g_on) return true;
    if(g_failed) return false;                       // ja tentou e nao deu (sem libdbus/bus): nao insiste
    g_failed=true;
    if(std::getenv("REMIX_NO_MPRIS")) return false;
    bool dbg=std::getenv("REMIX_DEBUG")!=nullptr;
    if(!LoadApi()){ if(dbg) fprintf(stderr,"[remix] MPRIS: libdbus-1.so.3 nao encontrada\n"); return false; }
    DBusError err; A.error_init(&err);
    g_conn=A.bus_get(BUS_SESSION,&err);
    if(!g_conn||A.error_is_set(&err)){ if(dbg) fprintf(stderr,"[remix] MPRIS: sem bus de sessao (%s)\n",err.message?err.message:"?"); if(A.error_is_set(&err)) A.error_free(&err); g_conn=nullptr; return false; }
    A.set_exit_on_disconnect(g_conn,0);
    g_busName="org.mpris.MediaPlayer2.remix";
    int r=A.bus_request_name(g_conn,g_busName.c_str(),NAME_FLAG_DO_NOT_QUEUE,&err);
    if(A.error_is_set(&err)){ if(dbg) fprintf(stderr,"[remix] MPRIS: request_name falhou (%s)\n",err.message?err.message:"?"); A.error_free(&err); return false; }
    if(r!=REPLY_PRIMARY_OWNER){
        g_busName+=".instance"+std::to_string((long)getpid());
        r=A.bus_request_name(g_conn,g_busName.c_str(),NAME_FLAG_DO_NOT_QUEUE,&err);
        if(A.error_is_set(&err)){ A.error_free(&err); return false; }
        if(r!=REPLY_PRIMARY_OWNER) return false;
    }
    s_lastStatus=Status(); s_lastTrackPath=HasTrack()?g_tracks[(size_t)g_current].path:L""; s_lastVol=VolPercent(); s_lastShuffle=g_cfg.shuffle; s_lastRepeat=g_cfg.repeat;
    g_on=true; g_failed=false;
    if(std::getenv("REMIX_DEBUG")) fprintf(stderr,"[remix] MPRIS ligado como %s\n",g_busName.c_str());
    return true;
}
static void Poll(){
    if(!g_on) return;
    if(!A.read_write(g_conn,0)){ g_on=false; return; }
    for(int n=0;n<32;n++){
        DBusMessage* m=A.pop_message(g_conn); if(!m) break;
        if(A.msg_get_type(m)==MSG_METHOD_CALL) HandleCall(m);
        A.msg_unref(m);
    }
    CheckChanges();
    A.flush(g_conn);
}
static void Stop(){
    if(!g_on) return;
    DBusError err; A.error_init(&err);
    A.bus_release_name(g_conn,g_busName.c_str(),&err); if(A.error_is_set(&err)) A.error_free(&err);
    A.flush(g_conn); A.conn_unref(g_conn); g_conn=nullptr; g_on=false; g_failed=false;
}
static bool Active(){ return g_on; }

} // namespace mpris
