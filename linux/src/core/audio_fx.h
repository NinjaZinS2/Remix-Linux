#pragma once
// Efeitos de audio do player (PC) e o volume "seguro". Tudo roda na thread de audio do miniaudio,
// entre o som e a saida:
//
//   som (pitch = slow/speed) -> grave (low-shelf) -> reverb -> 8D -> equalizador -> mestre -> saida
//
//  - Volume: curva cubica (a mesma dos controles do PipeWire/Pulse): 100% = 0 dB, 80% = -5,8 dB,
//    50% = -18 dB, 10% = -60 dB. Antes era linear e 3% ja era -30 dB: subir para 100% dava +30 dB
//    de uma vez. O mestre sobe no maximo 40 dB por segundo (descer e rapido) e tem um limitador que
//    nunca deixa passar de -0,3 dBFS, entao grave/reverb/equalizador nao "estouram" o som.
//  - Slow/speed mudam a velocidade junto com o tom (estilo "slowed"/"sped up"): ma_sound_set_pitch.
//  - Niveis 0 (desligado) a 3. Os parametros sao atomicos: a UI muda, o audio le no proximo bloco.
#include "audio/miniaudio.h"
#include <atomic>
#include <vector>
#include <cmath>
#include <cstring>
#include <algorithm>

namespace afx {

struct Levels { std::atomic<int> slow{ 0 }, speed{ 0 }, reverb{ 0 }, bass{ 0 }, eightD{ 0 }; };
inline Levels& L() { static Levels* l = new Levels(); return *l; }

// Velocidade (e tom) do som conforme slow/speed: slow e speed nao somam, o ultimo escolhido vale.
inline float PitchNow() {
    static const float slow[4] = { 1.0f, 0.90f, 0.82f, 0.75f }, fast[4] = { 1.0f, 1.10f, 1.20f, 1.30f };
    int s = std::max(0, std::min(3, L().slow.load())), f = std::max(0, std::min(3, L().speed.load()));
    if (s) return slow[s];
    return fast[f];
}
inline double BassDb(int lv) { static const double g[4] = { 0.0, 4.0, 8.0, 12.0 }; return g[std::max(0, std::min(3, lv))]; }
// Volume: % -> ganho linear (cubico).
inline float VolumeGain(int percent) { if (percent <= 0) return 0.0f; float x = std::min(100, percent) / 100.0f; return x * x * x; }

// ---------------------------------------------------------------- reverb --
// Freeverb (Jezar at Dreampoint, dominio publico): 8 combs + 4 allpass por canal.
struct Comb {
    std::vector<float> buf; size_t i = 0; float store = 0, fb = 0.84f, damp = 0.2f;
    float Run(float x) { float y = buf[i]; store = y * (1 - damp) + store * damp; buf[i] = x + store * fb; if (++i >= buf.size()) i = 0; return y; }
    void Clear() { std::fill(buf.begin(), buf.end(), 0.0f); store = 0; }
};
struct Allpass {
    std::vector<float> buf; size_t i = 0;
    float Run(float x) { float b = buf[i]; float y = -x + b; buf[i] = x + b * 0.5f; if (++i >= buf.size()) i = 0; return y; }
    void Clear() { std::fill(buf.begin(), buf.end(), 0.0f); }
};
struct ReverbNode {
    ma_node_base base;
    ma_uint32 channels = 2;
    Comb cl[8], cr[8]; Allpass al[4], ar[4];
    int last = 0; float wet = 0, dry = 1, fade = 0;   // fade: entra/sai suave ao ligar/desligar
};
inline void ReverbSetup(ReverbNode& r, ma_uint32 sr) {
    static const int comb[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, ap[4] = { 556, 441, 341, 225 };
    double k = sr / 44100.0;
    for (int i = 0; i < 8; ++i) { r.cl[i].buf.assign((size_t)std::max(8.0, comb[i] * k), 0.0f); r.cr[i].buf.assign((size_t)std::max(8.0, (comb[i] + 23) * k), 0.0f); }
    for (int i = 0; i < 4; ++i) { r.al[i].buf.assign((size_t)std::max(8.0, ap[i] * k), 0.0f); r.ar[i].buf.assign((size_t)std::max(8.0, (ap[i] + 23) * k), 0.0f); }
}
inline void ReverbProcess(ma_node* node, const float** in, ma_uint32* fin, float** out, ma_uint32* fout) {
    ReverbNode& r = *(ReverbNode*)node;
    ma_uint32 n = std::min(*fin, *fout); *fin = n; *fout = n;
    const float* x = in[0]; float* y = out[0]; ma_uint32 ch = r.channels;
    int lv = std::max(0, std::min(3, L().reverb.load()));
    if (lv == 0 && r.fade <= 0.0001f) {
        if (y != x) memcpy(y, x, (size_t)n * ch * sizeof(float));
        if (r.last != 0) { for (int i = 0; i < 8; ++i) { r.cl[i].Clear(); r.cr[i].Clear(); } for (int i = 0; i < 4; ++i) { r.al[i].Clear(); r.ar[i].Clear(); } r.last = 0; }
        return;
    }
    static const float room[4] = { 0.0f, 0.55f, 0.75f, 0.90f }, wetL[4] = { 0.0f, 0.20f, 0.30f, 0.42f }, dampL[4] = { 0.0f, 0.35f, 0.28f, 0.22f };
    if (lv) {
        float fb = 0.28f * room[lv] + 0.70f, dp = 0.4f * dampL[lv];
        for (int i = 0; i < 8; ++i) { r.cl[i].fb = r.cr[i].fb = fb; r.cl[i].damp = r.cr[i].damp = dp; }
        r.wet = wetL[lv]; r.dry = 1.0f - wetL[lv] * 0.45f; r.last = lv;
    }
    float target = lv ? 1.0f : 0.0f, step = 1.0f / 4410.0f;
    for (ma_uint32 f = 0; f < n; ++f) {
        float l = x[f * ch], rr = ch > 1 ? x[f * ch + 1] : l;
        float inp = (l + rr) * 0.015f, ol = 0, orr = 0;
        for (int i = 0; i < 8; ++i) { ol += r.cl[i].Run(inp); orr += r.cr[i].Run(inp); }
        for (int i = 0; i < 4; ++i) { ol = r.al[i].Run(ol); orr = r.ar[i].Run(orr); }
        if (r.fade < target) r.fade = std::min(target, r.fade + step); else if (r.fade > target) r.fade = std::max(target, r.fade - step);
        float w = r.wet * 3.0f * r.fade, d = 1.0f + (r.dry - 1.0f) * r.fade;
        y[f * ch] = l * d + ol * w;
        if (ch > 1) y[f * ch + 1] = rr * d + orr * w;
        for (ma_uint32 c = 2; c < ch; ++c) y[f * ch + c] = x[f * ch + c];
    }
}

// -------------------------------------------------------------------- 8D --
// O som "gira" em volta da cabeca: panorama de potencia constante com um LFO lento, um pouco de
// abafado no lado mais longe (sombra da cabeca) e parte do estereo original mantida.
struct EightDNode {
    ma_node_base base;
    ma_uint32 channels = 2, sr = 44100;
    double phase = 0; float mix = 0, lpL = 0, lpR = 0;
};
inline void EightDProcess(ma_node* node, const float** in, ma_uint32* fin, float** out, ma_uint32* fout) {
    EightDNode& e = *(EightDNode*)node;
    ma_uint32 n = std::min(*fin, *fout); *fin = n; *fout = n;
    const float* x = in[0]; float* y = out[0]; ma_uint32 ch = e.channels;
    int lv = std::max(0, std::min(3, L().eightD.load()));
    if ((lv == 0 && e.mix <= 0.0001f) || ch < 2) { if (y != x) memcpy(y, x, (size_t)n * ch * sizeof(float)); e.mix = 0; return; }
    static const float hz[4] = { 0.0f, 0.08f, 0.13f, 0.20f }, depth[4] = { 0.0f, 0.70f, 0.88f, 1.0f };
    double inc = 2.0 * 3.14159265358979 * (lv ? hz[lv] : 0.1f) / e.sr;
    float dep = lv ? depth[lv] : depth[1], target = lv ? 1.0f : 0.0f, step = 1.0f / 2205.0f;
    for (ma_uint32 f = 0; f < n; ++f) {
        float l = x[f * ch], r = x[f * ch + 1];
        float s = (float)sin(e.phase) * dep; e.phase += inc; if (e.phase > 6.283185307179586) e.phase -= 6.283185307179586;
        float th = (s * 0.85f + 1.0f) * 0.78539816f, gl = cos(th), gr = sin(th);   // no extremo o lado longe fica ~-18 dB (nunca mudo)
        float mono = (l + r) * 0.5f;
        float nl = (mono * 0.8f + l * 0.2f) * gl * 1.41421356f, nr = (mono * 0.8f + r * 0.2f) * gr * 1.41421356f;
        float a = 0.08f + 0.5f * std::max(0.0f, s), b = 0.08f + 0.5f * std::max(0.0f, -s);   // lado longe mais abafado
        e.lpL += (nl - e.lpL) * (1.0f - a); e.lpR += (nr - e.lpR) * (1.0f - b);
        if (e.mix < target) e.mix = std::min(target, e.mix + step); else if (e.mix > target) e.mix = std::max(target, e.mix - step);
        y[f * ch] = l + (e.lpL - l) * e.mix;
        y[f * ch + 1] = r + (e.lpR - r) * e.mix;
        for (ma_uint32 c = 2; c < ch; ++c) y[f * ch + c] = x[f * ch + c];
    }
}

// ---------------------------------------------------------------- mestre --
// Volume com subida controlada + limitador (ataque instantaneo, soltura ~80 ms, teto -0,3 dBFS).
struct MasterNode {
    ma_node_base base;
    ma_uint32 channels = 2, sr = 44100;
    std::atomic<float> target{ 0.512f };
    float cur = 0.512f, env = 1.0f;
};
inline void MasterProcess(ma_node* node, const float** in, ma_uint32* fin, float** out, ma_uint32* fout) {
    MasterNode& m = *(MasterNode*)node;
    ma_uint32 n = std::min(*fin, *fout); *fin = n; *fout = n;
    const float* x = in[0]; float* y = out[0]; ma_uint32 ch = m.channels;
    const float ceil = 0.966f;                                   // -0,3 dBFS
    float tgt = m.target.load();
    float upPerSample = (float)pow(10.0, 40.0 / 20.0 / m.sr);   // +40 dB/s no maximo
    float downK = 1.0f - (float)exp(-1.0 / (0.012 * m.sr));     // desce em ~12 ms
    float rel = 1.0f - (float)exp(-1.0 / (0.08 * m.sr));
    for (ma_uint32 f = 0; f < n; ++f) {
        if (m.cur < tgt) { float nx = m.cur < 0.01f ? 0.01f : m.cur * upPerSample; m.cur = nx > tgt ? tgt : nx; }   // de -40 dB para cima: no maximo 40 dB/s
        else if (m.cur > tgt) { m.cur += (tgt - m.cur) * downK; if (m.cur < 0.000001f && tgt == 0) m.cur = 0; }
        float peak = 0;
        for (ma_uint32 c = 0; c < ch; ++c) { float v = fabsf(x[f * ch + c] * m.cur); if (v > peak) peak = v; }
        float need = peak > ceil ? ceil / peak : 1.0f;
        if (need < m.env) m.env = need; else m.env += (1.0f - m.env) * rel;
        float g = m.cur * m.env;
        for (ma_uint32 c = 0; c < ch; ++c) { float v = x[f * ch + c] * g; y[f * ch + c] = v > ceil ? ceil : (v < -ceil ? -ceil : v); }
    }
}

inline ma_node_vtable* ReverbVt() { static ma_node_vtable v = { ReverbProcess, nullptr, 1, 1, 0 }; return &v; }
inline ma_node_vtable* EightDVt() { static ma_node_vtable v = { EightDProcess, nullptr, 1, 1, 0 }; return &v; }
inline ma_node_vtable* MasterVt() { static ma_node_vtable v = { MasterProcess, nullptr, 1, 1, 0 }; return &v; }

} // namespace afx
