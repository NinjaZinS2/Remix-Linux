#pragma once
// Processos externos sem shell (yt-dlp, spotdl, ffmpeg): argumentos em vetor, sem
// problema de aspas ou acentos. Linux: posix_spawn num grupo proprio (Kill mata
// tambem os filhos, ex.: o ffmpeg que o yt-dlp chama). Windows: CreateProcessW sem
// janela, dentro de um Job Object (a arvore inteira morre junto) e herdando so os
// handles dos pipes.
#include "platform.h"
#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <chrono>
#include <cstring>
#ifndef _WIN32
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
extern char** environ;
#endif

class Proc {
public:
    Proc() = default;
    Proc(const Proc&) = delete;
    Proc& operator=(const Proc&) = delete;
    ~Proc() { Kill(); Wait(); CloseAll(); }
    bool Start(const std::vector<std::wstring>& args, bool pipeOut, bool pipeErr, bool pipeIn = false);
    long ReadOut(char* b, size_t n) { return ReadH(outR, b, n); }
    long ReadErr(char* b, size_t n) { return ReadH(errR, b, n); }
    bool WriteIn(const char* b, size_t n);
    void CloseIn();
    void Kill();
    int Wait();
    bool Started() const { return started; }
private:
    bool started = false, waited = false;
    int exitCode = -1;
#ifdef _WIN32
    HANDLE hProc = nullptr, hJob = nullptr, outR = nullptr, errR = nullptr, inW = nullptr;
    static long ReadH(HANDLE h, char* b, size_t n) { if (!h) return 0; DWORD rd = 0; if (!ReadFile(h, b, (DWORD)n, &rd, NULL)) return 0; return (long)rd; }
#else
    pid_t pid = -1;
    int outR = -1, errR = -1, inW = -1;
    static long ReadH(int fd, char* b, size_t n) { if (fd < 0) return 0; for (;;) { ssize_t r = read(fd, b, n); if (r < 0 && errno == EINTR) continue; return r < 0 ? 0 : (long)r; } }
#endif
    void CloseAll();
};

#ifdef _WIN32
#ifndef PROC_THREAD_ATTRIBUTE_JOB_LIST
#define PROC_THREAD_ATTRIBUTE_JOB_LIST 0x0002000D
#endif
inline std::wstring QuoteArgW(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring r = L"\"";
    for (size_t i = 0;; ++i) {
        size_t bs = 0;
        while (i < a.size() && a[i] == L'\\') { ++i; ++bs; }
        if (i == a.size()) { r.append(bs * 2, L'\\'); break; }
        if (a[i] == L'"') { r.append(bs * 2 + 1, L'\\'); r.push_back(L'"'); }
        else { r.append(bs, L'\\'); r.push_back(a[i]); }
    }
    r.push_back(L'"');
    return r;
}
inline bool Proc::Start(const std::vector<std::wstring>& args, bool pipeOut, bool pipeErr, bool pipeIn) {
    if (started || args.empty()) return false;
    SECURITY_ATTRIBUTES sa{ sizeof(sa), NULL, TRUE };
    HANDLE oR = NULL, oW = NULL, eR = NULL, eW = NULL, iR = NULL, iW = NULL, nulIn = NULL, nulOut = NULL;
    auto cl = [](HANDLE& h) { if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h); h = NULL; };
    bool ok = true;
    if (pipeOut && !CreatePipe(&oR, &oW, &sa, 1 << 16)) ok = false;
    if (ok && pipeErr && !CreatePipe(&eR, &eW, &sa, 1 << 16)) ok = false;
    if (ok && pipeIn && !CreatePipe(&iR, &iW, &sa, 1 << 16)) ok = false;
    if (!ok) { cl(oR); cl(oW); cl(eR); cl(eW); cl(iR); cl(iW); return false; }
    if (oR) SetHandleInformation(oR, HANDLE_FLAG_INHERIT, 0);
    if (eR) SetHandleInformation(eR, HANDLE_FLAG_INHERIT, 0);
    if (iW) SetHandleInformation(iW, HANDLE_FLAG_INHERIT, 0);
    if (!pipeIn) nulIn = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    if (!pipeOut || !pipeErr) nulOut = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, NULL);
    STARTUPINFOEXW si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = pipeIn ? iR : nulIn;
    si.StartupInfo.hStdOutput = pipeOut ? oW : nulOut;
    si.StartupInfo.hStdError = pipeErr ? eW : nulOut;
    HANDLE inherit[3]; DWORD ni = 0;
    auto addH = [&](HANDLE h) { if (!h || h == INVALID_HANDLE_VALUE) return; for (DWORD k = 0; k < ni; ++k) if (inherit[k] == h) return; inherit[ni++] = h; };
    addH(si.StartupInfo.hStdInput); addH(si.StartupInfo.hStdOutput); addH(si.StartupInfo.hStdError);
    std::wstring line;
    for (size_t i = 0; i < args.size(); ++i) { if (i) line.push_back(L' '); line += QuoteArgW(args[i]); }
    // Job "mata junto": se o Remix fechar (ou cair), o yt-dlp/ffmpeg nao fica rodando sozinho.
    hJob = CreateJobObjectW(NULL, NULL);
    if (hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION li{};
        li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &li, sizeof(li))) { CloseHandle(hJob); hJob = nullptr; }
    }
    // Windows 10 1607+: o processo ja nasce dentro do job (PROC_THREAD_ATTRIBUTE_JOB_LIST), sem
    // ser criado suspenso e retomado depois - padrao que antivirus associam a injecao de codigo.
    auto create = [&](bool withJob, PROCESS_INFORMATION& pi) -> BOOL {
        DWORD count = withJob ? 2 : 1;
        SIZE_T asz = 0;
        InitializeProcThreadAttributeList(NULL, count, 0, &asz);
        std::vector<BYTE> attr(asz ? asz : 1);
        STARTUPINFOEXW sx = si;
        sx.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attr.data();
        bool listOk = asz && InitializeProcThreadAttributeList(sx.lpAttributeList, count, 0, &asz);
        bool handlesOk = listOk && ni && UpdateProcThreadAttribute(sx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_HANDLE_LIST, inherit, ni * sizeof(HANDLE), NULL, NULL);
        bool jobOk = listOk && withJob && UpdateProcThreadAttribute(sx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_JOB_LIST, &hJob, sizeof(HANDLE), NULL, NULL);
        if (withJob && !jobOk) { if (listOk) DeleteProcThreadAttributeList(sx.lpAttributeList); return FALSE; }
        bool ext = handlesOk || jobOk;
        std::vector<wchar_t> cmd(line.begin(), line.end()); cmd.push_back(0);   // CreateProcessW pode escrever na linha de comando
        BOOL r = CreateProcessW(NULL, cmd.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW | (ext ? EXTENDED_STARTUPINFO_PRESENT : 0), NULL, NULL,
                                ext ? (LPSTARTUPINFOW)&sx : &sx.StartupInfo, &pi);
        if (listOk) DeleteProcThreadAttributeList(sx.lpAttributeList);
        return r;
    };
    PROCESS_INFORMATION pinfo{};
    bool inJob = hJob && create(true, pinfo);
    BOOL created = inJob ? TRUE : create(false, pinfo);
    cl(oW); cl(eW); cl(iR); cl(nulIn); cl(nulOut);
    if (!created) { cl(oR); cl(eR); cl(iW); if (hJob) { CloseHandle(hJob); hJob = nullptr; } return false; }
    if (hJob && !inJob && !AssignProcessToJobObject(hJob, pinfo.hProcess)) { CloseHandle(hJob); hJob = nullptr; }   // Windows antigo: entra no job logo depois
    CloseHandle(pinfo.hThread);
    hProc = pinfo.hProcess; outR = oR; errR = eR; inW = iW;
    started = true;
    return true;
}
inline bool Proc::WriteIn(const char* b, size_t n) {
    if (!inW) return false;
    while (n) { DWORD w = 0; if (!WriteFile(inW, b, (DWORD)std::min<size_t>(n, 1 << 20), &w, NULL) || !w) return false; b += w; n -= w; }
    return true;
}
inline void Proc::CloseIn() { if (inW) { CloseHandle(inW); inW = nullptr; } }
inline void Proc::Kill() { if (!started || waited) return; if (hJob) TerminateJobObject(hJob, 1); else if (hProc) TerminateProcess(hProc, 1); }
inline int Proc::Wait() {
    if (!started || waited) return exitCode;
    WaitForSingleObject(hProc, INFINITE);
    DWORD c = 1; GetExitCodeProcess(hProc, &c);
    exitCode = (int)c; waited = true;
    return exitCode;
}
inline void Proc::CloseAll() {
    for (HANDLE* h : { &outR, &errR, &inW, &hProc, &hJob }) if (*h) { CloseHandle(*h); *h = nullptr; }
}
#else
inline bool Proc::Start(const std::vector<std::wstring>& args, bool pipeOut, bool pipeErr, bool pipeIn) {
    if (started || args.empty()) return false;
    static std::once_flag once;
    std::call_once(once, [] {
        signal(SIGPIPE, SIG_IGN);
        for (int fd = 0; fd < 3; ++fd) if (fcntl(fd, F_GETFD) == -1) { int n = open("/dev/null", O_RDWR); (void)n; }
    });
    std::vector<std::string> a8; a8.reserve(args.size());
    for (auto& a : args) a8.push_back(WideToUtf8(a));
    std::vector<char*> av;
    for (auto& s : a8) av.push_back((char*)s.c_str());
    av.push_back(nullptr);
    int po[2] = { -1, -1 }, pe[2] = { -1, -1 }, pi[2] = { -1, -1 };
    auto closep = [](int* p) { if (p[0] >= 0) close(p[0]); if (p[1] >= 0) close(p[1]); p[0] = p[1] = -1; };
    if ((pipeOut && pipe2(po, O_CLOEXEC) != 0) || (pipeErr && pipe2(pe, O_CLOEXEC) != 0) || (pipeIn && pipe2(pi, O_CLOEXEC) != 0)) {
        closep(po); closep(pe); closep(pi); return false;
    }
    posix_spawn_file_actions_t fa; posix_spawn_file_actions_init(&fa);
    if (pipeIn) posix_spawn_file_actions_adddup2(&fa, pi[0], 0); else posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", O_RDONLY, 0);
    if (pipeOut) posix_spawn_file_actions_adddup2(&fa, po[1], 1); else posix_spawn_file_actions_addopen(&fa, 1, "/dev/null", O_WRONLY, 0);
    if (pipeErr) posix_spawn_file_actions_adddup2(&fa, pe[1], 2); else posix_spawn_file_actions_addopen(&fa, 2, "/dev/null", O_WRONLY, 0);
    posix_spawnattr_t at; posix_spawnattr_init(&at);
    sigset_t empty; sigemptyset(&empty);
    sigset_t defs; sigemptyset(&defs); sigaddset(&defs, SIGPIPE);
    posix_spawnattr_setsigmask(&at, &empty);
    posix_spawnattr_setsigdefault(&at, &defs);
    posix_spawnattr_setpgroup(&at, 0);
    posix_spawnattr_setflags(&at, POSIX_SPAWN_SETPGROUP | POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);
    int rc = posix_spawnp(&pid, av[0], &fa, &at, av.data(), environ);
    posix_spawn_file_actions_destroy(&fa);
    posix_spawnattr_destroy(&at);
    if (po[1] >= 0) { close(po[1]); po[1] = -1; }
    if (pe[1] >= 0) { close(pe[1]); pe[1] = -1; }
    if (pi[0] >= 0) { close(pi[0]); pi[0] = -1; }
    if (rc != 0) { pid = -1; closep(po); closep(pe); closep(pi); return false; }
    outR = po[0]; errR = pe[0]; inW = pi[1];
    started = true;
    return true;
}
inline bool Proc::WriteIn(const char* b, size_t n) {
    if (inW < 0) return false;
    while (n) { ssize_t w = write(inW, b, n); if (w < 0 && errno == EINTR) continue; if (w <= 0) return false; b += w; n -= (size_t)w; }
    return true;
}
inline void Proc::CloseIn() { if (inW >= 0) { close(inW); inW = -1; } }
inline void Proc::Kill() { if (!started || waited || pid <= 0) return; kill(-pid, SIGKILL); kill(pid, SIGKILL); }
inline int Proc::Wait() {
    if (!started || waited) return exitCode;
    int st = 0;
    while (waitpid(pid, &st, 0) < 0 && errno == EINTR) {}
    waited = true;
    exitCode = WIFEXITED(st) ? WEXITSTATUS(st) : -1;
    return exitCode;
}
inline void Proc::CloseAll() { for (int* fd : { &outR, &errR, &inW }) if (*fd >= 0) { close(*fd); *fd = -1; } }
#endif

// Roda e captura stdout/stderr. timeoutMs <= 0 = sem limite. onLine recebe cada linha
// (stdout e stderr, usado para progresso). cancel = mata o processo quando virar true.
struct CapResult { int code = -1; std::string out, err; bool started = false, timedOut = false, canceled = false; };
// keepMax = quanto guardar de stdout/stderr (0 = nada: so onLine; o tunel roda horas).
inline CapResult RunCapture(const std::vector<std::wstring>& args, int timeoutMs, const std::atomic<bool>* cancel = nullptr,
                            std::function<void(const std::string&)> onLine = nullptr, size_t keepMax = (size_t)96 * 1024 * 1024) {
    CapResult r;
    Proc p;
    if (!p.Start(args, true, true, false)) return r;
    r.started = true;
    std::mutex lm;
    std::string lb[2];
    std::atomic<int> eofs{ 0 };
    auto pump = [&](int which) {
        char b[16384];
        for (;;) {
            long n = which == 0 ? p.ReadOut(b, sizeof b) : p.ReadErr(b, sizeof b);
            if (n <= 0) break;
            std::lock_guard<std::mutex> lk(lm);
            std::string& dst = which == 0 ? r.out : r.err;
            if (dst.size() < keepMax) dst.append(b, (size_t)n);
            if (onLine) {
                lb[which].append(b, (size_t)n);
                size_t pos;
                while ((pos = lb[which].find_first_of("\r\n")) != std::string::npos) {
                    std::string ln = lb[which].substr(0, pos);
                    lb[which].erase(0, pos + 1);
                    if (!ln.empty()) onLine(ln);
                }
                if (lb[which].size() > 65536) lb[which].clear();   // linha sem fim: nao cresce para sempre
            }
        }
        ++eofs;
    };
    std::thread t1(pump, 0), t2(pump, 1);
    auto t0 = std::chrono::steady_clock::now();
    for (;;) {
        if (eofs.load() >= 2) break;
        if (cancel && cancel->load()) { r.canceled = true; p.Kill(); break; }
        if (timeoutMs > 0 && std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count() > timeoutMs) { r.timedOut = true; p.Kill(); break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
    t1.join(); t2.join();
    r.code = p.Wait();
    return r;
}
