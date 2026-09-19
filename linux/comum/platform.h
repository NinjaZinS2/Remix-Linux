#pragma once
// Camada fina de plataforma para o NUCLEO portavel (config.h, theme.h,
// playlist.h). No Windows e o proprio <windows.h>. No Linux fornece os mesmos
// nomes que o nucleo usa (BYTE, DWORD, COLORREF, RGB, RECT...) para que o
// codigo compartilhado compile sem alteracoes. Regra: nenhum <windows.h>
// direto no nucleo — sempre por aqui.
#include <string>
#include <cstdint>
#include <cwchar>
#include <cwctype>

#ifdef _WIN32
    #include <winsock2.h>   // antes de windows.h: o Host (host_net.h) usa Winsock2
    #include <ws2tcpip.h>
    #include <windows.h>
    #define REMIX_SEP     L'\\'
    #define REMIX_SEP_STR L"\\"
#else
    #include <chrono>
    #include <thread>
    typedef uint8_t  BYTE;
    typedef uint16_t WORD;
    typedef uint32_t DWORD;
    typedef uint32_t UINT;
    typedef uint32_t UINT32;
    typedef int32_t  LONG;
    typedef int      BOOL;
    typedef unsigned long ULONG;
    typedef uint64_t ULONGLONG;
    typedef int64_t  LONGLONG;
    typedef uint32_t COLORREF;   // 0x00BBGGRR, igual ao Windows
    typedef wchar_t  WCHAR;
    #define RGB(r,g,b)   ((COLORREF)(((BYTE)(r)) | (((WORD)(BYTE)(g)) << 8) | (((DWORD)(BYTE)(b)) << 16)))
    #define GetRValue(c) ((BYTE)((c) & 0xFF))
    #define GetGValue(c) ((BYTE)(((c) >> 8) & 0xFF))
    #define GetBValue(c) ((BYTE)(((c) >> 16) & 0xFF))
    #ifndef MAX_PATH
    #define MAX_PATH 4096
    #endif
    struct RECT  { LONG left, top, right, bottom; };
    struct POINT { LONG x, y; };
    #define REMIX_SEP     L'/'
    #define REMIX_SEP_STR L"/"

    inline ULONGLONG GetTickCount64() {
        using namespace std::chrono;
        return (ULONGLONG)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
    inline DWORD GetTickCount() { return (DWORD)GetTickCount64(); }
    inline void Sleep(DWORD ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }
    #define _wcsicmp(a, b) wcscasecmp((a), (b))
    // Parse manual (sem wcstol): evita amarrar o binario a simbolos novos da
    // glibc (__isoc23_*), o que quebraria o .deb em distros mais antigas.
    inline int remix_wtoi(const wchar_t* s) {
        if (!s) return 0;
        while (*s == L' ' || *s == L'\t') ++s;
        bool neg = false;
        if (*s == L'-') { neg = true; ++s; } else if (*s == L'+') ++s;
        long v = 0;
        while (*s >= L'0' && *s <= L'9') { v = v * 10 + (*s - L'0'); if (v > 2147483647L) { v = 2147483647L; break; } ++s; }
        return (int)(neg ? -v : v);
    }
    #define _wtoi(s) remix_wtoi((s))
#endif

// Utilitarios de parse independentes de plataforma (mesmo motivo acima).
inline unsigned remix_parse_hex(const std::wstring& s, size_t start = 0, size_t count = std::wstring::npos) {
    unsigned v = 0;
    size_t end = (count == std::wstring::npos) ? s.size() : std::min(s.size(), start + count);
    for (size_t i = start; i < end; ++i) {
        wchar_t c = s[i]; unsigned d;
        if (c >= L'0' && c <= L'9') d = (unsigned)(c - L'0');
        else if (c >= L'a' && c <= L'f') d = (unsigned)(c - L'a' + 10);
        else if (c >= L'A' && c <= L'F') d = (unsigned)(c - L'A' + 10);
        else break;
        v = (v << 4) | d;
    }
    return v;
}
inline int remix_parse_int(const std::wstring& s) {
    size_t i = 0; while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    bool neg = false; if (i < s.size() && (s[i] == L'-' || s[i] == L'+')) { neg = s[i] == L'-'; ++i; }
    long v = 0; while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') { v = v * 10 + (s[i] - L'0'); if (v > 2147483647L) break; ++i; }
    return (int)(neg ? -v : v);
}

// Log de diagnostico: no Windows vai para o remix-log.txt (ao lado do app ou em
// %LOCALAPPDATA%\Remix); no Linux para o stderr com REMIX_DEBUG=1. Cada casca implementa.
void PlatformLog(const char* msg);
#include <cstdio>
#include <exception>
// Roda f() registrando no log qualquer excecao C++ que escapar, em vez de derrubar o app
// (excecao que sai de uma thread chama std::terminate). Usado na entrada das threads.
template <class F> inline void RemixSafe(const char* where, F&& f) {
    try { f(); }
    catch (const std::exception& e) { char b[640]; snprintf(b, sizeof b, "aviso: excecao em %s: %s", where, e.what()); PlatformLog(b); }
    catch (...) { char b[240]; snprintf(b, sizeof b, "aviso: excecao desconhecida em %s", where); PlatformLog(b); }
}
