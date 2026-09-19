#pragma once
// RICH PRESENCE do Discord: mostra no seu perfil o que esta tocando no Remix.
//
// Nao usa biblioteca nenhuma: o proprio Discord abre um "soquete" local e fala
// um protocolo simples (4 bytes de opcode + 4 bytes de tamanho + JSON).
//   Linux  : $XDG_RUNTIME_DIR/discord-ipc-0 (e as pastas do Flatpak e do Snap)
//   Windows: \\.\pipe\discord-ipc-0
// Precisa de um "Application ID" (Configuracoes > SOUNDPAD E DISCORD, ou
// RpcAppId no config.ini): e so criar um aplicativo em
// https://discord.com/developers/applications e copiar o Application ID. O nome
// que aparece no perfil e o nome desse aplicativo.
//
// Nada e enviado para lugar nenhum alem do proprio Discord que ja esta aberto no
// PC; com o campo vazio, nem soquete e aberto.
#include "platform.h"
#include "config.h"
#include <string>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>
#include <cstring>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

namespace drpc {

struct Estado {
    std::mutex m;
    std::string appId;                 // vazio = desligado
    std::wstring titulo, artista;
    bool tocando = false;
    int posSec = 0, durSec = 0;
    bool mudou = false;                // tem coisa nova para mandar
    std::atomic<bool> rodando{ false };
    std::atomic<bool> ligado{ false }; // conectado ao Discord agora
    std::atomic<bool> parar{ false };
    std::wstring erro;
};
inline Estado& St() { static Estado* e = new Estado(); return *e; }

// ------------------------------------------------------------ soquete -----
#ifdef _WIN32
using Sock = HANDLE;
inline Sock SockInvalido() { return INVALID_HANDLE_VALUE; }
inline bool SockOk(Sock s) { return s != INVALID_HANDLE_VALUE; }
inline void Fechar(Sock& s) { if (SockOk(s)) { CloseHandle(s); s = INVALID_HANDLE_VALUE; } }
inline Sock Conectar() {
    for (int i = 0; i < 10; i++) {
        wchar_t nome[64]; swprintf(nome, 64, L"\\\\.\\pipe\\discord-ipc-%d", i);
        HANDLE h = CreateFileW(nome, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
    }
    return INVALID_HANDLE_VALUE;
}
inline bool Escrever(Sock s, const char* p, size_t n) {
    DWORD w = 0;
    while (n) { if (!WriteFile(s, p, (DWORD)n, &w, nullptr) || w == 0) return false; p += w; n -= w; }
    return true;
}
inline bool Ler(Sock s, char* p, size_t n) {
    DWORD r = 0;
    while (n) { if (!ReadFile(s, p, (DWORD)n, &r, nullptr) || r == 0) return false; p += r; n -= r; }
    return true;
}
#else
using Sock = int;
inline Sock SockInvalido() { return -1; }
inline bool SockOk(Sock s) { return s >= 0; }
inline void Fechar(Sock& s) { if (s >= 0) { close(s); s = -1; } }
// O soquete pode estar na pasta de runtime, no /tmp e dentro das pastas do
// Flatpak/Snap: o Remix tenta todas, entao funciona em qualquer distro.
inline std::vector<std::string> Pastas() {
    std::vector<std::string> v;
    const char* xdg = getenv("XDG_RUNTIME_DIR");
    const char* tmpdir = getenv("TMPDIR");
    std::vector<std::string> bases;
    if (xdg && *xdg) bases.push_back(xdg);
    if (tmpdir && *tmpdir) bases.push_back(tmpdir);
    bases.push_back("/tmp");
    static const char* sub[] = { "", "/app/com.discordapp.Discord", "/app/com.discordapp.DiscordCanary",
                                 "/app/com.discordapp.DiscordPTB", "/snap.discord", "/snap.discord-canary" };
    for (auto& b : bases) for (const char* s : sub) v.push_back(b + s);
    return v;
}
inline Sock Conectar() {
    for (auto& dir : Pastas()) {
        for (int i = 0; i < 10; i++) {
            std::string caminho = dir + "/discord-ipc-" + std::to_string(i);
            if (caminho.size() + 1 > sizeof(((sockaddr_un*)nullptr)->sun_path)) continue;
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (fd < 0) continue;
            sockaddr_un a{}; a.sun_family = AF_UNIX;
            memcpy(a.sun_path, caminho.c_str(), caminho.size() + 1);
            if (connect(fd, (sockaddr*)&a, (socklen_t)(offsetof(sockaddr_un, sun_path) + caminho.size() + 1)) == 0) return fd;
            close(fd);
        }
    }
    return -1;
}
inline bool Escrever(Sock s, const char* p, size_t n) {
    while (n) {
        ssize_t w = send(s, p, n, MSG_NOSIGNAL);
        if (w <= 0) { if (errno == EINTR) continue; return false; }
        p += w; n -= (size_t)w;
    }
    return true;
}
inline bool Ler(Sock s, char* p, size_t n) {
    while (n) {
        ssize_t r = recv(s, p, n, 0);
        if (r <= 0) { if (errno == EINTR) continue; return false; }
        p += r; n -= (size_t)r;
    }
    return true;
}
#endif

inline void Le32(char* p, unsigned v) { p[0] = (char)(v & 0xFF); p[1] = (char)((v >> 8) & 0xFF); p[2] = (char)((v >> 16) & 0xFF); p[3] = (char)((v >> 24) & 0xFF); }
inline unsigned De32(const char* p) { return (unsigned char)p[0] | ((unsigned)(unsigned char)p[1] << 8) | ((unsigned)(unsigned char)p[2] << 16) | ((unsigned)(unsigned char)p[3] << 24); }

inline bool Enviar(Sock s, unsigned op, const std::string& corpo) {
    std::string quadro(8, '\0');
    Le32(&quadro[0], op); Le32(&quadro[4], (unsigned)corpo.size());
    return Escrever(s, quadro.data(), 8) && Escrever(s, corpo.data(), corpo.size());
}
inline bool Receber(Sock s, unsigned& op, std::string& corpo) {
    char cab[8];
    if (!Ler(s, cab, 8)) return false;
    op = De32(cab); unsigned n = De32(cab + 4);
    if (n > 1u << 20) return false;
    corpo.assign(n, '\0');
    return n == 0 || Ler(s, &corpo[0], n);
}
inline std::string JEsc(const std::string& s) {
    std::string o;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { o.push_back('\\'); o.push_back((char)c); }
        else if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\t') o += "\\t";
        else if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; }
        else o.push_back((char)c);
    }
    return o;
}
// O Discord corta textos com menos de 2 ou mais de 128 caracteres.
inline std::string Campo(const std::wstring& w, const char* padrao) {
    std::string s = WideToUtf8(w);
    if (s.size() > 120) { s.resize(120); while (!s.empty() && (s.back() & 0xC0) == 0x80) s.pop_back(); s += "..."; }
    if (s.size() < 2) s = padrao;
    return s;
}

// -------------------------------------------------------------- ciclo -----
inline std::string MontarAtividade(const Estado& e, long long agora) {
    std::string o = "{\"cmd\":\"SET_ACTIVITY\",\"nonce\":\"remix\",\"args\":{\"pid\":";
#ifdef _WIN32
    o += std::to_string((long long)GetCurrentProcessId());
#else
    o += std::to_string((long long)getpid());
#endif
    o += ",\"activity\":{\"type\":2";   // 2 = "Ouvindo"
    o += ",\"details\":\"" + JEsc(Campo(e.titulo, "Remix")) + "\"";
    o += ",\"state\":\"" + JEsc(Campo(e.artista, "no Remix")) + "\"";
    if (e.tocando && e.durSec > 0) {
        long long ini = agora - e.posSec, fim = ini + e.durSec;
        o += ",\"timestamps\":{\"start\":" + std::to_string(ini) + ",\"end\":" + std::to_string(fim) + "}";
    }
    o += ",\"assets\":{\"large_image\":\"remix\",\"large_text\":\"Remix Player\"}";
    o += "}}}";
    return o;
}
inline void Thread() {
    Sock s = SockInvalido();
    std::string idConectado;
    long long ultimoEnvio = 0;
    std::string ultimo;
    Estado& e = St();
    while (!e.parar.load()) {
        std::string appId; bool mudou;
        { std::lock_guard<std::mutex> lk(e.m); appId = e.appId; mudou = e.mudou; }
        if (appId.empty()) {
            if (SockOk(s)) { Fechar(s); e.ligado.store(false); }
            Sleep(1000); continue;
        }
        if (SockOk(s) && idConectado != appId) { Fechar(s); e.ligado.store(false); }
        if (!SockOk(s)) {
            s = Conectar();
            if (!SockOk(s)) { Sleep(8000); continue; }   // Discord fechado: tenta de novo daqui a pouco
            if (!Enviar(s, 0, "{\"v\":1,\"client_id\":\"" + JEsc(appId) + "\"}")) { Fechar(s); Sleep(5000); continue; }
            unsigned op = 0; std::string resp;
            if (!Receber(s, op, resp) || op == 2) {   // 2 = CLOSE (id invalido, por exemplo)
                { std::lock_guard<std::mutex> lk(e.m); e.erro = resp.find("Invalid client") != std::string::npos ? L"O Discord não conhece esse Application ID." : L""; }
                Fechar(s); Sleep(15000); continue;
            }
            idConectado = appId; ultimo.clear(); ultimoEnvio = 0;
            e.ligado.store(true);
            { std::lock_guard<std::mutex> lk(e.m); e.erro.clear(); }
        }
        long long agora = (long long)time(nullptr);
        std::string corpo;
        {
            std::lock_guard<std::mutex> lk(e.m);
            corpo = MontarAtividade(e, agora);
            e.mudou = false;
        }
        // O Discord aceita poucas atualizacoes por minuto: so manda quando muda
        // de verdade (ou de 15 em 15 s, para o tempo nao ficar parado).
        bool precisa = mudou || corpo != ultimo || agora - ultimoEnvio >= 15;
        if (precisa) {
            if (!Enviar(s, 1, corpo)) { Fechar(s); e.ligado.store(false); Sleep(3000); continue; }
            unsigned op = 0; std::string resp;
            if (!Receber(s, op, resp) || op == 2) { Fechar(s); e.ligado.store(false); Sleep(3000); continue; }
            ultimo = corpo; ultimoEnvio = agora;
        }
        Sleep(1000);
    }
    if (SockOk(s)) Fechar(s);
    e.ligado.store(false);
    e.rodando.store(false);
}
inline void Garantir() {
    Estado& e = St();
    if (e.rodando.exchange(true)) return;
    e.parar.store(false);
    std::thread([] { RemixSafe("discord rich presence", [] { Thread(); }); }).detach();
}
// Liga/desliga (appId vazio = desligado).
inline void SetAppId(const std::wstring& id) {
    Estado& e = St();
    std::string s;
    for (wchar_t c : id) if (c >= L'0' && c <= L'9') s.push_back((char)c);   // o Application ID e so numero
    { std::lock_guard<std::mutex> lk(e.m); if (e.appId == s) return; e.appId = s; e.mudou = true; e.erro.clear(); }
    if (!s.empty()) Garantir();
}
// Chamado pelo player quando a musica ou o estado muda (barato: so copia).
inline void Atualizar(const std::wstring& titulo, const std::wstring& artista, bool tocando, int posSec, int durSec) {
    Estado& e = St();
    std::lock_guard<std::mutex> lk(e.m);
    if (e.appId.empty()) return;
    bool dif = e.titulo != titulo || e.artista != artista || e.tocando != tocando || e.durSec != durSec ||
               (tocando && std::abs(e.posSec - posSec) > 4);
    e.titulo = titulo; e.artista = artista; e.tocando = tocando; e.posSec = posSec; e.durSec = durSec;
    if (dif) e.mudou = true;
}
inline void Parar() { Estado& e = St(); e.parar.store(true); }
inline bool Ligado() { return St().ligado.load(); }
inline std::wstring Erro() { Estado& e = St(); std::lock_guard<std::mutex> lk(e.m); return e.erro; }

} // namespace drpc
