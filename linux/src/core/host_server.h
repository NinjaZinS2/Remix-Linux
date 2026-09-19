#pragma once
// Host: o Remix do PC vira um servidor para o celular (docs/HOST.md).
//
//   - Servidor HTTP/1.1 proprio (host_net.h), uma thread por conexao, limite de conexoes,
//     timeouts, limite de tamanho e de taxa por origem. So aceita loopback (tunel) e rede
//     local (IPv4 privado; IPv6 so se a pessoa ligar).
//   - Vinculo: PIN + aceite no PC, ou QR code (token de uso unico que so aparece na tela do
//     PC; confirmacao no PC opcional). Token aleatorio de 256 bits em cookie HttpOnly/Strict.
//   - Autorizacao por aparelho: o celular so ve o que o PC liberou (biblioteca inteira por
//     aparelho, playlists hosteadas, playlists de outros aparelhos so se o dono compartilhar
//     E o PC liberar). O celular so recebe ids opacos: nenhum caminho sai ou entra.
//   - Online no celular: quem busca e converte e o PC (CLI de midia + ffmpeg); o celular recebe
//     so o audio. Ele nunca fala com YouTube/SoundCloud (nem para as capas).
//   - As threads do servidor NUNCA tocam g_tracks/g_playlists: leem uma copia (Publish)
//     feita pela thread da UI. O servidor avisa a UI por eventos (EV_HOST_PEDIDO, EV_HOST_STATUS).
//   - Tunel Cloudflare (cloudflared, "quick tunnel"): HTTPS ate o celular sem abrir porta,
//     atravessa CGNAT e nao entrega IP nem localizacao; o link so aparece depois de testado.
#include "host_net.h"
#include "host_web.h"
#include "descobrir.h"   // novidades e busca de playlists/albuns para o celular
#include "letras.h"      // letra sincronizada para o celular
#include "qrcode.h"
#include "stems.h"
#include "app_proc.h"
#include <thread>
#include <mutex>
#include <atomic>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include <string>
#include <chrono>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <functional>
#ifndef _WIN32
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

namespace host {

// ------------------------------------------------------------ utilidades --
inline long long NowSec() { return (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count(); }
inline bool RandomBytes(unsigned char* b, size_t n) {
#ifdef _WIN32
    typedef BOOLEAN(WINAPI* Fn)(PVOID, ULONG);
    static Fn fn = nullptr;
    if (!fn) { HMODULE h = LoadLibraryW(L"advapi32.dll"); if (h) fn = (Fn)GetProcAddress(h, "SystemFunction036"); }
    if (fn && fn(b, (ULONG)n)) return true;
#else
    int fd = open("/dev/urandom", O_RDONLY);
    if (fd >= 0) { size_t got = 0; while (got < n) { ssize_t r = read(fd, b + got, n - got); if (r <= 0) break; got += (size_t)r; } close(fd); if (got == n) return true; }
#endif
    // ultimo recurso (nao deveria acontecer): mistura relogio + endereco
    unsigned long long x = (unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count() ^ (unsigned long long)(uintptr_t)b;
    for (size_t i = 0; i < n; i++) { x ^= x << 13; x ^= x >> 7; x ^= x << 17; b[i] = (unsigned char)(x & 0xFF); }
    return false;
}
inline std::string RandomHex(size_t bytes) {
    std::vector<unsigned char> b(bytes); RandomBytes(b.data(), bytes);
    static const char* hx = "0123456789abcdef"; std::string s; s.reserve(bytes * 2);
    for (unsigned char c : b) { s.push_back(hx[c >> 4]); s.push_back(hx[c & 15]); }
    return s;
}
inline std::string IdFor(const std::wstring& path) {   // FNV-1a 64 do caminho em UTF-8 -> 16 hex
    std::string u = WideToUtf8(path);
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : u) { h ^= c; h *= 1099511628211ULL; }
    char b[24]; snprintf(b, sizeof b, "%016llx", h); return b;
}
inline bool IsId(const std::string& s) { if (s.size() != 16) return false; for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; return true; }
inline bool IsHex(const std::string& s, size_t n) { if (s.size() != n) return false; for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false; return true; }
inline bool DigitsOnly(const std::string& s, size_t mn, size_t mx) { if (s.size() < mn || s.size() > mx) return false; for (char c : s) if (c < '0' || c > '9') return false; return true; }
inline bool ConstEq(const std::string& a, const std::string& b) { if (a.size() != b.size()) return false; unsigned d = 0; for (size_t i = 0; i < a.size(); i++) d |= (unsigned)(a[i] ^ b[i]); return d == 0; }
inline std::string JsonEsc(const std::string& s) {   // tambem escapa < > & (a pagina nunca usa innerHTML, mas custa nada)
    std::string o; o.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
        case '"': o += "\\\""; break; case '\\': o += "\\\\"; break; case '\n': o += "\\n"; break; case '\r': o += "\\r"; break; case '\t': o += "\\t"; break;
        case '<': o += "\\u003c"; break; case '>': o += "\\u003e"; break; case '&': o += "\\u0026"; break;
        default: if (c < 0x20) { char b[8]; snprintf(b, sizeof b, "\\u%04x", c); o += b; } else o.push_back((char)c);
        }
    }
    return o;
}
inline std::string JStrA(const std::string& utf8) { return "\"" + JsonEsc(utf8) + "\""; }
inline std::string JStr(const std::wstring& w) { return JStrA(WideToUtf8(w)); }
// Leitor de JSON "raso": {"chave":"valor","n":123,"b":true}. Devolve o valor como texto
// (string sem aspas e sem escapes basicos). Suficiente para os pedidos do celular.
inline std::string JGet(const std::string& j, const std::string& key) {
    std::string k = "\"" + key + "\"";
    size_t p = j.find(k); if (p == std::string::npos) return "";
    p = j.find(':', p + k.size()); if (p == std::string::npos) return "";
    p++; while (p < j.size() && (j[p] == ' ' || j[p] == '\t')) p++;
    if (p >= j.size()) return "";
    if (j[p] == '"') {
        std::string o; p++;
        while (p < j.size() && j[p] != '"') {
            if (j[p] == '\\' && p + 1 < j.size()) {
                char e = j[p + 1];
                if (e == 'u' && p + 5 < j.size()) {   // \uXXXX -> UTF-8
                    unsigned cp = (unsigned)strtoul(j.substr(p + 2, 4).c_str(), nullptr, 16);
                    if (cp < 0x80) o.push_back((char)cp); else if (cp < 0x800) { o.push_back((char)(0xC0 | (cp >> 6))); o.push_back((char)(0x80 | (cp & 0x3F))); }
                    else { o.push_back((char)(0xE0 | (cp >> 12))); o.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); o.push_back((char)(0x80 | (cp & 0x3F))); }
                    p += 6; continue;
                }
                o.push_back(e == 'n' ? '\n' : e == 't' ? '\t' : e == 'r' ? '\r' : e); p += 2; continue;
            }
            o.push_back(j[p++]);
        }
        return o;
    }
    size_t e = p; while (e < j.size() && j[e] != ',' && j[e] != '}' && j[e] != ' ') e++;
    return j.substr(p, e - p);
}
inline std::string UrlDecode(const std::string& s) {
    std::string o; o.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1]) && isxdigit((unsigned char)s[i + 2])) { o.push_back((char)strtoul(s.substr(i + 1, 2).c_str(), nullptr, 16)); i += 2; }
        else if (s[i] == '+') o.push_back(' ');
        else o.push_back(s[i]);
    }
    return o;
}
// Nome vindo do celular: sem controle, sem excesso, aparado.
inline std::string CleanName(const std::string& in, size_t maxLen) {
    std::string o;
    for (unsigned char c : in) { if (c < 0x20 || c == 0x7F) continue; o.push_back((char)c); if (o.size() >= maxLen) break; }
    while (!o.empty() && o.back() == ' ') o.pop_back();
    size_t b = 0; while (b < o.size() && o[b] == ' ') b++;
    return o.substr(b);
}
inline std::string Esc(const std::string& s) { std::string o; for (char c : s) { if (c == '%') o += "%25"; else if (c == '+') o += "%2B"; else if (c == '|') o += "%7C"; else if (c == '\n') o += "%0A"; else if (c == '\r') continue; else o.push_back(c); } return o; }
inline std::string Unesc(const std::string& s) { return UrlDecode(s); }
inline std::string ContentTypeFor(const std::wstring& path) {
    std::wstring e = std::filesystem::path(path).extension().wstring(); for (auto& c : e) c = (wchar_t)towlower(c);
    if (e == L".mp3") return "audio/mpeg"; if (e == L".m4a" || e == L".mp4" || e == L".aac") return "audio/mp4";
    if (e == L".ogg" || e == L".oga" || e == L".opus") return "audio/ogg"; if (e == L".flac") return "audio/flac";
    if (e == L".wav") return "audio/wav"; if (e == L".wma") return "audio/x-ms-wma"; if (e == L".webm") return "audio/webm";
    if (e == L".jpg" || e == L".jpeg") return "image/jpeg"; if (e == L".png") return "image/png"; if (e == L".webp") return "image/webp";
    return "application/octet-stream";
}
inline std::string HtmlEsc(const std::string& s) { std::string o; for (char c : s) { if (c == '<') o += "&lt;"; else if (c == '>') o += "&gt;"; else if (c == '&') o += "&amp;"; else if (c == '"') o += "&quot;"; else o.push_back(c); } return o; }
inline bool AllowedThumbUrl(const std::wstring& w) { return ThumbUrlAllowed(w); }   // so https, sem usuario/porta, so CDNs conhecidos (online_resolve.h)

// Capa das novidades (tela inicial do celular): o CSP da pagina so deixa carregar
// imagem do proprio PC, entao cada capa ganha um id curto e o Host serve o arquivo.
inline std::string DescCapaId(const std::wstring& url) {
    unsigned long long x = 1469598103934665603ULL;
    for (wchar_t ch : url) { x ^= (unsigned long long)ch; x *= 1099511628211ULL; }
    char b[24]; snprintf(b, sizeof b, "%016llx", x); return b;
}

// ------------------------------------------------------------ estado ------
struct HTrack { std::string id; std::wstring title, artist, path, coverPath; int dur = 0; };
// Musica online (resultado de busca ou item de playlist): o celular so conhece o id; a URL fica no PC.
struct OItem { std::wstring url, play, title, artist, thumb; int dur = 0; long long used = 0; };
struct HPlaylist { std::wstring slug, name; std::vector<std::string> ids; };
// persist=false ("Lembrar" desmarcado): vale so enquanto o Remix estiver aberto e some depois de 12 h sem uso;
// os outros somem depois de 180 dias sem uso. Nenhum dos dois vale para sempre.
// key = chave permanente do aparelho: entra por link/QR em QUALQUER endereco (o cookie do navegador e so
// um atalho daquele dominio; trocar de tunel ou abrir como app na tela de inicio perde o cookie, nao o vinculo).
struct Device { std::string id, name, token, ip, key; long long created = 0, lastSeen = 0, savedSeen = 0; bool lib = false, persist = true; };
constexpr long long DEV_TEMP_IDLE = 12 * 3600LL, DEV_IDLE = 180 * 24 * 3600LL;
struct PairReq { std::string id, name, ip; long long created = 0; int estado = 0; std::string token, devId; bool viaTunnel = false, viaQr = false, lembrar = true; };   // estado: 0 pendente, 1 aceito, 2 recusado
struct DevPlaylist { std::string dev, slug, name; std::vector<std::string> ids; bool share = false, pcOk = false; };
struct RateInfo { long long winStart = 0; int count = 0; int fails = 0; long long lockUntil = 0; };
struct StreamCtl {   // um streaming online em andamento (o PC converte e manda para o celular)
    std::atomic<bool> cancel{ false }; std::mutex m; Proc* a = nullptr; Proc* b = nullptr; hsock_t sock = HSOCK_BAD;
    // Para de fora: mata os processos e derruba a conexao (RST) sem esperar um envio bloqueado terminar.
    void Stop() { cancel = true; std::lock_guard<std::mutex> lk(m); if (a) a->Kill(); if (b) b->Kill(); if (sock != HSOCK_BAD) { hostnet::Abort(sock); hostnet::Wake(sock); } }
};
struct Options { int port = 49875; bool lanOk = true, lan6 = false, onlineOk = true, qrConfirm = false; std::string pin, accent = "#1e90ff"; std::wstring name; };
struct State {
    std::mutex m;                                   // protege tudo abaixo (menos os atomics)
    bool running = false; Options opt; std::wstring hostName;
    hsock_t ls = HSOCK_BAD, ls6 = HSOCK_BAD; std::thread th; std::atomic<bool> stop{ false }; std::atomic<int> conns{ 0 };
    std::vector<hostnet::Prefix64> own6;
    // biblioteca publicada pela UI
    std::vector<HTrack> tracks; std::map<std::string, size_t> byId; std::map<std::string, bool> libIds; std::vector<HPlaylist> pls;
    std::map<std::string, OItem> onl;               // id -> musica online
    std::map<std::string, std::wstring> descCapas;  // capa das novidades: id curto -> URL (o celular nao busca fora)
    std::map<std::wstring, std::string> targets;    // slug -> "" (nao hosteada) | "ALL" | "id1,id2"
    std::vector<Device> devs; std::vector<PairReq> reqs; std::vector<DevPlaylist> dpls;
    std::map<std::string, std::vector<std::string>> seen;   // dispositivo -> ids online que ele buscou (autoriza tocar/adicionar)
    std::map<std::string, int> searching; int searchingAll = 0;
    std::map<std::string, std::shared_ptr<StreamCtl>> streams; int streamsAll = 0;
    std::map<std::string, RateInfo> rate; long long pinWin = 0; int pinFailsAll = 0; long long pinLockAll = 0;
    bool loaded = false;                            // host.ini ja lido (nunca gravar por cima sem ler)
    std::atomic<unsigned> authGen{ 0 };             // sobe a cada mudanca de permissao: transferencias em andamento reconferem
    std::atomic<unsigned> runGen{ 0 };              // sobe no Stop: tudo que estava em andamento para
    std::map<hsock_t, std::string> live;            // conexoes abertas -> chave de origem (o Stop derruba todas)
    std::map<std::string, int> connsBy;
    std::string qrToken; long long qrCreated = 0;
    std::vector<std::string> lanUrls, lan6Urls; std::string lastError;
    std::thread tunTh; std::atomic<bool> tunCancel{ false }; std::atomic<bool> tunRunning{ false }; std::atomic<unsigned> tunGen{ 0 };
    std::string tunUrl, tunPending, tunStatus = "desligado";
    std::atomic<unsigned> version{ 0 };             // sobe a cada mudanca (a UI olha para redesenhar)
};
inline State& St() { static State* s = new State(); return *s; }
inline void Bump() { St().version++; }
inline void AuthChanged() { St().authGen++; St().version++; }
inline std::string OnlineId(const std::wstring& url) { return IdFor(L"online|" + url); }

// ------------------------------------------------------------ host.ini ----
inline std::wstring IniPath() { return Config::Join(Config::BaseDir(), L"host.ini"); }
inline std::string W8(const std::wstring& w) { return Esc(WideToUtf8(w)); }
inline std::wstring U8(const std::string& s) { return Utf8ToWide(Unesc(s)); }
inline std::vector<std::string> Split(const std::string& v, char sep) { std::vector<std::string> f; size_t p = 0; while (true) { size_t c = v.find(sep, p); f.push_back(v.substr(p, c == std::string::npos ? std::string::npos : c - p)); if (c == std::string::npos) break; p = c + 1; } return f; }
inline void SaveIni() {   // chamar com St().m travado
    State& s = St();
    if (!s.loaded) return;   // sem ter lido o arquivo, gravar apagaria os aparelhos e as playlists
    std::string o = "# Remix Host: aparelhos vinculados, playlists hosteadas e playlists dos aparelhos. Nao compartilhe (tem os tokens).\n[Dispositivos]\n";
    std::map<std::string, bool> temp;   // aparelho temporario ("Lembrar" desmarcado) nao vai para o disco
    for (auto& d : s.devs) {
        if (!d.persist) { temp[d.id] = true; continue; }
        o += "dev=" + d.id + "|" + d.token + "|" + std::to_string(d.created) + "|" + Esc(d.name) + "|" + Esc(d.ip) + "|" + (d.lib ? "1" : "0") + "|1|" + std::to_string(d.lastSeen) + "|" + d.key + "\n";
    }
    o += "[Playlists]\n";
    for (auto& kv : s.targets) if (!kv.second.empty()) o += Esc(WideToUtf8(kv.first)) + "=" + kv.second + "\n";
    std::map<std::string, bool> usados;
    for (auto& p : s.dpls) if (!temp.count(p.dev)) for (auto& id : p.ids) if (s.onl.count(id)) usados[id] = true;
    o += "[Online]\n";
    for (auto& kv : usados) { const OItem& it = s.onl[kv.first]; o += kv.first + "=" + W8(it.url) + "|" + W8(it.play) + "|" + W8(it.title) + "|" + W8(it.artist) + "|" + std::to_string(it.dur) + "|" + W8(it.thumb) + "\n"; }
    for (auto& p : s.dpls) {
        if (temp.count(p.dev)) continue;
        o += "[Celular:" + p.dev + ":" + p.slug + "]\nnome=" + Esc(p.name) + "\ncompartilhar=" + (p.share ? "1" : "0") + "\nliberada=" + (p.pcOk ? "1" : "0") + "\nids=";
        for (size_t i = 0; i < p.ids.size(); i++) { if (i) o += ","; o += p.ids[i]; }
        o += "\n";
    }
    std::wstring path = IniPath(), tmp = path + L".tmp";
#ifdef _WIN32
    FILE* f = _wfopen(tmp.c_str(), L"wb");
#else
    FILE* f = fopen(WideToUtf8(tmp).c_str(), "wb");
#endif
    if (!f) return;
    fwrite(o.data(), 1, o.size(), f); fclose(f);
    std::error_code ec; std::filesystem::remove(std::filesystem::path(path), ec); std::filesystem::rename(std::filesystem::path(tmp), std::filesystem::path(path), ec);
#ifndef _WIN32
    chmod(WideToUtf8(path).c_str(), 0600);   // tem os tokens: so o dono le
#endif
}
inline bool SafeSlug(const std::string& s) { if (s.empty() || s.size() > 40) return false; for (char c : s) if (!(isalnum((unsigned char)c) || c == '-')) return false; return true; }
inline void LoadIni() {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    std::vector<Device> temps; std::vector<DevPlaylist> tempPls;   // temporarios so existem na memoria: sobrevivem a religar o Host
    for (auto& d : s.devs) if (!d.persist) temps.push_back(d);
    for (auto& p : s.dpls) for (auto& d : temps) if (p.dev == d.id) { tempPls.push_back(p); break; }
    struct Keep { State& s; std::vector<Device>& t; std::vector<DevPlaylist>& tp; ~Keep() {
        for (auto& d : t) { bool have = false; for (auto& x : s.devs) if (x.id == d.id) have = true; if (!have) s.devs.push_back(d); }
        for (auto& p : tp) s.dpls.push_back(p);
    } };
    s.devs.clear(); s.targets.clear(); s.dpls.clear(); s.loaded = true;
    bool novaChave = false;
    Keep keep{ s, temps, tempPls };
#ifdef _WIN32
    FILE* f = _wfopen(IniPath().c_str(), L"rb");
#else
    FILE* f = fopen(WideToUtf8(IniPath()).c_str(), "rb");
#endif
    if (!f) return;
    std::string all; char b[4096]; size_t n; while ((n = fread(b, 1, sizeof b, f)) > 0 && all.size() < (size_t)64 * 1024 * 1024) all.append(b, n); fclose(f);
    std::string sect; int cur = -1; size_t pos = 0;
    while (pos <= all.size()) {
        size_t e = all.find('\n', pos); std::string ln = all.substr(pos, e == std::string::npos ? std::string::npos : e - pos); pos = (e == std::string::npos) ? all.size() + 1 : e + 1;
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back();
        if (ln.empty() || ln[0] == '#') continue;
        if (ln[0] == '[' && ln.back() == ']') {
            sect = ln.substr(1, ln.size() - 2); cur = -1;
            if (sect.rfind("Celular:", 0) == 0) { size_t c = sect.find(':', 8); if (c != std::string::npos) { DevPlaylist p; p.dev = sect.substr(8, c - 8); p.slug = sect.substr(c + 1); if (IsHex(p.dev, 12) && SafeSlug(p.slug)) { s.dpls.push_back(p); cur = (int)s.dpls.size() - 1; } } }
            continue;
        }
        size_t eq = ln.find('='); if (eq == std::string::npos) continue;
        std::string k = ln.substr(0, eq), v = ln.substr(eq + 1);
        if (sect == "Dispositivos" && k == "dev") {
            auto fl = Split(v, '|');
            if (fl.size() >= 4 && IsHex(fl[0], 12) && IsHex(fl[1], 64)) {
                Device d; d.id = fl[0]; d.token = fl[1]; d.created = atoll(fl[2].c_str()); d.name = Unesc(fl[3]); if (fl.size() > 4) d.ip = Unesc(fl[4]); if (fl.size() > 5) d.lib = fl[5] == "1";
                d.lastSeen = fl.size() > 7 ? atoll(fl[7].c_str()) : NowSec(); d.savedSeen = d.lastSeen;
                if (fl.size() > 8 && IsHex(fl[8], 32)) d.key = fl[8]; else { d.key = RandomHex(16); novaChave = true; }   // versao antiga: ganha chave agora
                if (fl.size() > 6 && fl[6] == "0") continue;          // temporario gravado por versao antiga: nao volta
                if (NowSec() - d.lastSeen > DEV_IDLE) continue;       // 180 dias sem uso: precisa vincular de novo
                s.devs.push_back(d);
            }
        } else if (sect == "Playlists") { s.targets[Utf8ToWide(Unesc(k))] = v; }
        else if (sect == "Online") { auto fl = Split(v, '|'); if (IsId(k) && fl.size() >= 6) { OItem it; it.url = U8(fl[0]); it.play = U8(fl[1]); it.title = U8(fl[2]); it.artist = U8(fl[3]); it.dur = atoi(fl[4].c_str()); it.thumb = U8(fl[5]); s.onl[k] = it; } }
        else if (cur >= 0) {
            DevPlaylist& p = s.dpls[(size_t)cur];
            if (k == "nome") p.name = Unesc(v); else if (k == "compartilhar") p.share = v == "1"; else if (k == "liberada") p.pcOk = v == "1";
            else if (k == "ids") for (auto& id : Split(v, ',')) if (IsId(id)) p.ids.push_back(id);
        }
    }
    s.dpls.erase(std::remove_if(s.dpls.begin(), s.dpls.end(), [&](const DevPlaylist& p) { for (auto& d : s.devs) if (d.id == p.dev) return false; return true; }), s.dpls.end());
    if (novaChave) SaveIni();   // aparelhos vindos de uma versao sem chave: guarda a chave nova agora
}

// Antes de mexer em playlists hosteadas com o Host desligado: le o host.ini primeiro (sem isso o
// SaveIni gravaria por cima so o que esta na memoria e apagaria aparelhos e playlists).
inline void EnsureLoaded() { { std::lock_guard<std::mutex> lk(St().m); if (St().loaded) return; } LoadIni(); }

// ------------------------------------------------------------ snapshot ----
// Thread da UI: copia a biblioteca e as playlists do PC para o servidor.
inline void Publish(const std::vector<Track>& lib, const std::vector<Playlist>& pls) {
    std::vector<HTrack> tr; std::map<std::string, size_t> by; std::map<std::string, bool> libIds; std::map<std::string, OItem> onl;
    auto add = [&](const std::wstring& path, const std::wstring& title, const std::wstring& artist, const std::wstring& cover, int dur) {
        if (path.empty()) return std::string();
        std::string id = IdFor(path);
        if (by.count(id)) return id;
        HTrack t; t.id = id; t.path = path; t.title = title.empty() ? std::filesystem::path(path).stem().wstring() : title; t.artist = artist; t.coverPath = cover; t.dur = dur;
        by[id] = tr.size(); tr.push_back(t); return id;
    };
    for (auto& t : lib) if (!IsOnlineTrack(t)) libIds[add(t.path, t.title, t.artist, t.coverPath, t.durSec)] = true;
    std::vector<HPlaylist> hp;
    for (auto& p : pls) {
        HPlaylist h; h.slug = p.slug; h.name = p.name;
        for (auto& e : p.entries) {
            if (!e.url.empty()) {   // musica online da playlist: vai por streaming
                std::string id = OnlineId(e.url); OItem it; it.url = e.url; it.play = e.play; it.title = e.title; it.artist = e.artist; it.thumb = e.thumb; it.dur = e.dur;
                onl[id] = it; h.ids.push_back(id); continue;
            }
            if (e.path.empty()) continue;
            std::string id = add(e.path, e.title, e.artist, L"", e.dur); if (!id.empty()) h.ids.push_back(id);
        }
        hp.push_back(h);
    }
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    s.tracks.swap(tr); s.byId.swap(by); s.libIds.swap(libIds); s.pls.swap(hp); s.authGen++;   // arquivo trocado/renomeado: transferencias reconferem
    for (auto& kv : onl) { auto it = s.onl.find(kv.first); if (it == s.onl.end()) s.onl[kv.first] = kv.second; else { long long u = it->second.used; std::wstring play = it->second.play; it->second = kv.second; it->second.used = u; if (it->second.play.empty()) it->second.play = play; } }
}

// ------------------------------------------------------------ autorizacao --
// Tudo abaixo com St().m travado. Regra: o celular so ve o que o PC liberou.
//  - biblioteca inteira: so se o PC ligou "BIBLIOTECA" para aquele aparelho;
//  - playlists do PC: so as hosteadas para ele (ALL ou a lista);
//  - playlists de outros aparelhos: so se o dono pediu para compartilhar E o PC liberou;
//  - online: resultados que ele mesmo buscou e itens das playlists que ele pode ver.
inline bool HostedTo(const std::wstring& slug, const std::string& devId) {
    auto it = St().targets.find(slug); if (it == St().targets.end() || it->second.empty()) return false;
    if (it->second == "ALL") return true;
    return ("," + it->second + ",").find("," + devId + ",") != std::string::npos;
}
inline bool InList(const std::vector<std::string>& v, const std::string& id) { return std::find(v.begin(), v.end(), id) != v.end(); }
inline const Device* DevById(const std::string& id) { for (auto& d : St().devs) if (d.id == id) return &d; return nullptr; }
// O que o proprio aparelho alcanca sem contar playlists compartilhadas por outros (sem recursao).
// Uma playlist compartilhada so mostra aos outros o que o DONO ainda pode ver hoje: se o PC tirar
// a biblioteca ou parar de hostear para o dono, as faixas somem da compartilhada tambem.
inline bool OwnReach(const Device& d, const std::string& id, bool local) {
    State& s = St();
    if (local) {
        if (d.lib && s.libIds.count(id)) return true;
        for (auto& p : s.pls) if (HostedTo(p.slug, d.id) && InList(p.ids, id)) return true;
        return false;
    }
    for (auto& p : s.pls) if (HostedTo(p.slug, d.id) && InList(p.ids, id)) return true;
    for (auto& p : s.dpls) if (p.dev == d.id && InList(p.ids, id)) return true;   // online que ele mesmo pos na playlist dele
    auto sv = s.seen.find(d.id); return sv != s.seen.end() && InList(sv->second, id);
}
inline bool CanSee(const Device& d, const std::string& id) {
    State& s = St();
    bool local = s.byId.count(id) > 0, online = !local && s.onl.count(id) > 0;
    if (!local && !online) return false;
    if (online && !s.opt.onlineOk) return false;
    if (OwnReach(d, id, local)) return true;
    for (auto& p : s.dpls) {
        if (p.dev == d.id || !p.share || !p.pcOk || !InList(p.ids, id)) continue;
        const Device* o = DevById(p.dev);
        if (o && OwnReach(*o, id, local)) return true;
    }
    return false;
}
// Mesmo resultado do CanSee para muitos ids de uma vez (monta o conjunto uma vez so).
inline void AddReach(const Device& d, std::set<std::string>& v) {
    State& s = St();
    if (d.lib) for (auto& kv : s.libIds) v.insert(kv.first);
    for (auto& p : s.pls) if (HostedTo(p.slug, d.id)) v.insert(p.ids.begin(), p.ids.end());
    for (auto& p : s.dpls) if (p.dev == d.id) for (auto& id : p.ids) if (!s.byId.count(id)) v.insert(id);
    auto sv = s.seen.find(d.id); if (sv != s.seen.end()) for (auto& id : sv->second) if (!s.byId.count(id)) v.insert(id);
}
inline std::set<std::string> Visible(const Device& d) {
    State& s = St(); std::set<std::string> v; AddReach(d, v);
    std::map<std::string, std::set<std::string>> owners;
    for (auto& p : s.dpls) {
        if (p.dev == d.id || !p.share || !p.pcOk) continue;
        auto it = owners.find(p.dev);
        if (it == owners.end()) { const Device* o = DevById(p.dev); it = owners.emplace(p.dev, std::set<std::string>()).first; if (o) AddReach(*o, it->second); }
        for (auto& id : p.ids) if (it->second.count(id)) v.insert(id);
    }
    for (auto i = v.begin(); i != v.end();) {
        bool local = s.byId.count(*i) > 0, online = !local && s.onl.count(*i) > 0;
        if (local || (online && s.opt.onlineOk)) ++i; else i = v.erase(i);
    }
    return v;
}
inline std::string ItemJson(const std::string& id) {   // com a trava; "" se o id nao existe mais
    State& s = St();
    auto bi = s.byId.find(id);
    if (bi != s.byId.end()) { const HTrack& t = s.tracks[bi->second]; return "{\"id\":\"" + t.id + "\",\"t\":" + JStr(t.title) + ",\"a\":" + JStr(t.artist) + ",\"d\":" + std::to_string(t.dur) + ",\"c\":" + (t.coverPath.empty() ? "0" : "1") + ",\"o\":0}"; }
    auto oi = s.onl.find(id);
    if (oi != s.onl.end()) { const OItem& t = oi->second; return "{\"id\":\"" + id + "\",\"t\":" + JStr(t.title.empty() ? t.url : t.title) + ",\"a\":" + JStr(t.artist) + ",\"d\":" + std::to_string(t.dur) + ",\"c\":" + (AllowedThumbUrl(t.thumb) ? "1" : "0") + ",\"o\":1}"; }
    return "";
}
inline std::string ItemsJson(const std::set<std::string>& vis, const std::vector<std::string>& ids) {
    std::string o = "["; bool first = true; size_t n = 0;
    for (auto& id : ids) { if (!vis.count(id)) continue; std::string j = ItemJson(id); if (j.empty()) continue; if (!first) o += ","; first = false; o += j; if (++n >= 5000) break; }
    return o + "]";
}
inline void RememberSeen(const std::string& devId, const std::string& id) {
    auto& v = St().seen[devId]; if (InList(v, id)) return; v.push_back(id); if (v.size() > 400) v.erase(v.begin(), v.begin() + 100);
}

// ------------------------------------------------------------ limites -----
// Com St().m travado. Quem nao e aparelho vinculado: 90 pedidos / 10 s por origem. Aparelho vinculado
// (token valido, nao so "parece token"): 600 / 10 s (a tela de uma playlist grande pede dezenas de capas).
inline bool RateOk(const std::string& key, bool knownDev) {
    State& s = St(); long long now = NowSec();
    if (s.rate.size() > 4000)   // limpa so o que ja venceu: travas de PIN e erros recentes ficam
        for (auto it = s.rate.begin(); it != s.rate.end();) { if (now - it->second.winStart >= 10 && now >= it->second.lockUntil && (it->second.fails == 0 || now - it->second.winStart > 3600)) it = s.rate.erase(it); else ++it; }
    if (s.rate.size() > 8000)   // ainda cheio (alguem variando a origem): so as travas e os erros de PIN ficam
        for (auto it = s.rate.begin(); it != s.rate.end();) { if (now >= it->second.lockUntil && it->second.fails == 0) it = s.rate.erase(it); else ++it; }
    RateInfo& r = s.rate[key];
    if (now - r.winStart >= 10) { r.winStart = now; r.count = 0; }
    int lim = knownDev ? 600 : 90;
    if (r.count > lim) return false;   // nao conta mais depois do limite
    return ++r.count <= lim;
}
// PIN: trava por origem (5 erros = 60 s, dobrando) e trava geral (30 erros em 10 min de qualquer origem =
// PIN desligado por 10 min; o QR continua funcionando). Assim trocar de IP nao ajuda a adivinhar.
inline bool PairLocked(const std::string& key) { State& s = St(); long long now = NowSec(); if (now < s.pinLockAll) return true; auto it = s.rate.find(key); return it != s.rate.end() && now < it->second.lockUntil; }
inline void PairFail(const std::string& key) {
    State& s = St(); long long now = NowSec();
    RateInfo& r = s.rate[key]; r.fails++; if (r.fails >= 5) { int k = r.fails - 5; if (k > 4) k = 4; r.lockUntil = now + 60LL * (1 << k); }
    if (now - s.pinWin > 600) { s.pinWin = now; s.pinFailsAll = 0; }
    if (++s.pinFailsAll >= 30 && now >= s.pinLockAll) { s.pinLockAll = now + 600; AppPost(EV_HOST_STATUS, L"Host: muitas tentativas erradas de PIN. Vincular por PIN ficou travado por 10 minutos (o QR code continua funcionando).", 0); }
}
inline void PairOk(const std::string& key) { State& s = St(); s.rate[key].fails = 0; s.rate[key].lockUntil = 0; }
inline void ExpireReqs() {   // com St().m travado
    State& s = St(); long long now = NowSec();
    s.reqs.erase(std::remove_if(s.reqs.begin(), s.reqs.end(), [&](const PairReq& r) { return now - r.created > 180 && r.estado != 1; }), s.reqs.end());
    s.reqs.erase(std::remove_if(s.reqs.begin(), s.reqs.end(), [&](const PairReq& r) { return r.estado == 1 && now - r.created > 900; }), s.reqs.end());
    if (!s.qrToken.empty() && now - s.qrCreated > 600) s.qrToken.clear();   // QR vale 10 min
}

// ------------------------------------------------------------ HTTP --------
struct Req { std::string method, path, query, body, ip, rateKey, cookieDev; bool viaTunnel = false, loopback = false, trustCf = false; std::map<std::string, std::string> h; long long r0 = -1, r1 = -1; bool hasRange = false; };
struct Resp { int status = 200; std::string ctype = "application/json; charset=utf-8"; std::string body; std::vector<std::string> extra; bool cache = false; };
inline const char* StatusText(int c) {
    switch (c) { case 200: return "OK"; case 204: return "No Content"; case 206: return "Partial Content"; case 400: return "Bad Request"; case 401: return "Unauthorized"; case 403: return "Forbidden";
    case 404: return "Not Found"; case 405: return "Method Not Allowed"; case 408: return "Request Timeout"; case 413: return "Payload Too Large"; case 416: return "Range Not Satisfiable"; case 421: return "Misdirected Request"; case 429: return "Too Many Requests"; case 431: return "Request Header Fields Too Large";
    case 502: return "Bad Gateway"; case 503: return "Service Unavailable"; default: return "Error"; }
}
inline std::string SecHeaders() { return "X-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nX-Frame-Options: DENY\r\nCross-Origin-Resource-Policy: same-origin\r\nPermissions-Policy: camera=(), microphone=(), geolocation=(), interest-cohort=()\r\n"; }
inline std::string Head(const Resp& r, long long len) {
    std::string o = "HTTP/1.1 " + std::to_string(r.status) + " " + StatusText(r.status) + "\r\n";
    o += "Content-Type: " + r.ctype + "\r\n";
    if (len >= 0) o += "Content-Length: " + std::to_string(len) + "\r\n";
    o += "Connection: close\r\n" + SecHeaders();
    o += r.cache ? "Cache-Control: private, max-age=86400\r\n" : "Cache-Control: no-store\r\n";
    if (r.ctype.rfind("text/html", 0) == 0) o += "Content-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; media-src 'self'; connect-src 'self'; manifest-src 'self'; worker-src 'self'; frame-ancestors 'none'; base-uri 'none'; form-action 'none'\r\n";
    for (auto& e : r.extra) o += e + "\r\n";
    return o + "\r\n";
}
inline void SendResp(hsock_t s, const Resp& r) { std::string h = Head(r, (long long)r.body.size()); hostnet::SendAll(s, h); if (!r.body.empty()) hostnet::SendAll(s, r.body); }
inline void SendJson(hsock_t s, int status, const std::string& json, const std::vector<std::string>& extra = {}) { Resp r; r.status = status; r.body = json; r.extra = extra; SendResp(s, r); }
inline void SendErr(hsock_t s, int status, const char* erro) { SendJson(s, status, std::string("{\"erro\":\"") + erro + "\"}"); }
inline std::string ErrJson(const char* erro) { return std::string("{\"erro\":\"") + erro + "\"}"; }

inline bool TokenChar(unsigned char c) { return c != 0 && (isalnum(c) || strchr("!#$%&'*+-.^_`|~", c) != nullptr); }
inline bool ValidIp(const std::string& ip) {
    if (ip.empty() || ip.size() > 45) return false;
    unsigned char a[16];
    return inet_pton(AF_INET, ip.c_str(), a) == 1 || inet_pton(AF_INET6, ip.c_str(), a) == 1;
}
// Le o pedido inteiro (cabecalho ate 16 KB, corpo ate 64 KB) em no maximo 15 s no total: quem manda um
// byte por vez nao segura a conexao para sempre. false = pedido invalido (status em err; 0 = so fechar).
inline bool ReadReq(hsock_t s, Req& r, int& err) {
    std::string buf; char b[4096]; err = 400;
    auto t0 = std::chrono::steady_clock::now();
    auto recvLeft = [&](long& n) {
        long long ms = 15000 - (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
        if (ms <= 0) { err = 408; return false; }
        hostnet::SetTimeout(s, (int)std::min<long long>(ms, 10000));
        n = hostnet::Recv(s, b, sizeof b);
        if (n <= 0) { err = 0; return false; }
        return true;
    };
    while (buf.find("\r\n\r\n") == std::string::npos) {
        if (buf.size() > 16384) { err = 431; return false; }
        long n = 0; if (!recvLeft(n)) return false; buf.append(b, (size_t)n);
    }
    size_t he = buf.find("\r\n\r\n"); std::string head = buf.substr(0, he); std::string rest = buf.substr(he + 4);
    size_t le = head.find("\r\n"); std::string line = head.substr(0, le == std::string::npos ? head.size() : le);
    size_t s1 = line.find(' '); if (s1 == std::string::npos) return false; size_t s2 = line.find(' ', s1 + 1); if (s2 == std::string::npos) return false;
    r.method = line.substr(0, s1); std::string target = line.substr(s1 + 1, s2 - s1 - 1);
    if (target.empty() || target[0] != '/' || target.size() > 2048) return false;
    for (unsigned char ch : target) if (ch < 0x21 || ch == 0x7F) return false;
    size_t q = target.find('?'); r.path = target.substr(0, q); if (q != std::string::npos) r.query = target.substr(q + 1);
    if (r.path.find("..") != std::string::npos || r.path.find('%') != std::string::npos || r.path.find('\\') != std::string::npos) return false;
    size_t p = (le == std::string::npos) ? head.size() : le + 2; int nh = 0;
    std::map<std::string, int> seenH;
    while (p < head.size()) {
        size_t e = head.find("\r\n", p); std::string hl = head.substr(p, e == std::string::npos ? std::string::npos : e - p); p = (e == std::string::npos) ? head.size() : e + 2;
        size_t c = hl.find(':'); if (c == std::string::npos || c == 0) return false;   // linha sem nome/sem ':'
        std::string k = hl.substr(0, c), v = hl.substr(c + 1);
        for (auto& ch : k) { if (!TokenChar((unsigned char)ch)) return false; ch = (char)tolower((unsigned char)ch); }   // "Nome :" (espaco antes do ':') e invalido (RFC 9112)
        for (unsigned char ch : v) if ((ch < 0x20 && ch != '\t') || ch == 0x7F) return false;
        while (!v.empty() && (v[0] == ' ' || v[0] == '\t')) v.erase(0, 1); while (!v.empty() && (v.back() == ' ' || v.back() == '\t')) v.pop_back();
        if (++nh > 64) { err = 431; return false; }
        if ((k == "content-length" || k == "transfer-encoding" || k == "host") && seenH[k]++) return false;   // repetido: ambiguo, recusa
        if (k == "cookie" && r.h.count(k)) r.h[k] += "; " + v; else r.h[k] = v;
    }
    if (r.h.count("transfer-encoding")) { err = 413; return false; }
    long long cl = 0; auto it = r.h.find("content-length"); if (it != r.h.end()) { if (it->second.empty() || it->second.size() > 9) return false; for (char ch : it->second) if (ch < '0' || ch > '9') return false; cl = atoll(it->second.c_str()); }
    if (cl < 0 || cl > 65536) { err = 413; return false; }
    r.body = rest;
    while ((long long)r.body.size() < cl) { long n = 0; if (!recvLeft(n)) return false; r.body.append(b, (size_t)n); }
    r.body.resize((size_t)cl);
    hostnet::SetTimeout(s, 10000);
    auto rg = r.h.find("range");
    if (rg != r.h.end() && rg->second.rfind("bytes=", 0) == 0) {
        std::string v = rg->second.substr(6); size_t d = v.find('-');
        if (d != std::string::npos && v.find(',') == std::string::npos) { r.hasRange = true; r.r0 = d ? atoll(v.substr(0, d).c_str()) : -1; r.r1 = (d + 1 < v.size()) ? atoll(v.substr(d + 1).c_str()) : -1; }
    }
    auto ck = r.h.find("cookie");
    if (ck != r.h.end()) { size_t p2 = ck->second.find("remix_dev="); if (p2 != std::string::npos) { size_t e = ck->second.find(';', p2); r.cookieDev = ck->second.substr(p2 + 10, e == std::string::npos ? std::string::npos : e - p2 - 10); } }
    // Atras do tunel o cloudflared chega de 127.0.0.1 e manda o IP de quem acessou. So confiamos nesses
    // cabecalhos com o tunel ligado, em conexao local (ninguem da rede consegue forjar) e com IP valido.
    if (r.trustCf) {
        auto cf = r.h.find("cf-connecting-ip");
        if (cf != r.h.end() && ValidIp(cf->second)) {
            r.ip = cf->second; r.viaTunnel = true; std::string k = cf->second;
            if (k.find(':') != std::string::npos) { auto g = Split(k, ':'); k.clear(); for (size_t i = 0; i < g.size() && i < 4; i++) k += g[i] + ":"; k += ":/64"; }
            r.rateKey = "cf:" + k;
        }
        auto xp = r.h.find("x-forwarded-proto"); if (xp != r.h.end() && xp->second == "https") r.viaTunnel = true;
    }
    return true;
}
// Defesa contra "DNS rebinding": um site de fora que faz o nome dele apontar para o IP do PC. O navegador
// manda o nome do site no Host; aqui so vale IP, localhost, nome da maquina na rede e o link do tunel.
inline bool HostHeaderOk(const Req& r) {
    auto it = r.h.find("host"); if (it == r.h.end()) return true;   // so navegador faz rebinding, e navegador sempre manda Host
    std::string h = it->second; for (auto& c : h) c = (char)tolower((unsigned char)c);
    if (h.empty() || h.size() > 255) return false;
    if (h[0] == '[') {
        size_t e = h.find(']'); if (e == std::string::npos) return false;
        std::string ip = h.substr(1, e - 1), port = h.substr(e + 1);
        if (!port.empty() && (port[0] != ':' || port.size() < 2 || port.find_first_not_of("0123456789", 1) != std::string::npos)) return false;
        size_t z = ip.find('%'); if (z != std::string::npos) ip = ip.substr(0, z);
        unsigned char a[16]; return inet_pton(AF_INET6, ip.c_str(), a) == 1;
    }
    size_t c = h.rfind(':');
    if (c != std::string::npos) { if (c + 1 >= h.size() || h.find_first_not_of("0123456789", c + 1) != std::string::npos) return false; h = h.substr(0, c); }
    unsigned char a4[4]; if (inet_pton(AF_INET, h.c_str(), a4) == 1) return true;
    for (char ch : h) if (!(isalnum((unsigned char)ch) || ch == '-' || ch == '.')) return false;
    if (h == "localhost" || h.find('.') == std::string::npos) return true;   // nome da maquina sem dominio
    auto ends = [&](const char* suf) { size_t n = strlen(suf); return h.size() > n && h.compare(h.size() - n, n, suf) == 0; };
    for (const char* suf : { ".local", ".lan", ".home", ".home.arpa", ".internal", ".localdomain" }) if (ends(suf)) return true;
    return r.loopback && ends(".trycloudflare.com");
}
inline std::string QueryGet(const std::string& q, const std::string& key) {
    for (auto& part : Split(q, '&')) { size_t e = part.find('='); if (e != std::string::npos && part.substr(0, e) == key) return UrlDecode(part.substr(e + 1)); }
    return "";
}
// Arquivo com Range (o <audio> do navegador pede pedacos para avancar/voltar). still = reconfere a
// permissao quando algo muda no meio (REMOVER, BIBLIOTECA: NAO, parar de hostear): corta na hora.
inline void SendFile(hsock_t s, const Req& r, const std::wstring& path, const std::string& ctype, bool cache, const std::function<bool()>& still = nullptr) {
    unsigned rg = St().runGen.load(), ag = St().authGen.load();
    std::error_code ec; unsigned long long sz = std::filesystem::file_size(std::filesystem::path(path), ec);
    if (ec) { SendErr(s, 404, "arquivo"); return; }
#ifdef _WIN32
    FILE* f = _wfopen(path.c_str(), L"rb");
#else
    FILE* f = fopen(WideToUtf8(path).c_str(), "rb");
#endif
    if (!f) { SendErr(s, 404, "arquivo"); return; }
    long long a = 0, bnd = (long long)sz - 1; int status = 200;
    if (r.hasRange && sz > 0) {
        if (r.r0 < 0) { long long n = r.r1; if (n <= 0) n = 1; if (n > (long long)sz) n = (long long)sz; a = (long long)sz - n; }
        else { a = r.r0; if (r.r1 >= 0 && r.r1 < bnd) bnd = r.r1; }
        if (a > bnd || a >= (long long)sz) { fclose(f); Resp e; e.status = 416; e.body = "{}"; e.extra.push_back("Content-Range: bytes */" + std::to_string(sz)); SendResp(s, e); return; }
        status = 206;
    }
    Resp h; h.status = status; h.ctype = ctype; h.cache = cache; h.extra.push_back("Accept-Ranges: bytes");
    if (status == 206) h.extra.push_back("Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(bnd) + "/" + std::to_string(sz));
    long long len = bnd - a + 1; if (sz == 0) len = 0;
    hostnet::SendAll(s, Head(h, len));
    if (r.method == "HEAD" || len == 0) { fclose(f); return; }
#ifdef _WIN32
    _fseeki64(f, a, SEEK_SET);
#else
    fseeko(f, (off_t)a, SEEK_SET);
#endif
    hostnet::SetTimeout(s, 30000);
    std::vector<char> buf(65536); long long left = len;
    while (left > 0) {
        if (St().runGen.load() != rg) { hostnet::Abort(s); break; }
        if (still && St().authGen.load() != ag) { ag = St().authGen.load(); if (!still()) { hostnet::Abort(s); break; } }
        size_t want = (size_t)std::min<long long>((long long)buf.size(), left); size_t n = fread(buf.data(), 1, want, f); if (!n) break;
        if (!hostnet::SendAll(s, buf.data(), n)) break; left -= (long long)n;
    }
    fclose(f);
}
// Versao + resumo do app.js/app.css/index: vai na URL (?v=) para o celular nao usar arquivo velho do cache.
inline const std::string& BuildTag() {
    static const std::string tag = [] {
        unsigned long long h = 1469598103934665603ULL;
        for (const char* p : { hostweb::INDEX_HTML, hostweb::APP_CSS, hostweb::APP_JS }) for (const char* c = p; *c; c++) { h ^= (unsigned char)*c; h *= 1099511628211ULL; }
        char b[24]; snprintf(b, sizeof b, "%08llx", (unsigned long long)(h & 0xffffffffULL));
        return WideToUtf8(REMIX_VERSAO) + "-" + b;
    }();
    return tag;
}
inline std::string ReplaceAll(std::string s, const std::string& a, const std::string& b) { size_t p = 0; while ((p = s.find(a, p)) != std::string::npos) { s.replace(p, a.size(), b); p += b.size(); } return s; }
inline std::string DevSlug(const std::string& name) {
    std::string o; for (unsigned char c : name) { if (c < 128 && isalnum(c)) o.push_back((char)tolower(c)); else if (c == ' ' || c == '-' || c == '_') { if (!o.empty() && o.back() != '-') o.push_back('-'); } if (o.size() >= 24) break; }
    while (!o.empty() && o.back() == '-') o.pop_back(); if (o.empty()) o = "playlist";
    return o + "-" + RandomHex(3);
}
inline std::string CookieFor(const std::string& token, bool persist, bool secure) {
    return "Set-Cookie: remix_dev=" + token + "; Path=/; HttpOnly; SameSite=Strict" + std::string(persist ? "; Max-Age=31536000" : "") + (secure ? "; Secure" : "");
}
inline Device NewDevice(const std::string& name, const std::string& ip, bool persist) {   // com a trava
    Device d; d.id = RandomHex(6); d.name = name; d.token = RandomHex(32); d.key = RandomHex(16); d.ip = ip; d.created = d.lastSeen = d.savedSeen = NowSec(); d.persist = persist; d.lib = false;
    St().devs.push_back(d); SaveIni(); Bump(); return d;
}
// Tira o aparelho de tudo (com a trava): lista, playlists dele, buscas, streaming e listas de playlists hosteadas.
inline void RevokeLocked(const std::string& devId) {
    State& s = St();
    s.devs.erase(std::remove_if(s.devs.begin(), s.devs.end(), [&](const Device& d) { return d.id == devId; }), s.devs.end());
    s.dpls.erase(std::remove_if(s.dpls.begin(), s.dpls.end(), [&](const DevPlaylist& p) { return p.dev == devId; }), s.dpls.end());
    s.seen.erase(devId);
    auto st = s.streams.find(devId); if (st != s.streams.end() && st->second) st->second->Stop();
    for (auto& kv : s.targets) {
        if (kv.second.empty() || kv.second == "ALL") continue;
        std::string o; for (auto& id : Split(kv.second, ',')) if (!id.empty() && id != devId) { if (!o.empty()) o += ","; o += id; }
        kv.second = o;
    }
    SaveIni(); AuthChanged();
}
// Confere o cookie (com a trava). Aparelho parado demais (12 h temporario, 180 dias os outros) sai da lista.
inline bool AuthDevLocked(const std::string& tok, const std::string& ip, Device& out) {
    State& s = St(); if (!IsHex(tok, 64)) return false;
    long long now = NowSec();
    for (auto& d : s.devs) {
        if (!ConstEq(d.token, tok)) continue;
        if (now - d.lastSeen > (d.persist ? DEV_IDLE : DEV_TEMP_IDLE)) { std::string id = d.id; RevokeLocked(id); return false; }
        d.lastSeen = now; d.ip = ip;
        if (d.persist && now - d.savedSeen > 86400) { d.savedSeen = now; SaveIni(); }
        out = d; return true;
    }
    return false;
}
// O aparelho (mesmo id E mesmo token) ainda existe e ainda pode ver o id? (com a trava)
inline bool StillAllowedLocked(const Device& dev, const std::string& id) {
    for (auto& d : St().devs) if (d.id == dev.id && ConstEq(d.token, dev.token)) return CanSee(d, id);
    return false;
}

// ------------------------------------------------------------ online ------
inline int Mp3Encoder() {   // 1 = libmp3lame; 0 = so AAC (ADTS). O ffmpeg de algumas distros nao tem o mp3.
    static std::atomic<int> v{ -1 };
    if (v.load() >= 0) return v.load();
    CapResult r = RunCapture({ fonte::Ffmpeg(), L"-hide_banner", L"-encoders" }, 15000, nullptr);
    v = r.out.find("libmp3lame") != std::string::npos ? 1 : 0; return v.load();
}
struct StreamSlot {   // tira o streaming da contagem quando a conexao acaba
    std::string dev; std::shared_ptr<StreamCtl> ctl;
    ~StreamSlot() { { std::lock_guard<std::mutex> lk(ctl->m); ctl->sock = HSOCK_BAD; }   // antes de a conexao fechar
        State& s = St(); std::lock_guard<std::mutex> lk(s.m); s.streamsAll--; auto it = s.streams.find(dev); if (it != s.streams.end() && it->second == ctl) s.streams.erase(it); }
};
inline bool HttpLink(const std::wstring& u) { return u.rfind(L"https://", 0) == 0 || u.rfind(L"http://", 0) == 0; }
// ---- efeitos e stems no audio do celular ------------------------------------------------------
// O celular so recebe o audio pronto: o PC aplica tudo com o ffmpeg (a tela bloqueada do iPhone continua
// funcionando, porque a pagina toca um <audio> comum). Mesmas intensidades 1..3 do PC.
struct FxReq { int slow = 0, speed = 0, reverb = 0, bass = 0, d8 = 0, stem = 0; bool Any() const { return slow || speed || reverb || bass || d8; } };
inline FxReq ParseFx(const std::string& fx, const std::string& stem) {
    FxReq f;
    for (auto& part : Split(fx, ',')) {
        size_t c = part.find(':'); if (c == std::string::npos) continue;
        std::string k = part.substr(0, c); int v = std::max(0, std::min(3, atoi(part.substr(c + 1).c_str())));
        if (k == "slow") f.slow = v; else if (k == "speed") f.speed = v; else if (k == "reverb") f.reverb = v; else if (k == "bass") f.bass = v; else if (k == "8d") f.d8 = v;
    }
    if (f.slow) f.speed = 0;
    f.stem = stems::ModeFromKey(Utf8ToWide(stem.size() < 20 ? stem : std::string()));
    return f;
}
inline double FxRate(const FxReq& f) { static const double sl[4] = { 1, 0.90, 0.82, 0.75 }, sp[4] = { 1, 1.10, 1.20, 1.30 }; return f.slow ? sl[f.slow] : sp[f.speed]; }
// Grafo do -filter_complex: [0:a] -> ... -> [o]. Reverb = convolucao com uma resposta gerada (ruido que decai).
inline std::string FxGraph(const FxReq& f) {
    char b[400]; std::string g = "[0:a]aresample=44100,aformat=channel_layouts=stereo";
    double r = FxRate(f);
    if (r != 1.0) { snprintf(b, sizeof b, ",asetrate=%d,aresample=44100", (int)lround(44100 * r)); g += b; }
    if (f.bass) { snprintf(b, sizeof b, ",bass=g=%d:f=110:w=0.7", f.bass * 4); g += b; }
    if (f.reverb) {
        static const double wet[4] = { 0, 0.35, 0.6, 0.9 }, dec[4] = { 0, 6.0, 4.8, 3.6 }, dur[4] = { 0, 1.2, 1.6, 2.2 };
        snprintf(b, sizeof b, "aevalsrc='(random(0)*2-1)*exp(-%.1f*t)|(random(1)*2-1)*exp(-%.1f*t)':s=44100:d=%.1f[ir];", dec[f.reverb], dec[f.reverb], dur[f.reverb]);
        std::string pre = b;
        snprintf(b, sizeof b, ",asplit=2[dry][wv];[wv][ir]afir=dry=10:wet=10:length=1:gtype=none,volume=%.2f[wet];[dry][wet]amix=inputs=2:normalize=0:duration=first", wet[f.reverb]);
        g = pre + g + b;
    }
    if (f.d8) { static const double hz[4] = { 0, 0.08, 0.13, 0.20 }, am[4] = { 0, 0.70, 0.85, 0.95 }; snprintf(b, sizeof b, ",apulsator=mode=sine:timing=hz:hz=%.2f:amount=%.2f:offset_l=0:offset_r=0.5,volume=3dB", hz[f.d8], am[f.d8]); g += b; }
    return g + ",alimiter=limit=0.95:level=0[o]";
}

// Duracao de um arquivo local, em segundos (0 = nao sei). Guarda por caminho+tamanho+data.
inline int ProbeDurSec(const std::wstring& path) {
    struct Cache { std::mutex m; std::map<std::wstring, std::pair<std::wstring, int>> map; };
    static Cache* cc = new Cache();
    std::error_code ec; std::filesystem::path fp(path);
    auto sz = std::filesystem::file_size(fp, ec); if (ec) return 0;
    auto mt = std::filesystem::last_write_time(fp, ec);
    std::wstring sig = std::to_wstring((unsigned long long)sz) + L"|" + std::to_wstring(ec ? 0 : (long long)mt.time_since_epoch().count());
    { std::lock_guard<std::mutex> lk(cc->m); auto it = cc->map.find(path); if (it != cc->map.end() && it->second.first == sig) return it->second.second; }
    int dur = 0;
    ma_decoder_config dc = ma_decoder_config_init(ma_format_unknown, 0, 0); ma_decoder d;
#ifdef _WIN32
    bool ok = ma_decoder_init_file_w(path.c_str(), &dc, &d) == MA_SUCCESS;
#else
    bool ok = ma_decoder_init_file(WideToUtf8(path).c_str(), &dc, &d) == MA_SUCCESS;
#endif
    if (ok) { ma_uint64 fr = 0; if (ma_decoder_get_length_in_pcm_frames(&d, &fr) == MA_SUCCESS && d.outputSampleRate) dur = (int)(fr / d.outputSampleRate); ma_decoder_uninit(&d); }
    { std::lock_guard<std::mutex> lk(cc->m); if (cc->map.size() > 3000) cc->map.clear(); cc->map[path] = { sig, dur }; }
    return dur;
}
// Stream convertido para o celular. online: link da musica (URL direta ou pipe do CLI de midia);
// senao localFile: arquivo do PC (faixa com efeito ou stem ja separado).
//
// Com a duracao conhecida e o mp3 (192 kbps, taxa fixa = 24000 bytes por segundo) o Remix diz o TAMANHO do
// stream e aceita "Range: bytes=N-": o byte N vira tempo (N/24000 * velocidade do efeito) e o ffmpeg comeca
// de la. Assim o navegador trata como arquivo normal: arrastar a barra nao recomeca a musica, a tela
// bloqueada do iPhone mostra a duracao e, quando o Safari precisa pedir o audio de novo (engasgo de rede,
// fim de bloco), ele continua do ponto certo em vez de voltar para o comeco com o relogio adiantado.
inline void ServeStream(hsock_t c, const Req& r, const Device& dev, const std::string& id, int startSec, const FxReq& fx, bool online, const std::wstring& localFile, int durSrc) {
    State& s = St();
    unsigned rg = s.runGen.load();
    std::wstring play;
    OItem it;
    if (online) {
        { std::lock_guard<std::mutex> lk(s.m); auto f = s.onl.find(id); if (f == s.onl.end()) { SendErr(c, 404, "faixa"); return; } f->second.used = NowSec(); it = f->second; }
        play = it.play.empty() ? it.url : it.play;
        if (!HttpLink(play)) { SendErr(c, 404, "faixa"); return; }   // so link http(s): nada que vire opcao ou arquivo local
    }
    fonte::Garantir();
    if (!fonte::FfmpegOk() || (online && !fonte::Configurada())) { SendErr(c, 503, "sem_ferramentas"); return; }
    // "Range: bytes=a-b" curtinho = o navegador so esta espiando (cabecalho, fim do arquivo): esse pedido
    // nao substitui o audio que ja esta tocando neste aparelho.
    bool probe = r.hasRange && r.r1 >= 0 && (r.r1 - std::max(0LL, r.r0) + 1) <= 512 * 1024;
    auto ctl = std::make_shared<StreamCtl>();
    {
        std::lock_guard<std::mutex> lk(s.m);
        // reconfere aqui: o EnsureTools pode ter levado segundos e a permissao pode ter mudado
        if ((online && !s.opt.onlineOk) || !StillAllowedLocked(dev, id)) { SendErr(c, 404, "faixa"); return; }
        auto old = s.streams.find(dev.id);
        bool mine = !probe && old != s.streams.end() && old->second && !old->second->cancel.load();
        if (s.streamsAll - (mine ? 1 : 0) >= (probe ? 6 : 4)) { SendErr(c, 429, "streams"); return; }   // o stream que vai ser trocado nao conta
        if (!probe && old != s.streams.end() && old->second) old->second->Stop();   // o mesmo aparelho so ouve uma por vez
        { std::lock_guard<std::mutex> lk2(ctl->m); ctl->sock = c; }
        if (!probe) s.streams[dev.id] = ctl;
        s.streamsAll++;
    }
    StreamSlot slot{ probe ? std::string() : dev.id, ctl };
    unsigned ag = s.authGen.load();
    auto allowed = [&]() {
        if (s.runGen.load() != rg || ctl->cancel.load()) return false;
        if (s.authGen.load() == ag) return true;
        ag = s.authGen.load(); std::lock_guard<std::mutex> lk(s.m); return (!online || s.opt.onlineOk) && StillAllowedLocked(dev, id);
    };
    hostnet::SetTimeout(c, 150000);
    MediaInfo mi;
    if (online) {
        if (NeedsMatch(DetectSource(play))) {   // Spotify/Deezer/Apple: acha a mesma musica no YouTube Music
            OTrack t; t.url = it.url; t.title = it.title; t.artist = it.artist; t.dur = it.dur; std::wstring e;
            if (!MatchOnYouTube(t, e, &ctl->cancel) || t.play.empty() || !HttpLink(t.play)) { SendErr(c, 404, "nao_achei"); return; }
            play = t.play; std::lock_guard<std::mutex> lk(s.m); auto f = s.onl.find(id); if (f != s.onl.end()) f->second.play = play;
        }
        std::wstring gerr;   // extracao compartilhada com o player do PC e os downloads (cache de ~25 min)
        if (!GetMediaInfo(play, mi, gerr, &ctl->cancel)) { SendErr(c, 502, "nao_abriu"); return; }
    }
    if (!allowed()) return;
    std::wstring ssStr;
    bool mp3 = Mp3Encoder() == 1, pipeMode = online && mi.url.empty();
    if (online && durSrc <= 0 && mi.dur > 0) durSrc = mi.dur;
    if (!online && durSrc <= 0) durSrc = ProbeDurSec(localFile);
    // tamanho do stream (so com mp3 de taxa fixa e duracao conhecida): 192 kbps = 24000 bytes/s
    const double BPS = 24000.0;
    double rate = FxRate(fx);
    long long total = 0; bool ranged = false;
    if (mp3 && durSrc > startSec) { total = (long long)(((double)(durSrc - startSec) / rate) * BPS); ranged = total > 1000; }
    long long a = 0, bnd = total - 1; int status = 200;
    if (ranged && r.hasRange) {
        if (r.r0 < 0) { long long n = r.r1 > 0 ? r.r1 : 1; if (n > total) n = total; a = total - n; }
        else { a = r.r0; if (r.r1 >= 0 && r.r1 < bnd) bnd = r.r1; }
        if (a > bnd || a >= total) { Resp e; e.status = 416; e.body = "{}"; e.extra.push_back("Content-Range: bytes */" + std::to_string(total)); SendResp(c, e); return; }
        status = 206;
    }
    long long len = ranged ? (bnd - a + 1) : 0;
    double skipSec = ranged && a > 0 ? (double)a / BPS * rate : 0;   // byte pedido -> tempo na musica
    {   // cabecalho ja da para responder um HEAD (o Safari usa para descobrir tamanho e duracao)
        std::string h0 = std::string("HTTP/1.1 ") + (status == 206 ? "206 Partial Content" : "200 OK") + "\r\nContent-Type: " + (mp3 ? "audio/mpeg" : "audio/aac") + "\r\nCache-Control: no-store\r\nConnection: close\r\n";
        if (ranged) {
            h0 += "Accept-Ranges: bytes\r\nContent-Length: " + std::to_string(len) + "\r\n";
            if (status == 206) h0 += "Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(bnd) + "/" + std::to_string(total) + "\r\n";
        }
        h0 += SecHeaders() + "\r\n";
        if (r.method == "HEAD") { hostnet::SendAll(c, h0); return; }
    }
    std::vector<std::wstring> fa = { fonte::Ffmpeg(), L"-nostdin", L"-hide_banner", L"-loglevel", L"error" };
    double startAt = std::max(0, startSec) + skipSec;
    { char sb[32]; snprintf(sb, sizeof sb, "%.3f", startAt); ssStr = Utf8ToWide(sb); }
    std::wstring& ss = ssStr;
    bool doSeek = startAt > 0.001;
    if (!online) {
        for (const wchar_t* x : { L"-protocol_whitelist", L"file" }) fa.push_back(x);
        if (doSeek) { fa.push_back(L"-ss"); fa.push_back(ss); }
        fa.push_back(L"-i"); fa.push_back(localFile);
    } else if (!pipeMode) {
        if (mi.url.rfind("https://", 0) != 0 && mi.url.rfind("http://", 0) != 0) { SendErr(c, 502, "url"); return; }   // so http(s): nada de arquivo/protocolo local vindo de fora
        for (const wchar_t* x : { L"-protocol_whitelist", L"https,http,tls,tcp,crypto", L"-reconnect", L"1", L"-reconnect_streamed", L"1", L"-reconnect_delay_max", L"5" }) fa.push_back(x);
        if (doSeek) { fa.push_back(L"-ss"); fa.push_back(ss); }
        if (!mi.headers.empty()) { std::string hs; for (auto& x : mi.headers) { std::string l; for (char ch : x) if (ch != '\r' && ch != '\n') l.push_back(ch); hs += l + "\r\n"; } fa.push_back(L"-headers"); fa.push_back(Utf8ToWide(hs)); }
        fa.push_back(L"-i"); fa.push_back(Utf8ToWide(mi.url));
    } else { fa.push_back(L"-protocol_whitelist"); fa.push_back(L"pipe"); fa.push_back(L"-i"); fa.push_back(L"pipe:0"); if (doSeek) { fa.push_back(L"-ss"); fa.push_back(ss); } }
    if (fx.Any()) { fa.push_back(L"-filter_complex"); fa.push_back(Utf8ToWide(FxGraph(fx))); fa.push_back(L"-map"); fa.push_back(L"[o]"); }
    else for (const wchar_t* x : { L"-vn", L"-sn", L"-dn" }) fa.push_back(x);
    for (const wchar_t* x : { L"-map_metadata", L"-1", L"-ac", L"2", L"-ar", L"44100" }) fa.push_back(x);
    if (mp3) for (const wchar_t* x : { L"-id3v2_version", L"0", L"-write_xing", L"0" }) fa.push_back(x);   // stream puro, sem cabecalho de arquivo
    if (mp3) for (const wchar_t* x : { L"-c:a", L"libmp3lame", L"-b:a", L"192k", L"-f", L"mp3", L"pipe:1" }) fa.push_back(x);
    else for (const wchar_t* x : { L"-c:a", L"aac", L"-b:a", L"192k", L"-f", L"adts", L"pipe:1" }) fa.push_back(x);
    Proc dec, src; std::thread pump;
    if (!dec.Start(fa, true, false, pipeMode)) { SendErr(c, 503, "ffmpeg"); return; }
    if (pipeMode) {
        auto ya = fonte::CmdTocar(play);
        if (!fonte::Abrir(src, ya, true, false, false)) { dec.Kill(); dec.Wait(); SendErr(c, 503, "sem_ferramentas"); return; }
        pump = std::thread([&] { char b[65536]; for (;;) { long n = src.ReadOut(b, sizeof b); if (n <= 0) break; if (!dec.WriteIn(b, (size_t)n)) break; } dec.CloseIn(); });
    }
    { std::lock_guard<std::mutex> lk(ctl->m); ctl->a = &dec; ctl->b = pipeMode ? &src : nullptr; }
    if (!allowed()) { dec.Kill(); if (pipeMode) src.Kill(); }
    std::string head = std::string("HTTP/1.1 ") + (status == 206 ? "206 Partial Content" : "200 OK") + "\r\nContent-Type: " + (mp3 ? "audio/mpeg" : "audio/aac") + "\r\nCache-Control: no-store\r\nConnection: close\r\n";
    if (ranged) {
        head += "Accept-Ranges: bytes\r\nContent-Length: " + std::to_string(len) + "\r\n";
        if (status == 206) head += "Content-Range: bytes " + std::to_string(a) + "-" + std::to_string(bnd) + "/" + std::to_string(total) + "\r\n";
    }
    head += SecHeaders() + "\r\n";
    bool okHead = hostnet::SendAll(c, head);
    char buf[32768]; long long sent = 0;
    while (okHead) {
        if (!allowed()) { hostnet::Abort(c); break; }
        long n = dec.ReadOut(buf, sizeof buf); if (n <= 0) break;
        if (ranged) { if (sent + n > len) n = (long)(len - sent); if (n <= 0) break; }
        if (!hostnet::SendAll(c, buf, (size_t)n)) break;
        sent += n;
        if (ranged && sent >= len) break;
    }
    if (okHead && ranged && sent > 0 && sent < len && allowed()) {   // o ffmpeg acabou antes do tamanho anunciado: completa (o navegador nao fica esperando)
        static const char zeros[8192] = { 0 };
        while (sent < len) { long n = (long)std::min<long long>((long long)sizeof zeros, len - sent); if (!hostnet::SendAll(c, zeros, (size_t)n)) break; sent += n; }
    }
    if (okHead && sent == 0 && !ctl->cancel.load() && online && !pipeMode) ForgetMediaInfo(play);   // link direto nao rendeu nada: extrai de novo na proxima
    if (ctl->cancel.load() || s.runGen.load() != rg) hostnet::Abort(c);   // parado de fora (troca de faixa, REMOVER, online desligado, Host desligado): corta ja
    { std::lock_guard<std::mutex> lk(ctl->m); ctl->a = nullptr; ctl->b = nullptr; }
    dec.Kill(); if (pipeMode) src.Kill();
    if (pump.joinable()) pump.join();
    dec.Wait(); if (pipeMode) src.Wait();
}

// ---- stems do celular -------------------------------------------------------------------------
// Chave igual a do PC (mesmo arquivo/link = mesma separacao no cache). Com a trava.
inline bool StemSourceLocked(const std::string& id, std::wstring& srcId, bool& online, std::wstring& play, std::wstring& title, std::wstring& artist, int& dur) {
    State& s = St();
    auto bi = s.byId.find(id);
    if (bi != s.byId.end()) { const HTrack& t = s.tracks[bi->second]; srcId = t.path; online = false; title = t.title; artist = t.artist; dur = t.dur; return true; }
    auto oi = s.onl.find(id);
    if (oi != s.onl.end()) { srcId = oi->second.url; online = true; play = oi->second.play; title = oi->second.title; artist = oi->second.artist; dur = oi->second.dur; return true; }
    return false;
}
inline std::string StemStatusJson(const std::wstring& key) {
    if (stems::Complete(key)) return "{\"instalado\":true,\"estado\":\"pronto\",\"pct\":100}";
    bool inst = stems::InstalledCached();
    auto j = stems::Find(key); int st = j ? j->state.load() : -1;
    static const char* nome[] = { "fila", "baixando", "separando", "pronto", "falhou", "cancelado" };
    std::string e = st >= 0 && st <= 5 ? nome[st] : "nao";
    std::string o = std::string("{\"instalado\":") + (inst ? "true" : "false") + ",\"estado\":\"" + e + "\",\"pct\":" + std::to_string(j ? j->pct.load() : 0);
    if (st == stems::S_FAILED && j) { std::lock_guard<std::mutex> lk(j->m); o += ",\"erro\":" + JStr(j->err); }
    return o + "}";
}

// ---- ritmo (onda do celular) ------------------------------------------------------------------
// Energia e "batida" a cada 25 ms (0..255), calculadas no PC decodificando mono a 8 kHz. A pagina desenha
// a onda sincronizada com o tempo do <audio>, sem Web Audio (que para quando o iPhone bloqueia a tela).
struct RhythmCache { std::mutex m; std::map<std::string, std::pair<std::string, long long>> map; };
inline RhythmCache& RC() { static RhythmCache* r = new RhythmCache(); return *r; }
inline std::string RhythmFrom(const std::vector<std::wstring>& ffIn, const std::atomic<bool>* cancel) {
    std::vector<std::wstring> a = { fonte::Ffmpeg(), L"-nostdin", L"-hide_banner", L"-loglevel", L"error" };
    for (auto& x : ffIn) a.push_back(x);
    for (const wchar_t* x : { L"-vn", L"-ac", L"1", L"-ar", L"8000", L"-f", L"f32le", L"pipe:1" }) a.push_back(x);
    CapResult r = RunCapture(a, 120000, cancel, nullptr, (size_t)64 * 1024 * 1024);
    size_t n = r.out.size() / 4; if (n < 800) return "";
    const float* x = (const float*)(const void*)r.out.data();
    const size_t hop = 200;   // 25 ms a 8 kHz
    std::vector<float> full, low; float lp = 0;
    for (size_t i = 0; i + hop <= n; i += hop) {
        double sf = 0, sl = 0;
        for (size_t k = 0; k < hop; ++k) { float v = x[i + k]; lp += (v - lp) * 0.111f; sf += (double)v * v; sl += (double)lp * lp; }
        full.push_back((float)sqrt(sf / hop)); low.push_back((float)sqrt(sl / hop));
    }
    std::vector<float> on(full.size(), 0.0f); float emax = 0.0001f;
    for (size_t i = 0; i < full.size(); ++i) { emax = std::max(emax, full[i]); if (i) on[i] = std::max(0.0f, low[i] - low[i - 1]) * 1.5f + std::max(0.0f, full[i] - full[i - 1]); }
    std::string o = "{\"hop\":25,\"e\":["; char b[8];
    for (size_t i = 0; i < full.size(); ++i) { snprintf(b, sizeof b, i ? ",%d" : "%d", (int)std::min(255.0f, full[i] / emax * 255.0f)); o += b; }
    o += "],\"b\":[";
    const size_t W = 120;   // normaliza a batida pelo maior valor dos 3 s em volta
    for (size_t i = 0; i < on.size(); ++i) {
        float mx = 0.0001f; for (size_t k = i > W ? i - W : 0; k < std::min(on.size(), i + W); ++k) mx = std::max(mx, on[k]);
        float v = on[i] / mx; v = v > 0.35f ? (v - 0.35f) / 0.65f : 0.0f;
        snprintf(b, sizeof b, i ? ",%d" : "%d", (int)(v * 255.0f)); o += b;
    }
    return o + "]}";
}

// ------------------------------------------------------------ rotas -------
inline void Serve(hsock_t c, const hostnet::Peer& peer) {
    State& s = St();
    Req r; r.ip = peer.ip; r.rateKey = hostnet::RateKey(peer);
    r.loopback = (peer.family == AF_INET && peer.addr[0] == 127) || (peer.family == AF_INET6 && peer.ip == "::1");
    r.trustCf = r.loopback && s.tunRunning.load();
    int err = 400;
    if (!ReadReq(c, r, err)) { if (err) { Resp e; e.status = err; e.body = "{\"erro\":\"pedido\"}"; SendResp(c, e); } return; }
    if (!HostHeaderOk(r)) { SendErr(c, 421, "host"); return; }
    Device dev; bool auth = false;
    {
        std::lock_guard<std::mutex> lk(s.m);
        auth = AuthDevLocked(r.cookieDev, r.ip, dev);   // o limite maior so vale para token de verdade
        if (!RateOk(r.rateKey, auth)) { Resp e; e.status = 429; e.body = "{\"erro\":\"calma\"}"; e.extra.push_back("Retry-After: 10"); SendResp(c, e); return; }
    }
    const std::string& P = r.path;
    bool isGet = r.method == "GET" || r.method == "HEAD";
    // ---- estaticos (sem login)
    if (isGet) {
        Resp o; o.cache = true;
        if (P == "/" || P == "/index.html") { std::string acc; { std::lock_guard<std::mutex> lk(s.m); acc = s.opt.accent; } o.cache = false; o.ctype = "text/html; charset=utf-8"; o.body = ReplaceAll(ReplaceAll(hostweb::INDEX_HTML, "@ACCENT@", acc), "@BUILD@", BuildTag()); SendResp(c, o); return; }
        if (P == "/app.js") { o.ctype = "text/javascript; charset=utf-8"; o.body = hostweb::APP_JS; SendResp(c, o); return; }
        if (P == "/app.css") { o.ctype = "text/css; charset=utf-8"; o.body = hostweb::APP_CSS; SendResp(c, o); return; }
        if (P == "/manifest.webmanifest") { o.ctype = "application/manifest+json"; o.body = hostweb::MANIFEST_JSON; SendResp(c, o); return; }
        if (P == "/sw.js") { o.cache = false; o.ctype = "text/javascript; charset=utf-8"; o.body = hostweb::SW_JS; SendResp(c, o); return; }
        if (P == "/icon.png") { SendFile(c, r, Config::Join(Config::Join(Config::AssetDir(), L"branding"), L"icon.png"), "image/png", true); return; }
    }
    if (P == "/api/ping") {
        // A pagina "conectar" (aberta pelo WhatsApp, outra origem) usa para achar o PC na rede local: para
        // outra origem sai so {"app":"remix"} (sem nome do PC nem versao). Nome e versao so na mesma origem.
        std::vector<std::string> cors = { "Access-Control-Allow-Origin: *", "Access-Control-Allow-Methods: GET", "Access-Control-Allow-Private-Network: true", "Access-Control-Max-Age: 600" };
        if (r.method == "OPTIONS") { Resp o; o.status = 204; o.extra = cors; SendResp(c, o); return; }
        if (r.h.count("origin")) { SendJson(c, 200, "{\"app\":\"remix\"}", cors); return; }
        std::wstring nm; { std::lock_guard<std::mutex> lk(s.m); nm = s.hostName; }
        SendJson(c, 200, "{\"app\":\"remix\",\"v\":" + JStr(REMIX_VERSAO) + ",\"nome\":" + JStr(nm) + "}"); return;
    }
    if (r.method == "POST" && r.h["x-remix"] != "1") { SendErr(c, 403, "origem"); return; }
    // ---- pareamento por PIN
    if (P == "/api/parear") {
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::string pin = JGet(r.body, "pin"), nome = CleanName(JGet(r.body, "nome"), 40); bool lembrar = JGet(r.body, "lembrar") != "false"; if (nome.empty()) nome = "Celular";
        std::lock_guard<std::mutex> lk(s.m);
        if (PairLocked(r.rateKey)) { SendJson(c, 429, "{\"erro\":\"travado\"}", { "Retry-After: 60" }); return; }
        if (!DigitsOnly(pin, 4, 12) || !ConstEq(pin, s.opt.pin)) { PairFail(r.rateKey); SendErr(c, 401, "pin"); return; }
        PairOk(r.rateKey); ExpireReqs();
        int mine = 0; for (auto& q : s.reqs) if (q.ip == r.ip && q.estado == 0) mine++;
        if (mine >= 3 || s.reqs.size() >= 20) { SendJson(c, 429, "{\"erro\":\"pedidos\"}"); return; }
        PairReq q; q.id = RandomHex(8); q.name = nome; q.ip = r.ip; q.created = NowSec(); q.viaTunnel = r.viaTunnel; q.lembrar = lembrar; s.reqs.push_back(q); Bump();
        AppPost(EV_HOST_PEDIDO, Utf8ToWide(q.id), 0);
        SendJson(c, 200, "{\"req\":\"" + q.id + "\"}"); return;
    }
    // ---- pareamento por QR (o token aparece so na tela do PC; vale 10 min e uma vez)
    if (P == "/api/parear/qr") {
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::string tok = JGet(r.body, "token"), nome = CleanName(JGet(r.body, "nome"), 40); bool lembrar = JGet(r.body, "lembrar") != "false"; if (nome.empty()) nome = "Celular";
        std::lock_guard<std::mutex> lk(s.m); ExpireReqs();
        if (PairLocked(r.rateKey)) { SendJson(c, 429, "{\"erro\":\"travado\"}", { "Retry-After: 60" }); return; }
        if (!IsHex(tok, 32) || s.qrToken.empty() || !ConstEq(tok, s.qrToken)) { PairFail(r.rateKey); SendErr(c, 401, "qr"); return; }
        PairOk(r.rateKey); s.qrToken.clear(); Bump();   // usado: o painel gera outro
        if (s.opt.qrConfirm) {
            if (s.reqs.size() >= 20) { SendJson(c, 429, "{\"erro\":\"pedidos\"}"); return; }
            PairReq q; q.id = RandomHex(8); q.name = nome; q.ip = r.ip; q.created = NowSec(); q.viaTunnel = r.viaTunnel; q.viaQr = true; q.lembrar = lembrar; s.reqs.push_back(q); Bump();
            AppPost(EV_HOST_PEDIDO, Utf8ToWide(q.id), 0);
            SendJson(c, 200, "{\"estado\":\"pendente\",\"req\":\"" + q.id + "\"}"); return;
        }
        Device d = NewDevice(nome, r.ip, lembrar);
        AppPost(EV_HOST_STATUS, L"\"" + Utf8ToWide(nome) + L"\" foi vinculado pelo QR code. Libere o que ele pode ouvir no painel HOST.", 0);
        SendJson(c, 200, "{\"estado\":\"aceito\"}", { CookieFor(d.token, lembrar, r.viaTunnel) }); return;
    }
    if (P == "/api/entrar") {   // chave permanente do aparelho (link/QR de religar): vale em qualquer endereco
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::string key = JGet(r.body, "chave");
        std::lock_guard<std::mutex> lk(s.m);
        if (PairLocked(r.rateKey)) { SendJson(c, 429, "{\"erro\":\"travado\"}", { "Retry-After: 60" }); return; }
        if (!IsHex(key, 32)) { PairFail(r.rateKey); SendErr(c, 401, "chave"); return; }
        for (auto& d : s.devs) {
            if (d.key.empty() || !ConstEq(key, d.key)) continue;
            PairOk(r.rateKey);
            d.ip = r.ip; d.lastSeen = NowSec(); d.persist = true;
            if (d.lastSeen - d.savedSeen > 3600) { d.savedSeen = d.lastSeen; SaveIni(); }
            Bump();
            SendJson(c, 200, "{\"ok\":1,\"nome\":" + JStrA(d.name) + "}", { CookieFor(d.token, true, r.viaTunnel) });
            return;
        }
        PairFail(r.rateKey); SendErr(c, 401, "chave"); return;
    }
    if (P == "/api/parear/estado") {
        std::string id = QueryGet(r.query, "req");
        if (!IsHex(id, 16)) { SendErr(c, 400, "req"); return; }
        std::lock_guard<std::mutex> lk(s.m); ExpireReqs();
        for (auto& q : s.reqs) if (q.id == id && q.ip == r.ip) {
            if (q.estado == 1) {
                if (q.token.empty()) { SendJson(c, 200, "{\"estado\":\"expirado\"}"); return; }   // o cookie so sai uma vez
                std::string ck = CookieFor(q.token, q.lembrar, r.viaTunnel); q.token.clear();
                SendJson(c, 200, "{\"estado\":\"aceito\"}", { ck }); return;
            }
            SendJson(c, 200, std::string("{\"estado\":\"") + (q.estado == 2 ? "recusado" : "pendente") + "\"}"); return;
        }
        SendJson(c, 200, "{\"estado\":\"expirado\"}"); return;
    }
    // ---- daqui para baixo so aparelho vinculado
    if (!auth) { SendErr(c, 401, "pareie"); return; }
    if (P == "/api/estado") {
        fonte::Garantir(); bool tools = fonte::Configurada() && fonte::FfmpegOk();
        std::string o;
        { std::lock_guard<std::mutex> lk(s.m);
          o = "{\"host\":" + JStr(s.hostName) + ",\"dispositivo\":" + JStrA(dev.name) + ",\"id\":\"" + dev.id + "\",\"v\":" + JStr(REMIX_VERSAO) +
              ",\"biblioteca\":" + (dev.lib ? "true" : "false") + ",\"online\":" + (s.opt.onlineOk && tools ? "true" : "false") + ",\"faixas\":" + std::to_string(dev.lib ? s.libIds.size() : 0) + ",\"temporario\":" + (dev.persist ? "false" : "true") + ",\"stems\":" + (stems::InstalledCached() ? "true" : "false") + "}"; }
        SendJson(c, 200, o); return;   // envia sem a trava: celular lento nao congela o Host
    }
    if (P == "/api/biblioteca") {
        std::string o = "{\"faixas\":[";
        { std::lock_guard<std::mutex> lk(s.m); if (dev.lib) { bool first = true; for (auto& kv : s.libIds) { std::string j = ItemJson(kv.first); if (j.empty()) continue; if (!first) o += ","; first = false; o += j; } } }
        SendJson(c, 200, o + "]}"); return;
    }
    if (P == "/api/playlists") {
        std::string o;
        {
            std::lock_guard<std::mutex> lk(s.m);
            std::set<std::string> vis = Visible(dev);   // monta uma vez (nada de conferir faixa por faixa em todas as listas)
            o = "{\"pc\":["; bool first = true;
            for (auto& p : s.pls) { if (!HostedTo(p.slug, dev.id)) continue; if (!first) o += ","; first = false; o += "{\"slug\":" + JStr(p.slug) + ",\"nome\":" + JStr(p.name) + ",\"itens\":" + ItemsJson(vis, p.ids) + "}"; }
            o += "],\"minhas\":["; first = true;
            for (auto& p : s.dpls) { if (p.dev != dev.id) continue; if (!first) o += ","; first = false; o += "{\"slug\":" + JStrA(p.slug) + ",\"nome\":" + JStrA(p.name) + ",\"compartilhar\":" + (p.share ? "true" : "false") + ",\"liberada\":" + (p.pcOk ? "true" : "false") + ",\"itens\":" + ItemsJson(vis, p.ids) + "}"; }
            o += "],\"compartilhadas\":["; first = true;
            for (auto& p : s.dpls) {
                if (p.dev == dev.id || !p.share || !p.pcOk) continue;
                const Device* dono = DevById(p.dev); if (!dono) continue;
                if (!first) o += ","; first = false; o += "{\"slug\":" + JStrA(p.dev + "-" + p.slug) + ",\"nome\":" + JStrA(p.name) + ",\"dono\":" + JStrA(dono->name) + ",\"itens\":" + ItemsJson(vis, p.ids) + "}";
            }
            o += "]}";
        }
        SendJson(c, 200, o); return;
    }
    if (P == "/api/minhas") {
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::string acao = JGet(r.body, "acao"), slug = JGet(r.body, "slug"), nome = CleanName(JGet(r.body, "nome"), 60), id = JGet(r.body, "id"), valor = JGet(r.body, "valor");
        int st = 200; std::string out = "{\"ok\":1}";
        [&] {
            std::lock_guard<std::mutex> lk(s.m);
            if (acao == "criar") {
                int n = 0; for (auto& p : s.dpls) if (p.dev == dev.id) n++;
                if (n >= 50) { st = 400; out = ErrJson("limite"); return; }
                if (nome.empty()) nome = "Playlist";
                DevPlaylist p; p.dev = dev.id; p.name = nome; p.slug = DevSlug(nome); s.dpls.push_back(p); SaveIni(); Bump();
                out = "{\"ok\":1,\"slug\":" + JStrA(p.slug) + "}"; return;
            }
            if (!SafeSlug(slug)) { st = 404; out = ErrJson("playlist"); return; }
            DevPlaylist* pl = nullptr; for (auto& p : s.dpls) if (p.dev == dev.id && p.slug == slug) { pl = &p; break; }
            if (!pl) { st = 404; out = ErrJson("playlist"); return; }   // playlist de outro aparelho: nunca editavel daqui
            // Playlist ja liberada que muda (nome ou faixa nova): volta a pedir liberacao no PC. Assim ninguem
            // passa a mostrar outra coisa para os outros aparelhos depois que o PC olhou e liberou.
            auto relock = [&] { if (pl->share && pl->pcOk) { pl->pcOk = false; AppPost(EV_HOST_STATUS, L"\"" + Utf8ToWide(dev.name) + L"\" mudou a playlist compartilhada \"" + Utf8ToWide(pl->name) + L"\": ela saiu dos outros aparelhos até você liberar de novo no painel HOST.", 0); } };
            if (acao == "renomear") { if (nome.empty()) { st = 400; out = ErrJson("nome"); return; } if (nome != pl->name) { relock(); pl->name = nome; } }
            else if (acao == "apagar") { s.dpls.erase(std::remove_if(s.dpls.begin(), s.dpls.end(), [&](const DevPlaylist& p) { return p.dev == dev.id && p.slug == slug; }), s.dpls.end()); }
            else if (acao == "add") { if (!IsId(id) || !CanSee(dev, id)) { st = 404; out = ErrJson("faixa"); return; } if (pl->ids.size() >= 5000) { st = 400; out = ErrJson("limite"); return; } if (!InList(pl->ids, id)) { relock(); pl->ids.push_back(id); } }
            else if (acao == "addvarios") {   // playlist inteira de um link: um pedido so em vez de um por musica
                int n = 0;
                for (size_t i = 0; i + 16 <= r.body.size(); ++i) {
                    std::string cand = r.body.substr(i, 16);
                    if (!IsId(cand) || (i > 0 && isalnum((unsigned char)r.body[i - 1]))) continue;
                    if (i + 16 < r.body.size() && isalnum((unsigned char)r.body[i + 16])) continue;
                    i += 15;
                    if (!CanSee(dev, cand) || InList(pl->ids, cand)) continue;
                    if (pl->ids.size() >= 5000 || n >= 500) break;
                    if (!n) relock();
                    pl->ids.push_back(cand); n++;
                }
                out = "{\"ok\":1,\"n\":" + std::to_string(n) + "}";
            }
            else if (acao == "remover") { pl->ids.erase(std::remove(pl->ids.begin(), pl->ids.end(), id), pl->ids.end()); }
            else if (acao == "compartilhar") {
                bool on = valor == "true" || valor == "1";
                if (on && !pl->share) AppPost(EV_HOST_STATUS, L"\"" + Utf8ToWide(dev.name) + L"\" quer compartilhar a playlist \"" + Utf8ToWide(pl->name) + L"\" com os outros aparelhos. Libere no painel HOST.", 0);
                pl->share = on; if (!on) pl->pcOk = false;
            }
            else { st = 400; out = ErrJson("acao"); return; }
            SaveIni(); AuthChanged();
        }();
        SendJson(c, st, out); return;
    }
    if (P == "/api/descobrir") {   // novidades e recomendacoes da tela inicial (as mesmas do PC)
        if (!s.opt.onlineOk) { SendJson(c, 200, "{\"off\":1,\"fileiras\":[]}"); return; }
        desc::Atualizar(false, nullptr);
        desc::Home h = desc::Copia();
        std::string o = "{\"at\":" + std::to_string(h.at) + ",\"carregando\":" + (desc::Carregando() ? "1" : "0") + ",\"fileiras\":[";
        bool p1 = true;
        for (auto& f : h.fileiras) {
            if (!p1) o += ","; p1 = false;
            o += "{\"titulo\":" + JStr(f.titulo) + ",\"nota\":" + JStr(f.nota) + ",\"chave\":" + JStr(f.chave) + ",\"itens\":[";
            bool p2 = true;
            for (auto& it : f.itens) {
                if (!p2) o += ","; p2 = false;
                std::string cid;
                if (AllowedThumbUrl(it.capa)) { cid = DescCapaId(it.capa); std::lock_guard<std::mutex> lk(s.m); s.descCapas[cid] = it.capa; }
                o += "{\"t\":" + JStr(it.titulo) + ",\"s\":" + JStr(it.sub) + ",\"c\":" + JStrA(cid) +
                     ",\"l\":" + JStr(it.link) + ",\"k\":" + std::to_string((int)it.kind) + ",\"d\":" + std::to_string(it.dur) + "}";
            }
            o += "]}";
        }
        o += "]}";
        SendJson(c, 200, o); return;
    }
    if (P.rfind("/api/desccapa/", 0) == 0) {   // capa de uma novidade (o PC baixa e guarda; o celular nao fala com a internet)
        std::string id = P.substr(14);
        if (!IsHex(id, 16)) { SendErr(c, 404, "capa"); return; }
        std::wstring url;
        { std::lock_guard<std::mutex> lk(s.m); auto it = s.descCapas.find(id); if (it != s.descCapas.end()) url = it->second; }
        if (url.empty() || !AllowedThumbUrl(url)) { SendErr(c, 404, "capa"); return; }
        hostnet::SetTimeout(c, 60000);
        std::wstring f = FetchThumb(url, true);
        if (f.empty()) { SendErr(c, 404, "capa"); return; }
        SendFile(c, r, f, "image/jpeg", true, nullptr); return;
    }
    if (P == "/api/minhachave") {   // o aparelho ja vinculado pega a propria chave (botao "copiar link deste aparelho")
        std::string key; { std::lock_guard<std::mutex> lk(s.m); for (auto& d : s.devs) if (d.id == dev.id) key = d.key; }
        if (key.empty()) { SendErr(c, 404, "chave"); return; }
        SendJson(c, 200, "{\"chave\":\"" + key + "\"}"); return;
    }
    if (P == "/api/sair") {
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        { std::lock_guard<std::mutex> lk(s.m); RevokeLocked(dev.id); }   // igual ao REMOVER do PC (tira tambem das listas de playlists)
        SendJson(c, 200, "{\"ok\":1}", { "Set-Cookie: remix_dev=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0" }); return;
    }
    if (P.rfind("/api/faixa/", 0) == 0 || P.rfind("/api/capa/", 0) == 0) {
        bool capa = P[5] == 'c'; std::string id = P.substr(capa ? 10 : 11);
        if (!IsId(id)) { SendErr(c, 404, "id"); return; }
        std::wstring path, cover, thumb; bool online = false;
        {
            std::lock_guard<std::mutex> lk(s.m);
            if (!CanSee(dev, id)) { SendErr(c, 404, "faixa"); return; }   // 404 e nao 403: nao confirma que o id existe
            auto it = s.byId.find(id);
            if (it != s.byId.end()) { path = s.tracks[it->second].path; cover = s.tracks[it->second].coverPath; }
            else { auto oi = s.onl.find(id); if (oi == s.onl.end()) { SendErr(c, 404, "faixa"); return; } online = true; thumb = oi->second.thumb; }
        }
        auto still = [&dev, id]() { std::lock_guard<std::mutex> lk(St().m); return StillAllowedLocked(dev, id); };
        if (capa) {
            if (online) { if (!AllowedThumbUrl(thumb)) { SendErr(c, 404, "capa"); return; } hostnet::SetTimeout(c, 60000); std::wstring f = FetchThumb(thumb, true); if (f.empty()) { SendErr(c, 404, "capa"); return; } SendFile(c, r, f, "image/jpeg", true, still); return; }
            if (cover.empty()) { SendErr(c, 404, "capa"); return; }
            SendFile(c, r, cover, ContentTypeFor(cover), true, still); return;
        }
        if (online) { SendErr(c, 404, "use_ouvir"); return; }
        {   // com efeito ou stem o PC converte (stream sem tamanho; avancar = ?t=)
            FxReq fx = ParseFx(QueryGet(r.query, "fx"), QueryGet(r.query, "stem"));
            if (fx.Any() || fx.stem) {
                std::wstring file = path;
                if (fx.stem) {
                    std::wstring key = stems::KeyFor(path, false);
                    if (!stems::Complete(key)) { SendJson(c, 409, StemStatusJson(key)); return; }
                    file = stems::FileFor(key, fx.stem);
                }
                int t = atoi(QueryGet(r.query, "t").c_str()); if (t < 0 || t > 24 * 3600) t = 0;
                int dsec = 0; { std::lock_guard<std::mutex> lk(s.m); auto bi = s.byId.find(id); if (bi != s.byId.end()) dsec = s.tracks[bi->second].dur; }
                ServeStream(c, r, dev, id, t, fx, false, file, dsec); return;
            }
        }
        SendFile(c, r, path, ContentTypeFor(path), false, still); return;
    }
    if (P == "/api/online/buscar") {
        std::string q = CleanName(QueryGet(r.query, "q"), 200); int fonte = atoi(QueryGet(r.query, "fonte").c_str()); if (fonte < 0 || fonte > 2) fonte = 0;
        if (q.empty()) { SendErr(c, 400, "q"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; } }
        fonte::Garantir();
        if (!fonte::Configurada()) { SendErr(c, 503, "sem_ferramentas"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (s.searching[dev.id] >= 1 || s.searchingAll >= 3) { SendErr(c, 429, "buscando"); return; } s.searching[dev.id]++; s.searchingAll++; }
        struct Done { std::string d; ~Done() { std::lock_guard<std::mutex> lk(St().m); if (--St().searching[d] <= 0) St().searching.erase(d); St().searchingAll--; } } done{ dev.id };
        hostnet::SetTimeout(c, 120000);
        // cancela a busca se o celular desistir (fechou a tela/conexao) ou se o Host desligar
        std::atomic<bool> cancel{ false }, fim{ false }; unsigned rg = s.runGen.load();
        std::thread watch([&] { while (!fim.load()) { if (s.runGen.load() != rg || hostnet::PeerGone(c)) { cancel = true; return; } std::this_thread::sleep_for(std::chrono::milliseconds(250)); } });
        std::vector<OTrack> res; std::wstring e;
        OnlineSearch(Utf8ToWide(q), fonte, res, e, &cancel, 20);
        fim = true; watch.join();
        if (cancel.load()) return;
        { std::vector<std::wstring> pre; for (size_t i = 0; i < res.size() && pre.size() < 3; ++i) if (HttpLink(res[i].play.empty() ? res[i].url : res[i].play)) pre.push_back(res[i].play.empty() ? res[i].url : res[i].play); PreResolve(pre); }   // tocar um dos primeiros comeca na hora
        std::string o = "{\"faixas\":["; bool first = true;
        {
            std::lock_guard<std::mutex> lk(s.m);
            if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; }
            if (s.onl.size() > 20000) {   // limpa o que ninguem usa (mantem o que esta em playlists)
                std::map<std::string, bool> keep; for (auto& p : s.dpls) for (auto& id : p.ids) keep[id] = true; for (auto& p : s.pls) for (auto& id : p.ids) keep[id] = true;
                for (auto it = s.onl.begin(); it != s.onl.end();) { if (!keep.count(it->first) && NowSec() - it->second.used > 3600) it = s.onl.erase(it); else ++it; }
            }
            for (auto& t : res) {
                if (t.url.empty() || !HttpLink(t.url)) continue;
                std::string id = OnlineId(t.url); OItem& it = s.onl[id];
                it.url = t.url; if (it.play.empty() && HttpLink(t.play)) it.play = t.play; it.title = t.title; it.artist = t.artist; it.thumb = t.thumb; it.dur = t.dur; it.used = NowSec();
                RememberSeen(dev.id, id);
                std::string j = ItemJson(id); if (j.empty()) continue; if (!first) o += ","; first = false; o += j;
            }
        }
        SendJson(c, 200, o + "]}"); return;
    }
    if (P.rfind("/api/online/ouvir/", 0) == 0) {
        std::string id = P.substr(18);
        if (!IsId(id)) { SendErr(c, 404, "id"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; } if (!CanSee(dev, id) || !s.onl.count(id)) { SendErr(c, 404, "faixa"); return; } }
        int t = atoi(QueryGet(r.query, "t").c_str()); if (t < 0 || t > 24 * 3600) t = 0;
        FxReq fx = ParseFx(QueryGet(r.query, "fx"), QueryGet(r.query, "stem"));
        if (fx.stem) {   // stem de musica online: toca o arquivo separado do cache
            std::wstring url; { std::lock_guard<std::mutex> lk(s.m); auto f = s.onl.find(id); if (f != s.onl.end()) url = f->second.url; }
            std::wstring key = stems::KeyFor(url, true);
            if (url.empty() || !stems::Complete(key)) { SendJson(c, 409, StemStatusJson(key)); return; }
            int dsec = 0; { std::lock_guard<std::mutex> lk(s.m); auto f = s.onl.find(id); if (f != s.onl.end()) dsec = f->second.dur; }
            ServeStream(c, r, dev, id, t, fx, false, stems::FileFor(key, fx.stem), dsec); return;
        }
        int dsec = 0; { std::lock_guard<std::mutex> lk(s.m); auto f = s.onl.find(id); if (f != s.onl.end()) dsec = f->second.dur; }
        ServeStream(c, r, dev, id, t, fx, true, L"", dsec); return;
    }
    if (P == "/api/online/listas") {   // celular procura playlists ou albuns prontos (catalogo publico do Deezer)
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; } }
        std::wstring q = Utf8ToWide(JGet(r.body, "q"));
        while (!q.empty() && (q.back() == L' ' || q.back() == L'\n' || q.back() == L'\r')) q.pop_back();
        if (q.empty() || q.size() > 120) { SendErr(c, 400, "busca"); return; }
        int tipo = atoi(JGet(r.body, "tipo").c_str());
        { std::lock_guard<std::mutex> lk(s.m); if (s.searching[dev.id] >= 1 || s.searchingAll >= 3) { SendErr(c, 429, "buscando"); return; } s.searching[dev.id]++; s.searchingAll++; }
        struct Done { std::string d; ~Done() { std::lock_guard<std::mutex> lk(St().m); if (--St().searching[d] <= 0) St().searching.erase(d); St().searchingAll--; } } done{ dev.id };
        hostnet::SetTimeout(c, 60000);
        std::vector<desc::Item> v = (tipo == 2) ? desc::BuscarAlbuns(q, 24) : desc::BuscarPlaylists(q, 24);
        std::string o = "{\"itens\":[";
        bool p1 = true;
        for (auto& it : v) {
            if (!p1) o += ","; p1 = false;
            std::string cid;
            if (AllowedThumbUrl(it.capa)) { cid = DescCapaId(it.capa); std::lock_guard<std::mutex> lk(s.m); s.descCapas[cid] = it.capa; }
            o += "{\"t\":" + JStr(it.titulo) + ",\"s\":" + JStr(it.sub) + ",\"c\":" + JStrA(cid) + ",\"l\":" + JStr(it.link) + ",\"k\":" + std::to_string((int)it.kind) + "}";
        }
        o += "]}";
        SendJson(c, 200, o); return;
    }
    if (P == "/api/letra") {   // letra da musica que o celular esta ouvindo (LRCLIB, guardada no PC)
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::string id = JGet(r.body, "id");
        if (!IsId(id)) { SendErr(c, 404, "faixa"); return; }
        std::wstring titulo, artista, chave; int dur = 0;
        {
            std::lock_guard<std::mutex> lk(s.m);
            if (!CanSee(dev, id)) { SendErr(c, 404, "faixa"); return; }
            auto bi = s.byId.find(id);
            if (bi != s.byId.end()) { const HTrack& t = s.tracks[bi->second]; titulo = t.title; artista = t.artist; chave = t.path; dur = t.dur; }
            else { auto oi = s.onl.find(id); if (oi == s.onl.end()) { SendErr(c, 404, "faixa"); return; } titulo = oi->second.title; artista = oi->second.artist; chave = oi->second.url; dur = oi->second.dur; }
        }
        letra::Letra L = letra::Para(chave, titulo, artista, dur, nullptr);
        std::string o = "{\"estado\":" + std::to_string(L.estado) + ",\"sync\":" + (L.sync ? "1" : "0") + ",\"fonte\":" + JStr(L.fonte) + ",\"linhas\":[";
        bool p1 = true;
        if (L.sync) for (auto& ln : L.linhas) { if (!p1) o += ","; p1 = false; o += "{\"ms\":" + std::to_string(ln.ms) + ",\"t\":" + JStr(ln.txt) + "}"; }
        o += "],\"texto\":" + JStr(L.sync ? std::wstring() : L.texto) + "}";
        SendJson(c, 200, o); return;
    }
    if (P == "/api/online/link") {   // celular cola um link de playlist/album (Spotify, YouTube, Deezer, Apple, SoundCloud)
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::wstring url = Utf8ToWide(JGet(r.body, "url"));
        while (!url.empty() && (url.back() == L' ' || url.back() == L'\n' || url.back() == L'\r')) url.pop_back();
        if (url.size() > 600 || !HttpLink(url)) { SendErr(c, 400, "link"); return; }
        OSrc src = DetectSource(url);
        if (src == OS_UNKNOWN || src == OS_OTHER || src == OS_GPM) { SendErr(c, 400, "link_nao_suportado"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; } }
        fonte::Garantir();
        if (!fonte::Configurada()) { SendErr(c, 503, "sem_ferramentas"); return; }
        { std::lock_guard<std::mutex> lk(s.m); if (s.searching[dev.id] >= 1 || s.searchingAll >= 3) { SendErr(c, 429, "buscando"); return; } s.searching[dev.id]++; s.searchingAll++; }
        struct Done { std::string d; ~Done() { std::lock_guard<std::mutex> lk(St().m); if (--St().searching[d] <= 0) St().searching.erase(d); St().searchingAll--; } } done{ dev.id };
        hostnet::SetTimeout(c, 300000);
        std::atomic<bool> cancel{ false }, fim{ false }; unsigned rg = s.runGen.load();
        std::thread watch([&] { while (!fim.load()) { if (s.runGen.load() != rg || hostnet::PeerGone(c)) { cancel = true; return; } std::this_thread::sleep_for(std::chrono::milliseconds(250)); } });
        OResolved res = ResolveLink(url, &cancel);
        fim = true; watch.join();
        if (cancel.load()) return;
        if (res.items.empty()) { SendJson(c, 502, "{\"erro\":\"link_vazio\",\"msg\":" + JStr(res.err.empty() ? L"Não achei músicas nesse link." : res.err) + "}"); return; }
        { std::vector<std::wstring> pre; for (size_t i = 0; i < res.items.size() && pre.size() < 3; ++i) { std::wstring pl = res.items[i].play.empty() ? res.items[i].url : res.items[i].play; if (HttpLink(pl) && !NeedsMatch(DetectSource(pl))) pre.push_back(pl); } PreResolve(pre); }
        std::string o = "{\"nome\":" + JStr(res.name) + ",\"fonte\":" + JStrA(WideToUtf8(SourceName(res.src))) + ",\"faixas\":[";
        bool first = true;
        {
            std::lock_guard<std::mutex> lk(s.m);
            if (!s.opt.onlineOk) { SendErr(c, 403, "online_desligado"); return; }
            for (auto& t : res.items) {
                if (t.url.empty() || !HttpLink(t.url)) continue;
                std::string id = OnlineId(t.url); OItem& it = s.onl[id];
                it.url = t.url; if (it.play.empty() && HttpLink(t.play)) it.play = t.play; it.title = t.title; it.artist = t.artist; it.thumb = t.thumb; it.dur = t.dur; it.used = NowSec();
                RememberSeen(dev.id, id);
                std::string j = ItemJson(id); if (j.empty()) continue;
                if (!first) o += ","; first = false; o += j;
            }
        }
        SendJson(c, 200, o + "]}"); return;
    }
    if (P == "/api/preparar") {   // o celular avisa quais sao as proximas: o PC ja extrai o link (cache de 25 min)
        if (r.method != "POST") { SendErr(c, 405, "metodo"); return; }
        std::vector<std::wstring> plays;
        {
            std::lock_guard<std::mutex> lk(s.m);
            if (s.opt.onlineOk) {
                for (size_t i = 0; i + 16 <= r.body.size() && plays.size() < 3; ++i) {
                    std::string cand = r.body.substr(i, 16);
                    if (!IsId(cand) || (i > 0 && isalnum((unsigned char)r.body[i - 1]))) continue;
                    if (i + 16 < r.body.size() && isalnum((unsigned char)r.body[i + 16])) continue;
                    i += 15;
                    if (!CanSee(dev, cand)) continue;
                    auto oi = s.onl.find(cand); if (oi == s.onl.end()) continue;
                    std::wstring pl = oi->second.play.empty() ? oi->second.url : oi->second.play;
                    if (HttpLink(pl) && !NeedsMatch(DetectSource(pl))) plays.push_back(pl);
                }
            }
        }
        if (!plays.empty()) { fonte::Garantir(); if (fonte::Configurada()) PreResolve(plays); }
        SendJson(c, 200, "{\"ok\":1}"); return;
    }
    if (P.rfind("/api/stems/", 0) == 0) {   // GET: como esta a separacao desta musica; POST: comecar
        std::string id = P.substr(11);
        if (!IsId(id)) { SendErr(c, 404, "id"); return; }
        std::wstring srcId, play, title, artist; bool online = false; int dur = 0;
        { std::lock_guard<std::mutex> lk(s.m); if (!CanSee(dev, id) || !StemSourceLocked(id, srcId, online, play, title, artist, dur)) { SendErr(c, 404, "faixa"); return; } }
        std::wstring key = stems::KeyFor(srcId, online);
        // separar pesa no PC: no maximo 4 esperando na fila (a musica atual sempre passa na frente)
        if (r.method == "POST" && !stems::Complete(key) && stems::InstalledCached() && (stems::Find(key) || stems::QueuedCount() < 4))
            stems::Request(srcId, online, play, title, artist, dur, JGet(r.body, "fila") != "true", [](const std::wstring& k, int st) { AppPost(EV_STEMS, k, st); });
        SendJson(c, 200, StemStatusJson(key)); return;
    }
    if (P.rfind("/api/ritmo/", 0) == 0) {   // energia + batida a cada 25 ms, para a onda do celular
        std::string id = P.substr(11);
        if (!IsId(id)) { SendErr(c, 404, "id"); return; }
        std::string sq = QueryGet(r.query, "stem"); int stemM = stems::ModeFromKey(Utf8ToWide(sq.size() < 20 ? sq : std::string()));
        std::wstring srcId, play, title, artist; bool online = false; int dur = 0;
        { std::lock_guard<std::mutex> lk(s.m); if (!CanSee(dev, id) || !StemSourceLocked(id, srcId, online, play, title, artist, dur)) { SendErr(c, 404, "faixa"); return; } }
        std::string ck = id + "|" + std::to_string(stemM);
        { std::lock_guard<std::mutex> lk(RC().m); auto hit = RC().map.find(ck); if (hit != RC().map.end()) { std::string j = hit->second.first; hit->second.second = NowSec(); SendJson(c, 200, j); return; } }
        static std::atomic<int> rodando{ 0 };   // decodificar a musica inteira pesa: 2 de cada vez
        if (rodando.load() >= 2) { SendJson(c, 429, "{\"erro\":\"calma\"}", { "Retry-After: 3" }); return; }
        rodando++; struct Solta { ~Solta() { rodando--; } } solta;
        fonte::Garantir();
        const std::string vazio = "{\"hop\":25,\"e\":[],\"b\":[]}";
        if (!fonte::FfmpegOk()) { SendJson(c, 200, vazio); return; }
        std::vector<std::wstring> in;
        std::wstring stemFile = stemM ? stems::FileFor(stems::KeyFor(srcId, online), stemM) : L"";
        if (!stemFile.empty()) in = { L"-protocol_whitelist", L"file", L"-i", stemFile };
        else if (!online) in = { L"-protocol_whitelist", L"file", L"-i", srcId };
        else {
            std::wstring pl = play.empty() ? srcId : play;
            if (!HttpLink(pl) || NeedsMatch(DetectSource(pl))) { SendJson(c, 200, vazio); return; }
            MediaInfo mi; std::wstring e;
            if (!GetMediaInfo(pl, mi, e, nullptr) || mi.url.empty() || (mi.url.rfind("https://", 0) != 0 && mi.url.rfind("http://", 0) != 0)) { SendJson(c, 200, vazio); return; }
            in = { L"-protocol_whitelist", L"https,http,tls,tcp,crypto" };
            if (!mi.headers.empty()) { std::string hs; for (auto& x : mi.headers) { std::string l; for (char ch : x) if (ch != '\r' && ch != '\n') l.push_back(ch); hs += l + "\r\n"; } in.push_back(L"-headers"); in.push_back(Utf8ToWide(hs)); }
            in.push_back(L"-i"); in.push_back(Utf8ToWide(mi.url));
        }
        hostnet::SetTimeout(c, 120000);
        std::string j = RhythmFrom(in, nullptr);
        if (j.empty()) j = vazio;
        else { std::lock_guard<std::mutex> lk(RC().m); if (RC().map.size() >= 40) { auto old = RC().map.begin(); for (auto i = RC().map.begin(); i != RC().map.end(); ++i) if (i->second.second < old->second.second) old = i; RC().map.erase(old); } RC().map[ck] = { j, NowSec() }; }
        SendJson(c, 200, j); return;
    }
    SendErr(c, 404, "rota");
}
inline void AcceptLoop() {
    State& s = St();
    while (!s.stop.load()) {
        hsock_t ls4, ls6; std::vector<hostnet::Prefix64> own; bool lanOk, lan6;
        { std::lock_guard<std::mutex> lk(s.m); ls4 = s.ls; ls6 = s.ls6; own = s.own6; lanOk = s.opt.lanOk; lan6 = s.opt.lan6; }
        hsock_t ready = hostnet::WaitAccept(ls4, ls6, 500);
        if (ready == HSOCK_BAD) continue;
        hostnet::Peer peer; hsock_t c = hostnet::Accept(ready, peer);
        if (c == HSOCK_BAD) { if (s.stop.load()) break; std::this_thread::sleep_for(std::chrono::milliseconds(20)); continue; }
        if (!hostnet::PeerAllowed(peer, lanOk, lan6, own)) { hostnet::Close(c); continue; }   // fora da rede local e nao e o tunel: nem responde
        // Limites: 64 conexoes no total e 12 por origem da rede local (uma origem so nao ocupa tudo). O tunel
        // chega todo por 127.0.0.1, entao a origem local tem folga maior (48).
        bool lb = (peer.family == AF_INET && peer.addr[0] == 127) || (peer.family == AF_INET6 && peer.ip == "::1");
        std::string key = hostnet::RateKey(peer); bool full = false;
        {
            std::lock_guard<std::mutex> lk(s.m);
            int& mine = s.connsBy[key];
            if (s.conns.load() >= 64 || mine >= (lb ? 48 : 12)) { full = true; if (mine <= 0) s.connsBy.erase(key); }
            else { mine++; s.live[c] = key; s.conns++; }
        }
        if (full) { hostnet::SendAll(c, "HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\nRetry-After: 2\r\n\r\n"); hostnet::Close(c); continue; }
        auto release = [c, key]() { State& st = St(); { std::lock_guard<std::mutex> lk(st.m); st.live.erase(c); auto it = st.connsBy.find(key); if (it != st.connsBy.end() && --it->second <= 0) st.connsBy.erase(it); } hostnet::Close(c); st.conns--; };
        try { std::thread([c, peer, release]() { Serve(c, peer); release(); }).detach(); }
        catch (...) { release(); }
    }
}

// ------------------------------------------------------------ tunel -------
inline std::wstring CloudflaredPath() {
    namespace fs = std::filesystem; std::error_code ec;
#ifdef _WIN32
    const wchar_t* exe = L"cloudflared.exe";
#else
    const wchar_t* exe = L"cloudflared";
#endif
    std::wstring t = Config::Join(Config::Join(Config::AssetDir(), L"tools"), exe);
    if (fs::exists(fs::path(t), ec)) return t;
#ifdef _WIN32
    wchar_t buf[MAX_PATH]; if (SearchPathW(NULL, L"cloudflared.exe", NULL, MAX_PATH, buf, NULL)) return buf;
#else
    const char* pathEnv = std::getenv("PATH"); std::string p = pathEnv ? pathEnv : ""; size_t pos = 0;
    while (pos <= p.size()) { size_t c = p.find(':', pos); std::string dir = p.substr(pos, c == std::string::npos ? std::string::npos : c - pos); if (!dir.empty()) { fs::path cand = fs::path(dir) / "cloudflared"; if (fs::exists(cand, ec) && access(cand.c_str(), X_OK) == 0) return cand.wstring(); } if (c == std::string::npos) break; pos = c + 1; }
    if (const char* home = std::getenv("HOME")) { fs::path c = fs::path(home) / ".local" / "bin" / "cloudflared"; if (fs::exists(c, ec)) return c.wstring(); }
    for (const char* d : { "/usr/local/bin/cloudflared", "/usr/bin/cloudflared", "/opt/homebrew/bin/cloudflared" }) if (fs::exists(fs::path(d), ec)) return Utf8ToWide(d);
#endif
    return L"";
}
inline bool HaveCloudflared() { return !CloudflaredPath().empty(); }
inline void TunnelStop() {
    State& s = St(); s.tunCancel = true; s.tunGen++;
    if (s.tunTh.joinable()) s.tunTh.join();
    std::lock_guard<std::mutex> lk(s.m); s.tunUrl.clear(); s.tunPending.clear(); s.tunStatus = "desligado"; Bump();
}
// O link do quick tunnel aparece ANTES do nome existir no DNS (medido: ~1-2 s de NXDOMAIN). Quem
// abre nessa janela faz o roteador guardar "nao existe" por ate 30 min (SOA minimo do
// trycloudflare.com). Por isso: espera o cloudflared registrar a conexao, espera mais um pouco,
// confere que o link responde e SO ENTAO mostra/copia o link e gera o QR.
inline void VerifyTunnel(const std::string& url, unsigned gen) {
    State& s = St();
    for (int i = 0; i < 80 && s.tunGen.load() == gen; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));   // 8 s depois do registro
    for (int tent = 0; tent < 24 && s.tunGen.load() == gen && !s.tunCancel.load(); tent++) {
        std::string body;
        if (PlatformHttpGet(url + "/api/ping", body) && body.find("\"app\":\"remix\"") != std::string::npos) {
            { std::lock_guard<std::mutex> lk(s.m); if (s.tunGen.load() != gen) return; s.tunUrl = url; s.tunPending.clear(); s.tunStatus = "ligado"; }
            Bump(); AppPost(EV_HOST_STATUS, L"Túnel pronto e testado: " + Utf8ToWide(url), 1); return;
        }
        for (int i = 0; i < 50 && s.tunGen.load() == gen; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        std::lock_guard<std::mutex> lk(s.m); if (s.tunGen.load() == gen) { s.tunStatus = "conferindo o link (" + std::to_string(tent + 1) + ")..."; Bump(); }
    }
    std::lock_guard<std::mutex> lk(s.m);
    if (s.tunGen.load() != gen) return;
    s.tunUrl = url; s.tunPending.clear(); s.tunStatus = "ligado, mas nao consegui testar daqui"; Bump();
}
inline void TunnelStart(int port) {
    State& s = St();
    if (s.tunRunning.load()) return;
    if (s.tunTh.joinable()) s.tunTh.join();
    s.tunCancel = false; s.tunRunning = true; unsigned gen = ++s.tunGen;
    { std::lock_guard<std::mutex> lk(s.m); s.tunUrl.clear(); s.tunPending.clear(); s.tunStatus = "iniciando..."; }
    Bump();
    s.tunTh = std::thread([port, gen]() {
        State& st = St();
        std::wstring exe = CloudflaredPath();
        if (exe.empty()) { { std::lock_guard<std::mutex> lk(st.m); st.tunStatus = "cloudflared nao encontrado: instale pela sua distro"; } st.tunRunning = false; Bump(); AppPost(EV_HOST_STATUS, L"Túnel: cloudflared não encontrado (instale pela sua distro).", 0); return; }
        std::vector<std::wstring> args = { exe, L"tunnel", L"--url", L"http://127.0.0.1:" + std::to_wstring(port), L"--no-autoupdate" };
        std::string seenUrl; bool verifying = false;
        CapResult r = RunCapture(args, 0, &st.tunCancel, [&](const std::string& ln) {   // so linha a linha (keepMax 0): o tunel roda horas
            size_t p = ln.find("https://");
            if (p != std::string::npos && seenUrl.empty()) {
                size_t e = p; while (e < ln.size() && (isalnum((unsigned char)ln[e]) || ln[e] == '-' || ln[e] == '.' || ln[e] == ':' || ln[e] == '/')) e++;
                std::string url = ln.substr(p, e - p);
                while (!url.empty() && url.back() == '/') url.pop_back();
                if (url.size() > 8 && url.size() < 120 && url.find(".trycloudflare.com") != std::string::npos && url.find(' ') == std::string::npos) {
                    seenUrl = url; std::lock_guard<std::mutex> lk(St().m); St().tunPending = url; St().tunStatus = "aguardando o registro do túnel..."; Bump();
                }
            }
            if (!seenUrl.empty() && !verifying && ln.find("Registered tunnel connection") != std::string::npos) {
                verifying = true; { std::lock_guard<std::mutex> lk(St().m); St().tunStatus = "conferindo o link (DNS)..."; } Bump();
                std::string u = seenUrl; std::thread([u, gen]() { VerifyTunnel(u, gen); }).detach();
            }
        }, 0);
        bool cancel = st.tunCancel.load();
        { std::lock_guard<std::mutex> lk(st.m); if (st.tunGen.load() == gen) { st.tunUrl.clear(); st.tunPending.clear(); st.tunStatus = cancel ? "desligado" : (r.started ? "caiu (cloudflared fechou)" : "nao consegui iniciar o cloudflared"); } }
        st.tunRunning = false; Bump();
        if (!cancel) AppPost(EV_HOST_STATUS, r.started ? L"O túnel caiu. Ligue de novo no painel HOST." : L"Não consegui iniciar o cloudflared.", 0);
    });
}
inline void TunnelRestart(int port) { TunnelStop(); TunnelStart(port); }   // link novo (outro nome no DNS)

// ------------------------------------------------------------ API da UI ---
inline bool Running() { return St().running; }
inline std::string Start(const Options& o) {
    State& s = St();
    if (s.running) return "";
    if (!DigitsOnly(o.pin, 4, 12)) return "Defina um PIN de 4 a 12 numeros nas configuracoes.";
    if (o.port < 1024 || o.port > 65535) return "Porta invalida (1024 a 65535).";
    std::string err; hsock_t ls = hostnet::Listen(o.port, o.lanOk, err);
    if (ls == HSOCK_BAD) { std::lock_guard<std::mutex> lk(s.m); s.lastError = err; return err; }
    hsock_t ls6 = HSOCK_BAD; std::string err6;
    if (o.lanOk && o.lan6) ls6 = hostnet::Listen6(o.port, err6);
    LoadIni();
    hostnet::LocalAddrs la = hostnet::LocalAddresses();
    {
        std::lock_guard<std::mutex> lk(s.m);
        s.ls = ls; s.ls6 = ls6; s.opt = o; s.hostName = o.name.empty() ? Utf8ToWide(hostnet::HostName()) : o.name; s.lastError = err6;
        s.lanUrls.clear(); s.lan6Urls.clear(); s.own6 = la.pre6;
        if (o.lanOk) for (auto& ip : la.v4) s.lanUrls.push_back("http://" + ip + ":" + std::to_string(o.port));
        if (ls6 != HSOCK_BAD) for (auto& ip : la.v6) s.lan6Urls.push_back("http://[" + ip + "]:" + std::to_string(o.port));
        s.qrToken.clear();
    }
    s.stop = false; s.running = true;
    s.th = std::thread(AcceptLoop);
    Bump();
    return "";
}
inline void SetOptions(bool onlineOk, bool qrConfirm) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    if (s.opt.onlineOk && !onlineOk) for (auto& kv : s.streams) if (kv.second) kv.second->Stop();   // desligou: corta o que esta tocando
    s.opt.onlineOk = onlineOk; s.opt.qrConfirm = qrConfirm; AuthChanged();
}
inline void Stop() {
    State& s = St();
    if (!s.running) return;
    s.stop = true; s.runGen++;
    {   // derruba tudo que esta aberto (senao um download em andamento seguiria depois de desligar/religar)
        std::lock_guard<std::mutex> lk(s.m);
        for (auto& kv : s.streams) if (kv.second) kv.second->Stop();
        for (auto& kv : s.live) { hostnet::Abort(kv.first); hostnet::Wake(kv.first); }
    }
    if (s.th.joinable()) s.th.join();
    hostnet::Close(s.ls); hostnet::Close(s.ls6);
    for (int i = 0; i < 60 && s.conns.load() > 0; i++) std::this_thread::sleep_for(std::chrono::milliseconds(50));   // ate 3 s para as conexoes sairem (a porta fica livre para religar)
    { std::lock_guard<std::mutex> lk(s.m); s.ls = HSOCK_BAD; s.ls6 = HSOCK_BAD; s.reqs.clear(); s.qrToken.clear(); }
    s.running = false;
    TunnelStop();
    Bump();
}
// true = o pedido ainda estava valendo. false = expirou/ja foi respondido (a UI avisa em vez de dizer "aceito").
inline bool Approve(const std::string& reqId, bool ok) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    for (auto& q : s.reqs) if (q.id == reqId && q.estado == 0) {
        if (NowSec() - q.created > 180) return false;
        if (!ok) { q.estado = 2; Bump(); return true; }
        Device d = NewDevice(q.name, q.ip, q.lembrar);
        q.estado = 1; q.token = d.token; q.devId = d.id; Bump(); return true;
    }
    return false;
}
inline void Revoke(const std::string& devId) { State& s = St(); std::lock_guard<std::mutex> lk(s.m); RevokeLocked(devId); }
inline void SetDeviceLib(const std::string& devId, bool on) { State& s = St(); std::lock_guard<std::mutex> lk(s.m); for (auto& d : s.devs) if (d.id == devId) d.lib = on; SaveIni(); AuthChanged(); }
inline void SetDevPlaylistOk(const std::string& devId, const std::string& slug, bool ok) { State& s = St(); std::lock_guard<std::mutex> lk(s.m); for (auto& p : s.dpls) if (p.dev == devId && p.slug == slug) p.pcOk = ok && p.share; SaveIni(); AuthChanged(); }
inline std::string Targets(const std::wstring& slug) { EnsureLoaded(); State& s = St(); std::lock_guard<std::mutex> lk(s.m); auto it = s.targets.find(slug); return it == s.targets.end() ? "" : it->second; }
inline void SetTargets(const std::wstring& slug, const std::string& t) { EnsureLoaded(); State& s = St(); std::lock_guard<std::mutex> lk(s.m); if (t.empty()) s.targets.erase(slug); else s.targets[slug] = t; SaveIni(); AuthChanged(); }
// Liga/desliga um aparelho na lista de uma playlist. "ALL" (inclui aparelhos que ainda vao ser vinculados)
// so sai do "Hostear no celular"; marcar um por um fica sempre lista explicita.
inline void ToggleTargetDevice(const std::wstring& slug, const std::string& devId) {
    EnsureLoaded();
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    std::string t = s.targets[slug]; std::vector<std::string> ids;
    auto exists = [&](const std::string& id) { return DevById(id) != nullptr; };
    if (t == "ALL") { for (auto& d : s.devs) if (d.id != devId) ids.push_back(d.id); }
    else { bool had = false; for (auto& id : Split(t, ',')) { if (id.empty() || !exists(id)) continue; if (id == devId) had = true; else ids.push_back(id); } if (!had && exists(devId)) ids.push_back(devId); }
    std::string o; for (size_t i = 0; i < ids.size(); i++) { if (i) o += ","; o += ids[i]; }
    if (o.empty()) s.targets.erase(slug); else s.targets[slug] = o;
    SaveIni(); AuthChanged();
}
// QR de vinculo: URL base (tunel testado ou rede local) + #q=<token>. O token vai no fragmento
// (nunca chega a nenhum servidor em log/Referer); a pagina le e manda por POST.
inline std::string QrUrl(bool preferTunnel, bool& isTunnel) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m); ExpireReqs();
    std::string base;
    if (preferTunnel && !s.tunUrl.empty()) { base = s.tunUrl; isTunnel = true; }
    else if (!s.lanUrls.empty()) { base = s.lanUrls[0]; isTunnel = false; }
    else if (!s.tunUrl.empty()) { base = s.tunUrl; isTunnel = true; }
    if (base.empty() || !s.running) return "";
    if (s.qrToken.empty()) { s.qrToken = RandomHex(16); s.qrCreated = NowSec(); Bump(); }
    return base + "/#q=" + s.qrToken;
}
// Link permanente de um aparelho: base + #a=<chave>. Serve para religar quando o endereco muda
// (tunel novo, rede local, app na tela de inicio) sem precisar de PIN nem de QR novo.
inline std::string DeviceLinkUrl(const std::string& devId, bool preferTunnel, bool& isTunnel) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    std::string base;
    if (preferTunnel && !s.tunUrl.empty()) { base = s.tunUrl; isTunnel = true; }
    else if (!s.lanUrls.empty()) { base = s.lanUrls[0]; isTunnel = false; }
    else if (!s.tunUrl.empty()) { base = s.tunUrl; isTunnel = true; }
    if (base.empty() || !s.running) return "";
    for (auto& d : s.devs) if (d.id == devId && !d.key.empty()) return base + "/#a=" + d.key;
    return "";
}
inline void RotateQr() { State& s = St(); std::lock_guard<std::mutex> lk(s.m); s.qrToken.clear(); Bump(); }
inline long long QrSecondsLeft() { State& s = St(); std::lock_guard<std::mutex> lk(s.m); return s.qrToken.empty() ? 0 : std::max(0LL, 600 - (NowSec() - s.qrCreated)); }
// Copia do estado para a UI desenhar (sem segurar o mutex enquanto desenha).
struct View {
    bool running = false; int port = 0; bool lanOk = true, lan6 = false, onlineOk = true, qrConfirm = false; std::wstring hostName;
    std::string tunUrl, tunPending, tunStatus, lastError; bool tunRunning = false;
    std::vector<Device> devs; std::vector<PairReq> pending; std::vector<DevPlaylist> dpls; std::vector<std::string> lanUrls, lan6Urls; int streams = 0; unsigned version = 0;
};
inline View GetView() {
    EnsureLoaded();
    State& s = St(); View v; std::lock_guard<std::mutex> lk(s.m);
    v.running = s.running; v.port = s.opt.port; v.lanOk = s.opt.lanOk; v.lan6 = s.opt.lan6; v.onlineOk = s.opt.onlineOk; v.qrConfirm = s.opt.qrConfirm; v.hostName = s.hostName;
    v.tunUrl = s.tunUrl; v.tunPending = s.tunPending; v.tunStatus = s.tunStatus; v.lastError = s.lastError; v.tunRunning = s.tunRunning.load();
    v.devs = s.devs; for (auto& q : s.reqs) if (q.estado == 0) v.pending.push_back(q); v.dpls = s.dpls; v.lanUrls = s.lanUrls; v.lan6Urls = s.lan6Urls; v.streams = s.streamsAll; v.version = s.version.load();
    return v;
}
inline PairReq FindReq(const std::string& id) { State& s = St(); std::lock_guard<std::mutex> lk(s.m); for (auto& q : s.reqs) if (q.id == id) return q; return PairReq(); }
inline std::string TunnelUrl() { State& s = St(); std::lock_guard<std::mutex> lk(s.m); return s.tunUrl; }
inline std::string LanUrl() { State& s = St(); std::lock_guard<std::mutex> lk(s.m); return s.lanUrls.empty() ? "" : s.lanUrls[0]; }
// Pagina "conectar" (para mandar pelo WhatsApp): so links, sem PIN, sem token de QR, sem IP publico.
inline std::wstring WriteConnectHtml() {
    State& s = St(); std::string tun, nome, acc; std::vector<std::string> lans;
    { std::lock_guard<std::mutex> lk(s.m); tun = s.tunUrl; nome = WideToUtf8(s.hostName); lans = s.lanUrls; acc = s.opt.accent; }
    std::string ok; for (char c : tun) if (isalnum((unsigned char)c) || c == '-' || c == '.' || c == ':' || c == '/') ok.push_back(c); tun = ok;
    std::string lj = "["; for (size_t i = 0; i < lans.size(); i++) { if (i) lj += ","; lj += JStrA(lans[i]); } lj += "]";
    // links prontos no HTML: o visualizador do iPhone (WhatsApp/Arquivos) nao roda JavaScript
    std::string links;
    if (!tun.empty()) links += "<a class=\"b p\" rel=\"noreferrer\" href=\"" + HtmlEsc(tun) + "\">Abrir pela internet (túnel seguro)</a>\n";
    for (size_t i = 0; i < lans.size(); i++) {
        std::string shown = lans[i].rfind("http://", 0) == 0 ? lans[i].substr(7) : lans[i];
        links += "<a class=\"b" + std::string(tun.empty() && i == 0 ? " p" : "") + "\" rel=\"noreferrer\" href=\"" + HtmlEsc(lans[i]) + "\">Abrir na rede local (" + HtmlEsc(shown) + ")</a>\n";
    }
    std::string onAcc = "#121212";   // texto sobre a cor do tema: preto em cor clara, branco em cor escura
    if (acc.size() == 7 && acc[0] == '#') {
        auto lin = [](int v) { double c = v / 255.0; return c <= 0.03928 ? c / 12.92 : pow((c + 0.055) / 1.055, 2.4); };
        int r = (int)strtol(acc.substr(1, 2).c_str(), nullptr, 16), g = (int)strtol(acc.substr(3, 2).c_str(), nullptr, 16), b = (int)strtol(acc.substr(5, 2).c_str(), nullptr, 16);
        if (0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b) <= 0.179) onAcc = "#fff";
    }
    std::string html = hostweb::CONNECT_HTML;
    html = ReplaceAll(html, "@NOME@", HtmlEsc(nome)); html = ReplaceAll(html, "@LINKS@", links); html = ReplaceAll(html, "@LANS@", lj); html = ReplaceAll(html, "@ONACC@", onAcc); html = ReplaceAll(html, "#ACCENT", acc);
    std::wstring path = Config::Join(Config::BaseDir(), L"Remix-conectar.html");
#ifdef _WIN32
    FILE* f = _wfopen(path.c_str(), L"wb");
#else
    FILE* f = fopen(WideToUtf8(path).c_str(), "wb");
#endif
    if (!f) return L"";
    fwrite(html.data(), 1, html.size(), f); fclose(f);
    return path;
}

// ------------------------------------------------------------ painel (UI) --
struct PanelUI {
    bool open = false; int scroll = 0, contentH = 0; bool qrTunnel = true;
    RECT box{ 0,0,0,0 }, btnClose{ 0,0,0,0 }, btnToggle{ 0,0,0,0 }, btnTunnel{ 0,0,0,0 }, btnNewLink{ 0,0,0,0 }, btnHtml{ 0,0,0,0 }, btnPasta{ 0,0,0,0 };
    RECT btnPort{ 0,0,0,0 }, btnPin{ 0,0,0,0 }, btnName{ 0,0,0,0 }, btnLan{ 0,0,0,0 };
    RECT qrBox{ 0,0,0,0 }, btnCopyTun{ 0,0,0,0 }, btnCopyLan{ 0,0,0,0 }, btnQrMode{ 0,0,0,0 }, btnQrNew{ 0,0,0,0 }, btnOnline{ 0,0,0,0 }, btnQrConfirm{ 0,0,0,0 }, btnIpv6{ 0,0,0,0 }, info{ 0,0,0,0 };
    RECT list{ 0,0,0,0 };
    std::vector<RECT> accept, deny, revoke, devLib, devLink, plHost, dplOk; std::vector<std::vector<RECT>> plDev;
    std::string qrDev;   // != "" : o QR grande e o link permanente deste aparelho, nao o de vincular
    View v;   // copia usada pelo layout e pelo desenho (atualizada quando version muda)
    std::string qrText; qr::Code qr;   // QR ja codificado (so recodifica quando o texto muda)
};
inline PanelUI& PU() { static PanelUI* p = new PanelUI(); return *p; }

} // namespace host
