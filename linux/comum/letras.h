#pragma once
// LETRAS: a letra da música, acompanhando o tempo quando existe versão sincronizada.
//
// De onde vem: LRCLIB (lrclib.net), banco público e aberto de letras — sem conta,
// sem chave e sem anúncio. Primeiro tenta o casamento exato (artista + título +
// duração) e, se não achar, faz uma busca pelo nome.
//
// Uma vez achada, a letra fica guardada no disco PARA SEMPRE (pasta "letras" do
// Remix, um arquivo .lrc por música): fechar o app, desligar o PC ou ficar sem
// internet não perde mais nada. Quando não existe letra para aquela música, o
// Remix também anota isso (arquivo vazio) para não ficar perguntando toda vez.
#include "platform.h"
#include "config.h"
#include "app_playlists.h"
#include "online_resolve.h"
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <atomic>
#include <thread>
#include <algorithm>

namespace letra {

struct Linha { int ms = 0; std::wstring txt; };
struct Letra {
    std::vector<Linha> linhas;   // sincronizada (vazia = só texto corrido)
    std::wstring texto;          // letra simples, quando não tem sincronizada
    bool sync = false;
    int estado = 0;              // 0 = procurando, 1 = achou, 2 = não tem
    std::wstring fonte;
};

struct Estado {
    std::mutex m;
    std::map<std::wstring, Letra> cache;      // chave da música -> letra
    std::map<std::wstring, bool> buscando;
};
inline Estado& St() { static Estado* e = new Estado(); return *e; }

inline std::wstring Pasta() { return Config::Join(Config::BaseDir(), L"letras"); }
inline std::wstring Arquivo(const std::wstring& chave) {
    unsigned long long x = 1469598103934665603ULL;
    for (wchar_t c : chave) { x ^= (unsigned long long)c; x *= 1099511628211ULL; }
    wchar_t b[40]; swprintf(b, 40, L"%016llx.lrc", x);
    return Config::Join(Pasta(), b);
}
// "[01:23.45] texto" -> linha com tempo. Texto sem colchete entra como linha solta.
inline Letra Parse(const std::vector<std::wstring>& linhas) {
    Letra L;
    for (auto& ln : linhas) {
        if (ln.size() > 3 && ln[0] == L'[' && ln[1] >= L'0' && ln[1] <= L'9') {
            size_t f = ln.find(L']');
            if (f != std::wstring::npos) {
                std::wstring t = ln.substr(1, f - 1);
                int mm = 0, ss = 0, cs = 0;
                size_t dp = t.find(L':');
                if (dp != std::wstring::npos) {
                    mm = _wtoi(t.substr(0, dp).c_str());
                    std::wstring resto = t.substr(dp + 1);
                    size_t pt = resto.find_first_of(L".:");
                    ss = _wtoi(resto.substr(0, pt == std::wstring::npos ? resto.size() : pt).c_str());
                    if (pt != std::wstring::npos) { std::wstring c = resto.substr(pt + 1); while (c.size() < 2) c += L'0'; cs = _wtoi(c.substr(0, 2).c_str()); }
                }
                Linha x; x.ms = (mm * 60 + ss) * 1000 + cs * 10;
                x.txt = ln.substr(f + 1);
                while (!x.txt.empty() && x.txt.front() == L' ') x.txt.erase(x.txt.begin());
                L.linhas.push_back(x);
                continue;
            }
        }
        if (!ln.empty() && ln[0] == L'#') continue;
        if (!L.texto.empty()) L.texto += L"\n";
        L.texto += ln;
    }
    std::stable_sort(L.linhas.begin(), L.linhas.end(), [](const Linha& a, const Linha& b) { return a.ms < b.ms; });
    L.sync = !L.linhas.empty();
    L.estado = (L.sync || !L.texto.empty()) ? 1 : 2;
    return L;
}
inline bool LerDoDisco(const std::wstring& chave, Letra& out) {
    std::vector<std::wstring> lns;
    if (!ReadAllUtf8Lines(Arquivo(chave), lns)) return false;
    if (lns.empty() || (lns.size() == 1 && lns[0].empty())) { out = Letra(); out.estado = 2; return true; }
    out = Parse(lns);
    out.fonte = L"guardada no PC";
    return true;
}
inline void GravarNoDisco(const std::wstring& chave, const std::wstring& conteudo) {
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(Pasta()), ec);
    std::vector<std::wstring> lns;
    size_t i = 0;
    while (i <= conteudo.size()) {
        size_t e = conteudo.find(L'\n', i);
        if (e == std::wstring::npos) e = conteudo.size();
        std::wstring ln = conteudo.substr(i, e - i);
        if (!ln.empty() && ln.back() == L'\r') ln.pop_back();
        lns.push_back(ln);
        if (e == conteudo.size()) break;
        i = e + 1;
    }
    WriteAllUtf8Lines(Arquivo(chave), lns);
}
// Busca no LRCLIB: casamento exato e, se falhar, busca pelo nome.
inline std::wstring Buscar(const std::wstring& titulo, const std::wstring& artista, int dur) {
    auto pega = [](const JVal& o, std::wstring& fonte) -> std::wstring {
        std::string s = JS(o, "syncedLyrics");
        if (!s.empty()) { fonte = L"LRCLIB (sincronizada)"; return Utf8ToWide(s); }
        s = JS(o, "plainLyrics");
        if (!s.empty()) { fonte = L"LRCLIB"; return Utf8ToWide(s); }
        return L"";
    };
    std::wstring fonte;
    if (!titulo.empty()) {
        std::string url = "https://lrclib.net/api/get?track_name=" + OUrlEnc(titulo);
        if (!artista.empty()) url += "&artist_name=" + OUrlEnc(artista);
        if (dur > 0) url += "&duration=" + std::to_string(dur);
        JVal v;
        if (HttpJson(url, v) && !v.get("error")) { std::wstring r = pega(v, fonte); if (!r.empty()) return r; }
    }
    // busca aberta: pega o primeiro que tiver letra (de preferência sincronizada)
    std::string q = WideToUtf8(artista.empty() ? titulo : (artista + L" " + titulo));
    if (q.empty()) return L"";
    std::string body;
    if (!PlatformHttpGet("https://lrclib.net/api/search?q=" + OUrlEnc(Utf8ToWide(q)), body)) return L"";
    JVal v;
    if (!OParse(body, v) || v.t != JVal::ARR) return L"";
    for (int passe = 0; passe < 2; passe++) {
        for (auto& x : v.a) {
            if (x.t != JVal::OBJ) continue;
            std::string s = JS(x, passe == 0 ? "syncedLyrics" : "plainLyrics");
            if (s.empty()) continue;
            if (dur > 0) { int d = (int)x.num("duration", 0); if (d > 0 && std::abs(d - dur) > 12) continue; }
            return Utf8ToWide(s);
        }
    }
    return L"";
}
// O que a tela chama: devolve o que já tem e procura em segundo plano se precisar.
// avisar() é chamado quando chega letra nova.
inline Letra Para(const std::wstring& chave, const std::wstring& titulo, const std::wstring& artista, int dur, void (*avisar)()) {
    Estado& e = St();
    {
        std::lock_guard<std::mutex> lk(e.m);
        auto it = e.cache.find(chave);
        if (it != e.cache.end()) return it->second;
        if (e.buscando.count(chave)) { Letra L; L.estado = 0; return L; }
        e.buscando[chave] = true;
    }
    std::thread([chave, titulo, artista, dur, avisar] {
        Estado& e2 = St();
        Letra L; L.estado = 2;
        RemixSafe("letra", [&] {
            if (LerDoDisco(chave, L)) return;              // já estava guardada (inclusive "não tem")
            std::wstring txt = Buscar(titulo, artista, dur);
            GravarNoDisco(chave, txt);                     // guarda até quando não acha (arquivo vazio)
            if (txt.empty()) { L = Letra(); L.estado = 2; return; }
            std::vector<std::wstring> lns; size_t i = 0;
            while (i <= txt.size()) { size_t f = txt.find(L'\n', i); if (f == std::wstring::npos) f = txt.size(); std::wstring ln = txt.substr(i, f - i); if (!ln.empty() && ln.back() == L'\r') ln.pop_back(); lns.push_back(ln); if (f == txt.size()) break; i = f + 1; }
            L = Parse(lns);
            L.fonte = L.sync ? L"LRCLIB (sincronizada)" : L"LRCLIB";
        });
        { std::lock_guard<std::mutex> lk(e2.m); e2.cache[chave] = L; e2.buscando.erase(chave); }
        if (avisar) avisar();
    }).detach();
    Letra L; L.estado = 0; return L;
}
// Linha que está tocando agora (-1 = nenhuma ainda).
inline int LinhaAtual(const Letra& L, int posMs) {
    if (!L.sync) return -1;
    int r = -1;
    for (size_t i = 0; i < L.linhas.size(); i++) { if (L.linhas[i].ms <= posMs) r = (int)i; else break; }
    return r;
}
// Apaga a letra guardada de uma música (para procurar de novo).
inline void Esquecer(const std::wstring& chave) {
    std::error_code ec; std::filesystem::remove(std::filesystem::path(Arquivo(chave)), ec);
    Estado& e = St(); std::lock_guard<std::mutex> lk(e.m); e.cache.erase(chave);
}

} // namespace letra
