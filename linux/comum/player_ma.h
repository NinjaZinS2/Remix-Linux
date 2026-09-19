#pragma once
// Backend de audio (Windows e Linux): miniaudio (implementacao em audio_backend.c).
// Mesma interface publica do player.h (Media Foundation) para a casca
// reaproveitar toda a logica, mais:
//  - engine reaberta na taxa de amostragem do arquivo (44.1k/48k/...): nenhum
//    resampling no player, o audio sai como foi decodificado ("reproducao
//    original"; PipeWire/Pulse cuida do resto se o hardware pedir);
//  - formatos nativos: MP3, WAV, FLAC, OGG Vorbis (outros via conversao pela
//    casca, sys::TranscodeToWav);
//  - cadeia de efeitos (audio_fx.h): som (pitch) -> grave -> reverb -> 8D -> equalizador de
//    8 bandas (desligado = pulado) -> mestre (volume com curva, subida controlada e limitador) -> saida.
#include "platform.h"
#include "config.h"
#include <string>
#include <vector>
#include <cmath>
#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <cstring>
#include <memory>

#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define STB_VORBIS_HEADER_ONLY
#include "audio/stb_vorbis.c"
#include "audio/miniaudio.h"
#include "audio_fx.h"

// ---- audio online: PCM na memoria -----------------------------------------------
// A thread de streaming (app_online.h) escreve PCM 16-bit estereo aqui; o miniaudio le
// por uma fonte de dados propria. Nada vai para o disco. Enquanto o buffer nao chega,
// sai silencio SEM avancar a posicao; seek fora do trecho recebido vira pedido de
// reinicio (o ffmpeg recomeca do ponto com -ss).
struct PcmStream {
    std::mutex m;
    std::vector<int16_t> pcm;                        // estereo intercalado, a partir de baseFrame
    uint64_t baseFrame = 0;
    bool eof = false;
    bool canRestart = false;                         // da para recomecar o decodificador em outro ponto
    std::atomic<uint64_t> seekReq{ UINT64_MAX };
    std::atomic<uint64_t> readCursor{ 0 };
    std::atomic<uint64_t> lenFrames{ 0 };
    uint32_t rate = 48000;
    uint64_t EndFrame() const { return baseFrame + pcm.size() / 2; }
};
struct PcmStreamDS { ma_data_source_base base; PcmStream* st; ma_uint64 cursor; };
inline ma_result PcmDS_Read(ma_data_source* pDS, void* pOut, ma_uint64 frames, ma_uint64* pRead) {
    PcmStreamDS* d = (PcmStreamDS*)pDS; PcmStream* st = d->st;
    int16_t* o = (int16_t*)pOut; ma_uint64 got = 0; bool ended = false;
    {
        std::lock_guard<std::mutex> lk(st->m);
        uint64_t b = st->baseFrame, e = st->EndFrame();
        if (d->cursor >= b && d->cursor < e) {
            ma_uint64 can = std::min<ma_uint64>(frames, e - d->cursor);
            memcpy(o, &st->pcm[(size_t)((d->cursor - b) * 2)], (size_t)can * 4);
            got = can; d->cursor += can;
        } else if (d->cursor < b && st->canRestart && st->seekReq.load() == UINT64_MAX) {
            st->seekReq = d->cursor;
        }
        ended = st->eof && d->cursor >= st->EndFrame() && st->seekReq.load() == UINT64_MAX;
        st->readCursor = d->cursor;
    }
    if (got < frames && ended) { if (pRead) *pRead = got; return got ? MA_SUCCESS : MA_AT_END; }
    if (got < frames) memset(o + got * 2, 0, (size_t)(frames - got) * 4);
    if (pRead) *pRead = frames;
    return MA_SUCCESS;
}
inline ma_result PcmDS_Seek(ma_data_source* pDS, ma_uint64 frame) {
    PcmStreamDS* d = (PcmStreamDS*)pDS; PcmStream* st = d->st;
    uint64_t len = st->lenFrames.load();
    if (len && frame > len) frame = len;
    d->cursor = frame;
    std::lock_guard<std::mutex> lk(st->m);
    if (st->canRestart && (frame < st->baseFrame || frame > st->EndFrame() + (uint64_t)st->rate * 6)) st->seekReq = frame;
    st->readCursor = frame;
    return MA_SUCCESS;
}
inline ma_result PcmDS_Format(ma_data_source* pDS, ma_format* f, ma_uint32* ch, ma_uint32* sr, ma_channel* map, size_t cap) {
    PcmStreamDS* d = (PcmStreamDS*)pDS;
    if (f) *f = ma_format_s16;
    if (ch) *ch = 2;
    if (sr) *sr = d->st->rate;
    if (map && cap >= 2) { map[0] = MA_CHANNEL_FRONT_LEFT; map[1] = MA_CHANNEL_FRONT_RIGHT; }
    return MA_SUCCESS;
}
inline ma_result PcmDS_Cursor(ma_data_source* pDS, ma_uint64* c) { *c = ((PcmStreamDS*)pDS)->cursor; return MA_SUCCESS; }
inline ma_result PcmDS_Length(ma_data_source* pDS, ma_uint64* l) { uint64_t v = ((PcmStreamDS*)pDS)->st->lenFrames.load(); *l = v; return v ? MA_SUCCESS : MA_NOT_IMPLEMENTED; }
inline ma_data_source_vtable* PcmDSVtable() { static ma_data_source_vtable v = { PcmDS_Read, PcmDS_Seek, PcmDS_Format, PcmDS_Cursor, PcmDS_Length, nullptr, 0 }; return &v; }

class Player {
    PcmStreamDS* streamDs = nullptr;
    std::shared_ptr<PcmStream> streamKeep;   // o buffer do streaming fica vivo enquanto o som existir
    ma_sound snd{};
    bool sndInit = false;
    DWORD cachedLenMs = 0;
    ma_uint32 sampleRate = 0;
    std::wstring openPath;
    float curVol = 0.8f;

    // ---- engine + equalizador (estado global do processo) ----
    struct Eq { ma_peak_node node[8]; bool init = false; int gains[8] = {0,0,0,0,0,0,0,0}; bool on = false; };
    static ma_engine& Engine() { static ma_engine e; return e; }
    static bool& EngineOk() { static bool ok = false; return ok; }
    static Eq& EqState() { static Eq e; return e; }
    static const float* EqFreqs() { static const float f[8] = { 60.f, 150.f, 400.f, 1000.f, 2500.f, 6000.f, 10000.f, 15000.f }; return f; }

    static void DestroyEq() {
        Eq& e = EqState();
        if (!e.init) return;
        for (int i = 0; i < 8; ++i) ma_peak_node_uninit(&e.node[i], NULL);
        e.init = false;
    }
    // ---- efeitos + mestre (audio_fx.h) ----
    struct Fx { ma_loshelf_node bass; afx::ReverbNode reverb; afx::EightDNode eightD; afx::MasterNode master; bool init = false; int bassLv = 0; };
    static Fx& FxState() { static Fx* f = new Fx(); return *f; }
    static std::atomic<float>& VolTarget() { static std::atomic<float> v{ 0.512f }; return v; }
    static void DestroyFx() {
        Fx& f = FxState();
        if (!f.init) return;
        ma_node_uninit(&f.master, NULL); ma_node_uninit(&f.eightD, NULL); ma_node_uninit(&f.reverb, NULL); ma_loshelf_node_uninit(&f.bass, NULL);
        f.init = false;
    }
    static void BuildFx() {
        Fx& f = FxState();
        DestroyFx();
        if (!EngineOk()) return;
        ma_node_graph* g = ma_engine_get_node_graph(&Engine());
        ma_uint32 ch = ma_engine_get_channels(&Engine()), sr = ma_engine_get_sample_rate(&Engine());
        f.bassLv = afx::L().bass.load();
        ma_loshelf_node_config bc = ma_loshelf_node_config_init(ch, sr, afx::BassDb(f.bassLv), 1.0, 110.0);
        if (ma_loshelf_node_init(g, &bc, NULL, &f.bass) != MA_SUCCESS) return;
        ma_node_config nc = ma_node_config_init(); nc.pInputChannels = &ch; nc.pOutputChannels = &ch;
        f.reverb.channels = ch; afx::ReverbSetup(f.reverb, sr); f.reverb.fade = 0; f.reverb.last = 0;
        nc.vtable = afx::ReverbVt();
        if (ma_node_init(g, &nc, NULL, &f.reverb) != MA_SUCCESS) { ma_loshelf_node_uninit(&f.bass, NULL); return; }
        f.eightD.channels = ch; f.eightD.sr = sr; f.eightD.mix = 0; f.eightD.phase = 0;
        nc.vtable = afx::EightDVt();
        if (ma_node_init(g, &nc, NULL, &f.eightD) != MA_SUCCESS) { ma_node_uninit(&f.reverb, NULL); ma_loshelf_node_uninit(&f.bass, NULL); return; }
        f.master.channels = ch; f.master.sr = sr; f.master.target = VolTarget().load(); f.master.cur = VolTarget().load(); f.master.env = 1.0f;   // faixa nova: sem rampa
        nc.vtable = afx::MasterVt();
        if (ma_node_init(g, &nc, NULL, &f.master) != MA_SUCCESS) { ma_node_uninit(&f.eightD, NULL); ma_node_uninit(&f.reverb, NULL); ma_loshelf_node_uninit(&f.bass, NULL); return; }
        ma_node_attach_output_bus(&f.bass, 0, &f.reverb, 0);
        ma_node_attach_output_bus(&f.reverb, 0, &f.eightD, 0);
        ma_node_attach_output_bus(&f.master, 0, ma_engine_get_endpoint(&Engine()), 0);
        f.init = true;
    }
    // Liga o fim da cadeia: 8D -> (equalizador ligado ? equalizador -> mestre : mestre).
    static void RouteChain() {
        Fx& f = FxState(); Eq& e = EqState();
        if (!f.init) return;
        if (e.init) ma_node_attach_output_bus(&e.node[7], 0, &f.master, 0);
        if (e.on && e.init) ma_node_attach_output_bus(&f.eightD, 0, &e.node[0], 0);
        else ma_node_attach_output_bus(&f.eightD, 0, &f.master, 0);
    }
    static void BuildEq() {
        Eq& e = EqState();
        DestroyEq();
        if (!EngineOk()) return;
        ma_uint32 ch = ma_engine_get_channels(&Engine()), sr = ma_engine_get_sample_rate(&Engine());
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            float f = std::min(EqFreqs()[i], sr * 0.45f);
            ma_peak_node_config c = ma_peak_node_config_init(ch, sr, (double)e.gains[i], 1.0, (double)f);
            if (ma_peak_node_init(ma_engine_get_node_graph(&Engine()), &c, NULL, &e.node[i]) != MA_SUCCESS) { ok = false; break; }
        }
        if (!ok) return;
        for (int i = 0; i < 7; ++i) ma_node_attach_output_bus(&e.node[i], 0, &e.node[i + 1], 0);
        ma_node_attach_output_bus(&e.node[7], 0, ma_engine_get_endpoint(&Engine()), 0);
        e.init = true;
    }
    static void RouteSound(ma_sound* s) {
        Eq& e = EqState();
        if (!s) return;
        if (FxState().init) { ma_node_attach_output_bus(s, 0, &FxState().bass, 0); return; }
        if (e.on && e.init) ma_node_attach_output_bus(s, 0, &e.node[0], 0);
        else ma_node_attach_output_bus(s, 0, ma_engine_get_endpoint(&Engine()), 0);
    }
    // Contexto proprio para listar e escolher a saida ("onde tocar").
    static ma_context* OutCtx() {
        static ma_context ctx; static int st = 0;   // 0 = nao tentou, 1 = ok, 2 = falhou
        if (st == 0) st = ma_context_init(nullptr, 0, nullptr, &ctx) == MA_SUCCESS ? 1 : 2;
        return st == 1 ? &ctx : nullptr;
    }
    static bool AchaSaida(const std::wstring& nome, ma_device_id& id) {
        ma_context* c = OutCtx(); if (!c || nome.empty()) return false;
        ma_device_info* pb = nullptr; ma_uint32 pn = 0; ma_device_info* cb = nullptr; ma_uint32 cn = 0;
        if (ma_context_get_devices(c, &pb, &pn, &cb, &cn) != MA_SUCCESS) return false;
        for (ma_uint32 i = 0; i < pn; ++i) if (Utf8ToWide(pb[i].name) == nome) { id = pb[i].id; return true; }
        return false;
    }
    static bool InitEngine(ma_uint32 rate) {
        ma_engine_config cfg = ma_engine_config_init();
        cfg.sampleRate = rate; // 0 = taxa do dispositivo
        static ma_device_id devId;
        if (!OutDeviceName().empty() && AchaSaida(OutDeviceName(), devId)) {   // saida escolhida pela pessoa
            cfg.pContext = OutCtx();
            cfg.pPlaybackDeviceID = &devId;
        }
        if (std::getenv("REMIX_NULL_AUDIO")) {   // testes: saida "nula" (o tempo anda, nada sai nas caixas)
            static ma_context nullCtx; static bool nullOk = false;
            if (!nullOk) { ma_backend b[1] = { ma_backend_null }; nullOk = ma_context_init(b, 1, NULL, &nullCtx) == MA_SUCCESS; }
            if (nullOk) cfg.pContext = &nullCtx;
        }
        if (ma_engine_init(&cfg, &Engine()) != MA_SUCCESS) {
            if (rate == 0) return false;
            cfg.sampleRate = 0;
            if (ma_engine_init(&cfg, &Engine()) != MA_SUCCESS) return false;
        }
        EngineOk() = true;
        BuildFx();
        BuildEq();
        RouteChain();
        return true;
    }

public:
    bool loaded = false;
    bool playing = false;

    Player() = default;
    ~Player() { Close(); }

    // Onde tocar: vazio = o que o sistema estiver usando.
    static std::wstring& OutDeviceName() { static std::wstring v; return v; }
    static std::vector<std::wstring> OutDevices() {
        std::vector<std::wstring> out;
        ma_context* c = OutCtx(); if (!c) return out;
        ma_device_info* pb = nullptr; ma_uint32 pn = 0; ma_device_info* cb = nullptr; ma_uint32 cn = 0;
        if (ma_context_get_devices(c, &pb, &pn, &cb, &cn) != MA_SUCCESS) return out;
        for (ma_uint32 i = 0; i < pn; ++i) out.push_back(Utf8ToWide(pb[i].name));
        return out;
    }
    // Nome do dispositivo que esta tocando agora (para mostrar na tela).
    static std::wstring OutDeviceAtual() {
        if (!OutDeviceName().empty()) return OutDeviceName();
        ma_device* d = EngineOk() ? ma_engine_get_device(&Engine()) : nullptr;
        if (d && d->playback.name[0]) return Utf8ToWide(d->playback.name);
        return L"";
    }
    static void GlobalInit() { if (!EngineOk()) InitEngine(0); }
    static bool HasAudio() { return EngineOk(); }   // dispositivo de som abriu?
    static ma_backend Backend() { ma_device* d = EngineOk() ? ma_engine_get_device(&Engine()) : nullptr; return (d && d->pContext) ? d->pContext->backend : ma_backend_null; }
    static const char* BackendName() { return EngineOk() ? ma_get_backend_name(Backend()) : "nenhum"; }
    // Sem nenhuma saida de som (fone/caixa desconectados, servico de audio parado) o miniaudio cai no
    // backend "Null": abre, o tempo da faixa anda e nada toca.
    static bool SilentOutput() { return EngineOk() && Backend() == ma_backend_null; }
    static void GlobalShutdown() {
        if (!EngineOk()) return;
        DestroyEq();
        DestroyFx();
        ma_engine_uninit(&Engine());
        EngineOk() = false;
    }
    static bool Available() { return EngineOk(); }
    static unsigned EngineRate() { return EngineOk() ? ma_engine_get_sample_rate(&Engine()) : 0; }

    // Reabre o dispositivo na taxa pedida (chamado com o som fechado).
    static void EnsureEngineRate(ma_uint32 rate) {
        if (!rate || !EngineOk()) return;
        if (ma_engine_get_sample_rate(&Engine()) == rate) return;
        DestroyEq();
        DestroyFx();
        ma_engine_uninit(&Engine());
        EngineOk() = false;
        InitEngine(rate);
    }

    // Som curto "dispara e esquece" (splash).
    static void PlayOneShot(const std::wstring& path) {
        if (!EngineOk() || path.empty()) return;
#ifdef _WIN32
        // ma_engine_play_sound so aceita char* (fopen ANSI): usa o nome curto 8.3 (so ASCII) quando
        // existe, porque pastas com letras fora do codepage (ex.: japones) nao abririam
        std::wstring use = path;
        { wchar_t sh[MAX_PATH]; DWORD k = GetShortPathNameW(path.c_str(), sh, MAX_PATH); if (k > 0 && k < MAX_PATH) use = sh; }
        int n = WideCharToMultiByte(CP_ACP, 0, use.c_str(), -1, NULL, 0, NULL, NULL);
        std::string a; if (n > 1) { a.resize((size_t)n - 1); WideCharToMultiByte(CP_ACP, 0, use.c_str(), -1, &a[0], n, NULL, NULL); }
        if (!a.empty()) ma_engine_play_sound(&Engine(), a.c_str(), NULL);
#else
        ma_engine_play_sound(&Engine(), WideToUtf8(path).c_str(), NULL);
#endif
    }

    // Equalizador: ganhos em dB (-12..12) por banda.
    static void SetEq(const int* gainsDb, bool on, ma_sound* current = nullptr) {
        Eq& e = EqState();
        for (int i = 0; i < 8; ++i) e.gains[i] = std::max(-12, std::min(12, gainsDb[i]));
        e.on = on;
        if (e.init && EngineOk()) {
            ma_uint32 ch = ma_engine_get_channels(&Engine()), sr = ma_engine_get_sample_rate(&Engine());
            for (int i = 0; i < 8; ++i) {
                float f = std::min(EqFreqs()[i], sr * 0.45f);
                ma_peak_config pc = ma_peak2_config_init(ma_format_f32, ch, sr, (double)e.gains[i], 1.0, (double)f);
                ma_peak_node_reinit(&pc, &e.node[i]);
            }
        }
        RouteChain();
        if (current) RouteSound(current);
    }
    void ApplyEq() { if (sndInit) RouteSound(&snd); }
    // Efeitos: niveis 0..3. Grave reconfigura o filtro; slow/speed mudam o pitch do som atual.
    static void SetFx(int slow, int speed, int reverb, int bass, int eightD) {
        auto c = [](int v) { return std::max(0, std::min(3, v)); };
        afx::L().slow = c(slow); afx::L().speed = c(speed); afx::L().reverb = c(reverb); afx::L().bass = c(bass); afx::L().eightD = c(eightD);
        Fx& f = FxState();
        if (f.init && EngineOk() && f.bassLv != c(bass)) {
            ma_uint32 ch = ma_engine_get_channels(&Engine()), sr = ma_engine_get_sample_rate(&Engine());
            ma_loshelf_config lc = ma_loshelf2_config_init(ma_format_f32, ch, sr, afx::BassDb(c(bass)), 1.0, 110.0);
            if (ma_loshelf_node_reinit(&lc, &f.bass) == MA_SUCCESS) f.bassLv = c(bass);
        }
    }
    void ApplyFx() { if (sndInit) ma_sound_set_pitch(&snd, afx::PitchNow()); }
    static unsigned OutputLatencyMs() {   // atraso da saida de som (para a onda andar junto com o que se ouve)
        if (!EngineOk()) return 0;
        ma_device* d = ma_engine_get_device(&Engine());
        if (!d || !d->playback.internalSampleRate) return 0;
        return (unsigned)((ma_uint64)d->playback.internalPeriodSizeInFrames * std::max<ma_uint32>(1, d->playback.internalPeriods) * 1000ULL / d->playback.internalSampleRate);
    }

    // Testa se o arquivo e decodificavel nativamente; devolve a taxa nativa.
    static bool ProbeNative(const std::wstring& path, unsigned* outRate = nullptr) {
        ma_decoder_config dc = ma_decoder_config_init(ma_format_unknown, 0, 0);
        ma_decoder d;
#ifdef _WIN32
        if (ma_decoder_init_file_w(path.c_str(), &dc, &d) != MA_SUCCESS) return false;   // fopen ANSI nao abre "Poça"
#else
        if (ma_decoder_init_file(WideToUtf8(path).c_str(), &dc, &d) != MA_SUCCESS) return false;
#endif
        if (outRate) *outRate = d.outputSampleRate;
        ma_decoder_uninit(&d);
        return true;
    }

    bool Open(const std::wstring& path) {
        Close();
        if (!EngineOk()) return false;
        unsigned nativeRate = 0;
        if (!ProbeNative(path, &nativeRate)) return false;
        EnsureEngineRate(nativeRate);
        if (!EngineOk()) return false;
        ma_uint32 flags = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION;   // com pitch: slow/speed
#ifdef _WIN32
        if (ma_sound_init_from_file_w(&Engine(), path.c_str(), flags, NULL, NULL, &snd) != MA_SUCCESS) return false;
#else
        std::string u8 = WideToUtf8(path);
        if (ma_sound_init_from_file(&Engine(), u8.c_str(), flags, NULL, NULL, &snd) != MA_SUCCESS) return false;
#endif
        sndInit = true;
        ma_format fmt; ma_uint32 ch = 0;
        sampleRate = 0;
        ma_sound_get_data_format(&snd, &fmt, &ch, &sampleRate, NULL, 0);
        if (!sampleRate) sampleRate = nativeRate ? nativeRate : 44100;
        ma_uint64 len = 0;
        cachedLenMs = 0;
        if (ma_sound_get_length_in_pcm_frames(&snd, &len) == MA_SUCCESS && len > 0)
            cachedLenMs = (DWORD)std::min<ma_uint64>(len * 1000ULL / sampleRate, 0xFFFFFFFFULL);
        ma_sound_set_volume(&snd, FxState().init ? 1.0f : curVol);   // com a cadeia, o volume fica no mestre
        ma_sound_set_pitch(&snd, afx::PitchNow());
        RouteSound(&snd);
        loaded = true;
        playing = false;
        openPath = path;
        return true;
    }

    // Musica online: toca o PcmStream (que precisa viver ate o Close).
    bool OpenStream(const std::shared_ptr<PcmStream>& sp, unsigned durSec) {
        Close();
        PcmStream* st = sp.get();
        if (!EngineOk() || !st) return false;
        EnsureEngineRate(st->rate);
        if (!EngineOk()) return false;
        streamDs = new PcmStreamDS();
        ma_data_source_config dc = ma_data_source_config_init();
        dc.vtable = PcmDSVtable();
        if (ma_data_source_init(&dc, &streamDs->base) != MA_SUCCESS) { delete streamDs; streamDs = nullptr; return false; }
        streamDs->st = st; streamDs->cursor = 0;
        if (durSec) st->lenFrames = (uint64_t)durSec * st->rate;
        ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
        if (ma_sound_init_from_data_source(&Engine(), (ma_data_source*)streamDs, flags, NULL, &snd) != MA_SUCCESS) {
            ma_data_source_uninit(&streamDs->base); delete streamDs; streamDs = nullptr; return false;
        }
        sndInit = true;
        sampleRate = st->rate;
        cachedLenMs = durSec * 1000;
        ma_sound_set_volume(&snd, FxState().init ? 1.0f : curVol);
        ma_sound_set_pitch(&snd, afx::PitchNow());
        RouteSound(&snd);
        loaded = true; playing = false;
        openPath = L"stream";
        streamKeep = sp;
        return true;
    }
    bool IsStream() const { return streamDs != nullptr; }
    std::shared_ptr<PcmStream> SharedStream() const { return streamKeep; }   // buffer vivo do streaming (para a analise de onda/espectro)
    void SetStreamLengthMs(DWORD ms) { if (streamDs) { cachedLenMs = ms; streamDs->st->lenFrames = (uint64_t)ms * streamDs->st->rate / 1000; } }

    void Close() {
        playing = false;
        if (sndInit) { ma_sound_stop(&snd); ma_sound_uninit(&snd); sndInit = false; }
        if (streamDs) { ma_data_source_uninit(&streamDs->base); delete streamDs; streamDs = nullptr; }
        streamKeep.reset();
        loaded = false;
        cachedLenMs = 0;
        sampleRate = 0;
        openPath.clear();
    }

    void Play() {
        if (!sndInit) return;
        if (ma_sound_start(&snd) == MA_SUCCESS) playing = true;
    }
    void Pause() {
        if (!sndInit) return;
        ma_sound_stop(&snd);
        playing = false;
    }
    void Restart() { if (!sndInit) return; SeekMs(0); Play(); }

    void SeekMs(DWORD ms) {
        if (!sndInit || !sampleRate) return;
        ma_uint64 frame = (ma_uint64)ms * sampleRate / 1000ULL;
        ma_sound_seek_to_pcm_frame(&snd, frame);
    }

    DWORD GetPositionMs() {
        if (!sndInit || !sampleRate) return 0;
        ma_uint64 c = 0;
        if (ma_sound_get_cursor_in_pcm_frames(&snd, &c) != MA_SUCCESS) return 0;
        return (DWORD)std::min<ma_uint64>(c * 1000ULL / sampleRate, 0xFFFFFFFFULL);
    }

    DWORD GetLengthMs() {
        if (sndInit && !cachedLenMs && sampleRate) {
            ma_uint64 len = 0;
            if (ma_sound_get_length_in_pcm_frames(&snd, &len) == MA_SUCCESS && len > 0)
                cachedLenMs = (DWORD)std::min<ma_uint64>(len * 1000ULL / sampleRate, 0xFFFFFFFFULL);
        }
        return cachedLenMs;
    }

    void SetVolume(int percent) {
        percent = std::max(0, std::min(100, percent));
        curVol = afx::VolumeGain(percent);   // curva cubica (ver audio_fx.h)
        VolTarget() = curVol;
        if (FxState().init) FxState().master.target = curVol;   // o mestre chega la suave (e sobe no maximo 40 dB/s)
        else if (sndInit) ma_sound_set_volume(&snd, curVol);
    }

    bool ReachedEnd() {
        if (!sndInit) return false;
        if (ma_sound_at_end(&snd)) { playing = false; return true; }
        if (!playing) return false;
        DWORD len = GetLengthMs();
        if (len < 1000) return false;
        DWORD pos = GetPositionMs();
        return pos >= len - std::min<DWORD>(50, len / 20);
    }

    // Decodifica o arquivo INTEIRO em mono f32 na taxa nativa, entregando
    // blocos ao sink(frames, count, sampleRate). O sink devolve false para
    // cancelar. Usado pela analise de onda/espectro em thread de fundo.
    template <class Sink>
    static bool DecodeMono(const std::wstring& path, Sink&& sink) {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
        ma_decoder dec;
#ifdef _WIN32
        if (ma_decoder_init_file_w(path.c_str(), &cfg, &dec) != MA_SUCCESS) return false;
#else
        if (ma_decoder_init_file(WideToUtf8(path).c_str(), &cfg, &dec) != MA_SUCCESS) return false;
#endif
        ma_uint32 sr = dec.outputSampleRate ? dec.outputSampleRate : 44100;
        std::vector<float> buf(8192);
        bool any = false;
        while (true) {
            ma_uint64 got = 0;
            ma_result r = ma_decoder_read_pcm_frames(&dec, buf.data(), buf.size(), &got);
            if (got > 0) { any = true; if (!sink(buf.data(), (size_t)got, sr)) break; }
            if (r != MA_SUCCESS || got == 0) break;
        }
        ma_decoder_uninit(&dec);
        return any;
    }
};
