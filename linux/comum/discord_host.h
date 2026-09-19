#pragma once
// DISCORD: bot de musica do dono do PC (so no PC; o Host do celular e outra coisa, com outras regras).
//
//  - O dono cria um bot no Discord Developer Portal, cola o token no painel DISCORD e convida o bot.
//  - O Remix roda o processo do bot (Node.js, discord_bot.h) e fala com ele por stdin/stdout em JSON.
//  - TODA a logica fica aqui, com os mecanismos do app: fila por servidor, votacao, permissoes, busca (yt-dlp),
//    links (YouTube, SoundCloud, Spotify/Deezer/Apple com a mesma musica achada no YouTube), playlists do PC,
//    efeitos (o mesmo grafo do ffmpeg do Host) e o audio: o ffmpeg converte para Ogg/Opus e entrega ao bot em
//    http://127.0.0.1:<porta aleatoria>/<segredo>/<ticket de uso unico>.
//  - O dono decide: fila publica (qualquer um poe musica), playlists liberadas para o bot e se todos podem
//    toca-las, votacao (% de quem esta na chamada para pular/parar/embaralhar...), cargo DJ e limite por pessoa.
//  - Pelo PC o dono manda o bot tocar uma musica ou playlist (botao DISCORD no card) onde ele esta numa chamada.
// Seguranca: o token fica no discord.ini (Linux: permissao 600; Windows: protegido com DPAPI) e vai ao bot so pelo
// stdin, nunca para log ou tela. Links de quem usa o bot so de fontes conhecidas (o yt-dlp nao vira um leitor de
// URL qualquer da rede do dono). Arquivos locais so das playlists que o dono liberou. Nada escrito no pipe do bot
// com a trava principal presa (o Node bloqueia no stdout cheio: seria deadlock).
#include "host_server.h"
#include "discord_bot.h"
#include <deque>
#include <set>
#include <map>
#include <memory>
#include <random>
#include <condition_variable>
#ifdef _WIN32
#include <wincrypt.h>
#include <dpapi.h>
#else
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace dc {

using host::JStr;
using host::JStrA;
inline long long NowMs() { return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

static const int kMaxQueue = 500, kMaxVoice = 3, kMaxWorkers = 4;
static const int kColor = 0x1E90FF;

struct QItem {
    std::wstring title, artist; int dur = 0;
    std::wstring path;          // arquivo local (so de playlist liberada)
    std::wstring url, play;     // online: url = pagina da musica; play = link tocavel (achado no YouTube quando precisa)
    std::string by;             // id de quem pediu no Discord ("pc" = o dono pelo PC)
    std::wstring byName;
};
struct Cfg { bool publicQueue = true, publicPlaylists = true, online = true; int votePct = 50, maxPerUser = 30; std::wstring djRole = L"DJ"; std::set<std::wstring> pls; };
struct Vote { std::string id, guild, action, arg, iid, channel, msg; std::wstring what, byName; std::set<std::string> voters; long long expires = 0; unsigned trackNo = 0; };
struct SearchSess { std::string user, guild; std::vector<QItem> items; long long created = 0; };
struct Ticket { std::string guild; unsigned gen = 0; QItem item; int startSec = 0; host::FxReq fx; int vol = 100; long long created = 0; };
struct Guild {
    std::string id; std::wstring name;
    std::string text, voice, joiningTo; int humans = 0;
    std::deque<QItem> q; QItem cur; bool hasCur = false, paused = false; int loop = 0;   // loop: 0 nao, 1 musica, 2 fila
    unsigned gen = 0; std::shared_ptr<std::atomic<unsigned>> genRef = std::make_shared<std::atomic<unsigned>>(0);
    long long startedMs = 0, requestedMs = 0, pausedAt = 0, pausedMs = 0; int offsetSec = 0, fails = 0;
    host::FxReq fx; int vol = 100;
    std::string npChannel, npMsg; unsigned npSerial = 0;
    long long aloneSince = 0, idleSince = 0;
    bool pendingPlay = false, retried = false, npPending = false; unsigned trackNo = 0;
};
struct DPl { std::wstring slug, name; std::vector<QItem> items; };

struct State {
    std::mutex m, life;   // life: liga/desliga um de cada vez
    bool loaded = false; Cfg cfg; std::string token; bool on = false;
    // processo do bot
    std::unique_ptr<Proc> proc; std::thread reader, errReader, writer, ticker, acceptor;
    std::atomic<unsigned> runGen{ 0 }; std::atomic<bool> running{ false };
    std::shared_ptr<std::atomic<bool>> cancel = std::make_shared<std::atomic<bool>>(false);
    std::mutex wm; std::condition_variable wcv; std::deque<std::string> wq; bool wstop = false;
    int phase = 0;        // 0 desligado, 1 conectando, 2 pronto, 3 erro
    bool fatal = false; int restarts = 0; long long restartAt = 0;
    std::wstring status; std::deque<std::wstring> logs;
    std::string botId, appId; std::wstring botTag; std::set<std::string> owners; std::wstring nodeVer;
    std::map<std::string, Guild> guilds; std::map<std::string, std::wstring> guildNames;
    std::map<std::string, std::string> ownerVc; std::string ownerGuild;   // servidor -> canal onde o dono esta
    std::map<std::string, std::shared_ptr<Vote>> votes;
    std::map<std::string, SearchSess> searches;
    std::map<std::string, Ticket> tickets;
    std::map<std::string, std::deque<long long>> rate;
    std::set<std::string> busyUsers; int workers = 0;
    std::vector<DPl> pls; unsigned plsVer = 1, plsSent = 0;
    // audio
    hsock_t ls = HSOCK_BAD; int port = 0; std::string secret; std::atomic<int> streams{ 0 };
    unsigned genCounter = 0;
    std::atomic<unsigned> version{ 0 };
    std::function<void()> onChange;
    std::wstring presence; bool presenceSent = false;
    // ferramentas
    std::wstring nodePath; bool depsOk = false; long long toolsAt = 0; std::atomic<bool> probing{ false }, installing{ false }; std::wstring installMsg;
};
inline State& St() { static State* s = new State(); return *s; }
inline void Changed() { State& s = St(); s.version++; if (s.onChange) s.onChange(); }

// ---- texto ----------------------------------------------------------------------------------
inline std::wstring W(const std::string& s) { return Utf8ToWide(s); }
inline std::wstring Clip(std::wstring w, size_t n) { if (w.size() > n) { w.resize(n > 1 ? n - 1 : n); w += L"…"; } return w; }
inline std::wstring Md(std::wstring w) {   // titulo em negrito sem quebrar o markdown do Discord
    std::wstring o; for (wchar_t c : w) { if (wcschr(L"\\*_`~|>[]()#-", c) && c) o.push_back(L'\\'); if (c == L'\n' || c == L'\r') c = L' '; o.push_back(c); } return o;
}
inline std::wstring Dur(int s) {
    if (s <= 0) return L"?:??";
    wchar_t b[32]; if (s >= 3600) swprintf(b, 32, L"%d:%02d:%02d", s / 3600, (s / 60) % 60, s % 60); else swprintf(b, 32, L"%d:%02d", s / 60, s % 60); return b;
}
inline std::wstring Pos(int s) { return s <= 0 ? L"0:00" : Dur(s); }   // posicao (0 e "0:00"; Dur usa ?:?? para duracao desconhecida)
inline bool EqI(const std::wstring& a, const std::wstring& b) { return _wcsicmp(a.c_str(), b.c_str()) == 0; }
inline std::wstring Lower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
inline std::wstring ItemName(const QItem& it) { return it.artist.empty() ? it.title : it.title + L" — " + it.artist; }
inline bool SafeId(const std::string& s, size_t mx = 24) { if (s.empty() || s.size() > mx) return false; for (char c : s) if (!isdigit((unsigned char)c)) return false; return true; }
inline bool TokenShapeOk(const std::string& t) {
    if (t.size() < 50 || t.size() > 120) return false;
    int dots = 0; for (char c : t) { if (c == '.') dots++; else if (!(isalnum((unsigned char)c) || c == '_' || c == '-')) return false; }
    return dots == 2;
}

// ---- configuracao e token ----------------------------------------------------------------------
inline std::wstring IniPath() { return Config::Join(Config::BaseDir(), L"discord.ini"); }
#ifdef _WIN32
inline std::string Protect(const std::string& plain) {
    if (plain.empty()) return "";
    DATA_BLOB in; in.cbData = (DWORD)plain.size(); in.pbData = (BYTE*)plain.data(); DATA_BLOB out; out.cbData = 0; out.pbData = nullptr;
    if (!CryptProtectData(&in, L"Remix Discord", nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return "";
    static const char* hx = "0123456789abcdef"; std::string h = "dpapi:";
    for (DWORD i = 0; i < out.cbData; i++) { h.push_back(hx[out.pbData[i] >> 4]); h.push_back(hx[out.pbData[i] & 15]); }
    LocalFree(out.pbData); return h;
}
inline std::string Unprotect(const std::string& stored) {
    if (stored.rfind("dpapi:", 0) != 0) return "";
    std::vector<BYTE> b; for (size_t i = 6; i + 1 < stored.size(); i += 2) b.push_back((BYTE)strtoul(stored.substr(i, 2).c_str(), nullptr, 16));
    DATA_BLOB in; in.cbData = (DWORD)b.size(); in.pbData = b.data(); DATA_BLOB out; out.cbData = 0; out.pbData = nullptr;
    if (!CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &out)) return "";
    std::string r((char*)out.pbData, out.cbData); SecureZeroMemory(out.pbData, out.cbData); LocalFree(out.pbData); return r;
}
#endif
inline void SaveLocked() {
    State& s = St(); const Cfg& c = s.cfg;
    std::string o = "# Remix: bot do Discord (o token nunca sai deste arquivo; nao compartilhe)\n";
    o += "ligado=" + std::string(s.on ? "1" : "0") + "\nfilaPublica=" + (c.publicQueue ? "1" : "0") + "\nplaylistsPublicas=" + (c.publicPlaylists ? "1" : "0");
    o += "\nbuscaOnline=" + std::string(c.online ? "1" : "0") + "\nvotacao=" + std::to_string(c.votePct) + "\nlimitePorPessoa=" + std::to_string(c.maxPerUser);
    o += "\ncargoDJ=" + WideToUtf8(c.djRole) + "\n";
    for (auto& p : c.pls) o += "playlist=" + WideToUtf8(p) + "\n";
#ifdef _WIN32
    if (!s.token.empty()) o += "token=" + Protect(s.token) + "\n";
#else
    if (!s.token.empty()) o += "token=" + s.token + "\n";
#endif
    std::wstring path = IniPath(), tmp = path + L".tmp";
#ifdef _WIN32
    { std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc); f << o; }
    MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
#else
    std::string t8 = WideToUtf8(tmp);
    int fd = open(t8.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0) return;
    fchmod(fd, 0600);
    size_t off = 0; while (off < o.size()) { ssize_t w = write(fd, o.data() + off, o.size() - off); if (w <= 0) break; off += (size_t)w; }
    close(fd);
    rename(t8.c_str(), WideToUtf8(path).c_str());
#endif
}
inline void EnsureLoadedLocked() {
    State& s = St(); if (s.loaded) return;
    s.loaded = true;
    std::ifstream f(std::filesystem::path(IniPath()), std::ios::binary); if (!f) return;
    std::string ln;
    while (std::getline(f, ln)) {
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back();
        size_t eq = ln.find('='); if (eq == std::string::npos || ln[0] == '#') continue;
        std::string k = ln.substr(0, eq), v = ln.substr(eq + 1);
        if (k == "ligado") s.on = v == "1";
        else if (k == "filaPublica") s.cfg.publicQueue = v != "0";
        else if (k == "playlistsPublicas") s.cfg.publicPlaylists = v != "0";
        else if (k == "buscaOnline") s.cfg.online = v != "0";
        else if (k == "votacao") s.cfg.votePct = std::max(0, std::min(100, atoi(v.c_str())));
        else if (k == "limitePorPessoa") s.cfg.maxPerUser = std::max(1, std::min(500, atoi(v.c_str())));
        else if (k == "cargoDJ") s.cfg.djRole = Utf8ToWide(v.substr(0, 100));
        else if (k == "playlist" && s.cfg.pls.size() < 500) s.cfg.pls.insert(Utf8ToWide(v));
#ifdef _WIN32
        else if (k == "token") s.token = Unprotect(v);
#else
        else if (k == "token") s.token = v;
#endif
    }
    if (!TokenShapeOk(s.token)) s.token.clear();
}
inline void EnsureLoaded() { std::lock_guard<std::mutex> lk(St().m); EnsureLoadedLocked(); }

// ---- ferramentas (Node.js 22.12+ e os pacotes do bot) -----------------------------------------------
inline std::wstring BotDir() {
#ifdef _WIN32
    return Config::Join(Config::Join(Config::AssetDir(), L"tools"), L"discord");
#else
    const char* h = std::getenv("HOME");
    return h && *h ? Utf8ToWide(std::string(h) + "/.local/share/remix/discord") : Config::Join(Config::BaseDir(), L"discord");
#endif
}
inline std::wstring OwnNodeDir() {
#ifdef _WIN32
    return Config::Join(Config::Join(Config::AssetDir(), L"tools"), L"node");
#else
    const char* h = std::getenv("HOME");
    return h && *h ? Utf8ToWide(std::string(h) + "/.local/share/remix/node") : Config::Join(Config::BaseDir(), L"node");
#endif
}
inline bool NodeVersionOk(const std::string& out, std::wstring& ver) {
    size_t v = out.find('v'); if (v == std::string::npos) return false;
    int ma = 0, mi = 0; if (sscanf(out.c_str() + v, "v%d.%d", &ma, &mi) != 2) return false;
    std::string t = out.substr(v); while (!t.empty() && (t.back() == '\n' || t.back() == '\r' || t.back() == ' ')) t.pop_back();
    ver = Utf8ToWide(t);
    return ma > 22 || (ma == 22 && mi >= 12);
}
inline std::wstring FindNode(std::wstring& ver, std::wstring& why) {
    std::vector<std::wstring> cand;
#ifdef _WIN32
    cand.push_back(Config::Join(OwnNodeDir(), L"node.exe"));
    { std::wstring t = OFindTool(L"node"); if (!t.empty()) cand.push_back(t); }
#else
    if (const char* e = std::getenv("REMIX_NODE")) cand.push_back(Utf8ToWide(e));
    cand.push_back(Config::Join(Config::Join(OwnNodeDir(), L"bin"), L"node"));
    std::wstring sys = OFindTool(L"node"); if (!sys.empty()) cand.push_back(sys);
#endif
    for (auto& c : cand) {
        std::error_code ec; if (!std::filesystem::exists(std::filesystem::path(c), ec)) continue;
        CapResult r = RunCapture({ c, L"--version" }, 10000);
        std::wstring v;
        if (r.code == 0 && NodeVersionOk(r.out, v)) { ver = v; return c; }
        if (!v.empty()) why = L"Node.js " + v + L" é antigo (o bot precisa do 22.12 ou mais novo).";
    }
    return L"";
}
inline bool DepsOk() {
    std::error_code ec; namespace fs = std::filesystem; fs::path d(BotDir());
    return fs::exists(d / "node_modules" / "discord.js" / "package.json", ec) && fs::exists(d / "node_modules" / "@discordjs" / "voice" / "package.json", ec);
}
// Ferramentas (confere o disco no maximo a cada 10 s, em segundo plano).
inline void ProbeTools(bool force) {
    State& s = St();
    { std::lock_guard<std::mutex> lk(s.m); if (!force && s.toolsAt && NowMs() - s.toolsAt < 10000) return; if (s.probing.exchange(true)) return; s.toolsAt = NowMs(); }
    std::thread([] {
        std::wstring ver, why; std::wstring node = FindNode(ver, why); bool deps = DepsOk();
        State& st = St();
        { std::lock_guard<std::mutex> lk(st.m); st.nodePath = node; st.nodeVer = node.empty() ? why : ver; st.depsOk = deps; }
        st.probing = false; Changed();
    }).detach();
}
inline std::vector<std::wstring> NpmCommand(const std::wstring& node) {
    namespace fs = std::filesystem; std::error_code ec;
    fs::path nd = fs::path(node).parent_path();
    std::vector<fs::path> cli = { nd / "node_modules" / "npm" / "bin" / "npm-cli.js", nd.parent_path() / "lib" / "node_modules" / "npm" / "bin" / "npm-cli.js" };
#ifndef _WIN32
    fs::path real = fs::canonical(fs::path(node), ec);
    if (!ec) cli.push_back(real.parent_path().parent_path() / "lib" / "node_modules" / "npm" / "bin" / "npm-cli.js");
#endif
    for (auto& c : cli) if (fs::exists(c, ec)) return { node, c.wstring() };
#ifndef _WIN32
    std::wstring npm = OFindTool(L"npm"); if (!npm.empty()) return { npm };
#endif
    return {};
}
// Instala/atualiza os pacotes do bot (discord.js e @discordjs/voice) com o npm do Node achado.
inline void InstallAsync() {
    State& s = St();
    if (s.installing.exchange(true)) return;
    { std::lock_guard<std::mutex> lk(s.m); s.installMsg = L"Instalando o bot (discord.js e @discordjs/voice)..."; }
    Changed();
    std::thread([] {
        State& st = St(); std::wstring msg;
        std::wstring ver, why; std::wstring node = FindNode(ver, why);
        if (node.empty()) msg = why.empty() ? L"Precisa do Node.js 22.12+: rode o instalador de dependências e escolha DISCORD." : why;
        else {
            std::vector<std::wstring> npm = NpmCommand(node);
            std::error_code ec; std::filesystem::create_directories(std::filesystem::path(BotDir()), ec);
            { std::ofstream f(std::filesystem::path(Config::Join(BotDir(), L"package.json")), std::ios::binary | std::ios::trunc); f << dcbot::PACKAGE_JSON; }
            if (npm.empty()) msg = L"Não achei o npm junto do Node.js.";
            else {
                std::string last; std::wstring dir = BotDir();
                for (const wchar_t* x : { L"install", L"--prefix", dir.c_str(), L"--omit=dev", L"--no-audit", L"--no-fund", L"--no-update-notifier", L"--loglevel=error" }) npm.push_back(x);
                CapResult r = RunCapture(npm, 600000, nullptr, [&](const std::string& ln) { if (!ln.empty()) last = ln; });
                if (r.code == 0 && DepsOk()) msg = L"Bot instalado.";
                else msg = L"A instalação falhou" + (last.empty() ? std::wstring(L".") : L": " + Utf8ToWide(last.substr(0, 160)));
            }
        }
        { std::lock_guard<std::mutex> lk(st.m); st.installMsg = msg; st.toolsAt = 0; }
        st.installing = false;
        ProbeTools(true); Changed();
    }).detach();
}

// ---- conversa com o bot ---------------------------------------------------------------------------
inline void Push(std::string json) {
    State& s = St();
    { std::lock_guard<std::mutex> lk(s.wm); if (s.wstop || s.wq.size() > 20000) return; s.wq.push_back(std::move(json)); }
    s.wcv.notify_one();
}
inline void LogLocked(std::wstring line) {
    State& s = St();
    if (!s.token.empty()) { std::wstring t = Utf8ToWide(s.token); size_t p; while ((p = line.find(t)) != std::wstring::npos) line.replace(p, t.size(), L"***"); }
    if (line.size() > 300) line = line.substr(0, 300);
    s.logs.push_back(line); while (s.logs.size() > 60) s.logs.pop_front();
}
inline std::string Content(const std::wstring& t) { return "{\"content\":" + JStr(Clip(t, 1900)) + "}"; }
inline void Reply(const std::string& iid, const std::string& data, const char* mode = "edit", bool done = true) {
    if (iid.empty()) return;
    Push("{\"t\":\"reply\",\"iid\":" + JStrA(iid) + ",\"mode\":\"" + mode + "\",\"done\":" + (done ? "true" : "false") + ",\"data\":" + data + "}");
}
inline void ReplyText(const std::string& iid, const std::wstring& t) { Reply(iid, Content(t)); }
inline void ReplyPrivate(const std::string& iid, const std::wstring& t) { Reply(iid, Content(t), "private"); }
inline void SendTo(const std::string& channel, const std::string& data, const std::string& key = "") {
    if (!SafeId(channel)) return;
    Push("{\"t\":\"send\",\"channel\":" + JStrA(channel) + ",\"key\":" + JStrA(key) + ",\"data\":" + data + "}");
}
inline void Announce(const Guild& g, const std::wstring& t) { if (!g.text.empty()) SendTo(g.text, Content(t)); }
inline void EditMsg(const std::string& channel, const std::string& msg, const std::string& data) {
    if (!SafeId(channel) || !SafeId(msg)) return;
    Push("{\"t\":\"edit\",\"channel\":" + JStrA(channel) + ",\"msg\":" + JStrA(msg) + ",\"data\":" + data + "}");
}
inline void DeleteMsg(const std::string& channel, const std::string& msg) {
    if (!SafeId(channel) || !SafeId(msg)) return;
    Push("{\"t\":\"del\",\"channel\":" + JStrA(channel) + ",\"msg\":" + JStrA(msg) + "}");
}
inline std::string Button(const std::string& id, const std::wstring& label, int style = 2, bool disabled = false) {
    return "{\"type\":2,\"style\":" + std::to_string(style) + ",\"custom_id\":" + JStrA(id) + ",\"label\":" + JStr(Clip(label, 80)) + (disabled ? ",\"disabled\":true" : "") + "}";
}
inline std::string Row(const std::vector<std::string>& comps) { std::string o = "{\"type\":1,\"components\":["; for (size_t i = 0; i < comps.size() && i < 5; i++) { if (i) o += ","; o += comps[i]; } return o + "]}"; }

// ---- fila e reproducao (tudo com St().m travado) ---------------------------------------------------
inline Guild& GuildRefL(const std::string& id) {
    State& s = St(); Guild& g = s.guilds[id];
    if (g.id.empty()) { g.id = id; auto n = s.guildNames.find(id); if (n != s.guildNames.end()) g.name = n->second; }
    return g;
}
inline double RateOf(const host::FxReq& f) { return host::FxRate(f); }
inline int PosSecL(const Guild& g) {
    if (!g.hasCur) return 0;
    if (!g.startedMs) return g.offsetSec;
    long long end = g.paused ? g.pausedAt : NowMs();
    double el = std::max(0.0, (double)(end - g.startedMs - g.pausedMs) / 1000.0) * RateOf(g.fx);
    int p = g.offsetSec + (int)el;
    return g.cur.dur > 0 ? std::min(p, g.cur.dur) : p;
}
inline int CountByL(const Guild& g, const std::string& uid) { int n = 0; for (auto& x : g.q) if (x.by == uid) n++; return n; }
inline int ActiveVoiceL() { int n = 0; for (auto& kv : St().guilds) if (!kv.second.voice.empty() || !kv.second.joiningTo.empty()) n++; return n; }
inline std::wstring FxText(const host::FxReq& f) {
    std::wstring o; auto add = [&](const wchar_t* n, int lv) { if (lv) { if (!o.empty()) o += L", "; o += n; o += L" " + std::to_wstring(lv); } };
    add(L"slow", f.slow); add(L"speed", f.speed); add(L"reverb", f.reverb); add(L"grave", f.bass); add(L"8D", f.d8);
    return o;
}
inline const wchar_t* LoopText(int l) { return l == 1 ? L"música" : l == 2 ? L"fila" : L"desligado"; }
inline std::wstring Bar(int pos, int dur) {
    if (dur <= 0) return L"🔴 " + Pos(pos);
    int n = 16, k = std::max(0, std::min(n - 1, (int)((double)pos / dur * n)));
    std::wstring b; for (int i = 0; i < n; i++) b += (i == k ? L"🔘" : L"▬");
    return b + L"  " + Pos(pos) + L" / " + Dur(dur);
}
inline std::string NowPlayingJsonL(const Guild& g, bool progress) {
    const QItem& it = g.cur;
    std::wstring desc = (it.artist.empty() ? L"" : Md(Clip(it.artist, 120)) + L"  ·  ") + Dur(it.dur) + L"\nPedido por **" + Md(Clip(it.byName, 60)) + L"**";
    if (progress) desc += L"\n" + Bar(PosSecL(g), it.dur);
    std::wstring foot = L"Repetir: " + std::wstring(LoopText(g.loop)) + L"  ·  Fila: " + std::to_wstring(g.q.size());
    std::wstring fx = FxText(g.fx); if (!fx.empty()) foot += L"  ·  Efeitos: " + fx;
    if (g.vol != 100) foot += L"  ·  Volume " + std::to_wstring(g.vol) + L"%";
    std::string url = WideToUtf8(it.url);
    std::string e = "{\"color\":" + std::to_string(kColor) + ",\"author\":{\"name\":" + JStr(g.paused ? L"⏸️ Pausado" : L"🎶 Tocando agora") + "},\"title\":" + JStr(Clip(it.title.empty() ? L"(sem nome)" : it.title, 250));
    if (url.rfind("https://", 0) == 0 && url.size() < 400) e += ",\"url\":" + JStrA(url);
    e += ",\"description\":" + JStr(Clip(desc, 3000)) + ",\"footer\":{\"text\":" + JStr(Clip(foot, 1000)) + "}}";
    std::string gid = g.id;
    std::string row = Row({ Button("c:" + gid + ":" + (g.paused ? "resume" : "pause"), g.paused ? L"▶ Continuar" : L"⏸ Pausar", 2),
                            Button("c:" + gid + ":skip", L"⏭ Pular", 1), Button("c:" + gid + ":stop", L"⏹ Parar", 4),
                            Button("c:" + gid + ":loop", L"🔁 Repetir", 2), Button("c:" + gid + ":queue", L"📜 Fila", 2) });
    return "{\"embeds\":[" + e + "],\"components\":[" + row + "]}";
}
inline void DeleteNpL(Guild& g) { if (!g.npMsg.empty()) DeleteMsg(g.npChannel, g.npMsg); g.npMsg.clear(); g.npSerial++; g.npPending = false; }
inline void NewNpL(Guild& g) {
    DeleteNpL(g);
    if (g.text.empty()) return;
    g.npChannel = g.text;
    SendTo(g.text, NowPlayingJsonL(g, false), "np:" + g.id + ":" + std::to_string(g.npSerial));
}
inline void UpdateNpL(Guild& g) { if (!g.npMsg.empty() && g.hasCur && !g.npPending) EditMsg(g.npChannel, g.npMsg, NowPlayingJsonL(g, false)); }
inline void PresenceL() {
    State& s = St(); std::wstring t;
    for (auto& kv : s.guilds) if (kv.second.hasCur && !kv.second.paused) t = kv.second.cur.title;
    if (s.presenceSent && t == s.presence) return;
    s.presence = t; s.presenceSent = true;
    Push("{\"t\":\"presence\",\"text\":" + JStr(Clip(t, 100)) + "}");
}
inline std::wstring UrlFor(const std::string& ticket) { State& s = St(); return L"http://127.0.0.1:" + std::to_wstring(s.port) + L"/" + Utf8ToWide(s.secret) + L"/" + Utf8ToWide(ticket); }
inline void PlayCurL(Guild& g, int offset, bool newMessage) {
    State& s = St();
    g.gen = ++s.genCounter; g.genRef->store(g.gen);
    long long now = NowMs();
    for (auto it = s.tickets.begin(); it != s.tickets.end();) { if (now - it->second.created > 60000) it = s.tickets.erase(it); else ++it; }
    std::string tk = host::RandomHex(12);
    Ticket t; t.guild = g.id; t.gen = g.gen; t.item = g.cur; t.startSec = std::max(0, offset); t.fx = g.fx; t.vol = g.vol; t.created = now;
    s.tickets[tk] = t;
    g.offsetSec = t.startSec; g.startedMs = 0; g.pausedMs = 0; g.paused = false; g.requestedMs = now; g.pendingPlay = false;
    if (newMessage) g.retried = false;
    Push("{\"t\":\"play\",\"guild\":" + JStrA(g.id) + ",\"url\":" + JStr(UrlFor(tk)) + ",\"gen\":" + std::to_string(g.gen) + "}");
    if (newMessage) { DeleteNpL(g); g.npPending = true; }   // a mensagem nova sai quando o audio comecar (ja com a duracao)
    else UpdateNpL(g);
    PresenceL();
    if (newMessage) {   // adianta o link das proximas online (o yt-dlp leva ~3 s): a troca de faixa fica rapida
        std::vector<std::wstring> nx;
        for (size_t i = 0; i < g.q.size() && nx.size() < 2; ++i) {
            const QItem& it = g.q[i];
            if (it.path.empty() && !it.url.empty()) { std::wstring pl = it.play.empty() ? it.url : it.play; if (!NeedsMatch(DetectSource(pl))) nx.push_back(pl); }
        }
        if (!nx.empty()) PreResolve(nx);
    }
}
inline void StopAudioL(Guild& g) {
    g.gen = ++St().genCounter; g.genRef->store(g.gen);
    g.hasCur = false; g.paused = false; g.startedMs = 0;
    Push("{\"t\":\"stopaudio\",\"guild\":" + JStrA(g.id) + "}");
}
inline void StartNextL(Guild& g, bool skip) {
    if (g.hasCur && g.loop == 1 && !skip) { PlayCurL(g, 0, false); return; }
    if (g.hasCur && g.loop == 2) g.q.push_back(g.cur);
    if (g.q.empty()) {
        bool had = g.hasCur;
        StopAudioL(g); DeleteNpL(g); g.idleSince = NowMs(); PresenceL();
        if (had) Announce(g, L"✅ A fila acabou. Use /tocar para pôr mais músicas.");
        return;
    }
    g.cur = g.q.front(); g.q.pop_front(); g.hasCur = true; g.idleSince = 0; g.trackNo++;
    if (g.voice.empty()) { g.pendingPlay = true; g.startedMs = 0; g.requestedMs = 0; return; }   // entrando na chamada: toca quando o bot confirmar
    PlayCurL(g, 0, true);
}
inline void JoinL(Guild& g, const std::string& channel) {
    if (!SafeId(channel)) return;
    g.joiningTo = channel;
    Push("{\"t\":\"join\",\"guild\":" + JStrA(g.id) + ",\"channel\":" + JStrA(channel) + "}");
}
inline void LeaveL(Guild& g) {
    for (auto it = St().votes.begin(); it != St().votes.end();) { if (it->second->guild == g.id) it = St().votes.erase(it); else ++it; }
    g.q.clear(); StopAudioL(g); DeleteNpL(g); g.joiningTo.clear();
    Push("{\"t\":\"leave\",\"guild\":" + JStrA(g.id) + "}");
    g.voice.clear(); g.humans = 0; g.aloneSince = 0; g.idleSince = 0; g.fx = host::FxReq(); g.vol = 100; g.loop = 0;
    PresenceL();
}

// ---- playlists do PC (a UI publica; o bot so ve as liberadas) --------------------------------------
inline QItem FromEntry(const PlEntry& e) {
    QItem q;
    if (!e.url.empty()) { q.url = e.url; q.play = e.play; }
    else q.path = e.path;
    q.title = !e.title.empty() ? e.title : std::filesystem::path(e.path.empty() ? e.url : e.path).stem().wstring();
    q.artist = e.artist; q.dur = e.dur;
    return q;
}
inline std::string ListsJsonL() {
    State& s = St(); std::string o = "{\"t\":\"lists\",\"playlists\":["; bool first = true;
    for (auto& p : s.pls) { if (!s.cfg.pls.count(p.slug)) continue; if (!first) o += ","; first = false; o += "{\"id\":" + JStr(p.slug) + ",\"name\":" + JStr(Clip(p.name, 100)) + "}"; }
    return o + "]}";
}
inline void SendListsL() { State& s = St(); if (s.phase != 2) return; s.plsSent = s.plsVer; Push(ListsJsonL()); }
inline void Publish(const std::vector<Playlist>& expanded) {
    State& s = St(); std::vector<DPl> v;
    for (auto& p : expanded) { DPl d; d.slug = p.slug; d.name = p.name; for (auto& e : p.entries) { if (e.path.empty() && e.url.empty()) continue; d.items.push_back(FromEntry(e)); } v.push_back(std::move(d)); }
    std::lock_guard<std::mutex> lk(s.m);
    s.pls.swap(v); s.plsVer++; SendListsL();
}
inline const DPl* FindPlL(const std::wstring& key) {
    State& s = St();
    for (auto& p : s.pls) if (s.cfg.pls.count(p.slug) && (p.slug == key || EqI(p.name, key))) return &p;
    for (auto& p : s.pls) if (s.cfg.pls.count(p.slug) && Lower(p.name).find(Lower(key)) != std::wstring::npos) return &p;
    return nullptr;
}
// Busca nas playlists liberadas: todas as palavras no "titulo artista".
inline std::vector<QItem> LocalMatchesL(const std::wstring& query, size_t max) {
    State& s = St(); std::vector<QItem> out; std::vector<std::wstring> words; std::wstring cur;
    for (wchar_t c : Lower(query)) { if (iswspace(c)) { if (!cur.empty()) words.push_back(cur); cur.clear(); } else cur.push_back(c); }
    if (!cur.empty()) words.push_back(cur);
    if (words.empty()) return out;
    std::set<std::wstring> seen;
    for (auto& p : s.pls) {
        if (!s.cfg.pls.count(p.slug)) continue;
        for (auto& it : p.items) {
            std::wstring hay = Lower(it.title + L" " + it.artist); bool all = true;
            for (auto& w : words) if (hay.find(w) == std::wstring::npos) { all = false; break; }
            if (!all) continue;
            std::wstring key = it.path.empty() ? it.url : it.path; if (!seen.insert(key).second) continue;
            out.push_back(it); if (out.size() >= max) return out;
        }
    }
    return out;
}

// ---- contexto de um comando ---------------------------------------------------------------------
struct Ctx {
    std::string iid, kind, name, custom, guild, channel, uid, uvc, bvc;
    std::wstring uname, gname; bool admin = false, owner = false; int humans = 0;
    std::vector<std::wstring> roles; std::vector<std::string> values;
    JVal opts;
};
inline std::string OptS(const Ctx& c, const char* k) { const JVal* v = c.opts.get(k); return v && v->t == JVal::STR ? v->s : ""; }
inline long long OptI(const Ctx& c, const char* k, long long d) { const JVal* v = c.opts.get(k); return v && v->t == JVal::NUM ? (long long)v->n : d; }
inline bool OptB(const Ctx& c, const char* k) { const JVal* v = c.opts.get(k); return v && v->t == JVal::BOOL && v->b; }
inline bool IsDjL(const Ctx& c) {
    State& s = St(); if (c.owner || c.admin) return true;
    for (auto& r : c.roles) if (!s.cfg.djRole.empty() && EqI(r, s.cfg.djRole)) return true;
    return false;
}
inline bool PrivL(const Ctx& c, const Guild& g) { return IsDjL(c) || (!g.voice.empty() && c.uvc == g.voice && g.humans <= 1); }
inline int NeedVotes(int humans, int pct) { int h = std::max(1, humans); return std::max(1, std::min(h, (int)std::floor(h * pct / 100.0) + 1)); }   // 50% de 2 = os 2; de 3 = 2; de 4 = 3
inline bool RateOkL(const std::string& uid) {
    auto& d = St().rate[uid]; long long now = NowMs();
    while (!d.empty() && now - d.front() > 30000) d.pop_front();
    if (d.size() >= 15) return false;
    d.push_back(now); return true;
}
// Canal onde tocar para quem pediu (false = ja respondeu o motivo).
inline bool PickChannelL(const Ctx& c, Guild& g, std::string& target) {
    std::string cur = !g.voice.empty() ? g.voice : g.joiningTo;
    if (!cur.empty()) {
        if (c.uvc == cur) { target = cur; return true; }
        if (IsDjL(c) && !c.uvc.empty() && !g.hasCur && g.q.empty()) { target = c.uvc; return true; }   // parado: DJ chama o bot para o canal dele
        if (IsDjL(c)) { target = cur; return true; }
        ReplyPrivate(c.iid, L"O bot está em <#" + W(cur) + L">: entre lá para pedir músicas.");
        return false;
    }
    if (c.uvc.empty()) { ReplyPrivate(c.iid, L"Entre num canal de voz para eu tocar."); return false; }
    if (ActiveVoiceL() >= kMaxVoice) { ReplyPrivate(c.iid, L"O bot já está tocando em " + std::to_wstring(kMaxVoice) + L" servidores agora (limite do PC do dono). Tente mais tarde."); return false; }
    target = c.uvc; return true;
}
inline void EnqueueL(const Ctx& c, Guild& g, std::vector<QItem> items, bool next, const std::string& target, const std::wstring& listName) {
    State& s = St(); bool dj = IsDjL(c);
    size_t room = g.q.size() >= (size_t)kMaxQueue ? 0 : (size_t)kMaxQueue - g.q.size();
    if (!dj) { int mine = CountByL(g, c.uid); room = std::min(room, (size_t)std::max(0, s.cfg.maxPerUser - mine)); }
    bool cut = items.size() > room;
    if (cut) items.resize(room);
    if (items.empty()) { ReplyPrivate(c.iid, dj ? L"A fila está cheia (" + std::to_wstring(kMaxQueue) + L" músicas)." : L"Você já tem " + std::to_wstring(s.cfg.maxPerUser) + L" músicas na fila (limite do dono)."); return; }
    for (auto& it : items) { it.by = c.uid; it.byName = c.uname; }
    size_t pos = next ? 1 : g.q.size() + 1;
    if (next) g.q.insert(g.q.begin(), items.begin(), items.end()); else g.q.insert(g.q.end(), items.begin(), items.end());
    if (g.voice != target && g.joiningTo != target) JoinL(g, target);
    bool startNow = !g.hasCur;
    std::wstring msg;
    if (items.size() == 1) {
        const QItem& it = items[0];
        msg = startNow ? L"▶️ Tocando **" + Md(Clip(it.title, 150)) + L"**" : L"➕ Na fila (#" + std::to_wstring(pos) + L"): **" + Md(Clip(it.title, 150)) + L"**";
        if (!it.artist.empty()) msg += L" — " + Md(Clip(it.artist, 80));
        msg += L" `" + Dur(it.dur) + L"`";
    } else {
        msg = L"➕ " + std::to_wstring(items.size()) + L" músicas" + (listName.empty() ? L"" : L" de **" + Md(Clip(listName, 100)) + L"**") + L" na fila.";
        if (cut) msg += dj ? L" (a fila tem limite de " + std::to_wstring(kMaxQueue) + L")" : L" (limite do dono: " + std::to_wstring(s.cfg.maxPerUser) + L" por pessoa)";
    }
    ReplyText(c.iid, msg);
    if (startNow) StartNextL(g, true);
    else UpdateNpL(g);
}

// ---- votacao ---------------------------------------------------------------------------------
inline std::string VoteJsonL(const Vote& v, int need, int state) {   // state: 0 aberta, 1 aprovada, 2 expirou
    std::wstring t = state == 1 ? L"✅ Aprovado: " + v.what + L"." : state == 2 ? L"⌛ Votação encerrada sem votos suficientes para " + v.what + L"."
                                : L"🗳️ **" + Md(Clip(v.byName, 60)) + L"** quer " + v.what + L". Votos: " + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L" (quem está na chamada, até 1 minuto).";
    std::string d = "{\"content\":" + JStr(Clip(t, 1900));
    if (state == 0) d += ",\"components\":[" + Row({ Button("v:" + v.id, L"🗳️ Votar (" + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L")", 1) }) + "]";
    else d += ",\"components\":[]";
    return d + "}";
}
inline void ShowVoteL(Vote& v, int need, int state) {
    std::string data = VoteJsonL(v, need, state);
    if (!v.iid.empty()) Reply(v.iid, data, "edit", state != 0);
    else if (!v.msg.empty()) EditMsg(v.channel, v.msg, data);
}
inline std::wstring DoActionL(Guild& g, const std::string& action, const std::string& arg);
// true = pode fazer agora; false = ja respondeu (votacao aberta ou negado).
inline bool GateL(const Ctx& c, Guild& g, const std::string& action, const std::string& arg, const std::wstring& what, bool ownerOfItemOk) {
    State& s = St();
    if (PrivL(c, g) || ownerOfItemOk) return true;
    if (s.cfg.votePct <= 0) { ReplyPrivate(c.iid, L"Só o dono, admins e quem tem o cargo " + (s.cfg.djRole.empty() ? L"DJ" : s.cfg.djRole) + L" podem " + what + L"."); return false; }
    if (g.voice.empty() || c.uvc != g.voice) { ReplyPrivate(c.iid, L"Só quem está na chamada com o bot pode votar."); return false; }
    int need = NeedVotes(g.humans, s.cfg.votePct);
    for (auto& kv : s.votes) {   // ja tem essa votacao: conta este voto
        Vote& v = *kv.second;
        if (v.guild != g.id || v.action != action || v.arg != arg) continue;
        if (!v.voters.insert(c.uid).second) { ReplyPrivate(c.iid, L"Você já votou (" + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L")."); return false; }
        if ((int)v.voters.size() >= need) { std::wstring r = DoActionL(g, v.action, v.arg); ShowVoteL(v, need, 1); s.votes.erase(kv.first); ReplyText(c.iid, L"Seu voto fechou a votação. " + r); return false; }
        ShowVoteL(v, need, 0); ReplyText(c.iid, L"Voto contado (" + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L").");
        return false;
    }
    if (need <= 1) return true;
    auto v = std::make_shared<Vote>(); v->id = host::RandomHex(8); v->guild = g.id; v->action = action; v->arg = arg; v->what = what; v->byName = c.uname;
    v->voters.insert(c.uid); v->expires = NowMs() + 60000; v->trackNo = g.trackNo;
    s.votes[v->id] = v;
    if (c.kind == "slash") { v->iid = c.iid; ShowVoteL(*v, need, 0); }
    else { v->channel = c.channel; ReplyText(c.iid, L"Votação aberta no canal."); SendTo(c.channel, VoteJsonL(*v, need, 0), "vote:" + v->id); }
    return false;
}

// Acoes (depois de liberadas). Devolve o texto do resultado.
inline std::wstring DoActionL(Guild& g, const std::string& action, const std::string& arg) {
    State& s = St();
    if (action == "skip") {
        if (!g.hasCur) return L"Nada tocando.";
        std::wstring t = g.cur.title; StartNextL(g, true);
        return L"⏭️ Pulou **" + Md(Clip(t, 150)) + L"**.";
    }
    if (action == "pause") {
        if (!g.hasCur || g.paused) return g.paused ? L"Já está pausado." : L"Nada tocando.";
        Push("{\"t\":\"pause\",\"guild\":" + JStrA(g.id) + "}"); g.paused = true; g.pausedAt = NowMs(); UpdateNpL(g); PresenceL();
        return L"⏸️ Pausado.";
    }
    if (action == "resume") {
        if (!g.hasCur || !g.paused) return g.hasCur ? L"Já está tocando." : L"Nada tocando.";
        if (NowMs() - g.pausedAt > 120000 || !g.startedMs) { int p = PosSecL(g); PlayCurL(g, p, false); }   // pausa longa: recomeca o audio do ponto (o link online pode ter expirado)
        else { Push("{\"t\":\"resume\",\"guild\":" + JStrA(g.id) + "}"); g.pausedMs += NowMs() - g.pausedAt; g.paused = false; UpdateNpL(g); PresenceL(); }
        return L"▶️ Continuando.";
    }
    if (action == "stop" || action == "leave") { LeaveL(g); return action == "stop" ? L"⏹️ Parei, limpei a fila e saí da chamada." : L"👋 Saí da chamada."; }
    if (action == "shuffle") {
        if (g.q.size() < 2) return L"Não tem o que embaralhar.";
        std::vector<QItem> v(g.q.begin(), g.q.end()); std::random_device rd; std::mt19937 rng(rd()); std::shuffle(v.begin(), v.end(), rng);
        g.q.assign(v.begin(), v.end()); UpdateNpL(g);
        return L"🔀 Fila embaralhada (" + std::to_wstring(g.q.size()) + L" músicas).";
    }
    if (action == "clear") { size_t n = g.q.size(); g.q.clear(); UpdateNpL(g); return L"🧹 Fila limpa (" + std::to_wstring(n) + L" músicas)."; }
    if (action == "loop") { g.loop = arg == "track" ? 1 : arg == "queue" ? 2 : 0; UpdateNpL(g); return L"🔁 Repetir: " + std::wstring(LoopText(g.loop)) + L"."; }
    if (action == "seek") {
        if (!g.hasCur) return L"Nada tocando.";
        int sec = atoi(arg.c_str()); if (g.cur.dur > 0 && sec >= g.cur.dur) return L"Esse tempo passa do fim da música (" + Dur(g.cur.dur) + L").";
        PlayCurL(g, sec, false); return L"⏩ Indo para " + Dur(sec) + L".";
    }
    if (action == "fx") {
        std::string type = arg.substr(0, arg.find(':')); int lv = std::max(0, std::min(3, atoi(arg.c_str() + std::min(arg.size(), arg.find(':') + 1))));
        if (type == "off") g.fx = host::FxReq();
        else if (type == "slow") { g.fx.slow = lv; if (lv) g.fx.speed = 0; }
        else if (type == "speed") { g.fx.speed = lv; if (lv) g.fx.slow = 0; }
        else if (type == "reverb") g.fx.reverb = lv;
        else if (type == "bass") g.fx.bass = lv;
        else if (type == "8d") g.fx.d8 = lv;
        if (g.hasCur) PlayCurL(g, PosSecL(g), false);
        std::wstring t = FxText(g.fx);
        return t.empty() ? L"🎚️ Efeitos desligados." : L"🎚️ Efeitos: " + t + L".";
    }
    if (action == "vol") { g.vol = std::max(10, std::min(150, atoi(arg.c_str()))); if (g.hasCur) PlayCurL(g, PosSecL(g), false); return L"🔊 Volume " + std::to_wstring(g.vol) + L"%."; }
    if (action == "remove") {   // arg = posicao:assinatura (a fila pode ter mudado durante a votacao)
        size_t p = (size_t)atoi(arg.c_str()); std::string sig = arg.substr(std::min(arg.size(), arg.find(':') + 1));
        if (p < 1 || p > g.q.size()) return L"Essa posição não existe mais na fila.";
        const QItem& it = g.q[p - 1];
        if (!sig.empty() && sig != host::IdFor(it.path + it.url + it.title)) return L"A fila mudou: confira com /fila e tente de novo.";
        std::wstring t = it.title; g.q.erase(g.q.begin() + (long)(p - 1)); UpdateNpL(g);
        return L"🗑️ Removida: **" + Md(Clip(t, 150)) + L"**.";
    }
    if (action == "move") {
        size_t a = (size_t)atoi(arg.c_str()), b = (size_t)atoi(arg.c_str() + std::min(arg.size(), arg.find(':') + 1));
        if (a < 1 || a > g.q.size() || b < 1 || b > g.q.size()) return L"Posição inválida (veja /fila).";
        QItem it = g.q[a - 1]; g.q.erase(g.q.begin() + (long)(a - 1)); g.q.insert(g.q.begin() + (long)(b - 1), it);
        return L"↕️ **" + Md(Clip(it.title, 150)) + L"** agora é a #" + std::to_wstring(b) + L".";
    }
    (void)s;
    return L"?";
}

// ---- telas de resposta ------------------------------------------------------------------------------
inline std::string QueueJsonL(const Guild& g, int page) {
    int per = 10, total = (int)g.q.size(), pages = std::max(1, (total + per - 1) / per);
    page = std::max(1, std::min(page, pages));
    long long totalSec = 0; for (auto& x : g.q) totalSec += x.dur;
    std::wstring d;
    if (g.hasCur) d += L"**Agora:** " + Md(Clip(ItemName(g.cur), 120)) + L" `" + Pos(PosSecL(g)) + L"/" + Dur(g.cur.dur) + L"` · " + Md(Clip(g.cur.byName, 40)) + L"\n\n";
    if (!total) d += L"A fila está vazia. Use /tocar ou /playlist.";
    for (int i = (page - 1) * per; i < std::min(total, page * per); i++) {
        const QItem& x = g.q[(size_t)i];
        d += L"`" + std::to_wstring(i + 1) + L".` " + Md(Clip(ItemName(x), 90)) + L" `" + Dur(x.dur) + L"` · " + Md(Clip(x.byName, 30)) + L"\n";
    }
    wchar_t tb[64]; swprintf(tb, 64, L"%dh %02dmin", (int)(totalSec / 3600), (int)((totalSec / 60) % 60));
    std::wstring title = L"Fila: " + std::to_wstring(total) + (total == 1 ? L" música" : L" músicas") + (totalSec ? L" · " + std::wstring(tb) : L"");
    std::wstring foot = L"Página " + std::to_wstring(page) + L"/" + std::to_wstring(pages) + L"  ·  Repetir: " + LoopText(g.loop);
    std::string e = "{\"color\":" + std::to_string(kColor) + ",\"title\":" + JStr(title) + ",\"description\":" + JStr(Clip(d, 3900)) + ",\"footer\":{\"text\":" + JStr(foot) + "}}";
    std::string comps = Row({ Button("q:" + g.id + ":" + std::to_string(page - 1), L"◀", 2, page <= 1), Button("q:" + g.id + ":" + std::to_string(page + 1), L"▶", 2, page >= pages) });
    return "{\"content\":\"\",\"embeds\":[" + e + "],\"components\":[" + comps + "]}";
}
inline std::string HelpJsonL() {
    State& s = St(); const Cfg& c = s.cfg;
    std::wstring d = L"**Tocar:** /tocar (música ou link) · /buscar · /playlist · /playlists\n"
                     L"**Fila:** /fila · /agora · /remover · /mover · /embaralhar · /limpar · /repetir\n"
                     L"**Controle:** /pular · /pausar · /continuar · /avancar · /efeito · /volume · /parar · /sair\n"
                     L"(no Discord em inglês os nomes são /play, /search, /queue, /skip...)\n\n**Regras deste bot**\n";
    d += L"• Fila pública: " + std::wstring(c.publicQueue ? L"sim, qualquer um põe músicas" : L"não, só DJ/admins/dono") + L"\n";
    d += L"• Playlists do dono: " + std::wstring(c.publicPlaylists ? L"liberadas para todos" : L"só DJ/admins/dono") + L"\n";
    d += L"• Votação: " + (c.votePct > 0 ? std::to_wstring(c.votePct) + L"% de quem está na chamada para pular, pausar, parar, embaralhar..." : std::wstring(L"desligada (só DJ/admins/dono controlam)")) + L"\n";
    d += L"• Cargo DJ: **" + Md(c.djRole.empty() ? L"(nenhum)" : c.djRole) + L"** · limite de " + std::to_wstring(c.maxPerUser) + L" músicas por pessoa\n";
    d += L"• Quem pediu a música pode pular/pausar a própria; quem está sozinho com o bot controla tudo.";
    return "{\"embeds\":[{\"color\":" + std::to_string(kColor) + ",\"title\":\"Remix · bot de música\",\"description\":" + JStr(d) + "}]}";
}

// ---- comandos ---------------------------------------------------------------------------------
inline bool SupportedLink(const std::wstring& u) { OSrc s = DetectSource(u); return s != OS_UNKNOWN && s != OS_OTHER && s != OS_GPM && (u.rfind(L"https://", 0) == 0 || u.rfind(L"http://", 0) == 0); }
inline QItem FromOTrack(const OTrack& t) { QItem q; q.url = t.url; q.play = t.play; q.title = t.title; q.artist = t.artist; q.dur = t.dur; return q; }
// Lista de resultados com um menu para escolher (a sessao vale 10 min e so para quem buscou).
inline void SearchReplyL(const Ctx& c, const std::wstring& query, const std::vector<QItem>& all, const std::wstring& err) {
    State& st = St();
    if (all.empty()) { ReplyText(c.iid, L"Nada encontrado" + (err.empty() ? std::wstring(L".") : L": " + err)); return; }
    for (auto it = st.searches.begin(); it != st.searches.end();) { if (NowMs() - it->second.created > 600000) it = st.searches.erase(it); else ++it; }
    while (st.searches.size() > 200) st.searches.erase(st.searches.begin());
    std::string sid = host::RandomHex(8);
    SearchSess ss; ss.user = c.uid; ss.guild = c.guild; ss.items = all; ss.created = NowMs();
    if (ss.items.size() > 13) ss.items.resize(13);
    std::wstring d; std::string opts;
    for (size_t i = 0; i < ss.items.size(); i++) {
        const QItem& x = ss.items[i];
        d += L"`" + std::to_wstring(i + 1) + L".` " + Md(Clip(ItemName(x), 90)) + L" `" + Dur(x.dur) + L"`" + (x.path.empty() ? L"" : L" · 📂 playlist do dono") + L"\n";
        if (i) opts += ",";
        opts += "{\"label\":" + JStr(Clip(std::to_wstring(i + 1) + L". " + (x.title.empty() ? L"?" : x.title), 100)) + ",\"description\":" + JStr(Clip((x.artist.empty() ? L"" : x.artist + L" · ") + Dur(x.dur), 100)) + ",\"value\":\"" + std::to_string(i) + "\"}";
    }
    st.searches[sid] = ss;
    std::string data = "{\"embeds\":[{\"color\":" + std::to_string(kColor) + ",\"title\":" + JStr(L"Resultados: " + Clip(query, 80)) + ",\"description\":" + JStr(Clip(d, 3900)) + "}],\"components\":[{\"type\":1,\"components\":[{\"type\":3,\"custom_id\":\"s:" + sid + "\",\"placeholder\":" + JStr(L"Escolha para pôr na fila") + ",\"options\":[" + opts + "]}]}]}";
    Reply(c.iid, data);
}
inline void SearchJob(Ctx c, std::wstring query, int where, bool isLink, bool next, std::string target, int mode) {   // mode 0 = /tocar, 1 = /buscar
    State& s = St(); unsigned rg = s.runGen.load(); auto cancel = s.cancel;
    std::thread([=]() {
        std::vector<QItem> items; std::wstring err, name;
        if (isLink) {
            OResolved r = ResolveLink(query, cancel.get());
            for (auto& t : r.items) { if (items.size() >= 300) break; items.push_back(FromOTrack(t)); }
            name = r.name; err = r.err;
        } else {
            std::vector<OTrack> res; int lim = mode == 1 ? 8 : 1;
            if (!OnlineSearch(query, where, res, err, cancel.get(), lim) && where == 0) { res.clear(); OnlineSearch(query, 1, res, err, cancel.get(), lim); }
            for (auto& t : res) items.push_back(FromOTrack(t));
        }
        State& st = St(); std::lock_guard<std::mutex> lk(st.m);
        st.busyUsers.erase(c.uid); st.workers--;
        if (rg != st.runGen.load()) return;
        Guild& g = GuildRefL(c.guild);
        if (mode == 1) { std::vector<QItem> all = LocalMatchesL(query, 5); all.insert(all.end(), items.begin(), items.end()); SearchReplyL(c, query, all, err); return; }
        if (items.empty()) { ReplyPrivate(c.iid, L"Não achei" + (err.empty() ? std::wstring(L".") : L": " + err)); return; }
        EnqueueL(c, g, items, next, target, name);
    }).detach();
}
inline bool StartJobL(const Ctx& c) {
    State& s = St();
    if (s.busyUsers.count(c.uid)) { ReplyPrivate(c.iid, L"Espere a sua busca anterior terminar."); return false; }
    if (s.workers >= kMaxWorkers) { ReplyPrivate(c.iid, L"O PC do dono está ocupado com outras buscas: tente de novo em alguns segundos."); return false; }
    s.busyUsers.insert(c.uid); s.workers++;
    return true;
}
inline void SlashL(const Ctx& c, Guild& g) {
    State& s = St(); const std::string& n = c.name; bool dj = IsDjL(c);
    auto canAdd = [&]() { if (s.cfg.publicQueue || dj) return true; ReplyPrivate(c.iid, L"O dono deixou a fila fechada: só o dono, admins e quem tem o cargo " + s.cfg.djRole + L" põem músicas."); return false; };
    if (n == "help") { Reply(c.iid, HelpJsonL()); return; }
    if (n == "play") {
        if (!canAdd()) return;
        std::string target; if (!PickChannelL(c, g, target)) return;
        std::wstring q = Config::Trim(Utf8ToWide(OptS(c, "query"))); if (q.empty()) { ReplyPrivate(c.iid, L"Diga o nome da música ou cole um link."); return; }
        bool next = OptB(c, "next") && dj;
        if (IsUrlText(q)) {
            if (!SupportedLink(q)) { ReplyPrivate(c.iid, L"Link não suportado. Use YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music ou Bandcamp."); return; }
            if (!s.cfg.online) { ReplyPrivate(c.iid, L"O dono desligou músicas online no bot: use /playlist."); return; }
            if (!StartJobL(c)) return;
            SearchJob(c, q, 0, true, next, target, 0); return;
        }
        std::vector<QItem> local = LocalMatchesL(q, 1);
        if (!local.empty()) {   // so usa a das playlists quando o pedido descreve o titulo (senao "love" pegaria qualquer uma)
            size_t letters = 0, asked = 0; for (wchar_t ch : local[0].title) if (iswalnum(ch)) letters++; for (wchar_t ch : q) if (iswalnum(ch)) asked++;
            if (asked * 10 >= letters * 6 || !s.cfg.online) { EnqueueL(c, g, local, next, target, L""); return; }
        }
        if (!s.cfg.online) { ReplyPrivate(c.iid, L"Não achei nas playlists liberadas (o dono desligou a busca online)."); return; }
        if (!StartJobL(c)) return;
        SearchJob(c, q, 0, false, next, target, 0); return;
    }
    if (n == "search") {
        if (!canAdd()) return;
        std::wstring q = Config::Trim(Utf8ToWide(OptS(c, "query"))); if (q.empty()) { ReplyText(c.iid, L"O que buscar?"); return; }
        std::string src = OptS(c, "source"); int where = src == "yt" ? 1 : src == "sc" ? 2 : 0;
        if (!s.cfg.online) { SearchReplyL(c, q, LocalMatchesL(q, 13), L"a busca online está desligada; procurei só nas playlists liberadas"); return; }
        if (!StartJobL(c)) return;
        SearchJob(c, q, where, false, false, "", 1); return;
    }
    if (n == "playlist") {
        if (!s.cfg.publicPlaylists && !dj) { ReplyPrivate(c.iid, L"O dono não liberou as playlists para todo mundo."); return; }
        if (!canAdd()) return;
        const DPl* p = FindPlL(Utf8ToWide(OptS(c, "name")));
        if (!p) { ReplyPrivate(c.iid, L"Não achei essa playlist. Veja as liberadas com /playlists."); return; }
        if (p->items.empty()) { ReplyPrivate(c.iid, L"Essa playlist está vazia."); return; }
        std::string target; if (!PickChannelL(c, g, target)) return;
        std::vector<QItem> items = p->items;
        if (!s.cfg.online) items.erase(std::remove_if(items.begin(), items.end(), [](const QItem& x) { return x.path.empty(); }), items.end());
        if (OptB(c, "shuffle")) { std::random_device rd; std::mt19937 rng(rd()); std::shuffle(items.begin(), items.end(), rng); }
        EnqueueL(c, g, items, false, target, p->name); return;
    }
    if (n == "playlists") {
        if (!s.cfg.publicPlaylists && !dj) { ReplyText(c.iid, L"O dono não liberou as playlists para todo mundo."); return; }
        std::wstring d; int k = 0;
        for (auto& p : s.pls) { if (!s.cfg.pls.count(p.slug)) continue; if (++k > 40) { d += L"…"; break; } d += L"📂 **" + Md(Clip(p.name, 80)) + L"** · " + std::to_wstring(p.items.size()) + L" músicas\n"; }
        if (!k) d = L"Nenhuma playlist liberada para o bot ainda (o dono libera no painel DISCORD do Remix).";
        else d += L"\nToque com /playlist nome:...";
        Reply(c.iid, "{\"embeds\":[{\"color\":" + std::to_string(kColor) + ",\"title\":\"Playlists do dono\",\"description\":" + JStr(Clip(d, 3900)) + "}]}");
        return;
    }
    if (n == "queue") { Reply(c.iid, QueueJsonL(g, (int)OptI(c, "page", 1))); return; }
    if (n == "nowplaying") {
        if (!g.hasCur) { ReplyText(c.iid, L"Nada tocando agora."); return; }
        Reply(c.iid, NowPlayingJsonL(g, true)); return;
    }
    // controles: precisa estar na chamada com o bot (dono/admin de qualquer lugar)
    if (g.voice.empty() && n != "leave" && n != "stop") { ReplyPrivate(c.iid, L"O bot não está tocando neste servidor."); return; }
    if (!(c.owner || c.admin) && !g.voice.empty() && c.uvc != g.voice) { ReplyPrivate(c.iid, L"Entre em <#" + W(g.voice) + L"> para controlar a música."); return; }
    bool mine = g.hasCur && g.cur.by == c.uid;
    std::string action, arg; std::wstring what; bool itemOk = false;
    if (n == "skip") { if (!g.hasCur) { ReplyPrivate(c.iid, L"Nada tocando."); return; } action = "skip"; what = L"pular **" + Md(Clip(g.cur.title, 100)) + L"**"; itemOk = mine; }
    else if (n == "pause") { action = "pause"; what = L"pausar"; itemOk = mine; }
    else if (n == "resume") { action = "resume"; what = L"continuar"; itemOk = mine; }
    else if (n == "stop") { if (g.voice.empty() && g.joiningTo.empty()) { ReplyPrivate(c.iid, L"O bot não está numa chamada."); return; } action = "stop"; what = L"parar e limpar a fila"; }
    else if (n == "leave") { if (g.voice.empty() && g.joiningTo.empty()) { ReplyPrivate(c.iid, L"O bot não está numa chamada."); return; } action = "leave"; what = L"tirar o bot da chamada"; }
    else if (n == "shuffle") { action = "shuffle"; what = L"embaralhar a fila"; }
    else if (n == "clear") { action = "clear"; what = L"limpar a fila"; }
    else if (n == "loop") { action = "loop"; arg = OptS(c, "mode"); if (arg != "track" && arg != "queue") arg = "off"; what = L"repetir: " + std::wstring(LoopText(arg == "track" ? 1 : arg == "queue" ? 2 : 0)); }
    else if (n == "seek") {
        std::string t = OptS(c, "time"); int m = 0, sec = 0, h = 0, sN = 0;
        if (sscanf(t.c_str(), "%d:%d:%d", &h, &m, &sec) == 3) sN = h * 3600 + m * 60 + sec; else if (sscanf(t.c_str(), "%d:%d", &m, &sec) == 2) sN = m * 60 + sec; else sN = atoi(t.c_str());
        if (sN < 0) sN = 0;
        if (!g.hasCur) { ReplyPrivate(c.iid, L"Nada tocando."); return; }
        action = "seek"; arg = std::to_string(sN); what = L"ir para " + Dur(sN); itemOk = mine;
    }
    else if (n == "effect") {
        std::string ty = OptS(c, "type"); int curLv = ty == "slow" ? g.fx.slow : ty == "speed" ? g.fx.speed : ty == "reverb" ? g.fx.reverb : ty == "bass" ? g.fx.bass : ty == "8d" ? g.fx.d8 : 0;
        int lv = (int)OptI(c, "level", curLv ? 0 : 2); if (ty == "off") lv = 0; action = "fx"; arg = ty + ":" + std::to_string(lv); what = ty == "off" ? L"desligar os efeitos" : L"efeito " + Utf8ToWide(ty) + L" nível " + std::to_wstring(lv); itemOk = mine;
    }
    else if (n == "volume") { if (!dj) { ReplyPrivate(c.iid, L"Só DJ, admins e o dono mudam o volume (cada um pode ajustar o volume do bot no próprio Discord: botão direito no bot)."); return; } action = "vol"; arg = std::to_string(OptI(c, "percent", 100)); what = L"volume"; }
    else if (n == "remove") {
        long long p = OptI(c, "position", 0); if (p < 1 || p > (long long)g.q.size()) { ReplyPrivate(c.iid, L"Essa posição não existe (veja /fila)."); return; }
        const QItem& it = g.q[(size_t)(p - 1)];
        action = "remove"; arg = std::to_string(p) + ":" + host::IdFor(it.path + it.url + it.title); what = L"remover **" + Md(Clip(it.title, 100)) + L"**"; itemOk = it.by == c.uid;
    }
    else if (n == "move") { long long a = OptI(c, "from", 0), b = OptI(c, "to", 0); action = "move"; arg = std::to_string(a) + ":" + std::to_string(b); what = L"mover a #" + std::to_wstring(a) + L" para #" + std::to_wstring(b); }
    else { ReplyPrivate(c.iid, L"Comando desconhecido."); return; }
    if (!GateL(c, g, action, arg, what, itemOk)) return;
    ReplyText(c.iid, DoActionL(g, action, arg));
}
inline void ComponentL(const Ctx& c, Guild& g) {
    State& s = St(); const std::string& id = c.custom;
    if (id.rfind("q:", 0) == 0) {   // paginas da fila
        size_t a = id.find(':', 2); if (a == std::string::npos || id.substr(2, a - 2) != c.guild) return;
        Reply(c.iid, QueueJsonL(g, atoi(id.c_str() + a + 1))); return;
    }
    if (id.rfind("v:", 0) == 0) {
        auto it = s.votes.find(id.substr(2));
        if (it == s.votes.end() || it->second->guild != c.guild) { ReplyText(c.iid, L"Essa votação já acabou."); return; }
        Vote& v = *it->second; int need = NeedVotes(g.humans, s.cfg.votePct);
        if (g.voice.empty() || c.uvc != g.voice) { ReplyText(c.iid, L"Só quem está na chamada com o bot pode votar."); return; }
        if (!v.voters.insert(c.uid).second) { ReplyText(c.iid, L"Você já votou (" + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L")."); return; }
        if ((int)v.voters.size() >= need || PrivL(c, g)) {
            std::wstring r = DoActionL(g, v.action, v.arg); ShowVoteL(v, need, 1); s.votes.erase(it); ReplyText(c.iid, r); return;
        }
        ShowVoteL(v, need, 0); ReplyText(c.iid, L"Voto contado (" + std::to_wstring(v.voters.size()) + L"/" + std::to_wstring(need) + L").");
        return;
    }
    if (id.rfind("s:", 0) == 0) {   // escolheu um resultado da busca
        auto it = s.searches.find(id.substr(2));
        if (it == s.searches.end()) { ReplyText(c.iid, L"Essa busca expirou: busque de novo."); return; }
        if (it->second.user != c.uid || it->second.guild != c.guild) { ReplyText(c.iid, L"Essa busca é de outra pessoa."); return; }
        size_t k = c.values.empty() ? 9999 : (size_t)atoi(c.values[0].c_str());
        if (k >= it->second.items.size()) { ReplyText(c.iid, L"Escolha inválida."); return; }
        if (!s.cfg.publicQueue && !IsDjL(c)) { ReplyText(c.iid, L"O dono deixou a fila fechada."); return; }
        std::string target; if (!PickChannelL(c, g, target)) return;
        EnqueueL(c, g, { it->second.items[k] }, false, target, L""); return;
    }
    if (id.rfind("c:", 0) == 0) {   // botoes do "tocando agora"
        size_t a = id.find(':', 2); if (a == std::string::npos || id.substr(2, a - 2) != c.guild) { ReplyText(c.iid, L"Botão antigo."); return; }
        std::string act = id.substr(a + 1);
        if (act == "queue") { Reply(c.iid, QueueJsonL(g, 1)); return; }
        if (g.voice.empty()) { ReplyText(c.iid, L"O bot não está tocando."); return; }
        if (!(c.owner || c.admin) && c.uvc != g.voice) { ReplyText(c.iid, L"Entre em <#" + W(g.voice) + L"> para controlar."); return; }
        bool mine = g.hasCur && g.cur.by == c.uid; std::wstring what; std::string arg; bool itemOk = false;
        if (act == "pause") { what = L"pausar"; itemOk = mine; }
        else if (act == "resume") { what = L"continuar"; itemOk = mine; }
        else if (act == "skip") { if (!g.hasCur) { ReplyText(c.iid, L"Nada tocando."); return; } what = L"pular **" + Md(Clip(g.cur.title, 100)) + L"**"; itemOk = mine; }
        else if (act == "stop") what = L"parar e limpar a fila";
        else if (act == "loop") { act = "loop"; int nx = (g.loop + 1) % 3; arg = nx == 1 ? "track" : nx == 2 ? "queue" : "off"; what = L"repetir: " + std::wstring(LoopText(nx)); }
        else { ReplyText(c.iid, L"Botão desconhecido."); return; }
        if (!GateL(c, g, act, arg, what, itemOk)) return;
        ReplyText(c.iid, DoActionL(g, act, arg));
        return;
    }
    ReplyText(c.iid, L"Botão desconhecido.");
}

// ---- eventos do bot ---------------------------------------------------------------------------------
inline Ctx ParseCtx(const JVal& v) {
    Ctx c; c.iid = v.str("iid"); c.kind = v.str("kind"); c.name = v.str("name"); c.custom = v.str("custom");
    c.guild = v.str("guild"); c.channel = v.str("channel"); c.uvc = v.str("uvc"); c.bvc = v.str("bvc"); c.gname = W(v.str("gname"));
    c.humans = (int)v.num("humans", 0);
    if (const JVal* u = v.get("user")) {
        c.uid = u->str("id"); c.uname = W(u->str("name"));
        if (const JVal* b = u->get("admin")) c.admin = b->t == JVal::BOOL && b->b;
        if (const JVal* b = u->get("owner")) c.owner = b->t == JVal::BOOL && b->b;
        if (const JVal* r = u->get("roles")) if (r->t == JVal::ARR) for (auto& x : r->a) if (x.t == JVal::STR && c.roles.size() < 60) c.roles.push_back(W(x.s));
    }
    if (const JVal* o = v.get("opts")) if (o->t == JVal::OBJ) c.opts = *o;
    if (const JVal* a = v.get("values")) if (a->t == JVal::ARR) for (auto& x : a->a) if (x.t == JVal::STR && c.values.size() < 25) c.values.push_back(x.s);
    return c;
}
inline void OnBotL(const JVal& v, const std::string& t) {
    State& s = St();
    if (t == "hello") { LogLocked(L"bot iniciado (Node " + W(v.str("node")) + L")"); return; }
    if (t == "log") { LogLocked(W(v.str("msg"))); return; }
    if (t == "ready") {
        s.phase = 2; s.botId = v.str("id"); s.appId = v.str("appId"); s.botTag = W(v.str("tag")); s.restarts = 0; s.fatal = false;
        s.owners.clear(); if (const JVal* o = v.get("owners")) if (o->t == JVal::ARR) for (auto& x : o->a) if (x.t == JVal::STR) s.owners.insert(x.s);
        s.guildNames.clear(); if (const JVal* gs = v.get("guilds")) if (gs->t == JVal::ARR) for (auto& x : gs->a) s.guildNames[x.str("id")] = W(x.str("name"));
        for (auto& kv : s.guilds) { auto n = s.guildNames.find(kv.first); if (n != s.guildNames.end()) kv.second.name = n->second; }
        s.status = L"Conectado como " + s.botTag + L" em " + std::to_wstring(s.guildNames.size()) + (s.guildNames.size() == 1 ? L" servidor." : L" servidores.");
        LogLocked(s.status); SendListsL(); return;
    }
    if (t == "fatal") {
        std::string code = v.str("code"); s.phase = 3; s.fatal = code == "token" || code == "intents";   // rede fora do ar: tenta de novo sozinho
        s.status = code == "token" ? L"Token inválido: no Discord Developer Portal, abra o bot > Bot > Reset Token e cole o novo aqui."
                 : code == "intents" ? L"O Discord recusou as permissões do bot (intents)." : L"Não consegui conectar: " + W(v.str("msg"));
        LogLocked(s.status); return;
    }
    if (t == "guilds") {
        s.guildNames.clear(); if (const JVal* gs = v.get("guilds")) if (gs->t == JVal::ARR) for (auto& x : gs->a) s.guildNames[x.str("id")] = W(x.str("name"));
        for (auto it = s.guilds.begin(); it != s.guilds.end();) { if (!s.guildNames.count(it->first)) it = s.guilds.erase(it); else { it->second.name = s.guildNames[it->first]; ++it; } }
        return;
    }
    if (t == "cmd") {
        Ctx c = ParseCtx(v);
        if (!SafeId(c.guild) || !SafeId(c.uid) || c.iid.empty() || c.iid.size() > 40) return;
        Guild& g = GuildRefL(c.guild);
        if (!c.gname.empty()) { g.name = c.gname; s.guildNames[c.guild] = c.gname; }
        if (!c.bvc.empty() || g.joiningTo.empty()) { g.voice = c.bvc; g.humans = c.humans; }
        if (c.kind == "slash" && SafeId(c.channel)) g.text = c.channel;
        if (!RateOkL(c.uid)) { ReplyPrivate(c.iid, L"Calma: muitos comandos seguidos. Espere alguns segundos."); return; }
        if (c.kind == "slash") SlashL(c, g); else ComponentL(c, g);
        return;
    }
    if (t == "state") {
        std::string gid = v.str("guild"); auto it = s.guilds.find(gid); if (it == s.guilds.end()) return;
        Guild& g = it->second; unsigned gen = (unsigned)v.num("gen", 0); std::string st = v.str("s");
        if (gen != g.gen || !g.hasCur) return;
        if (st == "playing") { g.startedMs = NowMs(); g.pausedMs = 0; if (g.npPending) { g.npPending = false; NewNpL(g); } return; }
        bool failed = st == "error" || !g.startedMs;   // erro, ou acabou sem nunca ter tocado
        if (st == "error") LogLocked(L"falhou tocar " + g.cur.title + L": " + W(v.str("err")));
        int pos = PosSecL(g);
        if (!failed && g.cur.dur > 20 && pos < g.cur.dur - 8 && !g.retried && g.cur.path.empty()) {   // online parou no meio (link expirou?): retoma uma vez do ponto
            g.retried = true; LogLocked(L"retomando " + g.cur.title + L" em " + Dur(pos)); PlayCurL(g, pos, false); return;
        }
        if (failed) {
            if (++g.fails >= 5) { Announce(g, L"⚠️ Várias músicas seguidas não tocaram: parei. Veja se o yt-dlp e o ffmpeg do PC do dono estão em dia."); g.fails = 0; g.q.clear(); StopAudioL(g); DeleteNpL(g); PresenceL(); return; }
            Announce(g, L"⚠️ Não consegui tocar **" + Md(Clip(g.cur.title, 120)) + L"**: pulando.");
        } else g.fails = 0;
        StartNextL(g, failed);
        return;
    }
    if (t == "joined") {
        std::string gid = v.str("guild"); auto it = s.guilds.find(gid); if (it == s.guilds.end()) return;
        Guild& g = it->second; std::string ch = v.str("channel");
        if (ch != g.joiningTo) return;
        g.joiningTo.clear();
        bool ok = v.get("ok") && v.get("ok")->t == JVal::BOOL && v.get("ok")->b;
        if (!ok) {
            std::string e = v.str("err");
            std::wstring why = e == "sem_permissao_entrar" ? L"não tenho permissão para entrar nesse canal" : e == "sem_permissao_falar" ? L"não tenho permissão para falar nesse canal" : e == "canal_cheio" ? L"o canal está cheio" : L"falhou (" + W(e) + L")";
            Announce(g, L"❌ Não consegui entrar em <#" + W(ch) + L">: " + why + L".");
            g.q.clear(); if (g.voice.empty()) { g.hasCur = false; StopAudioL(g); }
            return;
        }
        g.voice = ch;
        if (g.hasCur && g.pendingPlay) PlayCurL(g, 0, true);
        else if (!g.hasCur && !g.q.empty()) StartNextL(g, true);
        return;
    }
    if (t == "left" || t == "vc") {
        std::string gid = v.str("guild"); auto it = s.guilds.find(gid); if (it == s.guilds.end()) return;
        Guild& g = it->second; std::string ch = t == "left" ? "" : v.str("channel");
        g.humans = (int)v.num("humans", 0);
        if (ch.empty()) {
            if (!g.joiningTo.empty()) return;   // entrando: o "vc" vazio e de antes
            if (!g.voice.empty() || g.hasCur) { g.q.clear(); StopAudioL(g); DeleteNpL(g); PresenceL(); }
            g.voice.clear(); g.humans = 0; g.aloneSince = 0; return;
        }
        g.voice = ch;
        if (g.humans > 0) g.aloneSince = 0; else if (!g.aloneSince) g.aloneSince = NowMs();
        return;
    }
    if (t == "ovc") {
        std::string gid = v.str("guild"), ch = v.str("channel");
        if (!SafeId(gid)) return;
        if (ch.empty()) { s.ownerVc.erase(gid); if (s.ownerGuild == gid) s.ownerGuild.clear(); }
        else { s.ownerVc[gid] = ch; s.ownerGuild = gid; }
        return;
    }
    if (t == "sent") {
        std::string key = v.str("key"), ch = v.str("channel"), msg = v.str("msg");
        if (key.rfind("np:", 0) == 0) {
            size_t a = key.find(':', 3); if (a == std::string::npos) return;
            auto it = s.guilds.find(key.substr(3, a - 3)); unsigned serial = (unsigned)atol(key.c_str() + a + 1);
            if (it != s.guilds.end() && it->second.npSerial == serial && it->second.hasCur) { it->second.npChannel = ch; it->second.npMsg = msg; }
            else DeleteMsg(ch, msg);   // a musica ja trocou: mensagem velha
        } else if (key.rfind("vote:", 0) == 0) {
            auto it = s.votes.find(key.substr(5)); if (it != s.votes.end()) { it->second->channel = ch; it->second->msg = msg; }
        }
        return;
    }
}

// ---- audio: servidor em 127.0.0.1 ---------------------------------------------------------------------
inline int OpusEncoder() {   // 1 = libopus; 0 = o opus nativo do ffmpeg (experimental, mas funciona)
    static std::atomic<int> v{ -1 };
    if (v.load() >= 0) return v.load();
    CapResult r = RunCapture({ FfmpegTool(), L"-hide_banner", L"-encoders" }, 15000, nullptr);
    v = r.out.find("libopus") != std::string::npos ? 1 : 0; return v.load();
}
inline void Send404(hsock_t c) { hostnet::SendAll(c, std::string("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n")); }
inline void ServeAudio(hsock_t c, unsigned rg) {
    State& s = St();
    hostnet::SetTimeout(c, 5000);
    std::string req; char b[1024];
    while (req.find("\r\n\r\n") == std::string::npos) { if (req.size() > 4096) return; long n = hostnet::Recv(c, b, sizeof b); if (n <= 0) return; req.append(b, (size_t)n); }
    if (req.rfind("GET /", 0) != 0) { Send404(c); return; }
    size_t sp = req.find(' ', 4); if (sp == std::string::npos) { Send404(c); return; }
    std::string path = req.substr(4, sp - 4);
    if (path.size() != 1 + 32 + 1 + 24 || path[33] != '/') { Send404(c); return; }
    Ticket t; std::shared_ptr<std::atomic<unsigned>> genRef;
    {
        std::lock_guard<std::mutex> lk(s.m);
        if (s.secret.size() != 32 || !host::ConstEq(path.substr(1, 32), s.secret)) { Send404(c); return; }
        auto it = s.tickets.find(path.substr(34)); if (it == s.tickets.end()) { Send404(c); return; }
        t = it->second; s.tickets.erase(it);
        auto g = s.guilds.find(t.guild); if (g == s.guilds.end() || NowMs() - t.created > 60000) { Send404(c); return; }
        genRef = g->second.genRef;
    }
    auto allowed = [&]() { return s.runGen.load() == rg && genRef->load() == t.gen; };
    if (!allowed()) { Send404(c); return; }
    EnsureTools();
    bool online = t.item.path.empty();
    if (!FfmpegOk() || (online && !YtdlpOk())) { Send404(c); return; }
    std::wstring play = online ? (t.item.play.empty() ? t.item.url : t.item.play) : L"";
    MediaInfo mi; auto cancel = s.cancel;
    if (online) {
        if (NeedsMatch(DetectSource(play))) {   // Spotify/Deezer/Apple: a mesma musica no YouTube Music
            OTrack ot; ot.url = t.item.url; ot.title = t.item.title; ot.artist = t.item.artist; ot.dur = t.item.dur; std::wstring e;
            if (!MatchOnYouTube(ot, e, cancel.get()) || ot.play.empty() || !host::HttpLink(ot.play)) { Send404(c); return; }
            play = ot.play;
            std::lock_guard<std::mutex> lk(s.m); auto g = s.guilds.find(t.guild);
            if (g != s.guilds.end() && g->second.hasCur && g->second.cur.url == t.item.url) g->second.cur.play = play;   // seek/efeito depois nao procura de novo
        }
        if (!host::HttpLink(play)) { Send404(c); return; }
        std::wstring gerr;
        if (!GetMediaInfo(play, mi, gerr, cancel.get())) { Send404(c); return; }
    } else {
        std::error_code ec; if (!std::filesystem::exists(std::filesystem::path(t.item.path), ec)) { Send404(c); return; }
    }
    int realDur = online ? mi.dur : 0;
    if (!online && t.item.dur <= 0) {   // duracao do arquivo (a playlist nao guarda): aparece no "tocando agora" e no /avancar
        ma_decoder_config dcfg = ma_decoder_config_init(ma_format_unknown, 0, 0); ma_decoder d;
#ifdef _WIN32
        bool ok = ma_decoder_init_file_w(t.item.path.c_str(), &dcfg, &d) == MA_SUCCESS;
#else
        bool ok = ma_decoder_init_file(WideToUtf8(t.item.path).c_str(), &dcfg, &d) == MA_SUCCESS;
#endif
        if (ok) { ma_uint64 frames = 0; if (ma_decoder_get_length_in_pcm_frames(&d, &frames) == MA_SUCCESS && d.outputSampleRate) realDur = (int)(frames / d.outputSampleRate); ma_decoder_uninit(&d); }
    }
    if (realDur > 0 && t.item.dur <= 0) {
        std::lock_guard<std::mutex> lk(s.m); auto g = s.guilds.find(t.guild);
        if (g != s.guilds.end() && g->second.hasCur && g->second.gen == t.gen && g->second.cur.dur <= 0) { g->second.cur.dur = realDur; UpdateNpL(g->second); }
    }
    if (!allowed()) { Send404(c); return; }
    bool pipeMode = online && mi.url.empty();
    std::vector<std::wstring> fa = { FfmpegTool(), L"-nostdin", L"-hide_banner", L"-loglevel", L"error" };
    std::wstring ss = std::to_wstring(t.startSec);
    if (!online) {
        for (const wchar_t* x : { L"-protocol_whitelist", L"file" }) fa.push_back(x);
        if (t.startSec > 0) { fa.push_back(L"-ss"); fa.push_back(ss); }
        fa.push_back(L"-i"); fa.push_back(t.item.path);
    } else if (!pipeMode) {
        if (mi.url.rfind("https://", 0) != 0 && mi.url.rfind("http://", 0) != 0) { Send404(c); return; }
        for (const wchar_t* x : { L"-protocol_whitelist", L"https,http,tls,tcp,crypto", L"-reconnect", L"1", L"-reconnect_streamed", L"1", L"-reconnect_delay_max", L"5" }) fa.push_back(x);
        if (t.startSec > 0) { fa.push_back(L"-ss"); fa.push_back(ss); }
        if (!mi.headers.empty()) { std::string hs; for (auto& x : mi.headers) { std::string l; for (char ch : x) if (ch != '\r' && ch != '\n') l.push_back(ch); hs += l + "\r\n"; } fa.push_back(L"-headers"); fa.push_back(Utf8ToWide(hs)); }
        fa.push_back(L"-i"); fa.push_back(Utf8ToWide(mi.url));
    } else { fa.push_back(L"-protocol_whitelist"); fa.push_back(L"pipe"); fa.push_back(L"-i"); fa.push_back(L"pipe:0"); if (t.startSec > 0) { fa.push_back(L"-ss"); fa.push_back(ss); } }
    if (t.fx.Any() || t.vol != 100) {
        std::string g = host::FxGraph(t.fx);
        if (t.vol != 100) { char vb[48]; snprintf(vb, sizeof vb, ",volume=%.2f,alimiter", t.vol / 100.0); size_t p = g.rfind(",alimiter"); if (p != std::string::npos) g.replace(p, 9, vb); }
        fa.push_back(L"-filter_complex"); fa.push_back(Utf8ToWide(g)); fa.push_back(L"-map"); fa.push_back(L"[o]");
    } else for (const wchar_t* x : { L"-vn", L"-sn", L"-dn" }) fa.push_back(x);
    for (const wchar_t* x : { L"-map_metadata", L"-1", L"-ac", L"2", L"-ar", L"48000" }) fa.push_back(x);
    if (OpusEncoder() == 1) for (const wchar_t* x : { L"-c:a", L"libopus", L"-b:a", L"96k", L"-vbr", L"on", L"-application", L"audio", L"-frame_duration", L"20" }) fa.push_back(x);
    else for (const wchar_t* x : { L"-c:a", L"opus", L"-strict", L"-2", L"-b:a", L"96k" }) fa.push_back(x);
    for (const wchar_t* x : { L"-f", L"ogg", L"-page_duration", L"100000", L"pipe:1" }) fa.push_back(x);
    Proc dec, src; std::thread pump;
    if (!dec.Start(fa, true, false, pipeMode)) { Send404(c); return; }
    if (pipeMode) {
        auto ya = YtdlpArgs();
        for (const wchar_t* x : { L"--no-playlist", L"-q", L"-f", L"bestaudio/best", L"-o", L"-" }) ya.push_back(x);
        ya.push_back(L"--"); ya.push_back(play);
        if (!src.Start(ya, true, false, false)) { dec.Kill(); dec.Wait(); Send404(c); return; }
        pump = std::thread([&] { char pb[65536]; for (;;) { long n = src.ReadOut(pb, sizeof pb); if (n <= 0) break; if (!dec.WriteIn(pb, (size_t)n)) break; } dec.CloseIn(); });
    }
    hostnet::SetTimeout(c, 0);   // pausado, o bot para de ler: nao e erro
    bool okHead = hostnet::SendAll(c, std::string("HTTP/1.1 200 OK\r\nContent-Type: audio/ogg\r\nCache-Control: no-store\r\nConnection: close\r\n\r\n"));
    char buf[16384]; long long sent = 0; bool sendFail = false;
    while (okHead) {
        if (!allowed()) { hostnet::Abort(c); break; }
        long n = dec.ReadOut(buf, sizeof buf); if (n <= 0) break;
        if (!hostnet::SendAll(c, buf, (size_t)n)) { sendFail = true; break; }
        sent += n;
    }
    bool cut = !allowed();
    dec.Kill(); if (pipeMode) src.Kill();
    if (pump.joinable()) pump.join();
    int code = dec.Wait(); if (pipeMode) src.Wait();
    if (online && !pipeMode && !cut && !sendFail && (sent == 0 || code != 0)) ForgetMediaInfo(play);   // link direto falhou/expirou: extrai de novo na proxima
}
inline hsock_t ListenLoopback(int& port) {
    port = 0;
    if (!hostnet::Init()) return HSOCK_BAD;
    hsock_t ls = hostnet::NewSocket(AF_INET); if (ls == HSOCK_BAD) return HSOCK_BAD;
#ifdef _WIN32
    int excl = 1; setsockopt(ls, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof excl);
#endif
    sockaddr_in a; std::memset(&a, 0, sizeof a); a.sin_family = AF_INET; a.sin_port = 0; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(ls, (sockaddr*)&a, sizeof a) != 0 || listen(ls, 16) != 0) { hostnet::Close(ls); return HSOCK_BAD; }
    socklen_t n = sizeof a; if (getsockname(ls, (sockaddr*)&a, &n) != 0) { hostnet::Close(ls); return HSOCK_BAD; }
    port = ntohs(a.sin_port);
    return ls;
}

// ---- manutencao (a cada meio segundo enquanto o bot roda) ----------------------------------------------
inline void TickL() {
    State& s = St(); long long now = NowMs();
    for (auto it = s.votes.begin(); it != s.votes.end();) {
        Vote& v = *it->second; auto g = s.guilds.find(v.guild);
        bool perTrack = v.action == "skip" || v.action == "pause" || v.action == "resume" || v.action == "seek" || v.action == "fx";
        bool stale = g == s.guilds.end() || (perTrack && g->second.trackNo != v.trackNo);
        if (now > v.expires || stale) { ShowVoteL(v, g == s.guilds.end() ? 1 : NeedVotes(g->second.humans, s.cfg.votePct), 2); it = s.votes.erase(it); }
        else ++it;
    }
    for (auto& kv : s.guilds) {
        Guild& g = kv.second;
        if (g.voice.empty()) continue;
        if (g.humans <= 0) { if (!g.aloneSince) g.aloneSince = now; else if (now - g.aloneSince > 180000) { Announce(g, L"👋 Saí porque ninguém estava ouvindo."); LeaveL(g); continue; } }
        else g.aloneSince = 0;
        if (!g.hasCur && g.q.empty() && g.joiningTo.empty()) { if (!g.idleSince) g.idleSince = now; else if (now - g.idleSince > 300000) { LeaveL(g); continue; } }
        if (g.hasCur && !g.paused && !g.startedMs && g.requestedMs && now - g.requestedMs > 90000) {   // o audio nunca comecou
            Announce(g, L"⚠️ **" + Md(Clip(g.cur.title, 120)) + L"** demorou demais para carregar: pulando.");
            StartNextL(g, true);
        }
    }
}

// ---- liga / desliga ------------------------------------------------------------------------------------
inline void StopLocked(State& s);
inline void ReaderLoop(Proc* p, unsigned rg) {
    std::string buf; char b[16384];
    for (;;) {
        long n = p->ReadOut(b, sizeof b); if (n <= 0) break;
        buf.append(b, (size_t)n);
        size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos); buf.erase(0, pos + 1);
            JVal v; if (line.size() > (1u << 20) || !OParse(line, v) || v.t != JVal::OBJ) continue;
            std::string t = v.str("t");
            { std::lock_guard<std::mutex> lk(St().m); if (St().runGen.load() != rg) return; OnBotL(v, t); }
            if (t == "ready" || t == "fatal" || t == "guilds" || t == "cmd" || t == "state" || t == "joined" || t == "left" || t == "vc") Changed();
        }
        if (buf.size() > (4u << 20)) buf.clear();
    }
    State& s = St();
    {
        std::lock_guard<std::mutex> lk(s.m);
        if (s.runGen.load() != rg) return;
        s.running = false; s.phase = 3;
        if (!s.fatal && s.on && s.restarts < 5) { s.restarts++; s.restartAt = NowMs() + 3000LL * s.restarts; s.status = L"O bot parou: reconectando..."; LogLocked(s.status); }
        else if (!s.fatal) { s.status = L"O bot parou várias vezes seguidas. Veja o registro e tente ligar de novo."; LogLocked(s.status); }
    }
    Changed();
}
inline void ErrLoop(Proc* p, unsigned rg) {
    std::string buf; char b[4096];
    for (;;) {
        long n = p->ReadErr(b, sizeof b); if (n <= 0) break;
        buf.append(b, (size_t)n); size_t pos;
        while ((pos = buf.find('\n')) != std::string::npos) {
            std::string line = buf.substr(0, pos); buf.erase(0, pos + 1);
            if (line.empty()) continue;
            std::lock_guard<std::mutex> lk(St().m); if (St().runGen.load() != rg) return; LogLocked(L"node: " + W(line));
        }
        if (buf.size() > 65536) buf.clear();
    }
}
inline bool WriteBotScript(std::wstring& err) {
    namespace fs = std::filesystem; std::error_code ec;
    fs::create_directories(fs::path(BotDir()), ec);
    std::wstring path = Config::Join(BotDir(), L"bot.mjs");
    std::string cur; { std::ifstream f(fs::path(path), std::ios::binary); if (f) cur.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>()); }
    if (cur == dcbot::BOT_JS) return true;
    std::ofstream f(fs::path(path), std::ios::binary | std::ios::trunc); f << dcbot::BOT_JS;
    if (!f) { err = L"Não consegui gravar o bot em " + BotDir(); return false; }
    return true;
}
// Liga o bot (bloqueia alguns ms: roda o "node --version"). A UI chama por StartAsync.
inline bool Start(std::wstring& err) {
    State& s = St(); std::lock_guard<std::mutex> life(s.life);
    std::string token; std::wstring botScript;
    {
        std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked();
        if (s.running) return true;
        token = s.token;
    }
    if (token.empty()) { err = L"Cole o token do bot primeiro (DEFINIR TOKEN)."; return false; }
    std::wstring ver, why; std::wstring node = FindNode(ver, why);
    std::wstring override_;
#ifndef _WIN32
    if (const char* fake = std::getenv("REMIX_DISCORD_BOT")) override_ = Utf8ToWide(fake);   // testes: outro script no lugar do bot
#endif
    if (node.empty()) { err = why.empty() ? L"Precisa do Node.js 22.12+: rode o instalador de dependências e escolha DISCORD." : why; return false; }
    if (override_.empty() && !DepsOk()) { err = L"Falta instalar o bot: toque em INSTALAR BOT."; return false; }
    if (override_.empty() && !WriteBotScript(err)) return false;
    botScript = override_.empty() ? Config::Join(BotDir(), L"bot.mjs") : override_;
    if (!FfmpegOk()) EnsureTools();
    int port = 0; hsock_t ls = ListenLoopback(port);
    if (ls == HSOCK_BAD) { err = L"Não consegui abrir a porta local do áudio."; return false; }
    auto proc = std::make_unique<Proc>();
    if (!proc->Start({ node, L"--no-warnings", botScript }, true, true, true)) { hostnet::Close(ls); err = L"Não consegui iniciar o Node.js."; return false; }
    unsigned rg;
    {
        std::lock_guard<std::mutex> lk(s.m);
        rg = ++s.runGen;
        s.cancel = std::make_shared<std::atomic<bool>>(false);
        s.proc = std::move(proc); s.ls = ls; s.port = port; s.secret = host::RandomHex(16);
        s.running = true; s.phase = 1; s.fatal = false; s.status = L"Conectando ao Discord..."; s.nodeVer = ver; s.restartAt = 0;
        s.tickets.clear(); s.votes.clear(); s.searches.clear(); s.busyUsers.clear(); s.workers = 0; s.guilds.clear(); s.ownerVc.clear(); s.ownerGuild.clear(); s.presenceSent = false;
        { std::lock_guard<std::mutex> wl(s.wm); s.wq.clear(); s.wstop = false; }
        LogLocked(L"ligando (Node " + ver + L")");
    }
    Proc* p = s.proc.get();
    s.writer = std::thread([p, rg] {
        State& st = St();
        for (;;) {
            std::string line;
            { std::unique_lock<std::mutex> lk(st.wm); st.wcv.wait(lk, [&] { return st.wstop || !st.wq.empty(); }); if (st.wq.empty()) return; line = std::move(st.wq.front()); st.wq.pop_front(); }
            line.push_back('\n');
            if (!p->WriteIn(line.data(), line.size())) return;
            (void)rg;
        }
    });
    s.reader = std::thread(ReaderLoop, p, rg);
    s.errReader = std::thread(ErrLoop, p, rg);
    s.acceptor = std::thread([rg] {
        State& st = St();
        while (st.runGen.load() == rg) {
            hsock_t r = hostnet::WaitAccept(st.ls, HSOCK_BAD, 400); if (r == HSOCK_BAD) continue;
            hostnet::Peer peer; hsock_t c = hostnet::Accept(st.ls, peer); if (c == HSOCK_BAD) continue;
            if (peer.family != AF_INET || !hostnet::IsLoopback4(peer.addr.data()) || st.streams.load() >= 8) { hostnet::Close(c); continue; }
            st.streams++;
            std::thread([c, rg] { ServeAudio(c, rg); hostnet::Close(c); St().streams--; }).detach();
        }
    });
    s.ticker = std::thread([rg] {
        State& st = St();
        while (st.runGen.load() == rg) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            std::lock_guard<std::mutex> lk(st.m); if (st.runGen.load() != rg) return;
            TickL();
            if (st.plsSent != st.plsVer) SendListsL();
        }
    });
    Push("{\"t\":\"login\",\"token\":" + JStrA(token) + "}");
    std::fill(token.begin(), token.end(), '\0');
    Changed();
    return true;
}
inline void Stop() {
    State& s = St(); std::lock_guard<std::mutex> life(s.life);
    std::unique_ptr<Proc> proc;
    {
        std::lock_guard<std::mutex> lk(s.m);
        if (!s.proc) { s.running = false; if (s.phase != 3 || !s.fatal) s.phase = 0; return; }
        s.runGen++; s.cancel->store(true);
        s.running = false; s.phase = 0; s.status = L"Desligado."; s.restartAt = 0;
        for (auto& kv : s.guilds) kv.second.genRef->store(0xFFFFFFFFu);
        s.guilds.clear(); s.votes.clear(); s.tickets.clear();
        proc = std::move(s.proc);
    }
    Push("{\"t\":\"quit\"}");
    { std::lock_guard<std::mutex> wl(s.wm); s.wstop = true; }
    s.wcv.notify_all();
    // o bot sai sozinho com "quit" (sai das chamadas); travado, morre em 2 s
    std::atomic<bool> done{ false };
    std::thread killer([&done, p = proc.get()] { for (int i = 0; i < 40 && !done.load(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(50)); if (!done.load()) p->Kill(); });
    if (s.writer.joinable()) s.writer.join();
    proc->CloseIn();
    proc->Wait(); done = true; killer.join();
    if (s.reader.joinable()) s.reader.join();
    if (s.errReader.joinable()) s.errReader.join();
    if (s.ticker.joinable()) s.ticker.join();
    if (s.acceptor.joinable()) s.acceptor.join();
    { std::lock_guard<std::mutex> lk(s.m); if (s.ls != HSOCK_BAD) { hostnet::Close(s.ls); s.ls = HSOCK_BAD; } s.port = 0; s.secret.clear(); LogLocked(L"desligado"); }
    { std::lock_guard<std::mutex> wl(s.wm); s.wq.clear(); }
    Changed();
}
inline void StartAsync() {
    std::thread([] {
        State& s = St(); std::wstring err;
        if (!Start(err)) { std::lock_guard<std::mutex> lk(s.m); s.phase = 3; s.status = err; s.on = false; SaveLocked(); LogLocked(err); }
        Changed();
    }).detach();
}
// Chamado pela UI a cada quadro: reconecta se o bot caiu.
inline void Poll() {
    State& s = St(); bool go = false;
    { std::unique_lock<std::mutex> lk(s.m, std::try_to_lock); if (!lk.owns_lock()) return; if (s.restartAt && NowMs() >= s.restartAt && s.on && !s.fatal) { s.restartAt = 0; go = true; } }
    if (go) std::thread([] { Stop(); { std::lock_guard<std::mutex> lk(St().m); St().on = true; } StartAsync(); }).detach();
}
inline void SetOn(bool on) {
    State& s = St();
    {
        std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked(); s.on = on; s.restarts = 0; SaveLocked();
        if (on && s.running) { s.version++; return; }   // ja esta ligado
        if (on) { s.phase = 1; s.status = L"Ligando..."; }
    }
    if (on) StartAsync(); else std::thread([] { Stop(); }).detach();
    Changed();
}
inline bool SetToken(const std::wstring& tw, std::wstring& err) {
    std::string t = WideToUtf8(Config::Trim(tw));
    if (t.rfind("Bot ", 0) == 0) t = t.substr(4);
    if (!TokenShapeOk(t)) { err = L"Isso não parece um token de bot (Developer Portal > seu app > Bot > Reset Token > Copy)."; return false; }
    State& s = St(); bool wasOn;
    { std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked(); s.token = t; wasOn = s.on; s.fatal = false; SaveLocked(); }
    if (wasOn) std::thread([] { Stop(); { std::lock_guard<std::mutex> lk(St().m); St().on = true; } StartAsync(); }).detach();
    Changed();
    return true;
}
inline void ClearToken() {
    State& s = St();
    { std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked(); s.token.clear(); s.on = false; SaveLocked(); }
    std::thread([] { Stop(); }).detach();
    Changed();
}
inline void UpdateCfg(const std::function<void(Cfg&)>& f) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked(); f(s.cfg); s.plsVer++; SaveLocked(); s.version++;
}
inline std::string InviteUrl() {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    if (s.appId.empty()) return "";
    // ver canais (1024) + mandar mensagens (2048) + links embutidos (16384) + conectar (1048576) + falar (2097152)
    return "https://discord.com/oauth2/authorize?client_id=" + s.appId + "&scope=bot%20applications.commands&permissions=3165184";
}

// ---- o dono pelo PC -----------------------------------------------------------------------------------
// Toca agora no servidor onde o dono esta numa chamada (ou onde o bot ja esta). replace: troca a fila inteira.
inline bool OwnerPlay(std::vector<QItem> items, const std::wstring& listName, bool replace, std::wstring& msg) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    if (s.phase != 2) { msg = L"Ligue o bot do Discord primeiro (painel DISCORD)."; return false; }
    if (items.empty()) { msg = L"Nada para tocar."; return false; }
    std::string gid, target;
    if (!s.ownerGuild.empty() && s.ownerVc.count(s.ownerGuild)) { gid = s.ownerGuild; target = s.ownerVc[gid]; }
    else for (auto& kv : s.ownerVc) { gid = kv.first; target = kv.second; break; }
    if (gid.empty()) for (auto& kv : s.guilds) if (!kv.second.voice.empty()) { gid = kv.first; target = kv.second.voice; break; }
    if (gid.empty()) { msg = L"Entre num canal de voz no Discord (num servidor onde o bot está) e toque de novo."; return false; }
    Guild& g = GuildRefL(gid);
    if (!g.voice.empty() && target != g.voice && g.hasCur) target = g.voice;   // ja tocando em outro canal: fica la
    if (g.voice.empty() && g.joiningTo.empty() && ActiveVoiceL() >= kMaxVoice) { msg = L"O bot já está em " + std::to_wstring(kMaxVoice) + L" chamadas."; return false; }
    if (items.size() > (size_t)kMaxQueue) items.resize(kMaxQueue);
    for (auto& it : items) { it.by = "pc"; it.byName = L"o dono (pelo PC)"; }
    if (replace) g.q.clear();
    g.q.insert(g.q.begin(), items.begin(), items.end());
    while (g.q.size() > (size_t)kMaxQueue) g.q.pop_back();
    if (g.voice != target && g.joiningTo != target) JoinL(g, target);
    StartNextL(g, true);
    Announce(g, items.size() == 1 ? L"🎛️ O dono pôs para tocar pelo PC: **" + Md(Clip(ItemName(items[0]), 150)) + L"**"
                                  : L"🎛️ O dono pôs para tocar pelo PC a playlist **" + Md(Clip(listName, 100)) + L"** (" + std::to_wstring(items.size()) + L" músicas)");
    msg = (items.size() == 1 ? L"Tocando no Discord: " + items[0].title : L"Playlist no Discord: " + listName) + L" (" + (g.name.empty() ? W(gid) : g.name) + L")";
    return true;
}
inline void OwnerAction(const std::string& gid, const std::string& action) {
    State& s = St(); std::lock_guard<std::mutex> lk(s.m);
    auto it = s.guilds.find(gid); if (it == s.guilds.end()) return;
    std::string act = action, arg;
    if (act == "pausetoggle") act = it->second.paused ? "resume" : "pause";
    if (act == "loop") { int nx = (it->second.loop + 1) % 3; arg = nx == 1 ? "track" : nx == 2 ? "queue" : "off"; }
    std::wstring r = DoActionL(it->second, act, arg);
    if (act == "skip" || act == "stop") Announce(it->second, L"🎛️ Pelo PC do dono: " + r);
    s.version++;
}

// ---- tela ---------------------------------------------------------------------------------------------
struct GuildView { std::string id; std::wstring name, title, artist; bool voice = false, joining = false, playing = false, paused = false; int pos = 0, dur = 0, queue = 0, humans = 0, loop = 0; };
struct View {
    int phase = 0; bool on = false, hasToken = false, nodeOk = false, depsOk = false, installing = false, probing = false, ownerInVoice = false;
    std::wstring status, botTag, nodeVer, installMsg; std::string appId; Cfg cfg; int guildCount = 0;
    std::vector<GuildView> guilds; std::vector<std::wstring> logs; unsigned version = 0;
};
inline View GetView() {
    State& s = St(); View v; std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked();
    v.phase = s.phase; v.on = s.on; v.hasToken = !s.token.empty(); v.nodeOk = !s.nodePath.empty(); v.depsOk = s.depsOk; v.installing = s.installing.load(); v.probing = s.probing.load() || !s.toolsAt;
    v.status = s.status; v.botTag = s.botTag; v.nodeVer = s.nodeVer; v.installMsg = s.installMsg; v.appId = s.appId; v.cfg = s.cfg; v.guildCount = (int)s.guildNames.size();
    v.ownerInVoice = !s.ownerVc.empty();
    for (auto& kv : s.guilds) {
        const Guild& g = kv.second; if (g.voice.empty() && g.joiningTo.empty() && !g.hasCur) continue;
        GuildView gv; gv.id = g.id; gv.name = g.name.empty() ? W(g.id) : g.name; gv.voice = !g.voice.empty(); gv.joining = !g.joiningTo.empty();
        gv.playing = g.hasCur && !g.paused; gv.paused = g.paused; gv.title = g.hasCur ? g.cur.title : L""; gv.artist = g.hasCur ? g.cur.artist : L"";
        gv.pos = PosSecL(g); gv.dur = g.hasCur ? g.cur.dur : 0; gv.queue = (int)g.q.size(); gv.humans = g.humans; gv.loop = g.loop;
        v.guilds.push_back(gv);
    }
    v.logs.assign(s.logs.begin(), s.logs.end());
    v.version = s.version.load();
    return v;
}
inline bool Ready() { State& s = St(); std::unique_lock<std::mutex> lk(s.m, std::try_to_lock); return lk.owns_lock() ? s.phase == 2 : false; }
inline bool AutoStartWanted() { State& s = St(); std::lock_guard<std::mutex> lk(s.m); EnsureLoadedLocked(); return s.on && !s.token.empty(); }

} // namespace dc
