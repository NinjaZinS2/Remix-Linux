#pragma once
// Teclas e atalhos configuraveis (compartilhado Windows/Linux).
// KeyCode e independente de plataforma: cada casca traduz VK_* / raylib KEY_* para ele.
// Cada acao (HotkeyAction) tem UMA combinacao (tecla + Ctrl/Shift/Alt) e um escopo:
// FOCO (so com a janela ativa; padrao, nao atrapalha jogos) ou GLOBAL (o sistema entrega
// mesmo com outro programa na frente / app em segundo plano).
#include <string>
#include <vector>
#include <cwctype>
#include <cwchar>

enum KeyCode : int {
    KC_NONE=0,
    // letras 'A'..'Z' (65..90) e digitos '0'..'9' (48..57) usam o proprio ASCII
    KC_SPACE=256, KC_LEFT, KC_RIGHT, KC_UP, KC_DOWN, KC_ENTER, KC_ESC, KC_BACKSPACE, KC_DELETE, KC_TAB, KC_INSERT, KC_HOME, KC_END, KC_PAGEUP, KC_PAGEDOWN,
    KC_F1, KC_F2, KC_F3, KC_F4, KC_F5, KC_F6, KC_F7, KC_F8, KC_F9, KC_F10, KC_F11, KC_F12,
    KC_EQUAL, KC_MINUS, KC_COMMA, KC_PERIOD, KC_SEMICOLON, KC_APOSTROPHE, KC_SLASH, KC_BACKSLASH, KC_LBRACKET, KC_RBRACKET, KC_GRAVE,
    KC_KP0, KC_KP1, KC_KP2, KC_KP3, KC_KP4, KC_KP5, KC_KP6, KC_KP7, KC_KP8, KC_KP9, KC_KPADD, KC_KPSUB, KC_KPMUL, KC_KPDIV, KC_KPDECIMAL, KC_KPENTER,
    KC_MEDIA_PLAY, KC_MEDIA_NEXT, KC_MEDIA_PREV, KC_MEDIA_STOP, KC_VOL_UP, KC_VOL_DOWN, KC_VOL_MUTE, KC_PAUSE, KC_PRINT, KC_SCROLL, KC_NUMLOCK, KC_CAPSLOCK,
    KC_COUNT
};
enum { KM_CTRL=1, KM_SHIFT=2, KM_ALT=4 };
struct Hotkey { int key=0; int mods=0; bool global=false; };
enum HotkeyAction : int { HK_PLAYPAUSE=0, HK_NEXT, HK_PREV, HK_VOLUP, HK_VOLDOWN, HK_MUTE, HK_RESTART, HK_SHUFFLE, HK_REPEAT, HK_SEARCH, HK_DELETE, HK_RENAME, HK_MOVEUP, HK_MOVEDOWN, HK_CLOSE, HK_QUIT, HK_SHOWHIDE, HK_COUNT };

struct KeyNameEntry { int kc; const wchar_t* id; const wchar_t* label; };
static const KeyNameEntry g_keyNames[] = {
    {KC_SPACE,L"Space",L"Espaço"},{KC_LEFT,L"Left",L"←"},{KC_RIGHT,L"Right",L"→"},{KC_UP,L"Up",L"↑"},{KC_DOWN,L"Down",L"↓"},
    {KC_ENTER,L"Enter",L"Enter"},{KC_ESC,L"Esc",L"Esc"},{KC_BACKSPACE,L"Backspace",L"Backspace"},{KC_DELETE,L"Delete",L"Del"},{KC_TAB,L"Tab",L"Tab"},
    {KC_INSERT,L"Insert",L"Ins"},{KC_HOME,L"Home",L"Home"},{KC_END,L"End",L"End"},{KC_PAGEUP,L"PageUp",L"PgUp"},{KC_PAGEDOWN,L"PageDown",L"PgDn"},
    {KC_F1,L"F1",L"F1"},{KC_F2,L"F2",L"F2"},{KC_F3,L"F3",L"F3"},{KC_F4,L"F4",L"F4"},{KC_F5,L"F5",L"F5"},{KC_F6,L"F6",L"F6"},
    {KC_F7,L"F7",L"F7"},{KC_F8,L"F8",L"F8"},{KC_F9,L"F9",L"F9"},{KC_F10,L"F10",L"F10"},{KC_F11,L"F11",L"F11"},{KC_F12,L"F12",L"F12"},
    {KC_EQUAL,L"Equal",L"="},{KC_MINUS,L"Minus",L"-"},{KC_COMMA,L"Comma",L","},{KC_PERIOD,L"Period",L"."},{KC_SEMICOLON,L"Semicolon",L";"},
    {KC_APOSTROPHE,L"Apostrophe",L"'"},{KC_SLASH,L"Slash",L"/"},{KC_BACKSLASH,L"Backslash",L"\\"},{KC_LBRACKET,L"LBracket",L"["},{KC_RBRACKET,L"RBracket",L"]"},{KC_GRAVE,L"Grave",L"`"},
    {KC_KP0,L"KP0",L"Num 0"},{KC_KP1,L"KP1",L"Num 1"},{KC_KP2,L"KP2",L"Num 2"},{KC_KP3,L"KP3",L"Num 3"},{KC_KP4,L"KP4",L"Num 4"},
    {KC_KP5,L"KP5",L"Num 5"},{KC_KP6,L"KP6",L"Num 6"},{KC_KP7,L"KP7",L"Num 7"},{KC_KP8,L"KP8",L"Num 8"},{KC_KP9,L"KP9",L"Num 9"},
    {KC_KPADD,L"KPAdd",L"Num +"},{KC_KPSUB,L"KPSub",L"Num -"},{KC_KPMUL,L"KPMul",L"Num *"},{KC_KPDIV,L"KPDiv",L"Num /"},{KC_KPDECIMAL,L"KPDecimal",L"Num ."},{KC_KPENTER,L"KPEnter",L"Num Enter"},
    {KC_MEDIA_PLAY,L"MediaPlay",L"Mídia Play"},{KC_MEDIA_NEXT,L"MediaNext",L"Mídia Próx."},{KC_MEDIA_PREV,L"MediaPrev",L"Mídia Ant."},{KC_MEDIA_STOP,L"MediaStop",L"Mídia Stop"},
    {KC_VOL_UP,L"VolUp",L"Vol +"},{KC_VOL_DOWN,L"VolDown",L"Vol -"},{KC_VOL_MUTE,L"VolMute",L"Vol Mudo"},
    {KC_PAUSE,L"Pause",L"Pause"},{KC_PRINT,L"Print",L"Print"},{KC_SCROLL,L"ScrollLock",L"ScrLk"},{KC_NUMLOCK,L"NumLock",L"NumLk"},{KC_CAPSLOCK,L"CapsLock",L"CapsLk"},
};
static bool KeyStrEqI(const wchar_t* a,const wchar_t* b){ while(*a&&*b){ if(towlower(*a)!=towlower(*b)) return false; ++a; ++b; } return *a==0&&*b==0; }
static std::wstring KeyId(int kc){ if((kc>='A'&&kc<='Z')||(kc>='0'&&kc<='9')) return std::wstring(1,(wchar_t)kc); for(const auto& e:g_keyNames) if(e.kc==kc) return e.id; return kc>0?L"K"+std::to_wstring(kc):L""; }
static std::wstring KeyLabel(int kc){ if((kc>='A'&&kc<='Z')||(kc>='0'&&kc<='9')) return std::wstring(1,(wchar_t)kc); for(const auto& e:g_keyNames) if(e.kc==kc) return e.label; return kc>0?L"K"+std::to_wstring(kc):L"—"; }
static int ParseKeyId(std::wstring s){
    while(!s.empty()&&iswspace(s.back())) s.pop_back();
    while(!s.empty()&&iswspace(s.front())) s.erase(s.begin());
    if(s.empty()) return 0;
    if(s.size()==1){ wchar_t c=(wchar_t)towupper(s[0]); if((c>='A'&&c<='Z')||(c>='0'&&c<='9')) return (int)c; }
    for(const auto& e:g_keyNames) if(KeyStrEqI(e.id,s.c_str())||wcscmp(e.label,s.c_str())==0) return e.kc;
    if(KeyStrEqI(s.c_str(),L"del")) return KC_DELETE; if(KeyStrEqI(s.c_str(),L"escape")) return KC_ESC; if(KeyStrEqI(s.c_str(),L"return")) return KC_ENTER;
    if(KeyStrEqI(s.c_str(),L"espaco")||KeyStrEqI(s.c_str(),L"espaço")) return KC_SPACE; if(KeyStrEqI(s.c_str(),L"pgup")) return KC_PAGEUP; if(KeyStrEqI(s.c_str(),L"pgdn")) return KC_PAGEDOWN;
    if(s.size()>1&&(s[0]==L'K'||s[0]==L'k')){ int v=_wtoi(s.c_str()+1); if(v>0) return v; }
    return 0;
}
static std::wstring ModsPrefix(int mods){ std::wstring s; if(mods&KM_CTRL) s+=L"Ctrl+"; if(mods&KM_SHIFT) s+=L"Shift+"; if(mods&KM_ALT) s+=L"Alt+"; return s; }
static std::wstring HotkeyLabel(const Hotkey& h){ if(!h.key) return L"—"; return ModsPrefix(h.mods)+KeyLabel(h.key); }
static std::wstring HotkeyToString(const Hotkey& h){ return ModsPrefix(h.mods)+(h.key?KeyId(h.key):L"None")+(h.global?L"|global":L"|foco"); }
static bool ParseHotkey(std::wstring s,Hotkey& out){
    Hotkey h; size_t bar=s.find(L'|'); std::wstring scope; if(bar!=std::wstring::npos){ scope=s.substr(bar+1); s=s.substr(0,bar); }
    h.global=KeyStrEqI(scope.c_str(),L"global");
    std::wstring keyPart=s;
    for(;;){ size_t p=keyPart.find(L'+'); if(p==std::wstring::npos||p+1>=keyPart.size()) break; std::wstring tok=keyPart.substr(0,p);
        if(KeyStrEqI(tok.c_str(),L"ctrl")) h.mods|=KM_CTRL; else if(KeyStrEqI(tok.c_str(),L"shift")) h.mods|=KM_SHIFT; else if(KeyStrEqI(tok.c_str(),L"alt")) h.mods|=KM_ALT; else break;
        keyPart=keyPart.substr(p+1); }
    h.key=KeyStrEqI(keyPart.c_str(),L"none")?0:ParseKeyId(keyPart);
    out=h; return true;
}
static const wchar_t* HkId(int a){ static const wchar_t* ids[HK_COUNT]={L"playpause",L"next",L"prev",L"volup",L"voldown",L"mute",L"restart",L"shuffle",L"repeat",L"search",L"delete",L"rename",L"moveup",L"movedown",L"close",L"quit",L"showhide"}; return (a>=0&&a<HK_COUNT)?ids[a]:L""; }
static const wchar_t* HkLabel(int a){ static const wchar_t* l[HK_COUNT]={L"Tocar / pausar",L"Próxima faixa",L"Faixa anterior",L"Volume +",L"Volume -",L"Mudo",L"Reiniciar a faixa",L"Aleatório (liga/desliga)",L"Repetir (liga/desliga)",L"Buscar faixa",L"Excluir (lixeira)",L"Renomear arquivo",L"Mover p/ cima (ordem manual)",L"Mover p/ baixo (ordem manual)",L"Fechar painéis / cancelar",L"Sair do Remix",L"Mostrar / esconder janela"}; return (a>=0&&a<HK_COUNT)?l[a]:L""; }
static int HkIndexById(const std::wstring& id){ for(int a=0;a<HK_COUNT;a++) if(KeyStrEqI(HkId(a),id.c_str())) return a; return -1; }
// Padrao: combinacoes de duas teclas (como no Spotify). Teclas soltas como Espaco, S e as setas
// disparavam sem querer. Ficam soltas so F2 (renomear) e Esc (fechar), que ninguem aperta a toa.
static Hotkey HkDefault(int a){
    Hotkey h;
    switch(a){
    case HK_PLAYPAUSE: h.key=KC_SPACE; h.mods=KM_CTRL; break;
    case HK_NEXT: h.key=KC_RIGHT; h.mods=KM_CTRL; break; case HK_PREV: h.key=KC_LEFT; h.mods=KM_CTRL; break;
    case HK_VOLUP: h.key=KC_UP; h.mods=KM_CTRL; break; case HK_VOLDOWN: h.key=KC_DOWN; h.mods=KM_CTRL; break;
    case HK_MUTE: h.key='M'; h.mods=KM_CTRL; break; case HK_RESTART: h.key='0'; h.mods=KM_CTRL; break;
    case HK_SHUFFLE: h.key='S'; h.mods=KM_CTRL; break; case HK_REPEAT: h.key='R'; h.mods=KM_CTRL; break;
    case HK_SEARCH: h.key='F'; h.mods=KM_CTRL; break;
    case HK_DELETE: h.key=KC_DELETE; h.mods=KM_CTRL; break; case HK_RENAME: h.key=KC_F2; break;
    case HK_MOVEUP: h.key=KC_UP; h.mods=KM_ALT; break; case HK_MOVEDOWN: h.key=KC_DOWN; h.mods=KM_ALT; break;
    case HK_CLOSE: h.key=KC_ESC; break; case HK_QUIT: h.key='Q'; h.mods=KM_CTRL; break; case HK_SHOWHIDE: h.key=0; break;
    default: break;
    }
    return h;
}
// Padrao antigo (teclas soltas, ate a 1.2.0 publicada): so para migrar quem nunca mexeu nos atalhos.
static Hotkey HkDefaultAntigo(int a){
    Hotkey h;
    switch(a){
    case HK_PLAYPAUSE: h.key=KC_SPACE; break; case HK_NEXT: h.key=KC_RIGHT; break; case HK_PREV: h.key=KC_LEFT; break;
    case HK_VOLUP: h.key=KC_UP; break; case HK_VOLDOWN: h.key=KC_DOWN; break; case HK_MUTE: h.key='M'; break; case HK_RESTART: h.key='R'; break;
    case HK_SHUFFLE: h.key='S'; break; case HK_REPEAT: h.key='L'; break; case HK_SEARCH: h.key='F'; h.mods=KM_CTRL; break;
    case HK_DELETE: h.key=KC_DELETE; break; case HK_RENAME: h.key=KC_F2; break;
    case HK_MOVEUP: h.key=KC_UP; h.mods=KM_CTRL; break; case HK_MOVEDOWN: h.key=KC_DOWN; h.mods=KM_CTRL; break;
    case HK_CLOSE: h.key=KC_ESC; break; case HK_QUIT: h.key='Q'; h.mods=KM_CTRL; break; case HK_SHOWHIDE: h.key=0; break;
    default: break;
    }
    return h;
}
