#pragma once
// Stems: separa a musica em vocal, bateria, baixo e "outros" usando um SEPARADOR EXTERNO que a
// pessoa instala e configura (Configuracoes > SEPARAR EM PARTES). O Remix nao embute nem instala
// separador nenhum: ele so executa a linha de comando configurada, com {entrada} e {saida}, e
// reconhece as partes pelo nome dos arquivos que aparecerem. Assim da para trocar de motor sem
// mexer no app -- inclusive por um mais leve ou mais preciso.
//
//  - Separar e a tarefa mais pesada do Remix. O perfil de CPU (leve/equilibrado/rapido) decide
//    quantos nucleos ela pode usar; o processo roda com prioridade baixa e preso a esses nucleos
//    (nice + taskset), entao nunca toma a maquina. Roda em segundo plano, uma musica por vez, e o
//    resultado fica no cache: da segunda vez em diante trocar de modo e na hora.
//  - Quem ja tinha o Python isolado de antes continua funcionando como alternativa.
//  - Cache: <cache>/stems/<chave>/{vocals,drums,bass,other,instrumental}.flac + "ok". A chave de um
//    arquivo local muda se o arquivo mudar (caminho + tamanho + data); a de uma musica online e o link.
//  - Musica online: baixa o audio primeiro (reaproveitando a extracao do CLI de midia em cache).
//  - Modos: completa, so vocal, so musica (tudo menos o vocal), bateria, baixo, outros.
#include "online_play.h"
#include <deque>
#include <map>
#include <memory>

int g_cfgStemsCpu();          // 1 = leve, 2 = equilibrado, 3 = rapido (app_core.h)
std::wstring g_cfgSepCmd();   // separador externo configurado pela pessoa (app_core.h)

namespace stems {

enum { M_FULL = 0, M_VOCAL, M_INST, M_DRUMS, M_BASS, M_OTHER, M_COUNT };
inline const wchar_t* ModeKey(int m) { static const wchar_t* k[M_COUNT] = { L"", L"vocal", L"instrumental", L"bateria", L"baixo", L"outros" }; return (m >= 0 && m < M_COUNT) ? k[m] : L""; }
inline const wchar_t* ModeName(int m) { static const wchar_t* k[M_COUNT] = { L"COMPLETA", L"SÓ VOCAL", L"SÓ MÚSICA", L"BATERIA", L"BAIXO", L"OUTROS" }; return (m >= 0 && m < M_COUNT) ? k[m] : L""; }
inline const wchar_t* ModeFile(int m) { static const wchar_t* k[M_COUNT] = { L"", L"vocals.flac", L"instrumental.flac", L"drums.flac", L"bass.flac", L"other.flac" }; return (m >= 0 && m < M_COUNT) ? k[m] : L""; }
inline int ModeFromKey(const std::wstring& k) { for (int m = 0; m < M_COUNT; ++m) if (k == ModeKey(m)) return m; return M_FULL; }

inline std::wstring Root() { return Config::Join(Config::CacheDir(), L"stems"); }

// ---- quanta CPU a separacao pode usar ------------------------------------------------------
// Separar uma musica e a tarefa mais pesada do Remix. Ela NUNCA pode tomar a maquina: o padrao
// e "leve" (um quarto dos nucleos). Alem de pedir menos threads ao separador, o processo e
// preso a esses nucleos com o taskset -- assim o limite vale mesmo para um separador externo
// que ignore a opcao de threads.
inline int PerfilCpu() { int p = g_cfgStemsCpu(); return (p < 1 || p > 3) ? 1 : p; }
inline const wchar_t* PerfilNome(int p) { static const wchar_t* k[4] = { L"", L"LEVE", L"EQUILIBRADO", L"RÁPIDO" }; return k[(p < 1 || p > 3) ? 1 : p]; }
inline int NucleosDoPerfil() {
    int total = (int)std::thread::hardware_concurrency(); if (total <= 0) total = 2;
    static const double pct[4] = { 0.25, 0.25, 0.5, 0.75 };
    int n = (int)(total * pct[PerfilCpu()] + 0.5);
    return std::max(1, std::min(total, n));
}
#ifndef _WIN32
inline bool TemPrograma(const wchar_t* nome) { return !fonte::AcharNoSistema(nome).empty(); }
// nice (prioridade baixa) + taskset (so N nucleos). Se faltar algum, usa o que der.
inline std::vector<std::wstring> ComLimiteDeCpu(const std::vector<std::wstring>& cmd) {
    if (cmd.empty()) return cmd;
    std::vector<std::wstring> a;
    std::wstring nice = fonte::AcharNoSistema(L"nice"), tset = fonte::AcharNoSistema(L"taskset");
    if (!nice.empty()) { a.push_back(nice); a.push_back(L"-n"); a.push_back(L"15"); }
    if (!tset.empty()) { a.push_back(tset); a.push_back(L"-c"); a.push_back(L"0-" + std::to_wstring(NucleosDoPerfil() - 1)); }
    for (auto& x : cmd) a.push_back(x);
    return a;
}
#else
inline std::vector<std::wstring> ComLimiteDeCpu(const std::vector<std::wstring>& cmd) { return cmd; }   // no Windows o runner baixa a prioridade sozinho
#endif
// Onde ficava o Python isolado das versoes antigas (alternativa, quando existe).
// Linux: dentro do Remix (portatil) ou ~/.local/share/remix/stems (instalado por pacote).
inline std::vector<std::wstring> ToolDirs() {
    std::vector<std::wstring> v;
    v.push_back(Config::Join(Config::Join(Config::AssetDir(), L"tools"), L"stems"));
#ifndef _WIN32
    if (const char* home = std::getenv("HOME")) v.push_back(Utf8ToWide(std::string(home) + "/.local/share/remix/stems"));
#endif
    return v;
}
inline std::wstring PythonIn(const std::wstring& dir) {
#ifdef _WIN32
    return Config::Join(Config::Join(Config::Join(dir, L"venv"), L"Scripts"), L"python.exe");
#else
    return Config::Join(Config::Join(Config::Join(dir, L"venv"), L"bin"), L"python");
#endif
}
inline bool HasDemucs(const std::wstring& dir) {
    namespace fs = std::filesystem; std::error_code ec;
    fs::path venv = fs::path(Config::Join(dir, L"venv"));
#ifdef _WIN32
    return fs::exists(venv / L"Lib" / L"site-packages" / L"demucs", ec);
#else
    fs::path lib = venv / "lib";
    for (fs::directory_iterator it(lib, ec), end; !ec && it != end; it.increment(ec))
        if (fs::exists(it->path() / "site-packages" / "demucs", ec)) return true;
    return false;
#endif
}
inline std::wstring ToolDir() {   // "" = nao instalado
    std::error_code ec;
    for (auto& d : ToolDirs()) if (std::filesystem::exists(std::filesystem::path(PythonIn(d)), ec) && HasDemucs(d)) return d;
    return L"";
}
inline bool SepExternoOk();   // definido mais abaixo (separador configurado pela pessoa)
// Da para separar? Vale o separador configurado OU o Python isolado antigo, se existir.
inline bool Installed() { return SepExternoOk() || !ToolDir().empty(); }
inline bool InstalledCached() {   // para desenhar (confere o disco no maximo a cada 5 s)
    static std::atomic<ULONGLONG> at{ 0 }; static std::atomic<bool> v{ false };
    ULONGLONG now = GetTickCount64();
    if (at.load() == 0 || now - at.load() > 5000) { v = Installed(); at = now; }
    return v.load();
}

inline std::wstring KeyFor(const std::wstring& src, bool online) {
    std::wstring base = online ? L"url|" + src : L"arq|" + src;
    if (!online) {
        std::error_code ec; std::filesystem::path p(src);
        base += L"|" + std::to_wstring((unsigned long long)std::filesystem::file_size(p, ec));
        auto t = std::filesystem::last_write_time(p, ec); if (!ec) base += L"|" + std::to_wstring((long long)t.time_since_epoch().count());
    }
    unsigned long long h = 1469598103934665603ULL;
    for (wchar_t c : base) { h ^= (unsigned long long)c; h *= 1099511628211ULL; }
    wchar_t b[24]; swprintf(b, 24, L"%016llx", h); return b;
}
inline std::wstring DirFor(const std::wstring& key) { return Config::Join(Root(), key); }
inline bool Complete(const std::wstring& key) { std::error_code ec; return !key.empty() && std::filesystem::exists(std::filesystem::path(Config::Join(DirFor(key), L"ok")), ec); }
inline std::wstring FileFor(const std::wstring& key, int mode) { if (mode <= M_FULL || mode >= M_COUNT || !Complete(key)) return L""; return Config::Join(DirFor(key), ModeFile(mode)); }
// Nem todo separador faz as cinco partes: muitos bons fazem so vocal + instrumental.
inline bool TemModo(const std::wstring& key, int mode) {
    if (mode <= M_FULL || mode >= M_COUNT) return true;
    std::error_code ec; return Complete(key) && std::filesystem::exists(std::filesystem::path(Config::Join(DirFor(key), ModeFile(mode))), ec);
}

// ---- separador externo (o que a pessoa configurou) -------------------------------------------
// Contrato: uma linha de comando com {entrada} (o arquivo) e {saida} (uma pasta vazia). O Remix
// executa, olha o que apareceu na pasta e reconhece cada parte pelo NOME do arquivo (vocal,
// instrumental, bateria, baixo, outros — em portugues ou ingles). Serve qualquer separador que
// grave um arquivo por parte, e a pessoa troca de motor quando quiser sem mexer no app.
inline std::vector<std::wstring> QuebrarLinha(const std::wstring& s) {
    std::vector<std::wstring> v; std::wstring cur; bool asp = false, tem = false;
    for (size_t i = 0; i < s.size(); ++i) {
        wchar_t c = s[i];
        if (c == L'"') { asp = !asp; tem = true; continue; }
        if (!asp && (c == L' ' || c == L'\t')) { if (tem || !cur.empty()) { v.push_back(cur); cur.clear(); tem = false; } continue; }
        cur.push_back(c); tem = true;
    }
    if (tem || !cur.empty()) v.push_back(cur);
    return v;
}
inline std::wstring SepPrograma() {
    auto v = QuebrarLinha(g_cfgSepCmd()); if (v.empty() || v[0].empty()) return L"";
    return (v[0].find(L'/') != std::wstring::npos || v[0].find(L'\\') != std::wstring::npos) ? (fonte::ExecutavelOk(v[0]) ? v[0] : L"") : fonte::AcharNoSistema(v[0]);
}
inline bool SepExternoOk() { return !SepPrograma().empty(); }
// Qual parte e este arquivo? -1 = nao reconhecido. A ordem importa: "no_vocals" e instrumental.
inline int ModoDoArquivo(std::wstring nome) {
    for (auto& c : nome) c = (wchar_t)towlower(c);
    auto tem = [&](const wchar_t* k) { return nome.find(k) != std::wstring::npos; };
    if (tem(L"instrumental") || tem(L"no_vocal") || tem(L"novocal") || tem(L"no-vocal") || tem(L"accompaniment") || tem(L"karaoke") || tem(L"backing")) return M_INST;
    if (tem(L"vocal") || tem(L"voz") || tem(L"voice")) return M_VOCAL;
    if (tem(L"drum") || tem(L"bateria")) return M_DRUMS;
    if (tem(L"bass") || tem(L"baixo")) return M_BASS;
    if (tem(L"other") || tem(L"outros")) return M_OTHER;
    return -1;
}
inline bool EhAudio(const std::wstring& ext) {
    std::wstring e = ext; for (auto& c : e) c = (wchar_t)towlower(c);
    return e == L".wav" || e == L".flac" || e == L".mp3" || e == L".m4a" || e == L".ogg" || e == L".opus" || e == L".aiff" || e == L".aif";
}
// Converte para o padrao do cache (<parte>.flac). Devolve os modos que entraram (bitmask).
inline int ImportarSeparados(const std::wstring& origem, const std::wstring& destino, const std::atomic<bool>* cancel) {
    namespace fs = std::filesystem; std::error_code ec;
    int achados = 0;
    for (fs::recursive_directory_iterator it(fs::path(origem), fs::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        if (cancel && cancel->load()) break;
        if (!it->is_regular_file(ec)) continue;
        if (!EhAudio(it->path().extension().wstring())) continue;
        std::wstring base = it->path().stem().wstring();
        int m = ModoDoArquivo(base);
        if (m < 0) m = ModoDoArquivo(it->path().parent_path().filename().wstring());   // alguns gravam uma pasta por parte
        if (m <= M_FULL || m >= M_COUNT || (achados & (1 << m))) continue;
        std::wstring dst = Config::Join(destino, ModeFile(m));
        CapResult r = RunCapture({ fonte::Ffmpeg(), L"-nostdin", L"-v", L"error", L"-y", L"-i", it->path().wstring(),
                                   L"-map", L"0:a:0", L"-c:a", L"flac", L"-compression_level", L"5", dst }, 300000, cancel);
        if (r.code == 0 && fs::exists(fs::path(dst), ec)) achados |= (1 << m);
    }
    // 4 partes sem "so musica": o instrumental e a soma das outras tres.
    if (!(achados & (1 << M_INST)) && (achados & (1 << M_DRUMS)) && (achados & (1 << M_BASS)) && (achados & (1 << M_OTHER))) {
        std::wstring dst = Config::Join(destino, ModeFile(M_INST));
        CapResult r = RunCapture({ fonte::Ffmpeg(), L"-nostdin", L"-v", L"error", L"-y",
                                   L"-i", Config::Join(destino, ModeFile(M_DRUMS)), L"-i", Config::Join(destino, ModeFile(M_BASS)), L"-i", Config::Join(destino, ModeFile(M_OTHER)),
                                   L"-filter_complex", L"amix=inputs=3:normalize=0", L"-c:a", L"flac", L"-compression_level", L"5", dst }, 300000, cancel);
        if (r.code == 0 && fs::exists(fs::path(dst), ec)) achados |= (1 << M_INST);
    }
    return achados;
}

// ---- o separador (Python) -------------------------------------------------------------------
static const char* RUNNER_PY = R"~~~(# Remix: separador do Python isolado das versoes antigas (gerado pelo app; pode apagar).
import os, sys
inp, out, ffmpeg, torch_home = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
threads = int(sys.argv[5]) if len(sys.argv) > 5 else 2
if torch_home:
    os.environ['TORCH_HOME'] = torch_home
try:
    if os.name == 'nt':
        import ctypes; ctypes.windll.kernel32.SetPriorityClass(ctypes.windll.kernel32.GetCurrentProcess(), 0x4000)
    else:
        os.nice(10)
except Exception:
    pass
import subprocess
import numpy as np, torch
from demucs.pretrained import get_model
from demucs.apply import apply_model
import soundfile as sf
torch.set_num_threads(max(1, threads))
model = get_model('htdemucs'); model.eval()
sr = model.samplerate
kw = {}
if os.name == 'nt':
    kw['creationflags'] = 0x08000000
raw = subprocess.run([ffmpeg, '-nostdin', '-v', 'error', '-i', inp, '-vn', '-f', 'f32le', '-ac', '2', '-ar', str(sr), 'pipe:1'], capture_output=True, check=True, **kw).stdout
wav = torch.from_numpy(np.frombuffer(raw, dtype=np.float32).reshape(-1, 2).T.copy())
if wav.shape[1] < sr:
    print('REMIXERR audio curto demais', flush=True); sys.exit(2)
ref = wav.mean(0); m, s = ref.mean(), ref.std() + 1e-8
with torch.no_grad():
    src = apply_model(model, ((wav - m) / s)[None], device='cpu', shifts=0, split=True, overlap=0.25, progress=True, num_workers=0)[0]
src = src * s + m
os.makedirs(out, exist_ok=True)
for name, x in zip(model.sources, src):
    sf.write(os.path.join(out, name + '.part.flac'), x.T.clamp(-1, 1).numpy(), sr, subtype='PCM_16')
inst = src.sum(0) - src[model.sources.index('vocals')]
sf.write(os.path.join(out, 'instrumental.part.flac'), inst.T.clamp(-1, 1).numpy(), sr, subtype='PCM_16')
for name in list(model.sources) + ['instrumental']:
    os.replace(os.path.join(out, name + '.part.flac'), os.path.join(out, name + '.flac'))
print('REMIXOK', flush=True)
)~~~";
inline std::wstring RunnerPath() {
    std::wstring p = Config::Join(Root(), L"remix_stems.py");
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(Root()), ec);
    std::string cur; { std::ifstream f(std::filesystem::path(p), std::ios::binary); if (f) cur.assign(std::istreambuf_iterator<char>(f), {}); }
    if (cur != RUNNER_PY) { std::ofstream o(std::filesystem::path(p), std::ios::binary | std::ios::trunc); o << RUNNER_PY; }
    return p;
}

// ---- fila ----------------------------------------------------------------------------------
enum { S_QUEUED = 0, S_DOWNLOADING, S_SEPARATING, S_READY, S_FAILED, S_CANCELED };
struct Job {
    std::wstring key, src, title, artist, play; int dur = 0; bool online = false;   // src = identidade (caminho ou link); play = link tocavel
    std::atomic<int> state{ S_QUEUED }, pct{ 0 };
    std::atomic<bool> cancel{ false };
    std::mutex m; std::wstring err;
};
struct QueueT { std::mutex m; std::deque<std::shared_ptr<Job>> q; std::map<std::wstring, std::shared_ptr<Job>> all; bool worker = false; std::shared_ptr<Job> cur; };
inline QueueT& Q() { static QueueT* q = new QueueT(); return *q; }
inline std::shared_ptr<Job> Find(const std::wstring& key) { std::lock_guard<std::mutex> lk(Q().m); auto it = Q().all.find(key); return it == Q().all.end() ? nullptr : it->second; }

// Mantem o cache em ate ~3 GB (apaga as separacoes mais antigas).
inline void Prune(unsigned long long maxBytes = 3ULL * 1024 * 1024 * 1024) {
    namespace fs = std::filesystem; std::error_code ec;
    struct D { fs::path p; fs::file_time_type t; unsigned long long sz; };
    std::vector<D> ds; unsigned long long total = 0;
    for (fs::directory_iterator it(fs::path(Root()), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_directory(ec)) continue;
        D d; d.p = it->path(); d.t = fs::last_write_time(d.p / "ok", ec); if (ec) { ec.clear(); continue; }
        d.sz = 0; for (fs::directory_iterator f(d.p, ec), e2; !ec && f != e2; f.increment(ec)) { std::error_code e3; d.sz += f->file_size(e3); }
        ec.clear(); total += d.sz; ds.push_back(d);
    }
    std::sort(ds.begin(), ds.end(), [](const D& a, const D& b) { return a.t < b.t; });
    for (auto& d : ds) { if (total <= maxBytes) break; fs::remove_all(d.p, ec); total -= std::min(total, d.sz); }
}

inline void RunJob(Job& j, const std::function<void(const std::wstring&, int)>& notify) {
    namespace fs = std::filesystem; std::error_code ec;
    auto fail = [&](const std::wstring& e) { { std::lock_guard<std::mutex> lk(j.m); j.err = e; } j.state = j.cancel ? S_CANCELED : S_FAILED; notify(j.key, j.state); };
    bool externo = SepExternoOk();                 // o separador que a pessoa configurou tem preferencia
    std::wstring tool = externo ? std::wstring() : ToolDir();
    if (!externo && tool.empty()) { fail(L"Nenhum separador configurado: em Configurações > SEPARAR EM PARTES (STEMS), aponte o programa que você instalou."); return; }
    fonte::Garantir();
    if (!fonte::FfmpegOk()) { fail(L"Precisa do ffmpeg para separar os stems."); return; }
    std::wstring dir = DirFor(j.key);
    fs::create_directories(fs::path(dir), ec);
    std::wstring input = j.src;
    if (j.online) {   // baixa o audio (so o audio, formato original) para separar
        j.state = S_DOWNLOADING; notify(j.key, j.state);
        std::wstring have;
        for (fs::directory_iterator it(fs::path(dir), ec), end; !ec && it != end; it.increment(ec)) if (it->path().stem() == L"entrada") have = it->path().wstring();
        ec.clear();
        if (have.empty()) {
            if (!fonte::Configurada()) { fail(L"Para separar uma música de fonte externa, configure uma CLI compatível."); return; }
            std::wstring play = j.play.empty() ? j.src : j.play;
            if (NeedsMatch(DetectSource(play))) {   // Spotify/Deezer/Apple: acha a mesma musica no YouTube Music
                OTrack t; t.url = j.src; t.title = j.title; t.artist = j.artist; t.dur = j.dur; std::wstring e;
                if (!MatchOnYouTube(t, e, &j.cancel) || t.play.empty()) { fail(e.empty() ? L"Não achei essa música no YouTube Music." : e); return; }
                play = t.play;
            }
            std::wstring infoFile; MediaInfo mi; std::wstring e;
            if (GetMediaInfo(play, mi, e, &j.cancel) && !mi.raw.empty()) {
                infoFile = Config::Join(dir, L"info.json");
                std::ofstream o(fs::path(infoFile), std::ios::binary | std::ios::trunc); o.write(mi.raw.data(), (std::streamsize)mi.raw.size());
            }
            for (int tent = 0; tent < 2 && have.empty() && !j.cancel; ++tent) {
                auto a = fonte::Cmd();
                for (const wchar_t* x : { L"--no-playlist", L"-q", L"-f", L"bestaudio/best", L"-o" }) a.push_back(x);
                a.push_back(Config::Join(dir, L"entrada.%(ext)s"));
                if (tent == 0 && !infoFile.empty()) { a.push_back(L"--load-info-json"); a.push_back(infoFile); }
                else { a.push_back(L"--"); a.push_back(play); }
                fonte::Rodar(a, 600000, &j.cancel, nullptr, 64 * 1024);
                for (fs::directory_iterator it(fs::path(dir), ec), end; !ec && it != end; it.increment(ec)) { auto ext = it->path().extension().wstring(); if (it->path().stem() == L"entrada" && ext != L".part" && ext != L".json") have = it->path().wstring(); }
                ec.clear();
            }
        }
        if (j.cancel) { fail(L"Cancelado."); return; }
        if (have.empty()) { fail(L"Não consegui preparar o áudio dessa música para separar."); return; }
        input = have;
    }
    j.state = S_SEPARATING; j.pct = 0; notify(j.key, j.state);
    std::string tail; bool okLine = false; int lastPct = -1; int achados = 0;
    auto progresso = [&](const std::string& ln) {   // qualquer "NN%" na saida vira barra de progresso
        if (ln.find("REMIXOK") != std::string::npos) okLine = true;
        size_t p = ln.find('%');
        if (p != std::string::npos && p > 0) { size_t b = p; while (b > 0 && isdigit((unsigned char)ln[b - 1])) --b; if (b < p) { int v = atoi(ln.substr(b, p - b).c_str()); if (v >= 0 && v <= 100 && v != lastPct) { lastPct = v; j.pct = v; notify(j.key, S_SEPARATING); } } }
        if (ln.find("Error") != std::string::npos || ln.find("error") != std::string::npos || ln.find("REMIXERR") != std::string::npos) { tail = ln; if (tail.size() > 200) tail = tail.substr(tail.size() - 200); }
    };
    CapResult r;
    if (externo) {
        std::wstring tmp = Config::Join(dir, L"saida");
        fs::remove_all(fs::path(tmp), ec); ec.clear(); fs::create_directories(fs::path(tmp), ec);
        auto partes = QuebrarLinha(g_cfgSepCmd());
        std::vector<std::wstring> cmd; cmd.push_back(SepPrograma());
        bool temEnt = false, temSai = false;
        auto troca = [&](std::wstring a) {
            for (;;) { size_t k = a.find(L"{entrada}"); if (k == std::wstring::npos) break; a = a.substr(0, k) + input + a.substr(k + 9); temEnt = true; }
            for (;;) { size_t k = a.find(L"{saida}"); if (k == std::wstring::npos) break; a = a.substr(0, k) + tmp + a.substr(k + 7); temSai = true; }
            return a;
        };
        for (size_t i = 1; i < partes.size(); ++i) cmd.push_back(troca(partes[i]));
        if (!temSai) { cmd.push_back(L"--output_dir"); cmd.push_back(tmp); }   // sem marcador: tenta o mais comum
        if (!temEnt) cmd.push_back(input);                                      // sem marcador: o arquivo vai no fim
        r = RunCapture(ComLimiteDeCpu(cmd), 0, &j.cancel, progresso, 256 * 1024);
        if (!j.cancel) achados = ImportarSeparados(tmp, dir, &j.cancel);
        std::error_code e2; fs::remove_all(fs::path(tmp), e2);
    } else {
        std::vector<std::wstring> args = ComLimiteDeCpu({ PythonIn(tool), RunnerPath(), input, dir, fonte::Ffmpeg(),
                                                         Config::Join(tool, L"torch"), std::to_wstring(NucleosDoPerfil()) });
        r = RunCapture(args, 0, &j.cancel, progresso, 256 * 1024);
        for (int m = M_VOCAL; m < M_COUNT; ++m) if (fs::exists(fs::path(Config::Join(dir, ModeFile(m))), ec)) achados |= (1 << m);
        ec.clear();
    }
    if (j.cancel) { fail(L"Cancelado."); return; }
    // Basta ter vocal ou instrumental: ha bons separadores que so fazem essas duas partes.
    bool util = (achados & (1 << M_VOCAL)) || (achados & (1 << M_INST));
    if (!util || r.code != 0) {
        std::string e = tail; if (e.empty() && !r.err.empty()) { e = r.err; size_t nl = e.find_last_of('\n', e.size() > 2 ? e.size() - 2 : 0); if (nl != std::string::npos) e = e.substr(nl + 1); if (e.size() > 200) e = e.substr(e.size() - 200); }
        fail(L"A separação falhou" + (e.empty() ? std::wstring(L".") : L": " + Utf8ToWide(e)));
        return;
    }
    { std::ofstream o(fs::path(Config::Join(dir, L"ok")), std::ios::binary | std::ios::trunc); o << achados << "\n"; }
    if (j.online) for (fs::directory_iterator it(fs::path(dir), ec), end; !ec && it != end; it.increment(ec)) if (it->path().stem() == L"entrada" || it->path().filename() == L"info.json") { std::error_code e2; fs::remove(it->path(), e2); }
    j.pct = 100; j.state = S_READY; notify(j.key, j.state);
    Prune();
}

// Pede a separacao (front = musica atual: passa na frente da fila). Devolve o trabalho (novo ou o que ja existia).
inline std::shared_ptr<Job> Request(const std::wstring& src, bool online, const std::wstring& play, const std::wstring& title, const std::wstring& artist, int dur, bool front, std::function<void(const std::wstring&, int)> notify) {
    std::wstring key = KeyFor(src, online);
    std::lock_guard<std::mutex> lk(Q().m);
    auto it = Q().all.find(key);
    if (it != Q().all.end()) {
        auto j = it->second; int st = j->state.load();
        if (st == S_QUEUED || st == S_DOWNLOADING || st == S_SEPARATING || (st == S_READY && Complete(key))) {
            if (front && st == S_QUEUED) { auto& q = Q().q; auto f = std::find(q.begin(), q.end(), j); if (f != q.end()) { q.erase(f); q.push_front(j); } }
            return j;
        }
        Q().all.erase(it);   // falhou/cancelou antes: tenta de novo
    }
    auto j = std::make_shared<Job>(); j->key = key; j->src = src; j->online = online; j->play = play; j->title = title; j->artist = artist; j->dur = dur;
    if (Complete(key)) { j->state = S_READY; j->pct = 100; Q().all[key] = j; return j; }
    Q().all[key] = j;
    if (front) Q().q.push_front(j); else Q().q.push_back(j);
    if (!Q().worker) {
        Q().worker = true;
        std::thread([notify] {
            for (;;) {
                std::shared_ptr<Job> jj;
                { std::lock_guard<std::mutex> lk2(Q().m); if (Q().q.empty()) { Q().worker = false; Q().cur = nullptr; return; } jj = Q().q.front(); Q().q.pop_front(); Q().cur = jj; }
                if (jj->cancel) { jj->state = S_CANCELED; continue; }
                RemixSafe("separacao de stems", [&] { RunJob(*jj, notify); });
                if (jj->state.load() < S_READY) { jj->state = S_FAILED; notify(jj->key, S_FAILED); }
            }
        }).detach();
    }
    return j;
}
// Cancela o que ainda esta na fila (e o atual, se "tudo").
inline void CancelQueued(bool alsoCurrent) {
    std::lock_guard<std::mutex> lk(Q().m);
    for (auto& j : Q().q) { j->cancel = true; j->state = S_CANCELED; }
    Q().q.clear();
    if (alsoCurrent && Q().cur) Q().cur->cancel = true;
}
inline std::shared_ptr<Job> Current() { std::lock_guard<std::mutex> lk(Q().m); return Q().cur; }
inline size_t QueuedCount() { std::lock_guard<std::mutex> lk(Q().m); return Q().q.size(); }

} // namespace stems
