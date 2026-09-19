#pragma once
// Casca de sistema do Linux: fila de eventos thread->UI, dialogos de arquivo
// (kdialog/zenity rodando em thread), monitor de pasta (inotify recursivo),
// instancia unica + IPC (socket Unix em $XDG_RUNTIME_DIR), HTTP (libcurl via
// dlopen: sem dependencia de link nem de pacote -devel) e pasta temporaria.
#include "platform.h"
#include "config.h"
#include <string>
#include <vector>
#include <deque>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <filesystem>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <poll.h>
#include <unistd.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <signal.h>

namespace sys {

// ------------------------------------------------------- fila de eventos ---
// Substitui os PostMessageW(WM_APP+n): threads empilham, o loop principal drena.
struct Ev { int type = 0; std::wstring s; int n = 0; };
inline std::mutex& EvMutex() { static std::mutex m; return m; }
inline std::deque<Ev>& EvQueue() { static std::deque<Ev>* q = new std::deque<Ev>(); return *q; }
inline void Post(int type, std::wstring s = L"", int n = 0) {
    std::lock_guard<std::mutex> lk(EvMutex());
    EvQueue().push_back(Ev{ type, std::move(s), n });
}
inline bool Poll(Ev& out) {
    std::lock_guard<std::mutex> lk(EvMutex());
    if (EvQueue().empty()) return false;
    out = std::move(EvQueue().front());
    EvQueue().pop_front();
    return true;
}

// ---------------------------------------------------------------- shell ---
inline std::string ShQuote(const std::string& s) {
    std::string o = "'";
    for (char c : s) { if (c == '\'') o += "'\\''"; else o.push_back(c); }
    o += "'";
    return o;
}
inline bool HaveCmd(const char* name) {
    const char* path = std::getenv("PATH");
    std::string p = path ? path : "/usr/local/bin:/usr/bin:/bin";
    size_t st = 0;
    while (st <= p.size()) {
        size_t e = p.find(':', st);
        std::string dir = p.substr(st, e == std::string::npos ? std::string::npos : e - st);
        if (!dir.empty() && access((dir + "/" + name).c_str(), X_OK) == 0) return true;
        if (e == std::string::npos) break;
        st = e + 1;
    }
    return false;
}
inline std::string RunCapture(const std::string& cmd) {
    std::string out;
    FILE* f = popen(cmd.c_str(), "r");
    if (!f) return out;
    char buf[4096]; size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.append(buf, n);
    pclose(f);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

// -------------------------------------------------------------- dialogos ---
enum DialogKind { DLG_FOLDER, DLG_IMAGE, DLG_AUDIO_MULTI };
inline bool HasDialogTool() { return HaveCmd("kdialog") || HaveCmd("zenity"); }
inline std::string DialogCommand(DialogKind k, const std::string& title) {
    const char* de = std::getenv("XDG_CURRENT_DESKTOP");
    bool kde = de && (strstr(de, "KDE") || strstr(de, "kde"));
    bool hasK = HaveCmd("kdialog"), hasZ = HaveCmd("zenity");
    const char* home = std::getenv("HOME");
    std::string start = home ? home : ".";
    bool useK = hasK && (kde || !hasZ);
    if (useK) {
        if (k == DLG_FOLDER) return "kdialog --getexistingdirectory " + ShQuote(start) + " --title " + ShQuote(title) + " 2>/dev/null";
        if (k == DLG_AUDIO_MULTI) return "kdialog --getopenfilename " + ShQuote(start) + " " + ShQuote("*.mp3 *.MP3 *.flac *.FLAC *.ogg *.OGG *.opus *.m4a *.M4A *.wav *.WAV *.aac *.wma *.aiff *.webm *.mka|Áudio") + " --multiple --separate-output --title " + ShQuote(title) + " 2>/dev/null";
        return "kdialog --getopenfilename " + ShQuote(start) + " " + ShQuote("*.jpg *.jpeg *.png *.bmp *.JPG *.JPEG *.PNG *.BMP|Imagens") + " --title " + ShQuote(title) + " 2>/dev/null";
    }
    if (hasZ) {
        if (k == DLG_FOLDER) return "zenity --file-selection --directory --title=" + ShQuote(title) + " 2>/dev/null";
        if (k == DLG_AUDIO_MULTI) return "zenity --file-selection --multiple --separator=" + ShQuote("\n") + " --title=" + ShQuote(title) + " --file-filter=" + ShQuote("Áudio | *.mp3 *.MP3 *.flac *.FLAC *.ogg *.OGG *.opus *.m4a *.M4A *.wav *.WAV *.aac *.wma *.aiff *.webm *.mka") + " --file-filter=" + ShQuote("Todos | *") + " 2>/dev/null";
        return "zenity --file-selection --title=" + ShQuote(title) + " --file-filter=" + ShQuote("Imagens | *.jpg *.jpeg *.png *.bmp *.JPG *.JPEG *.PNG *.BMP") + " --file-filter=" + ShQuote("Todos | *") + " 2>/dev/null";
    }
    return "";
}
// Abre o dialogo numa thread e publica o resultado (caminho ou vazio) como
// evento 'evType' (campo n = contexto, ex.: indice da faixa).
inline void PickAsync(DialogKind k, const std::wstring& title, int evType, int n = 0) {
    std::string cmd = DialogCommand(k, WideToUtf8(title));
    if (cmd.empty()) { Post(evType, L"", n); return; }
    std::thread([cmd, evType, n]() {
        std::string r = RunCapture(cmd);
        Post(evType, Utf8ToWide(r), n);
    }).detach();
}

// ------------------------------------------------------- monitor de pasta --
// inotify recursivo: marca a hora do ultimo evento e conta pendencias; a UI
// espera a janela de acalmia e dispara o rescan (mesma logica do Windows).
struct FolderWatch {
    std::thread th;
    std::atomic<bool> stop{ true };
    std::atomic<unsigned long long> lastChangeMs{ 0 };
    std::atomic<int> pending{ 0 };

    void Start(const std::wstring& dir) {
        Stop();
        std::string d = WideToUtf8(dir);
        stop = false;
        try { th = std::thread([this, d]() { Run(d); }); }
        catch (...) { stop = true; }
    }
    void Stop() {
        stop = true;
        if (th.joinable()) th.join();
    }
    ~FolderWatch() { Stop(); }

private:
    void Run(std::string root) {
        int fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (fd < 0) return;
        const uint32_t mask = IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE | IN_DELETE_SELF | IN_MOVE_SELF;
        std::map<int, std::string> wds;
        int count = 0;
        std::function<void(const std::string&)> addTree = [&](const std::string& p) {
            if (count >= 4096 || stop.load()) return;
            int wd = inotify_add_watch(fd, p.c_str(), mask);
            if (wd >= 0) { wds[wd] = p; ++count; }
            std::error_code ec;
            for (std::filesystem::directory_iterator it(p, std::filesystem::directory_options::skip_permission_denied, ec), end; it != end && !stop.load(); it.increment(ec)) {
                if (ec) break;
                std::error_code e2;
                if (it->is_symlink(e2)) continue;
                if (!it->is_directory(e2)) continue;
                std::string name = it->path().filename().string();
                if (!name.empty() && name[0] == '.') continue;
                addTree(it->path().string());
            }
        };
        addTree(root);
        std::vector<char> buf(64 * 1024);
        while (!stop.load()) {
            pollfd p{ fd, POLLIN, 0 };
            int r = poll(&p, 1, 300);
            if (r <= 0) continue;
            ssize_t n = read(fd, buf.data(), buf.size());
            if (n <= 0) continue;
            size_t off = 0;
            while (off + sizeof(inotify_event) <= (size_t)n) {
                auto* ev = (inotify_event*)(buf.data() + off);
                if ((ev->mask & IN_CREATE) && (ev->mask & IN_ISDIR) && ev->len) {
                    auto it = wds.find(ev->wd);
                    if (it != wds.end()) addTree(it->second + "/" + ev->name);
                }
                lastChangeMs = GetTickCount64();
                pending++;
                off += sizeof(inotify_event) + ev->len;
            }
        }
        close(fd);
    }
};

// ------------------------------------------------ instancia unica + IPC ---
inline std::string RuntimeDir() {
    const char* x = std::getenv("XDG_RUNTIME_DIR");
    if (x && *x) return x;
    return "/tmp";
}
// Com --home/REMIX_HOME a instancia e separada (config diferente): testes e copias
// portateis em outra pasta nao "grudam" no Remix que ja esta aberto.
inline std::string InstanceSuffix() {
    const char* h = std::getenv("REMIX_HOME"); if (!h || !*h) return "";
    unsigned long long x = 1469598103934665603ULL; for (const char* p = h; *p; ++p) { x ^= (unsigned char)*p; x *= 1099511628211ULL; }
    char b[24]; snprintf(b, sizeof b, "-%08llx", x & 0xffffffffULL); return b;
}
inline std::string SockPath() { return RuntimeDir() + "/remix-player" + InstanceSuffix() + ".sock"; }
inline std::string LockPath() { return RuntimeDir() + "/remix-player" + InstanceSuffix() + ".lock"; }
inline int& LockFd() { static int fd = -1; return fd; }

// true = somos a unica instancia (lock adquirido). false = ja existe outra.
inline bool AcquireSingleInstance() {
    int fd = open(LockPath().c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0) return true; // sem lock disponivel: segue normalmente
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) { close(fd); return false; }
    LockFd() = fd;
    return true;
}
inline bool SendToExisting(const std::wstring& cmd) {
    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0) return false;
    sockaddr_un a{}; a.sun_family = AF_UNIX;
    std::string p = SockPath();
    if (p.size() >= sizeof(a.sun_path)) { close(s); return false; }
    strcpy(a.sun_path, p.c_str());
    if (connect(s, (sockaddr*)&a, sizeof(a)) != 0) { close(s); return false; }
    std::string u8 = WideToUtf8(cmd) + "\n";
    size_t off = 0;
    while (off < u8.size()) { ssize_t w = write(s, u8.data() + off, u8.size() - off); if (w <= 0) break; off += (size_t)w; }
    close(s);
    return true;
}
// Servidor: cada conexao entrega uma linha de comando -> evento 'evType'.
inline void StartIpcServer(int evType) {
    std::string p = SockPath();
    unlink(p.c_str());
    int s = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (s < 0) return;
    sockaddr_un a{}; a.sun_family = AF_UNIX;
    if (p.size() >= sizeof(a.sun_path)) { close(s); return; }
    strcpy(a.sun_path, p.c_str());
    if (bind(s, (sockaddr*)&a, sizeof(a)) != 0 || listen(s, 4) != 0) { close(s); return; }
    std::thread([s, evType]() {
        while (true) {
            int c = accept(s, nullptr, nullptr);
            if (c < 0) { if (errno == EINTR) continue; break; }
            std::string data; char buf[4096]; ssize_t n;
            while ((n = read(c, buf, sizeof buf)) > 0) { data.append(buf, (size_t)n); if (data.size() > (1u << 20)) break; }
            close(c);
            while (!data.empty() && (data.back() == '\n' || data.back() == '\r')) data.pop_back();
            Post(evType, Utf8ToWide(data));
        }
    }).detach();
}
inline void CleanupIpc() {
    unlink(SockPath().c_str());
    if (LockFd() >= 0) { close(LockFd()); LockFd() = -1; }
}

// ------------------------------------------------------------------ HTTP ---
// libcurl carregada em tempo de execucao (libcurl.so.4 ou a variante gnutls
// do Debian/Ubuntu). Constantes da ABI estavel do curl.
struct CurlApi {
    void* lib = nullptr;
    void* (*easy_init)() = nullptr;
    int (*setopt)(void*, int, ...) = nullptr;
    int (*perform)(void*) = nullptr;
    int (*getinfo)(void*, int, ...) = nullptr;
    void (*cleanup)(void*) = nullptr;
    void* (*slist_append)(void*, const char*) = nullptr;
    void (*slist_free_all)(void*) = nullptr;
    int (*global_init)(long) = nullptr;
    bool ok = false;
};
inline CurlApi& Curl() {
    static CurlApi api;
    static std::once_flag once;
    std::call_once(once, []() {
        const char* names[] = { "libcurl.so.4", "libcurl-gnutls.so.4", "libcurl-nss.so.4", "libcurl.so" };
        for (auto n : names) { api.lib = dlopen(n, RTLD_NOW | RTLD_LOCAL); if (api.lib) break; }
        if (!api.lib) return;
        auto sym = [&](const char* s) { return dlsym(api.lib, s); };
        api.easy_init = (void* (*)())sym("curl_easy_init");
        api.setopt = (int (*)(void*, int, ...))sym("curl_easy_setopt");
        api.perform = (int (*)(void*))sym("curl_easy_perform");
        api.getinfo = (int (*)(void*, int, ...))sym("curl_easy_getinfo");
        api.cleanup = (void (*)(void*))sym("curl_easy_cleanup");
        api.slist_append = (void* (*)(void*, const char*))sym("curl_slist_append");
        api.slist_free_all = (void (*)(void*))sym("curl_slist_free_all");
        api.global_init = (int (*)(long))sym("curl_global_init");
        api.ok = api.easy_init && api.setopt && api.perform && api.getinfo && api.cleanup && api.slist_append && api.slist_free_all;
        if (api.ok && api.global_init) api.global_init(3L /* CURL_GLOBAL_DEFAULT */);
    });
    return api;
}
inline size_t CurlWrite(char* ptr, size_t sz, size_t nm, void* ud) {
    auto* s = (std::string*)ud;
    size_t n = sz * nm;
    if (s->size() + n > 40u * 1024u * 1024u) return 0; // limite 40 MB
    s->append(ptr, n);
    return n;
}
inline bool HttpAvailable() { return Curl().ok; }
inline std::string HttpGet(const std::string& url, std::string* contentType = nullptr, long* status = nullptr, bool follow = true) {
    auto& c = Curl();
    std::string out;
    if (!c.ok) return out;
    void* h = c.easy_init();
    if (!h) return out;
    enum { OPT_WRITEDATA = 10001, OPT_URL = 10002, OPT_USERAGENT = 10018, OPT_HTTPHEADER = 10023, OPT_ACCEPT_ENCODING = 10102,
           OPT_WRITEFUNCTION = 20011, OPT_TIMEOUT = 13, OPT_FOLLOWLOCATION = 52, OPT_MAXREDIRS = 68, OPT_CONNECTTIMEOUT = 78, OPT_NOSIGNAL = 99,
           INFO_RESPONSE_CODE = 0x200002, INFO_CONTENT_TYPE = 0x100012 };
    c.setopt(h, OPT_URL, url.c_str());
    c.setopt(h, OPT_WRITEFUNCTION, &CurlWrite);
    c.setopt(h, OPT_WRITEDATA, &out);
    c.setopt(h, OPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) RemixPlayer/1.2");
    c.setopt(h, OPT_FOLLOWLOCATION, follow ? 1L : 0L);   // follow=false: o Host busca capas a pedido do celular e nao segue para outro lugar
    c.setopt(h, OPT_MAXREDIRS, 8L);
    c.setopt(h, OPT_TIMEOUT, 25L);
    c.setopt(h, OPT_CONNECTTIMEOUT, 15L);
    c.setopt(h, OPT_NOSIGNAL, 1L);
    c.setopt(h, OPT_ACCEPT_ENCODING, "");
    void* hdrs = c.slist_append(nullptr, "Accept-Language: pt-BR,pt;q=0.9,en;q=0.8");
    c.setopt(h, OPT_HTTPHEADER, hdrs);
    int rc = c.perform(h);
    if (status) { long st = 0; c.getinfo(h, INFO_RESPONSE_CODE, &st); *status = st; }
    if (contentType) { char* ct = nullptr; c.getinfo(h, INFO_CONTENT_TYPE, &ct); if (ct) *contentType = ct; else contentType->clear(); }
    c.slist_free_all(hdrs);
    c.cleanup(h);
    if (rc != 0) out.clear();
    return out;
}

// ------------------------------------------------------------------ temp ---
inline std::wstring TempDir() {
    const char* t = std::getenv("TMPDIR");
    std::string d = (t && *t) ? t : "/tmp";
    while (d.size() > 1 && d.back() == '/') d.pop_back();
    return Utf8ToWide(d);
}
inline bool WriteFileBytes(const std::wstring& path, const std::string& data) {
    FILE* f = fopen(WideToUtf8(path).c_str(), "wb");
    if (!f) return false;
    size_t w = data.empty() ? 0 : fwrite(data.data(), 1, data.size(), f);
    fclose(f);
    return w == data.size();
}

inline void InitProcess() {
    signal(SIGPIPE, SIG_IGN); // sockets/curl nao derrubam o app
}

// ------------------------------------------- conversao de formato (ffmpeg) ---
// Formatos que o miniaudio nao decodifica (m4a/aac/opus/wma/...) viram um WAV
// PCM 16-bit na pasta de cache, na taxa de amostragem original. O arquivo e
// reaproveitado enquanto for mais novo que a origem.
inline bool HaveFfmpeg() { static int v = -1; if (v < 0) v = HaveCmd("ffmpeg") ? 1 : 0; return v == 1; }
inline std::wstring TranscodeToWav(const std::wstring& src) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path cache(Config::CacheDir());
    fs::create_directories(cache, ec);
    size_t h = std::hash<std::wstring>{}(src);
    char name[64]; snprintf(name, sizeof name, "%016llx.wav", (unsigned long long)h);
    fs::path dst = cache / name;
    if (fs::exists(dst, ec)) {
        auto ts = fs::last_write_time(fs::path(src), ec);
        std::error_code e2;
        auto td = fs::last_write_time(dst, e2);
        if (!ec && !e2 && td >= ts && fs::file_size(dst, e2) > 1000) return dst.wstring();
    }
    fs::path tmp = cache / (std::string(name) + ".part");
    std::string cmd = "ffmpeg -nostdin -loglevel error -y -i " + ShQuote(WideToUtf8(src)) +
                      " -vn -map_metadata -1 -acodec pcm_s16le -f wav " + ShQuote(tmp.string()) + " </dev/null >/dev/null 2>&1";
    int rc = system(cmd.c_str());
    if (rc != 0 || !fs::exists(tmp, ec)) { fs::remove(tmp, ec); return L""; }
    fs::rename(tmp, dst, ec);
    if (ec) return L"";
    return dst.wstring();
}
// Apaga conversoes antigas (e restos .part) para o cache nao crescer sem limite.
inline void CleanupCache(int maxAgeHours) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path cache(Config::CacheDir());
    if (!fs::exists(cache, ec)) return;
    auto now = fs::file_time_type::clock::now();
    for (fs::directory_iterator it(cache, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
        if (ec) break;
        std::error_code e2;
        if (!it->is_regular_file(e2)) continue;
        auto ext = it->path().extension().string();
        if (ext != ".wav" && ext != ".part") continue;
        auto t = fs::last_write_time(it->path(), e2);
        if (e2) continue;
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - t).count();
        if (ext == ".part" || age >= maxAgeHours) fs::remove(it->path(), e2);
    }
}

} // namespace sys
