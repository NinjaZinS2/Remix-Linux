#pragma once
// Soundpad (so no PC): botoes de som que tocam no MICROFONE, para o Discord, jogos e chamadas ouvirem.
//
//  - Linux: o Remix cria um microfone virtual no PipeWire/PulseAudio (pactl): uma saida nula "Remix Soundpad"
//    e uma entrada "Remix Microfone" (o monitor dela). No Discord/jogo, escolha "Remix Microfone" como
//    microfone. Ao desligar (ou fechar o Remix) os dois somem.
//  - Windows: criar microfone exige driver, entao usa um cabo virtual ja instalado (VB-CABLE, VoiceMeeter):
//    o Remix toca em "CABLE Input" e no Discord/jogo escolhe-se "CABLE Output" como microfone.
//  - "Minha voz": captura o microfone de verdade e mistura com os sons (senao quem ouve so escuta os sons).
//    Nunca captura o proprio microfone virtual (seria eco infinito), mesmo se ele for o padrao do sistema.
//  - "Ouvir no fone": toca os sons tambem na saida normal, para voce saber o que esta tocando.
//  - Os sons ficam copiados em <pasta do Remix>/soundpad (o original pode sumir). Formatos que o miniaudio nao
//    abre (m4a, opus, wma...) sao convertidos para WAV com o ffmpeg na hora de adicionar.
#include "player_ma.h"
#include "online_resolve.h"
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>
#include <filesystem>

namespace spad {

struct Sound { std::wstring id, name, file; int vol = 100; };
struct Playing { ma_sound mic{}, mon{}; bool micOk = false, monOk = false; std::wstring id; float lenSec = 0; };

// Voz de verdade: o dispositivo de captura escreve num buffer circular e esta fonte le dali para o motor do
// microfone virtual (sem nunca "acabar"). Se o buffer encher demais (relogios diferentes), pula o excesso.
struct VoiceDS { ma_data_source_base base; ma_pcm_rb* rb = nullptr; };
inline ma_result VoiceRead(ma_data_source* ds, void* out, ma_uint64 frames, ma_uint64* read) {
    VoiceDS* v = (VoiceDS*)ds; float* o = (float*)out; ma_uint64 got = 0;
    if (v->rb) {
        ma_uint32 avail = ma_pcm_rb_available_read(v->rb);
        if (avail > 48000 / 4) ma_pcm_rb_seek_read(v->rb, avail - 48000 / 20);   // mais de 250 ms atrasado: volta para 50 ms
        while (got < frames) {
            ma_uint32 want = (ma_uint32)std::min<ma_uint64>(frames - got, 4096); void* buf = nullptr;
            if (ma_pcm_rb_acquire_read(v->rb, &want, &buf) != MA_SUCCESS || want == 0) break;
            memcpy(o + got * 2, buf, (size_t)want * 2 * sizeof(float));
            ma_pcm_rb_commit_read(v->rb, want); got += want;
        }
    }
    if (got < frames) memset(o + got * 2, 0, (size_t)(frames - got) * 2 * sizeof(float));
    if (read) *read = frames;
    return MA_SUCCESS;
}
inline ma_result VoiceSeek(ma_data_source*, ma_uint64) { return MA_SUCCESS; }
inline ma_result VoiceFormat(ma_data_source*, ma_format* f, ma_uint32* ch, ma_uint32* sr, ma_channel* map, size_t cap) {
    if (f) *f = ma_format_f32;
    if (ch) *ch = 2;
    if (sr) *sr = 48000;
    if (map && cap >= 2) { map[0] = MA_CHANNEL_FRONT_LEFT; map[1] = MA_CHANNEL_FRONT_RIGHT; }
    return MA_SUCCESS;
}
inline ma_result VoiceCursor(ma_data_source*, ma_uint64* c) { *c = 0; return MA_SUCCESS; }
inline ma_result VoiceLength(ma_data_source*, ma_uint64* l) { *l = 0; return MA_NOT_IMPLEMENTED; }
inline ma_data_source_vtable* VoiceVt() { static ma_data_source_vtable v = { VoiceRead, VoiceSeek, VoiceFormat, VoiceCursor, VoiceLength, nullptr, 0 }; return &v; }

struct State {
    std::mutex m;
    std::vector<Sound> sounds; bool loaded = false;
    bool on = false, voice = true, monitor = true; int master = 100;
    std::wstring outName, inName;           // vazio = automatico
    ma_context ctx{}; bool ctxOk = false;
    ma_engine mic{}; bool micOk = false;
    ma_engine mon{}; bool monOk = false;
    ma_device cap{}; bool capOk = false; ma_pcm_rb rb{}; bool rbOk = false;
    VoiceDS vds{}; ma_sound voiceSnd{}; bool voiceOk = false;
    std::vector<std::unique_ptr<Playing>> playing;
    std::wstring status, device, inDevice;  // texto para a tela
    std::atomic<bool> busy{ false };        // ligando/desligando em segundo plano
    std::atomic<unsigned> version{ 0 };
};
inline State& S() { static State* s = new State(); return *s; }

// ---- arquivos ------------------------------------------------------------------------------
inline std::wstring Dir() { return Config::Join(Config::BaseDir(), L"soundpad"); }
inline std::wstring IniPath() { return Config::Join(Dir(), L"soundpad.ini"); }
inline void SaveLocked() {
    State& s = S(); std::error_code ec; std::filesystem::create_directories(std::filesystem::path(Dir()), ec);
    std::string o = "# Remix Soundpad\n";
    o += "ligado=" + std::string(s.on ? "1" : "0") + "\nvoz=" + (s.voice ? "1" : "0") + "\nfone=" + (s.monitor ? "1" : "0") + "\nvolume=" + std::to_string(s.master) + "\n";
    o += "saida=" + WideToUtf8(s.outName) + "\nentrada=" + WideToUtf8(s.inName) + "\n";
    for (auto& x : s.sounds) o += "som=" + WideToUtf8(x.id) + "|" + std::to_string(x.vol) + "|" + WideToUtf8(std::filesystem::path(x.file).filename().wstring()) + "|" + WideToUtf8(x.name) + "\n";
    std::wstring tmp = IniPath() + L".tmp";
    { std::ofstream f(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc); f << o; }
    std::filesystem::rename(std::filesystem::path(tmp), std::filesystem::path(IniPath()), ec);
    s.version++;
}
inline void Load() {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m);
    if (s.loaded) return;
    s.loaded = true;
    std::ifstream f(std::filesystem::path(IniPath()), std::ios::binary); if (!f) return;
    std::string ln;
    while (std::getline(f, ln)) {
        while (!ln.empty() && (ln.back() == '\r' || ln.back() == ' ')) ln.pop_back();
        size_t eq = ln.find('='); if (eq == std::string::npos || ln[0] == '#') continue;
        std::string k = ln.substr(0, eq), v = ln.substr(eq + 1);
        if (k == "ligado") s.on = v == "1";
        else if (k == "voz") s.voice = v != "0";
        else if (k == "fone") s.monitor = v != "0";
        else if (k == "volume") s.master = std::max(0, std::min(100, atoi(v.c_str())));
        else if (k == "saida") s.outName = Utf8ToWide(v);
        else if (k == "entrada") s.inName = Utf8ToWide(v);
        else if (k == "som" && s.sounds.size() < 500) {
            std::vector<std::string> p; size_t a = 0; for (int i = 0; i < 3; ++i) { size_t b = v.find('|', a); if (b == std::string::npos) break; p.push_back(v.substr(a, b - a)); a = b + 1; } p.push_back(v.substr(a));
            if (p.size() < 4) continue;
            std::wstring fn = Utf8ToWide(p[2]);
            if (fn.empty() || fn.find(L'/') != std::wstring::npos || fn.find(L'\\') != std::wstring::npos || fn.find(L"..") != std::wstring::npos) continue;   // so arquivo da pasta do soundpad
            Sound x; x.id = Utf8ToWide(p[0]); x.vol = std::max(5, std::min(100, atoi(p[1].c_str()))); x.file = Config::Join(Dir(), fn); x.name = Utf8ToWide(p[3]);
            std::error_code ec; if (std::filesystem::exists(std::filesystem::path(x.file), ec)) s.sounds.push_back(x);
        }
    }
}

// ---- microfone virtual -----------------------------------------------------------------------
#ifndef _WIN32
inline int LoadModule(const std::vector<std::wstring>& args) {
    std::vector<std::wstring> a = { L"pactl", L"load-module" }; a.insert(a.end(), args.begin(), args.end());
    CapResult r = RunCapture(a, 8000);
    if (r.code != 0) return -1;
    return atoi(r.out.c_str());
}
inline void FindModules(int& sink, int& src) {   // de uma execucao anterior (ex.: o Remix fechou sem desligar)
    sink = src = -1;
    CapResult r = RunCapture({ L"pactl", L"list", L"short", L"modules" }, 8000);
    size_t p = 0;
    while (p < r.out.size()) {
        size_t e = r.out.find('\n', p); std::string ln = r.out.substr(p, e == std::string::npos ? std::string::npos : e - p); p = e == std::string::npos ? r.out.size() : e + 1;
        if (ln.find("sink_name=remix_soundpad") != std::string::npos && ln.find("module-null-sink") != std::string::npos) sink = atoi(ln.c_str());
        if (ln.find("source_name=remix_mic") != std::string::npos && ln.find("module-remap-source") != std::string::npos) src = atoi(ln.c_str());
    }
}
inline bool CreateVirtualMic(std::wstring& err) {
    CapResult info = RunCapture({ L"pactl", L"info" }, 5000);
    if (!info.started || info.code != 0) { err = L"Não achei o pactl (PipeWire/PulseAudio). Instale o pacote pulseaudio-utils (ou pipewire-pulseaudio)."; return false; }
    int sink = -1, src = -1; FindModules(sink, src);
    // aspas: o pactl junta os argumentos num texto so; o valor com espaco precisa de ' por fora e " por dentro
    if (sink < 0) sink = LoadModule({ L"module-null-sink", L"sink_name=remix_soundpad", L"sink_properties='device.description=\"Remix Soundpad\"'" });
    if (sink < 0) { err = L"O PipeWire/PulseAudio recusou criar a saída do Soundpad."; return false; }
    if (src < 0) src = LoadModule({ L"module-remap-source", L"master=remix_soundpad.monitor", L"source_name=remix_mic", L"source_properties='device.description=\"Remix Microfone\"'" });
    if (src < 0) { err = L"O PipeWire/PulseAudio recusou criar o \"Remix Microfone\"."; return false; }
    return true;
}
inline void RemoveVirtualMic() {
    int a = -1, b = -1; FindModules(a, b);
    if (b >= 0) RunCapture({ L"pactl", L"unload-module", std::to_wstring(b) }, 5000);
    if (a >= 0) RunCapture({ L"pactl", L"unload-module", std::to_wstring(a) }, 5000);
}
#endif

inline std::string LowerA(std::string x) { for (auto& c : x) c = (char)tolower((unsigned char)c); return x; }
// Dispositivo que nao pode virar "minha voz": o proprio microfone virtual (ou a saida dele) = eco infinito.
inline bool LoopInput(const ma_device_info& d) {
    std::string l = LowerA(d.name);
#ifdef _WIN32
    return l.find("cable output") != std::string::npos || l.find("vb-audio") != std::string::npos || l.find("voicemeeter out") != std::string::npos;
#else
    std::string id = d.id.pulse;
    return id == "remix_mic" || (id.size() > 8 && id.compare(id.size() - 8, 8, ".monitor") == 0) || l == "remix microfone";
#endif
}
inline bool AutoOutput(const ma_device_info& d) {
    std::string l = LowerA(d.name);
#ifdef _WIN32
    return l.find("cable input") != std::string::npos || l.find("vb-audio") != std::string::npos || l.find("voicemeeter input") != std::string::npos;
#else
    return strcmp(d.id.pulse, "remix_soundpad") == 0 || l == "remix soundpad";
#endif
}
inline void DeviceNames(bool capture, std::vector<std::wstring>& out) {   // com S().m
    State& s = S(); out.clear();
    if (!s.ctxOk) s.ctxOk = ma_context_init(nullptr, 0, nullptr, &s.ctx) == MA_SUCCESS;
    ma_device_info* pb = nullptr; ma_uint32 pn = 0; ma_device_info* cb = nullptr; ma_uint32 cn = 0;
    if (!s.ctxOk || ma_context_get_devices(&s.ctx, &pb, &pn, &cb, &cn) != MA_SUCCESS) return;
    if (capture) { for (ma_uint32 i = 0; i < cn; ++i) if (!LoopInput(cb[i])) out.push_back(Utf8ToWide(cb[i].name)); }
    else for (ma_uint32 i = 0; i < pn; ++i) out.push_back(Utf8ToWide(pb[i].name));
}
// Proximo nome da lista (vazio = automatico -> 1o -> ... -> automatico).
inline std::wstring NextName(const std::vector<std::wstring>& v, const std::wstring& cur) {
    if (v.empty()) return L"";
    if (cur.empty()) return v[0];
    for (size_t i = 0; i < v.size(); ++i) if (v[i] == cur) return i + 1 < v.size() ? v[i + 1] : L"";
    return L"";
}
inline bool PickDevice(bool capture, ma_device_id& id, std::wstring& name) {   // com S().m
    State& s = S();
    ma_device_info* pb = nullptr; ma_uint32 pn = 0; ma_device_info* cb = nullptr; ma_uint32 cn = 0;
    if (!s.ctxOk || ma_context_get_devices(&s.ctx, &pb, &pn, &cb, &cn) != MA_SUCCESS) return false;
    ma_device_info* d = capture ? cb : pb; ma_uint32 n = capture ? cn : pn;
    const std::wstring& want = capture ? s.inName : s.outName;
    if (!want.empty()) { for (ma_uint32 i = 0; i < n; ++i) if (Utf8ToWide(d[i].name) == want && !(capture && LoopInput(d[i]))) { id = d[i].id; name = want; return true; } return false; }
    if (!capture) { for (ma_uint32 i = 0; i < n; ++i) if (AutoOutput(d[i])) { id = d[i].id; name = Utf8ToWide(d[i].name); return true; } return false; }
    for (ma_uint32 i = 0; i < n; ++i) if (d[i].isDefault && !LoopInput(d[i])) { id = d[i].id; name = Utf8ToWide(d[i].name); return true; }
    for (ma_uint32 i = 0; i < n; ++i) if (!LoopInput(d[i])) { id = d[i].id; name = Utf8ToWide(d[i].name); return true; }   // o padrao e o virtual: usa o 1o de verdade
    return false;
}

inline void CaptureCb(ma_device* d, void*, const void* in, ma_uint32 frames) {
    State* s = (State*)d->pUserData; if (!in || !s->rbOk) return;
    const float* src = (const float*)in; ma_uint32 done = 0;
    while (done < frames) {
        ma_uint32 want = frames - done; void* buf = nullptr;
        if (ma_pcm_rb_acquire_write(&s->rb, &want, &buf) != MA_SUCCESS || want == 0) break;   // cheio: descarta
        memcpy(buf, src + done * 2, (size_t)want * 2 * sizeof(float));
        ma_pcm_rb_commit_write(&s->rb, want); done += want;
    }
}
inline void UninitPlaying(Playing& p) {
    if (p.micOk) { ma_sound_stop(&p.mic); ma_sound_uninit(&p.mic); p.micOk = false; }
    if (p.monOk) { ma_sound_stop(&p.mon); ma_sound_uninit(&p.mon); p.monOk = false; }
}
inline void StopAllLocked() { State& s = S(); for (auto& p : s.playing) UninitPlaying(*p); s.playing.clear(); s.version++; }
inline void CloseVoiceLocked() {
    State& s = S();
    if (s.voiceOk) { ma_sound_stop(&s.voiceSnd); ma_sound_uninit(&s.voiceSnd); s.voiceOk = false; }
    if (s.capOk) { ma_device_uninit(&s.cap); s.capOk = false; }
    if (s.rbOk) { ma_pcm_rb_uninit(&s.rb); s.rbOk = false; }
    s.inDevice.clear();
}
inline bool OpenVoiceLocked(std::wstring& err) {
    State& s = S();
    CloseVoiceLocked();
    if (!s.micOk) return false;
    ma_device_id id; std::wstring name;
    if (!PickDevice(true, id, name)) { err = s.inName.empty() ? L"Nenhum microfone de verdade encontrado (a voz não vai junto)." : L"O microfone escolhido sumiu: toque em MICROFONE para trocar."; return false; }
    if (ma_pcm_rb_init(ma_format_f32, 2, 48000, nullptr, nullptr, &s.rb) != MA_SUCCESS) { err = L"Sem memória para a voz."; return false; }
    s.rbOk = true;
    ma_device_config dc = ma_device_config_init(ma_device_type_capture);
    dc.capture.pDeviceID = &id; dc.capture.format = ma_format_f32; dc.capture.channels = 2; dc.sampleRate = 48000;
    dc.dataCallback = CaptureCb; dc.pUserData = &s;
    if (ma_device_init(&s.ctx, &dc, &s.cap) != MA_SUCCESS) { err = L"Não consegui abrir o microfone \"" + name + L"\" (a voz não vai junto)."; CloseVoiceLocked(); return false; }
    s.capOk = true;
    ma_data_source_config c = ma_data_source_config_init(); c.vtable = VoiceVt();
    ma_data_source_init(&c, &s.vds.base); s.vds.rb = &s.rb;
    if (ma_sound_init_from_data_source(&s.mic, &s.vds, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &s.voiceSnd) != MA_SUCCESS) { err = L"Não consegui misturar a voz."; CloseVoiceLocked(); return false; }
    s.voiceOk = true; s.inDevice = name;
    ma_device_start(&s.cap); ma_sound_start(&s.voiceSnd);
    return true;
}
inline bool Running() { State& s = S(); std::lock_guard<std::mutex> lk(s.m); return s.micOk; }

// Liga o microfone virtual (motores de audio + captura da voz). Bloqueia (pactl): a UI usa SetOnAsync.
inline bool Start(std::wstring& err) {
    State& s = S();
    bool manualOut;
    { std::lock_guard<std::mutex> lk(s.m); if (s.micOk) return true; manualOut = !s.outName.empty(); }
#ifndef _WIN32
    if (!manualOut && !CreateVirtualMic(err)) return false;   // fora da trava: o pactl leva alguns ms
#endif
    std::lock_guard<std::mutex> lk(s.m);
    if (s.micOk) return true;
    if (s.monOk) { for (auto& p : s.playing) UninitPlaying(*p); s.playing.clear(); ma_engine_uninit(&s.mon); s.monOk = false; }   // teste no fone com o microfone desligado
    if (s.ctxOk) { ma_context_uninit(&s.ctx); s.ctxOk = false; }   // contexto novo enxerga a saida recem-criada
    s.ctxOk = ma_context_init(nullptr, 0, nullptr, &s.ctx) == MA_SUCCESS;
    if (!s.ctxOk) { err = L"Não consegui abrir o sistema de áudio."; return false; }
    ma_device_id id; std::wstring name;
    if (!PickDevice(false, id, name)) {
#ifdef _WIN32
        err = manualOut ? L"A saída escolhida não existe mais: toque em SAÍDA para trocar." : L"Nenhum cabo de áudio virtual encontrado. Instale o VB-CABLE (vb-audio.com/Cable) e escolha \"CABLE Output\" como microfone no Discord/jogo.";
#else
        err = manualOut ? L"A saída escolhida não existe mais: toque em SAÍDA para trocar." : L"A saída \"Remix Soundpad\" não apareceu no sistema de áudio.";
#endif
        return false;
    }
    ma_engine_config ec = ma_engine_config_init();
    ec.pContext = &s.ctx; ec.pPlaybackDeviceID = &id; ec.channels = 2; ec.sampleRate = 48000;
    if (ma_engine_init(&ec, &s.mic) != MA_SUCCESS) { err = L"Não consegui abrir \"" + name + L"\"."; return false; }
    s.micOk = true; s.device = name;
    ma_engine_set_volume(&s.mic, afx::VolumeGain(s.master));
    ma_engine_config mc = ma_engine_config_init(); mc.pContext = &s.ctx;
    s.monOk = !std::getenv("REMIX_NULL_AUDIO") && ma_engine_init(&mc, &s.mon) == MA_SUCCESS;   // testes: nada nas caixas
    if (s.monOk) ma_engine_set_volume(&s.mon, afx::VolumeGain(s.master));
    s.status.clear();
    if (s.voice) { std::wstring e2; if (!OpenVoiceLocked(e2)) s.status = e2; }
    s.version++;
    return true;
}
inline void Stop(bool removeDevice) {
    State& s = S(); bool manualOut;
    {
        std::lock_guard<std::mutex> lk(s.m);
        StopAllLocked(); CloseVoiceLocked();
        if (s.monOk) { ma_engine_uninit(&s.mon); s.monOk = false; }
        if (s.micOk) { ma_engine_uninit(&s.mic); s.micOk = false; }
        s.device.clear(); manualOut = !s.outName.empty(); s.version++;
    }
#ifndef _WIN32
    if (removeDevice && !manualOut) RemoveVirtualMic();
#else
    (void)removeDevice; (void)manualOut;
#endif
}
// UI: liga/desliga sem travar a tela; onDone roda na thread de fundo (use AppPost para redesenhar).
inline void SetOnAsync(bool on, std::function<void()> onDone) {
    State& s = S();
    if (s.busy.exchange(true)) return;
    { std::lock_guard<std::mutex> lk(s.m); s.on = on; SaveLocked(); }
    std::thread([on, onDone] {
        State& st = S(); std::wstring err;
        if (on) { if (!Start(err)) { std::lock_guard<std::mutex> lk(st.m); st.status = err; st.on = false; SaveLocked(); } }
        else { Stop(true); std::lock_guard<std::mutex> lk(st.m); st.status.clear(); }
        st.busy = false; st.version++;
        if (onDone) onDone();
    }).detach();
}
// Troca a saida/entrada (reinicia se estiver ligado).
inline void CycleDevice(bool capture, std::function<void()> onDone) {
    State& s = S();
    if (s.busy.load()) return;
    bool wasOn;
    {
        std::lock_guard<std::mutex> lk(s.m);
        std::vector<std::wstring> v; DeviceNames(capture, v);
        std::wstring& cur = capture ? s.inName : s.outName;
        cur = NextName(v, cur); wasOn = s.micOk; SaveLocked();
        if (capture) {   // so a voz muda: reabre a captura na hora
            if (s.micOk && s.voice) { std::wstring e; if (!OpenVoiceLocked(e)) s.status = e; else s.status.clear(); }
            if (onDone) onDone();
            return;
        }
    }
    if (!wasOn) { if (onDone) onDone(); return; }
    s.busy = true;
    std::thread([onDone] {
        State& st = S(); std::wstring err;
        Stop(true);
        if (!Start(err)) { std::lock_guard<std::mutex> lk(st.m); st.status = err; st.on = false; SaveLocked(); }
        st.busy = false; st.version++;
        if (onDone) onDone();
    }).detach();
}
inline void SetVoice(bool on) {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m); s.voice = on; SaveLocked();
    if (!s.micOk) return;
    if (on) { std::wstring e; if (!OpenVoiceLocked(e)) s.status = e; else s.status.clear(); }
    else CloseVoiceLocked();
}
inline void SetMonitor(bool on) { State& s = S(); std::lock_guard<std::mutex> lk(s.m); s.monitor = on; SaveLocked(); }
inline void SetMaster(int v) {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m); s.master = std::max(0, std::min(100, v)); SaveLocked();
    if (s.micOk) ma_engine_set_volume(&s.mic, afx::VolumeGain(s.master));
    if (s.monOk) ma_engine_set_volume(&s.mon, afx::VolumeGain(s.master));
}

// Toca (ou para, se ja estiver tocando) um som. Com o microfone desligado, toca so no fone (para testar).
inline bool Toggle(const std::wstring& id, std::wstring& err) {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m);
    for (size_t i = 0; i < s.playing.size(); ++i) if (s.playing[i]->id == id) { UninitPlaying(*s.playing[i]); s.playing.erase(s.playing.begin() + (long)i); s.version++; return true; }
    const Sound* snd = nullptr; for (auto& x : s.sounds) if (x.id == id) snd = &x;
    if (!snd) return false;
    if (!s.micOk && !s.monOk && !std::getenv("REMIX_NULL_AUDIO")) {   // desligado: abre so a saida normal para ouvir o som
        if (!s.ctxOk) s.ctxOk = ma_context_init(nullptr, 0, nullptr, &s.ctx) == MA_SUCCESS;
        ma_engine_config mc = ma_engine_config_init(); mc.pContext = s.ctxOk ? &s.ctx : nullptr;
        s.monOk = ma_engine_init(&mc, &s.mon) == MA_SUCCESS;
        if (s.monOk) ma_engine_set_volume(&s.mon, afx::VolumeGain(s.master));
    }
    auto p = std::make_unique<Playing>(); p->id = id;
    ma_uint32 fl = MA_SOUND_FLAG_STREAM | MA_SOUND_FLAG_NO_SPATIALIZATION | MA_SOUND_FLAG_NO_PITCH;
#ifdef _WIN32
    auto init = [&](ma_engine* e, ma_sound* so) { return ma_sound_init_from_file_w(e, snd->file.c_str(), fl, nullptr, nullptr, so) == MA_SUCCESS; };
#else
    std::string u8 = WideToUtf8(snd->file);
    auto init = [&](ma_engine* e, ma_sound* so) { return ma_sound_init_from_file(e, u8.c_str(), fl, nullptr, nullptr, so) == MA_SUCCESS; };
#endif
    if (s.micOk) p->micOk = init(&s.mic, &p->mic);
    if ((s.monitor || !s.micOk) && s.monOk) p->monOk = init(&s.mon, &p->mon);
    if (!p->micOk && !p->monOk) { err = L"Não consegui abrir esse som."; return false; }
    float g = afx::VolumeGain(snd->vol), len = 0;
    if (p->micOk) { ma_sound_set_volume(&p->mic, g); ma_sound_get_length_in_seconds(&p->mic, &len); ma_sound_start(&p->mic); }
    if (p->monOk) { ma_sound_set_volume(&p->mon, g); if (len <= 0) ma_sound_get_length_in_seconds(&p->mon, &len); ma_sound_start(&p->mon); }
    p->lenSec = len;
    s.playing.push_back(std::move(p)); s.version++;
    return true;
}
// 0..1 do som tocando; -1 se nao esta tocando.
inline float Progress(const std::wstring& id) {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m);
    for (auto& p : s.playing) if (p->id == id) {
        if (p->lenSec <= 0) return 0.0f;
        float c = 0; ma_sound_get_cursor_in_seconds(p->micOk ? &p->mic : &p->mon, &c);
        return std::max(0.0f, std::min(1.0f, c / p->lenSec));
    }
    return -1.0f;
}
inline int PlayingCount() { State& s = S(); std::lock_guard<std::mutex> lk(s.m); return (int)s.playing.size(); }
inline void StopAll() { State& s = S(); std::lock_guard<std::mutex> lk(s.m); StopAllLocked(); }
// Chamado pela UI a cada quadro: solta os sons que acabaram (true = mudou algo).
inline bool Tick() {
    State& s = S(); std::unique_lock<std::mutex> lk(s.m, std::try_to_lock); if (!lk.owns_lock()) return false;
    bool changed = false;
    for (size_t i = 0; i < s.playing.size();) {
        auto& p = s.playing[i];
        bool end = (!p->micOk || ma_sound_at_end(&p->mic)) && (!p->monOk || ma_sound_at_end(&p->mon));
        if (end) { UninitPlaying(*p); s.playing.erase(s.playing.begin() + (long)i); changed = true; }
        else ++i;
    }
    if (changed) s.version++;
    return changed;
}

// Adiciona arquivos (copia para a pasta do Soundpad; converte o que o miniaudio nao abre). Bloqueia.
inline int AddFiles(const std::vector<std::wstring>& files, std::wstring& err) {
    namespace fs = std::filesystem; std::error_code ec;
    fs::create_directories(fs::path(Dir()), ec);
    int added = 0;
    for (auto& f : files) {
        { std::lock_guard<std::mutex> lk(S().m); if (S().sounds.size() >= 500) { err = L"Limite de 500 sons."; break; } }
        fs::path src(f); if (!fs::is_regular_file(src, ec)) continue;
        if (fs::file_size(src, ec) > (uintmax_t)200 * 1024 * 1024) { err = L"Arquivo grande demais para o Soundpad (máx. 200 MB): " + src.filename().wstring(); continue; }
        std::wstring stem = src.stem().wstring(), ext = src.extension().wstring();
        for (auto& c : ext) c = (wchar_t)towlower(c);
        if (ext.size() > 6) ext = L".bin";
        unsigned long long h = 1469598103934665603ULL;
        std::wstring seed = f + L"|" + std::to_wstring((unsigned long long)fs::file_size(src, ec)) + L"|" + std::to_wstring(GetTickCount64()) + L"|" + std::to_wstring(added);
        for (wchar_t c : seed) { h ^= (unsigned long long)c; h *= 1099511628211ULL; }
        wchar_t idb[20]; swprintf(idb, 20, L"%012llx", h & 0xffffffffffffULL);
        std::wstring id = idb;
        std::wstring dst = Config::Join(Dir(), id + ext);
        fs::copy_file(src, fs::path(dst), fs::copy_options::overwrite_existing, ec);
        if (ec) { err = L"Não consegui copiar " + src.filename().wstring(); ec.clear(); continue; }
        if (!Player::ProbeNative(dst)) {   // m4a/opus/wma...: vira WAV
            fonte::Garantir();
            std::wstring wav = Config::Join(Dir(), id + L".wav");
            CapResult r = fonte::FfmpegOk() ? RunCapture({ fonte::Ffmpeg(), L"-nostdin", L"-loglevel", L"error", L"-y", L"-i", dst, L"-vn", L"-ac", L"2", L"-ar", L"48000", wav }, 120000) : CapResult();
            fs::remove(fs::path(dst), ec);
            if (r.code != 0 || !fs::exists(fs::path(wav), ec)) { err = (fonte::FfmpegOk() ? L"Não consegui ler: " : L"Formato não suportado (instale o ffmpeg): ") + src.filename().wstring(); fs::remove(fs::path(wav), ec); continue; }
            dst = wav;
        }
        Sound x; x.id = id; x.name = stem.size() > 40 ? stem.substr(0, 40) : stem; x.file = dst;
        { std::lock_guard<std::mutex> lk(S().m); S().sounds.push_back(x); SaveLocked(); }
        ++added;
    }
    return added;
}
inline void Remove(const std::wstring& id) {
    State& s = S(); std::wstring file;
    {
        std::lock_guard<std::mutex> lk(s.m);
        for (size_t i = 0; i < s.playing.size(); ++i) if (s.playing[i]->id == id) { UninitPlaying(*s.playing[i]); s.playing.erase(s.playing.begin() + (long)i); break; }
        for (size_t i = 0; i < s.sounds.size(); ++i) if (s.sounds[i].id == id) { file = s.sounds[i].file; s.sounds.erase(s.sounds.begin() + (long)i); break; }
        SaveLocked();
    }
    std::error_code ec; if (!file.empty()) std::filesystem::remove(std::filesystem::path(file), ec);
}
// Volume de um som: 100 -> 75 -> 50 -> 25 -> 100 (aplica tambem no que ja esta tocando).
inline int CycleSoundVolume(const std::wstring& id) {
    State& s = S(); std::lock_guard<std::mutex> lk(s.m); int nv = 100;
    for (auto& x : s.sounds) if (x.id == id) { nv = x.vol > 75 ? 75 : x.vol > 50 ? 50 : x.vol > 25 ? 25 : 100; x.vol = nv; }
    for (auto& p : s.playing) if (p->id == id) { float g = afx::VolumeGain(nv); if (p->micOk) ma_sound_set_volume(&p->mic, g); if (p->monOk) ma_sound_set_volume(&p->mon, g); }
    SaveLocked();
    return nv;
}
// Copia para a UI (sem segurar a trava enquanto desenha).
struct View {
    std::vector<Sound> sounds; std::vector<std::wstring> playing;
    bool on = false, running = false, voice = true, monitor = true, voiceOk = false, busy = false; int master = 100;
    std::wstring outName, inName, device, inDevice, status;
};
inline View GetView() {
    State& s = S(); View v; std::lock_guard<std::mutex> lk(s.m);
    v.sounds = s.sounds; for (auto& p : s.playing) v.playing.push_back(p->id);
    v.on = s.on; v.running = s.micOk; v.voice = s.voice; v.monitor = s.monitor; v.voiceOk = s.voiceOk; v.busy = s.busy.load(); v.master = s.master;
    v.outName = s.outName; v.inName = s.inName; v.device = s.device; v.inDevice = s.inDevice; v.status = s.status;
    return v;
}

} // namespace spad
