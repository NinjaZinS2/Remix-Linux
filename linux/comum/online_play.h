#pragma once
// Musica online, parte 2: streaming na memoria, fila de downloads e o diario
// (<cache>/online/estado.json) que permite limpar restos se o app fechar no meio.
// Incluido por app_core.h depois dos eventos EV_* (as threads avisam a UI por eles).
#include "online_resolve.h"
#include <deque>
#include <algorithm>
#include <map>
#include <ctime>
#include <cerrno>

inline std::wstring OnlineCacheDir() { return Config::Join(Config::CacheDir(), L"online"); }
inline unsigned long OProcId() { return (unsigned long)
#ifdef _WIN32
    GetCurrentProcessId();
#else
    getpid();
#endif
}
inline std::wstring OPack(std::initializer_list<std::wstring> v) { std::wstring s; bool first = true; for (auto& x : v) { if (!first) s.push_back(L'\x1f'); first = false; s += x; } return s; }
inline std::vector<std::wstring> OUnpack(const std::wstring& s) { std::vector<std::wstring> o; size_t st = 0; for (;;) { size_t e = s.find(L'\x1f', st); o.push_back(s.substr(st, e == std::wstring::npos ? std::wstring::npos : e - st)); if (e == std::wstring::npos) break; st = e + 1; } return o; }

// ---- streaming: canais na memoria (a musica atual + as proximas da fila) ----------------
// Cada canal tem o seu yt-dlp/ffmpeg e o seu buffer. Canal da fila guarda so o comeco
// (STREAM_PREFETCH_SEC) e espera a vez; ao virar a atual o som sai na hora e o ffmpeg
// continua de onde parou. A atual decodifica no maximo STREAM_AHEAD_SEC a frente.
static const int STREAM_PREFETCH_SEC = 75;
static const int STREAM_AHEAD_SEC = 6 * 60;
static const int STREAM_KEEP_SEC = 10 * 60;   // buffer total antes de descartar o que ja tocou
// Analise incremental do streaming: o canal decodifica e entrega o PCM aqui; o app_core
// converte em envelope (onda) e espectro (barras), igual ao que a musica local mostra.
struct StreamWave {
    std::mutex m;
    bool started = false; unsigned long gen = 0;
    uint64_t firstAbs = 0;            // frame absoluto do primeiro dado analisado
    uint32_t rate = 48000;
    int envHop = 2400, specHop = 2400;// *frames* (~50 ms por bucket/banda)
    size_t fed = 0;                   // frames mono ja analisados (para detectar salto/seek)
    float envAcc = 0; size_t envN = 0;
    std::vector<float> env;           // um valor por envHop, 0..1
    float ring[2048]; size_t rpos = 0; size_t specSince = 0;
    std::vector<float> spec;          // frames*48 bandas (a cada specHop)
};
struct StreamJob {
    int id = 0; OTrack t;
    std::shared_ptr<PcmStream> st;
    std::shared_ptr<StreamWave> wa;   // frente de analise (criada abaixo)
    std::atomic<bool> stop{ false }, prefetch{ false }, ready{ false };
    std::atomic<int> phase{ 0 };                  // 1 procurando, 2 conectando, 3 recebendo, 4 recebido inteiro, 5 erro, 6 pronta esperando a vez
    std::atomic<int> durSec{ 0 };
    std::atomic<uint64_t> endFrame{ 0 };          // ate onde ja decodificou (tela/diario sem travar o buffer)
    std::atomic<ULONGLONG> failAt{ 0 };
    std::mutex pm; Proc* p1 = nullptr; Proc* p2 = nullptr;   // processos vivos (Kill ao parar)
    std::mutex im; std::wstring via, err;
};
struct StreamPool { std::mutex m; std::vector<std::shared_ptr<StreamJob>> jobs; int nextId = 1; };
inline StreamPool& SPool() { static StreamPool* p = new StreamPool(); return *p; }
inline std::shared_ptr<StreamJob> FindStreamId(int id) {
    if (id <= 0) return nullptr;
    std::lock_guard<std::mutex> lk(SPool().m); for (auto& j : SPool().jobs) if (j->id == id) return j; return nullptr;
}
inline std::shared_ptr<StreamJob> FindStreamUrl(const std::wstring& url) {
    std::lock_guard<std::mutex> lk(SPool().m); for (auto& j : SPool().jobs) if (j->t.url == url) return j; return nullptr;
}
inline size_t StreamCount() { std::lock_guard<std::mutex> lk(SPool().m); return SPool().jobs.size(); }
inline void StopStreamJob(const std::shared_ptr<StreamJob>& j) {
    if (!j) return;
    j->stop = true;
    std::lock_guard<std::mutex> lk(j->pm);
    if (j->p1) j->p1->Kill();
    if (j->p2) j->p2->Kill();
}
// ATENCAO: o Player guarda o buffer (shared_ptr), mas o canal parado para de crescer: nao pare o canal tocando.
inline void DropStream(const std::shared_ptr<StreamJob>& j) {
    if (!j) return;
    StopStreamJob(j);
    std::lock_guard<std::mutex> lk(SPool().m); auto& v = SPool().jobs; v.erase(std::remove(v.begin(), v.end(), j), v.end());
}
inline void PruneStreams(const std::vector<std::wstring>& keepUrls, int keepId) {
    std::vector<std::shared_ptr<StreamJob>> drop;
    {
        std::lock_guard<std::mutex> lk(SPool().m); auto& v = SPool().jobs;
        for (auto it = v.begin(); it != v.end();) {
            bool keep = (*it)->id == keepId || std::find(keepUrls.begin(), keepUrls.end(), (*it)->t.url) != keepUrls.end();
            if (keep) ++it; else { drop.push_back(*it); it = v.erase(it); }
        }
    }
    for (auto& j : drop) StopStreamJob(j);
}
inline void StopAllStreams() {
    std::vector<std::shared_ptr<StreamJob>> all;
    { std::lock_guard<std::mutex> lk(SPool().m); all.swap(SPool().jobs); }
    for (auto& j : all) StopStreamJob(j);
}
struct StreamInfo { int id = 0; std::wstring url, title; int phase = 0; bool prefetch = false; uint64_t end = 0, len = 0; uint32_t rate = 48000; };
inline std::vector<StreamInfo> StreamSnapshot() {
    std::vector<StreamInfo> out;
    std::lock_guard<std::mutex> lk(SPool().m);
    for (auto& j : SPool().jobs) {
        StreamInfo i; i.id = j->id; i.url = j->t.url; i.title = j->t.title; i.phase = j->phase.load(); i.prefetch = j->prefetch.load();
        i.end = j->endFrame.load(); i.len = j->st->lenFrames.load(); i.rate = j->st->rate;
        out.push_back(i);
    }
    return out;
}
inline void StreamThread(std::shared_ptr<StreamJob> j) {
    EnsureTools();
    auto fail = [&](const std::wstring& e) {
        { std::lock_guard<std::mutex> lk(j->im); j->err = e; }
        j->failAt = GetTickCount64(); j->phase = 5;
        if (!j->stop) AppPost(EV_ONLINE_FAIL, e, j->id);
    };
    if (!YtdlpOk() || !FfmpegOk()) { fail(L"Para tocar online instale o yt-dlp e o ffmpeg (veja Configurações > ONLINE)."); return; }
    std::wstring err;
    j->phase = 1;
    OTrack t = j->t;
    if (t.play.empty() || NeedsMatch(DetectSource(t.play))) { if (!MatchOnYouTube(t, err, &j->stop)) { if (!j->stop) fail(err); return; } }
    if (j->stop) return;
    MediaInfo mi;
    if (!GetMediaInfo(t.play, mi, err, &j->stop)) { if (!j->stop) fail(err); return; }
    if (j->stop) return;
    OTrack m = t;
    if (!NeedsMatch(t.src)) { if (!mi.title.empty()) m.title = mi.title; if (!mi.artist.empty()) m.artist = mi.artist; if (!mi.album.empty()) m.album = mi.album; if (!mi.thumb.empty()) m.thumb = mi.thumb; }
    else if (m.thumb.empty()) m.thumb = mi.thumb;
    if (mi.dur > 0) m.dur = mi.dur;
    AppPost(EV_ONLINE_META, OPack({ t.url, m.title, m.artist, m.album, m.thumb, std::to_wstring(m.dur), t.play }), j->id);
    auto st = j->st;
    const uint32_t rate = st->rate;
    if (m.dur > 0) st->lenFrames = (uint64_t)m.dur * rate;
    bool pipeMode = mi.url.empty();
    { std::lock_guard<std::mutex> lk(st->m); st->canRestart = !pipeMode; }
    { std::lock_guard<std::mutex> lk(j->im); j->via = pipeMode ? L"yt-dlp | ffmpeg (pipe) -> PCM na memória" : L"yt-dlp (URL do áudio) -> ffmpeg -> PCM na memória"; }
    j->durSec = std::max(0, m.dur);
    j->ready = true; j->phase = 2;
    AppPost(EV_ONLINE_READY, std::to_wstring(m.dur), j->id);
    if (!m.thumb.empty()) {   // capa em paralelo: nao atrasa o som
        std::wstring url = t.url, th = m.thumb;
        std::thread([url, th] { std::wstring tf = FetchThumb(th); if (!tf.empty()) AppPost(EV_ONLINE_THUMB, OPack({ url, tf })); }).detach();
    }
    std::wstring ff = FfmpegTool();
    const uint64_t PREF = (uint64_t)STREAM_PREFETCH_SEC * rate, AHEAD = (uint64_t)STREAM_AHEAD_SEC * rate, KEEP = (uint64_t)STREAM_KEEP_SEC * rate;
    uint64_t startFrame = 0; bool gotAny = false; int resumes = 0;
    for (int attempt = 0; !j->stop; ++attempt) {
        std::vector<std::wstring> fa = { ff, L"-nostdin", L"-hide_banner", L"-loglevel", L"error" };
        if (!pipeMode) {
            if (mi.url.rfind("http", 0) == 0) for (const wchar_t* x : { L"-reconnect", L"1", L"-reconnect_streamed", L"1", L"-reconnect_delay_max", L"5" }) fa.push_back(x);
            if (startFrame > 0) { fa.push_back(L"-ss"); fa.push_back(std::to_wstring(startFrame * 1000 / rate) + L"ms"); }
            if (!mi.headers.empty()) { std::string hs; for (auto& x : mi.headers) hs += x + "\r\n"; fa.push_back(L"-headers"); fa.push_back(Utf8ToWide(hs)); }
            fa.push_back(L"-i"); fa.push_back(Utf8ToWide(mi.url));
        } else { fa.push_back(L"-i"); fa.push_back(L"pipe:0"); }
        for (const wchar_t* x : { L"-vn", L"-f", L"s16le", L"-ac", L"2", L"-ar", L"48000", L"pipe:1" }) fa.push_back(x);
        Proc dec, src;
        if (!dec.Start(fa, true, true, pipeMode)) { fail(L"Não consegui abrir o ffmpeg."); return; }
        std::thread pump;
        if (pipeMode) {
            auto ya = YtdlpArgs();
            for (const wchar_t* x : { L"--no-playlist", L"-q", L"-f", L"bestaudio/best", L"-o", L"-" }) ya.push_back(x);
            ya.push_back(L"--"); ya.push_back(t.play);
            if (!src.Start(ya, true, false, false)) { dec.Kill(); dec.Wait(); fail(L"Não consegui abrir o yt-dlp."); return; }
            pump = std::thread([&] { char b[65536]; for (;;) { long n = src.ReadOut(b, sizeof b); if (n <= 0) break; if (!dec.WriteIn(b, (size_t)n)) break; } dec.CloseIn(); });
        }
        { std::lock_guard<std::mutex> lk(j->pm); j->p1 = &dec; j->p2 = pipeMode ? &src : nullptr; }
        if (j->stop) { dec.Kill(); if (pipeMode) src.Kill(); }
        std::string errTail;
        std::thread errT([&] { char b[2048]; for (;;) { long n = dec.ReadErr(b, sizeof b); if (n <= 0) break; errTail.append(b, (size_t)n); if (errTail.size() > 4096) errTail.erase(0, errTail.size() - 4096); } });
        char buf[65536]; std::string carry; uint64_t restartAt = UINT64_MAX; bool gotData = false;
        for (;;) {
            for (;;) {   // espera a vez (canal da fila) ou o consumo (musica atual); o ffmpeg fica parado no pipe
                if (j->stop.load() || st->seekReq.load() != UINT64_MAX) break;
                uint64_t ahead;
                { std::lock_guard<std::mutex> lk(st->m); uint64_t from = std::max<uint64_t>(st->baseFrame, st->readCursor.load()), e = st->EndFrame(); ahead = e > from ? e - from : 0; }
                bool pre = j->prefetch.load();
                if (ahead < (pre ? PREF : AHEAD)) break;
                if (pre && j->phase.load() == 3) j->phase = 6;
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
            }
            if (j->phase.load() == 6 && !j->prefetch.load()) j->phase = 3;
            long n = dec.ReadOut(buf, sizeof buf);
            if (n <= 0) break;
            carry.append(buf, (size_t)n);
            size_t whole = (carry.size() / 4) * 4;
            if (whole) {
                gotData = true;
                std::lock_guard<std::mutex> lk(st->m);
                size_t old = st->pcm.size();
                st->pcm.resize(old + whole / 2);
                memcpy(&st->pcm[old], carry.data(), whole);
                carry.erase(0, whole);
                StreamWavePump(j.get(), &st->pcm[old], whole / 2, st->baseFrame + old / 2);   // alimenta a analise de onda/espectro (app_core.h)
                if (st->pcm.size() / 2 > KEEP + (uint64_t)60 * rate) {   // descarta de uma vez o que ja tocou ha mais de 1 min
                    uint64_t cur = st->readCursor.load(), keep = cur > (uint64_t)60 * rate ? cur - (uint64_t)60 * rate : 0;
                    if (keep > st->baseFrame) { uint64_t drop = std::min<uint64_t>(keep - st->baseFrame, st->pcm.size() / 2); st->pcm.erase(st->pcm.begin(), st->pcm.begin() + (size_t)drop * 2); st->baseFrame += drop; }
                }
                j->endFrame = st->EndFrame();
            }
            if (j->phase.load() == 2) j->phase = 3;
            if (!pipeMode && st->seekReq.load() != UINT64_MAX) { restartAt = st->seekReq.exchange(UINT64_MAX); dec.Kill(); break; }
            if (j->stop) { dec.Kill(); if (pipeMode) src.Kill(); break; }
        }
        { std::lock_guard<std::mutex> lk(j->pm); j->p1 = nullptr; j->p2 = nullptr; }
        if (pipeMode) src.Kill();
        if (pump.joinable()) pump.join();
        errT.join();
        int code = dec.Wait();
        if (pipeMode) src.Wait();
        if (gotData) gotAny = true;
        if (j->stop) return;
        if (restartAt != UINT64_MAX) { std::lock_guard<std::mutex> lk(st->m); st->pcm.clear(); st->baseFrame = restartAt; st->eof = false; j->endFrame = restartAt; startFrame = restartAt; continue; }
        uint64_t endNow, lenNow = st->lenFrames.load();
        { std::lock_guard<std::mutex> lk(st->m); endNow = st->EndFrame(); }
        if (gotData && !pipeMode && lenNow > 0 && endNow + (uint64_t)rate * 5 < lenNow && resumes < 5) { ++resumes; startFrame = endNow; continue; }   // a conexao caiu no meio: continua de onde parou
        if (code == 0 && gotAny) {
            { std::lock_guard<std::mutex> lk(st->m); st->eof = true; j->endFrame = st->EndFrame(); }
            j->phase = 4;
            while (!j->stop) {   // recebeu tudo: fica esperando um seek para fora do buffer (ou parar)
                if (!pipeMode && st->seekReq.load() != UINT64_MAX) { restartAt = st->seekReq.exchange(UINT64_MAX); break; }
                std::this_thread::sleep_for(std::chrono::milliseconds(60));
            }
            if (j->stop) return;
            { std::lock_guard<std::mutex> lk(st->m); st->pcm.clear(); st->baseFrame = restartAt; st->eof = false; j->endFrame = restartAt; }
            startFrame = restartAt; j->phase = 3;
            continue;
        }
        if (!gotAny && !pipeMode && attempt < 2) {   // a URL direta nao abriu: tenta pelo pipe do yt-dlp
            ForgetMediaInfo(t.play);
            pipeMode = true;
            { std::lock_guard<std::mutex> lk(st->m); st->canRestart = false; st->pcm.clear(); st->baseFrame = 0; j->endFrame = 0; }
            { std::lock_guard<std::mutex> lk(j->im); j->via = L"yt-dlp | ffmpeg (pipe) -> PCM na memória"; }
            startFrame = 0;
            continue;
        }
        if (gotAny) { { std::lock_guard<std::mutex> lk(st->m); st->eof = true; } j->phase = 4; return; }
        std::string e = errTail; while (!e.empty() && (e.back() == '\n' || e.back() == '\r')) e.pop_back();
        size_t nl = e.find_last_of("\r\n"); if (nl != std::string::npos) e = e.substr(nl + 1);
        if (e.size() > 160) e = e.substr(0, 160);
        fail(L"O streaming falhou" + (e.empty() ? std::wstring(L".") : L": " + Utf8ToWide(e)));
        return;
    }
}
inline std::shared_ptr<StreamJob> StartStreamJob(const OTrack& t, bool prefetch) {
    auto j = std::make_shared<StreamJob>();
    j->t = t; j->st = std::make_shared<PcmStream>(); j->prefetch = prefetch; j->wa = std::make_shared<StreamWave>();
    { std::lock_guard<std::mutex> lk(SPool().m); j->id = SPool().nextId++; SPool().jobs.push_back(j); }
    std::thread([j] {
        bool crashed = true;
        RemixSafe("streaming", [&] { StreamThread(j); crashed = false; });
        if (!crashed || j->stop.load()) return;
        if (j->ready.load()) { std::lock_guard<std::mutex> lk(j->st->m); j->st->eof = true; j->phase = 4; }   // toca o que ja chegou
        else { j->failAt = GetTickCount64(); j->phase = 5; AppPost(EV_ONLINE_FAIL, L"O streaming falhou (erro interno; veja o log).", j->id); }
    }).detach();
    return j;
}

// ---- downloads (um por vez) ---------------------------------------------------------
struct DlJob {
    int id = 0; OTrack t; std::wstring plName, dest, fmt;
    std::atomic<int> status{ 0 };   // 0 fila, 1 baixando, 2 pronto, 3 falhou, 4 cancelado
    std::atomic<float> pct{ 0 };
    std::atomic<bool> cancel{ false };
    std::mutex m; std::wstring tmp, finalPath, err;
};
struct DlQueue { std::mutex m; std::deque<std::shared_ptr<DlJob>> q; std::vector<std::shared_ptr<DlJob>> all; int workers = 0; int nextId = 1; };
// Varias musicas ao mesmo tempo: o que pesa em cada uma e a extracao (rede) e a conversao (1 nucleo
// de CPU), entao metade dos nucleos, entre 2 e 6. Uma playlist de 100 musicas cai de ~13 para ~2 min.
inline int DlWorkerMax() { unsigned c = std::thread::hardware_concurrency(); int n = c ? (int)c / 2 : 2; return n < 2 ? 2 : (n > 6 ? 6 : n); }
inline DlQueue& DQ() { static DlQueue* q = new DlQueue(); return *q; }
inline std::wstring SafeFileName(std::wstring s) {
    for (auto& c : s) if (c < 32 || wcschr(L"\\/:*?\"<>|", c)) c = L'-';
    s = Config::Trim(s);
    if (s.size() > 140) s.resize(140);
    while (!s.empty() && (s.back() == L'.' || s.back() == L' ')) s.pop_back();
    return s.empty() ? std::wstring(L"musica") : s;
}
inline bool LooseAudioExt(const std::wstring& e) { return e == L".mp3" || e == L".m4a" || e == L".opus" || e == L".ogg" || e == L".webm" || e == L".flac" || e == L".wav" || e == L".aac" || e == L".mka"; }
// Duracao de um arquivo baixado (qualquer formato: quem le e o ffmpeg). 0 = nao sei.
inline int MediaDurSec(const std::wstring& file) {
    if (!FfmpegOk()) return 0;
    CapResult r = RunCapture({ FfmpegTool(), L"-nostdin", L"-hide_banner", L"-i", file }, 60000);
    const std::string& t = r.err.find("Duration:") != std::string::npos ? r.err : r.out;
    size_t p = t.find("Duration:");
    if (p == std::string::npos) return 0;
    int h = 0, m = 0; double sec = 0;
    if (sscanf(t.c_str() + p + 9, " %d:%d:%lf", &h, &m, &sec) != 3) return 0;
    return h * 3600 + m * 60 + (int)sec;
}
inline void RunDownload(DlJob& j) {
    EnsureTools();
    std::error_code ec;
    auto finish = [&](int status, const std::wstring& e) { { std::lock_guard<std::mutex> lk(j.m); j.err = e; } if (!j.tmp.empty()) std::filesystem::remove_all(std::filesystem::path(j.tmp), ec); j.status = status; };
    if (!YtdlpOk() || !FfmpegOk()) { finish(3, L"Instale o yt-dlp e o ffmpeg para baixar (Configurações > ONLINE)."); return; }
    { std::lock_guard<std::mutex> lk(j.m); j.tmp = Config::Join(OnlineCacheDir(), L"job-" + std::to_wstring(OProcId()) + L"-" + std::to_wstring(j.id)); }
    std::filesystem::create_directories(std::filesystem::path(j.tmp), ec);
    OTrack t = j.t; std::wstring err;
    if (t.play.empty() || NeedsMatch(DetectSource(t.play))) { if (!MatchOnYouTube(t, err, &j.cancel)) { if (j.cancel) finish(4, L"Cancelado."); else finish(3, err); return; } }
    // Extracao: a mesma do streaming (se a musica ja tocou ou apareceu na busca, sai do cache na hora).
    // O yt-dlp baixa a partir desse JSON (--load-info-json) sem extrair de novo; se falhar, faz do jeito normal.
    std::wstring infoFile; MediaInfo mi;
    { std::wstring e; if (GetMediaInfo(t.play, mi, e, &j.cancel) && !mi.raw.empty()) {
        infoFile = Config::Join(j.tmp, L"info.json");
        std::ofstream o(std::filesystem::path(infoFile), std::ios::binary | std::ios::trunc); o.write(mi.raw.data(), (std::streamsize)mi.raw.size());
        if (!o) infoFile.clear();
    } }
    if (j.cancel) { finish(4, L"Cancelado."); return; }
    auto runYt = [&](bool fromInfo) {
        auto a = YtdlpArgs();
        for (const wchar_t* x : { L"--no-playlist", L"--newline", L"--progress", L"--progress-template", L"download:REMIXPCT %(progress._percent_str)s", L"-f", L"bestaudio/best", L"-x", L"--embed-metadata" }) a.push_back(x);
        if (j.fmt == L"mp3") for (const wchar_t* x : { L"--audio-format", L"mp3", L"--audio-quality", L"0", L"--embed-thumbnail" }) a.push_back(x);
        else if (j.fmt == L"m4a") for (const wchar_t* x : { L"--audio-format", L"m4a", L"--embed-thumbnail" }) a.push_back(x);
        a.push_back(L"-o"); a.push_back(Config::Join(j.tmp, L"audio.%(ext)s"));
        if (fromInfo) { a.push_back(L"--load-info-json"); a.push_back(infoFile); }
        else { a.push_back(L"--"); a.push_back(t.play); }
        return RunCapture(a, 0, &j.cancel, [&](const std::string& ln) { size_t p = ln.find("REMIXPCT"); if (p != std::string::npos) { float v = (float)atof(ln.c_str() + p + 8); if (v >= 0 && v <= 100) j.pct = v * 0.9f; } });
    };
    auto findProduced = [&]() {
        std::wstring f;
        for (std::filesystem::directory_iterator it(std::filesystem::path(j.tmp), ec), end; !ec && it != end; it.increment(ec)) {
            std::error_code e2;
            if (it->is_regular_file(e2) && LooseAudioExt(LowerExt(it->path()))) f = it->path().wstring();
        }
        ec.clear(); return f;
    };
    // Quanto a musica deve ter. Se o arquivo baixado vier bem menor, o yt-dlp trouxe so um pedaco
    // (link do cache velho, formato em fragmentos que parou no primeiro): apaga e baixa do jeito normal.
    int want = t.dur > 0 ? t.dur : mi.dur;
    auto curto = [&](const std::wstring& f, int* gotOut) {
        int got = (want > 40) ? MediaDurSec(f) : 0;
        if (gotOut) *gotOut = got;
        return got > 0 && got + 20 < want && got < (int)(want * 0.85);
    };
    CapResult r;
    std::wstring produced;
    if (!infoFile.empty()) {
        r = runYt(true);
        if (j.cancel) { finish(4, L"Cancelado."); return; }
        produced = findProduced();
        if (produced.empty()) ForgetMediaInfo(t.play);
        else if (curto(produced, nullptr)) {   // veio pela metade: joga fora e refaz a extracao
            std::error_code e4; std::filesystem::remove(std::filesystem::path(produced), e4);
            produced.clear(); ForgetMediaInfo(t.play);
        }
    }
    if (produced.empty()) {
        r = runYt(false);
        if (j.cancel) { finish(4, L"Cancelado."); return; }
        produced = findProduced();
        int got = 0;
        if (!produced.empty() && curto(produced, &got)) {
            wchar_t b[96]; swprintf(b, 96, L"Veio incompleta (%d:%02d de %d:%02d). Tente de novo.", got / 60, got % 60, want / 60, want % 60);
            finish(3, b); return;
        }
    }
    if (produced.empty()) { finish(3, OErr(r, L"O download falhou.")); return; }
    j.pct = 93;
    std::wstring ext = std::filesystem::path(produced).extension().wstring();
    if (!t.title.empty()) {   // titulo/artista certos (links do Spotify/Deezer/Apple ou da busca)
        std::wstring tagged = Config::Join(j.tmp, L"tagged" + ext);
        std::vector<std::wstring> fa = { FfmpegTool(), L"-nostdin", L"-loglevel", L"error", L"-y", L"-i", produced, L"-map", L"0", L"-c", L"copy", L"-metadata", L"title=" + t.title };
        if (!t.artist.empty()) { fa.push_back(L"-metadata"); fa.push_back(L"artist=" + t.artist); }
        if (!t.album.empty()) { fa.push_back(L"-metadata"); fa.push_back(L"album=" + t.album); }
        if (OLower(ext) == L".mp3") { fa.push_back(L"-id3v2_version"); fa.push_back(L"3"); }
        fa.push_back(tagged);
        CapResult fr = RunCapture(fa, 180000, &j.cancel);
        std::error_code e3;
        if (fr.code == 0 && std::filesystem::exists(std::filesystem::path(tagged), e3) && std::filesystem::file_size(std::filesystem::path(tagged), e3) > 1000) { std::filesystem::remove(std::filesystem::path(produced), e3); produced = tagged; }
    }
    if (j.cancel) { finish(4, L"Cancelado."); return; }
    std::filesystem::create_directories(std::filesystem::path(j.dest), ec);
    std::wstring base = SafeFileName((t.artist.empty() ? L"" : t.artist + L" - ") + (t.title.empty() ? L"musica" : t.title));
    std::wstring fin = Config::Join(j.dest, base + ext);
    for (int k = 2; std::filesystem::exists(std::filesystem::path(fin), ec) && k < 1000; ++k) fin = Config::Join(j.dest, base + L" (" + std::to_wstring(k) + L")" + ext);
    ec.clear();
    std::filesystem::rename(std::filesystem::path(produced), std::filesystem::path(fin), ec);
    if (ec) { ec.clear(); std::filesystem::copy_file(std::filesystem::path(produced), std::filesystem::path(fin), std::filesystem::copy_options::overwrite_existing, ec); if (ec) { finish(3, L"Não consegui gravar em " + j.dest); return; } }
    { std::lock_guard<std::mutex> lk(j.m); j.finalPath = fin; }
    j.pct = 100;
    finish(2, L"");
}
inline void DownloadWorker(std::shared_ptr<DlJob>& cur) {
    for (;;) {
        std::shared_ptr<DlJob> j;
        { std::lock_guard<std::mutex> lk(DQ().m); if (DQ().q.empty()) { DQ().workers--; cur = nullptr; return; } j = DQ().q.front(); DQ().q.pop_front(); }
        cur = j;
        if (j->cancel) { j->status = 4; continue; }
        j->status = 1;
        AppPost(EV_ONLINE_JOB, L"", j->id);
        RunDownload(*j);
        AppPost(EV_ONLINE_JOB, L"", j->id);
    }
}
inline int QueueDownload(const OTrack& t, const std::wstring& plName, const std::wstring& destBase, const std::wstring& fmt) {
    std::lock_guard<std::mutex> lk(DQ().m);
    for (auto& j : DQ().all) if (j->t.url == t.url && (j->status == 0 || j->status == 1)) return j->id;
    auto j = std::make_shared<DlJob>();
    j->id = DQ().nextId++; j->t = t; j->plName = plName; j->fmt = fmt;
    j->dest = plName.empty() ? destBase : Config::Join(destBase, SafeFileName(plName));
    DQ().q.push_back(j); DQ().all.push_back(j);
    if (DQ().workers < DlWorkerMax()) {   // mais um trabalhador (cada um pega musicas da fila ate ela esvaziar)
        DQ().workers++;
        std::thread([] {
            bool crashed = true; std::shared_ptr<DlJob> cur;
            RemixSafe("fila de downloads", [&] { DownloadWorker(cur); crashed = false; });
            if (!crashed) return;
            std::lock_guard<std::mutex> lk(DQ().m);
            if (cur && cur->status == 1) cur->status = 3;
            DQ().workers--;
        }).detach();
    }
    return j->id;
}
inline std::shared_ptr<DlJob> FindJob(int id) { std::lock_guard<std::mutex> lk(DQ().m); for (auto& j : DQ().all) if (j->id == id) return j; return nullptr; }
inline int CancelAllDownloads() {
    std::lock_guard<std::mutex> lk(DQ().m); int n = 0;
    for (auto& j : DQ().all) if (j->status == 0 || j->status == 1) { j->cancel = true; if (j->status == 0) j->status = 4; ++n; }
    DQ().q.clear();
    return n;
}
inline bool DownloadActivity(int& waiting, float& pct, std::wstring& title) {
    std::lock_guard<std::mutex> lk(DQ().m); waiting = 0; int running = 0; float sum = 0;
    for (auto& j : DQ().all) { if (j->status == 0) ++waiting; else if (j->status == 1) { if (!running) title = j->t.title; ++running; sum += j->pct; } }
    if (running) { pct = sum / running; if (running > 1) title = std::to_wstring(running) + L" ao mesmo tempo · " + title; }
    return running > 0 || waiting > 0;
}

// ---- diario: o que esta tocando/baixando, gravado no maximo 1x por segundo ----------------
// um diario por pasta do app (duas copias do Remix nao se atrapalham)
inline std::wstring JournalPath() {
    uint64_t h = 1469598103934665603ULL; for (wchar_t c : Config::NormSep(Config::BaseDir())) { h ^= (uint64_t)c; h *= 1099511628211ULL; }
    wchar_t n[40]; swprintf(n, 40, L"estado-%08llx.json", (unsigned long long)(h & 0xffffffffULL));
    return Config::Join(OnlineCacheDir(), n);
}
// pasta "job-<pid>-<n>" de OUTRO Remix ainda aberto: nao pode ser apagada
inline bool OtherInstanceAlive(const std::wstring& jobName) {
    unsigned long pid = wcstoul(jobName.c_str() + 4, nullptr, 10);
    if (pid == 0 || pid == OProcId()) return false;
#ifdef _WIN32
    HANDLE hp = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
    if (!hp) return false;
    DWORD code = 0; bool alive = GetExitCodeProcess(hp, &code) && code == STILL_ACTIVE;
    CloseHandle(hp); return alive;
#else
    return kill((pid_t)pid, 0) == 0 || errno == EPERM;
#endif
}
inline bool OlderThanMin(const std::filesystem::path& p, int minutes) {
    std::error_code ec; auto t = std::filesystem::last_write_time(p, ec); if (ec) return true;
    return std::filesystem::file_time_type::clock::now() - t > std::chrono::minutes(minutes);
}
inline const char* StreamPhaseName(int p) {
    switch (p) { case 1: return "procurando o audio"; case 2: return "conectando"; case 3: return "recebendo"; case 4: return "recebido inteiro"; case 5: return "erro"; case 6: return "pronta, esperando a vez"; default: return "iniciando"; }
}
inline void WriteJournal(bool clean, bool memo, const std::wstring& title, const std::wstring& url, int posSec, int durSec, int curId) {
    time_t now = time(nullptr); char stamp[32]; strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));
    char mm[16]; snprintf(mm, sizeof mm, "%d:%02d", posSec / 60, posSec % 60);
    std::vector<std::shared_ptr<StreamJob>> jobs; { std::lock_guard<std::mutex> lk(SPool().m); jobs = SPool().jobs; }
    auto bufPct = [](const std::shared_ptr<StreamJob>& x) { if (x->phase.load() == 4) return 100; uint64_t len = x->st->lenFrames.load(); return len ? (int)std::min<uint64_t>(100, x->endFrame.load() * 100 / len) : 0; };
    auto viaOf = [](const std::shared_ptr<StreamJob>& x) { std::lock_guard<std::mutex> lk(x->im); return x->via; };
    std::string j = std::string("{\n  \"app\": \"remix\",\n  \"atualizado\": \"") + stamp + "\",\n  \"encerrou_normal\": " + (clean ? "true" : "false") + ",\n";
    if (memo) {
        std::wstring via = L"parado (o streaming recomeça ao tocar)"; int buf = 0;
        for (auto& x : jobs) if (x->id == curId && x->t.url == url) { std::wstring v = viaOf(x); if (!v.empty()) via = v; buf = bufPct(x); }
        j += "  \"streaming\": {\"titulo\": " + JEscW(title) + ", \"url\": " + JEscW(url) + ", \"como\": " + JEscW(via) + ", \"onde\": \"memoria (nenhum arquivo no disco)\", \"minuto\": \"" + mm + "\", \"posicao_s\": " + std::to_string(posSec) + ", \"duracao_s\": " + std::to_string(durSec) + ", \"buffer_pct\": " + std::to_string(buf) + "},\n";
    }
    j += "  \"canais\": [";
    int n = 0;
    for (auto& x : jobs) {
        uint64_t rate = std::max<uint32_t>(1, x->st->rate);
        j += std::string(n ? ",\n" : "\n") + "    {\"canal\": " + std::to_string(n + 1) + ", \"papel\": \"" + (x->id == curId ? "tocando" : "fila") + "\", \"titulo\": " + JEscW(x->t.title) + ", \"url\": " + JEscW(x->t.url)
           + ", \"estado\": \"" + StreamPhaseName(x->phase.load()) + "\", \"como\": " + JEscW(viaOf(x)) + ", \"onde\": \"memoria\", \"recebido_ate_s\": " + std::to_string(x->endFrame.load() / rate) + ", \"buffer_pct\": " + std::to_string(bufPct(x)) + "}";
        ++n;
    }
    j += std::string(n ? "\n  " : "") + "],\n  \"downloads\": [";
    {
        std::lock_guard<std::mutex> lk(DQ().m); int k = 0, total = (int)DQ().all.size(), idx = 0;
        static const char* names[] = { "fila", "baixando", "pronto", "falhou", "cancelado" };
        for (auto& d : DQ().all) {
            int s = d->status.load(); ++idx;
            if (s >= 2 && idx <= total - 5) continue;   // terminados: so os 5 mais recentes
            std::wstring tmp; { std::lock_guard<std::mutex> lj(d->m); tmp = d->tmp; }
            char pct[16]; snprintf(pct, sizeof pct, "%.1f", (double)d->pct.load());
            j += std::string(k ? ",\n" : "\n") + "    {\"id\": " + std::to_string(d->id) + ", \"titulo\": " + JEscW(d->t.title) + ", \"url\": " + JEscW(d->t.url) + ", \"status\": \"" + names[s < 0 || s > 4 ? 0 : s] + "\", \"pct\": " + pct + ", \"pasta_temp\": " + JEscW(tmp) + ", \"destino\": " + JEscW(d->dest) + "}";
            ++k;
        }
        j += k ? "\n  " : "";
    }
    j += "]\n}\n";
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(OnlineCacheDir()), ec);
    WriteFileUtf8(JournalPath(), j);
}
inline size_t CountFilesIn(const std::wstring& dir) { size_t n = 0; std::error_code ec; for (std::filesystem::recursive_directory_iterator it(std::filesystem::path(dir), std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) { std::error_code e2; if (it->is_regular_file(e2)) ++n; } return n; }
// Na abertura: le o diario, apaga pastas temporarias de downloads que nao terminaram e
// devolve (lastUrl) a ultima musica em streaming (o streaming recomeca do inicio).
inline std::wstring OnlineStartupCleanup(std::wstring& lastUrl, std::wstring& lastTitle) {
    std::error_code ec; size_t removed = 0;
    std::wstring dir = OnlineCacheDir();
    std::string data;
    if (ReadFileUtf8(JournalPath(), data)) {
        JVal root;
        if (OParse(data, root) && root.t == JVal::OBJ) {
            if (const JVal* d = root.get("downloads")) if (d->t == JVal::ARR) for (auto& x : d->a) {
                if (x.t != JVal::OBJ || JS(x, "status") == "pronto") continue;
                std::wstring tmp = Utf8ToWide(JS(x, "pasta_temp"));
                if (!tmp.empty() && Config::StartsI(Config::NormSep(tmp), Config::NormSep(dir)) && !OtherInstanceAlive(std::filesystem::path(tmp).filename().wstring()) && std::filesystem::exists(std::filesystem::path(tmp), ec)) { removed += CountFilesIn(tmp); std::filesystem::remove_all(std::filesystem::path(tmp), ec); }
            }
            if (const JVal* s = root.get("streaming")) if (s->t == JVal::OBJ) { lastUrl = Utf8ToWide(JS(*s, "url")); lastTitle = Utf8ToWide(JS(*s, "titulo")); }
        }
    }
    for (std::filesystem::directory_iterator it(std::filesystem::path(dir), std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        std::wstring n = it->path().filename().wstring(); std::error_code e2;
        if (n.rfind(L"job-", 0) == 0 && it->is_directory(e2) && !OtherInstanceAlive(n)) { removed += CountFilesIn(it->path().wstring()); std::filesystem::remove_all(it->path(), e2); }
        else if ((n.rfind(L"resolve-", 0) == 0 || (n.size() > 4 && n.compare(n.size() - 4, 4, L".tmp") == 0)) && OlderThanMin(it->path(), 15)) { std::filesystem::remove(it->path(), e2); ++removed; }
    }
    return removed ? L"Limpei " + std::to_wstring(removed) + L" arquivo(s) incompleto(s) de um download interrompido." : L"";
}

// ---- tela de busca online ------------------------------------------------------------
struct OnlineUI {
    bool open = false, editing = true, fromLink = false;
    int source = 1, targetPl = -1, scroll = 0;
    int tipo = 0;                       // 0 = musicas, 1 = playlists prontas, 2 = albuns
    std::wstring query, linkName, linkUrl;
    std::mutex m; std::vector<OTrack> res; std::wstring status;
    std::vector<desc::Item> listas;     // resultados quando tipo != 0
    std::atomic<bool> busy{ false }; std::atomic<int> gen{ 0 }; ULONGLONG busySince = 0;
    RECT box{}, qbox{}, btnSearch{}, btnClose{}, src[3]{}, tab[3]{}, btnAddAll{};
    std::vector<RECT> rows, bPlay, bDl, bAdd;
};
inline OnlineUI& OU() { static OnlineUI* u = new OnlineUI(); return *u; }
inline void OnlineSearchAsync() {
    OnlineUI& u = OU();
    std::wstring q = Config::Trim(u.query); int where = u.source;
    if (q.empty()) return;
    int gen = ++u.gen;
    bool link = IsUrlText(q);
    if (!link && u.tipo != 0) {   // procurar playlists ou albuns prontos (Deezer publico)
        int tipo = u.tipo;
        u.busy = true; u.busySince = GetTickCount64(); u.scroll = 0; u.editing = false;
        { std::lock_guard<std::mutex> lk(u.m); u.res.clear(); u.listas.clear(); u.fromLink = false; u.linkName.clear(); u.linkUrl.clear(); u.status = L"Procurando..."; }
        std::thread([q, gen, tipo] {
            OnlineUI& u2 = OU();
            std::vector<desc::Item> v;
            RemixSafe("busca de listas", [&] {
                v = tipo == 1 ? desc::BuscarPlaylists(q, 20) : desc::BuscarAlbuns(q, 24);
                if (tipo == 1) {   // playlists tambem do YouTube (o catalogo do Deezer nao tem tudo)
                    std::vector<OLista> yt;
                    BuscarPlaylistsYoutube(q, yt, 20, nullptr);
                    for (auto& y : yt) {
                        desc::Item it; it.kind = desc::K_PLAYLIST; it.titulo = y.titulo;
                        it.sub = y.sub.empty() ? std::wstring(L"YouTube") : y.sub;
                        it.capa = y.capa; it.link = y.link; it.id = L"yt";
                        v.push_back(it);
                    }
                }
            });
            if (gen != u2.gen.load()) return;
            {
                std::lock_guard<std::mutex> lk(u2.m);
                u2.listas = v;
                u2.status = v.empty() ? L"Nada encontrado." : (std::to_wstring(v.size()) + (tipo == 1 ? L" playlists" : L" álbuns"));
            }
            u2.busy = false;
            AppPost(EV_ONLINE_SEARCH, L"", gen);
        }).detach();
        return;
    }
    { std::lock_guard<std::mutex> lk(u.m); u.listas.clear(); }
    u.busy = true; u.busySince = GetTickCount64(); u.scroll = 0; u.editing = false;
    { std::lock_guard<std::mutex> lk(u.m); u.res.clear(); u.fromLink = link; u.linkName.clear(); u.linkUrl = link ? q : L""; u.status = link ? L"Lendo o link..." : L"Buscando..."; }
    std::thread([q, where, gen, link] {
        OnlineUI& u = OU();
        std::vector<OTrack> out; std::wstring err, name;
        RemixSafe("busca online", [&] {
        if (link) { OResolved rr = ResolveLink(q, nullptr); out = rr.items; err = rr.err; name = rr.name; }
        else OnlineSearch(q, where, out, err, nullptr, 20, [&](const OTrack& t) {   // resultados aparecem conforme o yt-dlp acha
            std::lock_guard<std::mutex> lk(u.m);
            if (gen != u.gen.load()) return;
            u.res.push_back(t); u.status = L"Buscando... " + std::to_wstring(u.res.size()) + L" até agora";
        });
        });
        if (gen != u.gen.load()) return;
        if (!link && out.size() > 1) {
            // o que voce ouve desempata: um artista do seu gosto sobe ate 3 lugares
            std::vector<std::pair<double, size_t>> ord;
            for (size_t i = 0; i < out.size(); i++) ord.push_back({ (double)i - std::min(3.0, desc::PesoArtista(out[i].artist) * 0.5), i });
            std::stable_sort(ord.begin(), ord.end(), [](const std::pair<double, size_t>& a, const std::pair<double, size_t>& b) { return a.first < b.first; });
            std::vector<OTrack> nv; nv.reserve(out.size());
            for (auto& o : ord) nv.push_back(out[o.second]);
            out.swap(nv);
        }
        {
            std::lock_guard<std::mutex> lk(u.m);
            u.res = out; u.linkName = name;
            size_t n = out.size();
            u.status = n == 0 ? (err.empty() ? L"Nada encontrado." : err)
                              : (link ? (name.empty() ? L"" : name + L"  ·  ") + std::to_wstring(n) + (n == 1 ? L" música" : L" músicas") : std::to_wstring(n) + L" resultados");
        }
        u.busy = false;
        AppPost(EV_ONLINE_SEARCH, L"", gen);
        if (!link) {   // os primeiros resultados ja ficam prontos para tocar na hora
            std::vector<std::wstring> pre;
            for (size_t i = 0; i < out.size() && pre.size() < 3; ++i) pre.push_back(out[i].play.empty() ? out[i].url : out[i].play);
            PreResolve(pre);
        }
    }).detach();
}
// links resolvidos para adicionar numa playlist (ou criar uma nova)
struct ResolvedStore { std::mutex m; std::map<int, OResolved> map; int next = 1; };
inline ResolvedStore& RS() { static ResolvedStore* r = new ResolvedStore(); return *r; }
inline int ResolveLinkAsync(const std::wstring& url, const std::wstring& target) {
    int id; { std::lock_guard<std::mutex> lk(RS().m); id = RS().next++; }
    std::thread([url, target, id] { OResolved rr; RemixSafe("ler link", [&] { rr = ResolveLink(url, nullptr); }); if (rr.items.empty() && rr.err.empty()) rr.err = L"Não consegui ler esse link."; if (rr.name.empty()) rr.name = L"Playlist online"; { std::lock_guard<std::mutex> lk(RS().m); RS().map[id] = rr; } AppPost(EV_ONLINE_RESOLVED, OPack({ target, url }), id); }).detach();
    return id;
}
inline bool TakeResolved(int id, OResolved& out) { std::lock_guard<std::mutex> lk(RS().m); auto it = RS().map.find(id); if (it == RS().map.end()) return false; out = std::move(it->second); RS().map.erase(it); return true; }
