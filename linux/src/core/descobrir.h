#pragma once
// DESCOBRIR: novidades, paradas e recomendacoes para a tela inicial (PC e celular).
//
// De onde vem: APIs publicas que NAO pedem login nem chave —
//   - Deezer (api.deezer.com): paradas do pais, generos, playlists, artistas
//     parecidos, albuns por artista. So metadados (nome, artista, capa, link).
// Nada disso toca musica: quem toca e baixa continua sendo o motor online do
// Remix (a CLI configurada), entao vem a musica inteira e nao o trecho de 30 s das APIs.
//
// O que fica no disco: um cache das prateleiras (descobrir.cache, na pasta de
// cache) para a tela abrir na hora, e o perfil de gostos (gostos.ini, na pasta
// do Remix) montado com o que a pessoa ouve. Nada sai do PC.
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
#include <ctime>
#include <cstdio>

namespace desc {

enum Kind { K_FAIXA = 0, K_ALBUM = 1, K_PLAYLIST = 2, K_ARTISTA = 3 };

// Um quadradinho da tela: musica, album, playlist ou artista.
struct Item {
    std::wstring id;       // id no Deezer
    std::wstring titulo;   // nome principal
    std::wstring sub;      // artista / dono / numero de musicas
    std::wstring capa;     // URL da imagem (o cache de miniaturas do online cuida)
    std::wstring link;     // link que o motor online entende (ResolveLink)
    Kind kind = K_FAIXA;
    int dur = 0;
};
// Uma fileira ("Bombando agora", "Parecido com Travis Scott"...).
struct Shelf {
    std::wstring titulo, nota;
    Kind kind = K_FAIXA;
    std::wstring chave;    // id estavel da fileira (para o celular pedir de novo)
    std::vector<Item> itens;
};
struct Home {
    std::vector<Shelf> fileiras;
    long long at = 0;          // quando foi montada (epoch em segundos)
    std::wstring err;
};

// ------------------------------------------------------------------ util --
inline long long Agora() { return (long long)time(nullptr); }
inline std::wstring Limpa(std::wstring s) {   // campos vao para um arquivo separado por "|"
    for (auto& c : s) if (c == L'|' || c == L'\n' || c == L'\r' || c == L'\t') c = L' ';
    return s;
}
inline std::vector<std::wstring> Partes(const std::wstring& v) {
    std::vector<std::wstring> c; size_t s0 = 0;
    for (size_t k = 0; k <= v.size(); k++) if (k == v.size() || v[k] == L'|') { c.push_back(v.substr(s0, k - s0)); s0 = k + 1; }
    return c;
}
inline double LeNum(const std::wstring& s) {   // parse simples (sem wcstod: o binario nao pode depender de simbolo novo da glibc)
    size_t i = 0; while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    bool neg = false; if (i < s.size() && (s[i] == L'-' || s[i] == L'+')) { neg = s[i] == L'-'; ++i; }
    double v = 0;
    while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') { v = v * 10 + (s[i] - L'0'); ++i; }
    if (i < s.size() && s[i] == L'.') { ++i; double f = 0.1; while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') { v += (s[i] - L'0') * f; f *= 0.1; ++i; } }
    return neg ? -v : v;
}
inline long long LeLL(const std::wstring& s) {
    size_t i = 0; while (i < s.size() && (s[i] == L' ' || s[i] == L'\t')) ++i;
    bool neg = false; if (i < s.size() && (s[i] == L'-' || s[i] == L'+')) { neg = s[i] == L'-'; ++i; }
    long long v = 0;
    while (i < s.size() && s[i] >= L'0' && s[i] <= L'9') { v = v * 10 + (s[i] - L'0'); ++i; }
    return neg ? -v : v;
}
inline std::wstring Min(const std::wstring& s) { std::wstring r = s; for (auto& c : r) c = (wchar_t)towlower(c); return r; }
inline std::wstring Num(long long v) { return std::to_wstring(v); }

// --------------------------------------------------------- perfil de gostos --
// Peso do artista = quantas vezes a pessoa ouviu (com um empurrao para o que ela
// ouviu faz pouco tempo). Serve para escolher as fileiras "parecido com" e
// "dos artistas que voce ouve".
struct Gosto { double peso = 0; long long ultimo = 0; };
struct Perfil {
    std::mutex m;
    std::map<std::wstring, Gosto> artistas;          // nome em minusculas -> peso
    std::map<std::wstring, std::wstring> nomeReal;   // minusculas -> como aparece na tela
    std::map<std::wstring, Gosto> faixas;            // caminho ou URL -> vezes tocada
    bool carregado = false, sujo = false;
    long long salvoEm = 0;
};
inline Perfil& P() { static Perfil* p = new Perfil(); return *p; }
inline std::wstring GostosPath() { return Config::Join(Config::BaseDir(), L"gostos.ini"); }

inline void CarregarPerfil() {
    Perfil& p = P();
    { std::lock_guard<std::mutex> lk(p.m); if (p.carregado) return; p.carregado = true; }
    std::vector<std::wstring> lines;
    if (!ReadAllUtf8Lines(GostosPath(), lines)) return;
    std::lock_guard<std::mutex> lk(p.m);
    for (auto& ln : lines) {
        if (ln.size() < 3 || ln[1] != L'=') continue;
        wchar_t tipo = ln[0];
        std::vector<std::wstring> f = Partes(ln.substr(2));
        if (f.size() < 3) continue;
        Gosto g; g.peso = LeNum(f[1]); g.ultimo = LeLL(f[2]);
        if (tipo == L'a') { std::wstring k = Min(f[0]); p.artistas[k] = g; p.nomeReal[k] = f[0]; }
        else if (tipo == L'f') p.faixas[f[0]] = g;
    }
}
inline void SalvarPerfil() {
    Perfil& p = P();
    std::vector<std::wstring> out;
    {
        std::lock_guard<std::mutex> lk(p.m);
        if (!p.sujo) return;
        p.sujo = false; p.salvoEm = Agora();
        out.push_back(L"# Remix: o que você mais ouve (só no seu PC; apagar este arquivo zera as recomendações)");
        for (auto& kv : p.artistas) {
            std::wstring nome = p.nomeReal.count(kv.first) ? p.nomeReal[kv.first] : kv.first;
            wchar_t b[48]; swprintf(b, 48, L"|%.3f|%lld", kv.second.peso, (long long)kv.second.ultimo);
            out.push_back(L"a=" + Limpa(nome) + b);
        }
        int n = 0;
        for (auto& kv : p.faixas) {
            if (++n > 3000) break;
            wchar_t b[48]; swprintf(b, 48, L"|%.3f|%lld", kv.second.peso, (long long)kv.second.ultimo);
            out.push_back(L"f=" + Limpa(kv.first) + b);
        }
    }
    WriteAllUtf8Lines(GostosPath(), out);
}
// Chamado quando uma musica comeca a tocar de verdade.
inline void Registrar(const std::wstring& artista, const std::wstring& chave) {
    CarregarPerfil();
    Perfil& p = P();
    {
        std::lock_guard<std::mutex> lk(p.m);
        long long ag = Agora();
        if (!artista.empty()) {
            // varios artistas na mesma tag ("A, B feat. C"): o primeiro leva o peso
            std::wstring a = artista; size_t corte = a.find_first_of(L",&");
            std::wstring primeiro = corte == std::wstring::npos ? a : a.substr(0, corte);
            while (!primeiro.empty() && primeiro.back() == L' ') primeiro.pop_back();
            std::wstring k = Min(primeiro);
            if (!k.empty() && k != L"online" && k != L"?") { Gosto& g = p.artistas[k]; g.peso += 1.0; g.ultimo = ag; p.nomeReal[k] = primeiro; }
        }
        if (!chave.empty()) { Gosto& g = p.faixas[chave]; g.peso += 1.0; g.ultimo = ag; }
        p.sujo = true;
        if (ag - p.salvoEm < 60) return;   // nao grava a cada faixa
    }
    SalvarPerfil();
}
// Ultimas musicas tocadas (caminho ou URL), da mais recente para a mais antiga.
inline std::vector<std::wstring> Recentes(int n) {
    CarregarPerfil();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    std::vector<std::pair<long long, std::wstring>> v;
    for (auto& kv : p.faixas) v.push_back(std::make_pair(kv.second.ultimo, kv.first));
    std::sort(v.begin(), v.end(), [](const std::pair<long long, std::wstring>& a, const std::pair<long long, std::wstring>& b) { return a.first > b.first; });
    std::vector<std::wstring> out;
    for (auto& x : v) { if ((int)out.size() >= n) break; out.push_back(x.second); }
    return out;
}
// Musicas que a pessoa mais repete (para a fileira "voce nao larga").
inline std::vector<std::wstring> MaisTocadas(int n) {
    CarregarPerfil();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    std::vector<std::pair<double, std::wstring>> v;
    for (auto& kv : p.faixas) if (kv.second.peso >= 2) v.push_back(std::make_pair(kv.second.peso, kv.first));
    std::sort(v.begin(), v.end(), [](const std::pair<double, std::wstring>& a, const std::pair<double, std::wstring>& b) { return a.first > b.first; });
    std::vector<std::wstring> out;
    for (auto& x : v) { if ((int)out.size() >= n) break; out.push_back(x.second); }
    return out;
}
// Playlists mais usadas: guardadas no mesmo perfil, com a chave "pl:<slug>".
inline void RegistrarPlaylist(const std::wstring& slug) {
    if (slug.empty()) return;
    Registrar(L"", L"pl:" + slug);
}
inline double PesoPlaylist(const std::wstring& slug) {
    if (slug.empty()) return 0;
    CarregarPerfil();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    auto it = p.faixas.find(L"pl:" + slug);
    if (it == p.faixas.end()) return 0;
    long long dias = (Agora() - it->second.ultimo) / 86400; if (dias < 0) dias = 0;
    return it->second.peso * (1.0 + 2.0 / (1.0 + (double)dias));
}
// Peso de um artista no perfil (0 = nunca ouviu). Usado pela mistura do dia.
inline double PesoArtista(const std::wstring& artista) {
    if (artista.empty()) return 0;
    CarregarPerfil();
    std::wstring a = artista; size_t corte = a.find_first_of(L",&");
    std::wstring primeiro = corte == std::wstring::npos ? a : a.substr(0, corte);
    while (!primeiro.empty() && primeiro.back() == L' ') primeiro.pop_back();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    auto it = p.artistas.find(Min(primeiro));
    if (it == p.artistas.end()) return 0;
    long long dias = (Agora() - it->second.ultimo) / 86400; if (dias < 0) dias = 0;
    return it->second.peso * (1.0 + 2.0 / (1.0 + (double)dias));
}
inline double PesoFaixa(const std::wstring& chave) {
    CarregarPerfil();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    auto it = p.faixas.find(chave);
    return it == p.faixas.end() ? 0 : it->second.peso;
}
// Sorteio estavel do dia: a mesma mistura durante o dia, outra amanha.
inline double SorteioDoDia(const std::wstring& chave) {
    unsigned long long dia = (unsigned long long)(Agora() / 86400);
    unsigned long long x = 1469598103934665603ULL ^ dia;
    for (wchar_t c : chave) { x ^= (unsigned long long)c; x *= 1099511628211ULL; }
    return (double)((x >> 11) % 1000) / 1000.0;
}
// Artistas favoritos, do mais forte para o mais fraco (quem tocou faz pouco vale mais).
inline std::vector<std::wstring> TopArtistas(int n) {
    CarregarPerfil();
    Perfil& p = P(); std::lock_guard<std::mutex> lk(p.m);
    long long ag = Agora();
    std::vector<std::pair<double, std::wstring>> v;
    for (auto& kv : p.artistas) {
        double dias = (double)(ag - kv.second.ultimo) / 86400.0; if (dias < 0) dias = 0;
        double nota = kv.second.peso * (1.0 + 2.0 / (1.0 + dias));   // ouviu hoje vale ~3x
        v.push_back(std::make_pair(nota, p.nomeReal.count(kv.first) ? p.nomeReal.at(kv.first) : kv.first));
    }
    std::sort(v.begin(), v.end(), [](const std::pair<double, std::wstring>& a, const std::pair<double, std::wstring>& b) { return a.first > b.first; });
    std::vector<std::wstring> out;
    for (auto& x : v) { if ((int)out.size() >= n) break; out.push_back(x.second); }
    return out;
}

// ------------------------------------------------------------- Deezer ------
inline std::wstring DzImg(const JVal& o, const char* pequeno, const char* medio) {
    std::string s = JS(o, medio); if (s.empty()) s = JS(o, pequeno);
    return Utf8ToWide(s);
}
inline std::wstring DzId(const JVal& x) {
    std::string s = JS(x, "id");
    if (!s.empty()) return Utf8ToWide(s);
    char b[32]; snprintf(b, sizeof b, "%lld", (long long)x.num("id", 0));
    return Utf8ToWide(b);
}
inline Item ItemFaixa(const JVal& x) {
    Item it; it.kind = K_FAIXA; it.id = DzId(x);
    it.titulo = Utf8ToWide(JS(x, "title_short")); if (it.titulo.empty()) it.titulo = Utf8ToWide(JS(x, "title"));
    it.dur = (int)x.num("duration", 0);
    if (const JVal* a = x.get("artist")) it.sub = Utf8ToWide(JS(*a, "name"));
    if (const JVal* al = x.get("album")) it.capa = DzImg(*al, "cover_small", "cover_medium");
    it.link = Utf8ToWide(JS(x, "link"));
    if (it.link.empty() && !it.id.empty()) it.link = L"https://www.deezer.com/track/" + it.id;
    return it;
}
inline Item ItemAlbum(const JVal& x) {
    Item it; it.kind = K_ALBUM; it.id = DzId(x);
    it.titulo = Utf8ToWide(JS(x, "title"));
    if (const JVal* a = x.get("artist")) it.sub = Utf8ToWide(JS(*a, "name"));
    std::string rd = JS(x, "release_date");
    if (!rd.empty() && rd.size() >= 4) { if (!it.sub.empty()) it.sub += L"  ·  "; it.sub += Utf8ToWide(rd.substr(0, 4)); }
    it.capa = DzImg(x, "cover_small", "cover_medium");
    it.link = Utf8ToWide(JS(x, "link")); if (it.link.empty()) it.link = L"https://www.deezer.com/album/" + it.id;
    return it;
}
inline Item ItemPlaylist(const JVal& x) {
    Item it; it.kind = K_PLAYLIST; it.id = DzId(x);
    it.titulo = Utf8ToWide(JS(x, "title"));
    int n = (int)x.num("nb_tracks", 0);
    it.sub = n > 0 ? (std::to_wstring(n) + L" músicas") : Utf8ToWide(JS(x, "user"));
    it.capa = DzImg(x, "picture_small", "picture_medium");
    it.link = Utf8ToWide(JS(x, "link")); if (it.link.empty()) it.link = L"https://www.deezer.com/playlist/" + it.id;
    return it;
}
inline Item ItemArtista(const JVal& x) {
    Item it; it.kind = K_ARTISTA; it.id = DzId(x);
    it.titulo = Utf8ToWide(JS(x, "name"));
    int n = (int)x.num("nb_fan", 0);
    if (n >= 1000000) it.sub = std::to_wstring(n / 1000000) + L" mi de fãs";
    else if (n >= 1000) it.sub = std::to_wstring(n / 1000) + L" mil fãs";
    it.capa = DzImg(x, "picture_small", "picture_medium");
    it.link = Utf8ToWide(JS(x, "link")); if (it.link.empty()) it.link = L"https://www.deezer.com/artist/" + it.id;
    return it;
}
// Le uma lista do Deezer ({"data":[...]}) e transforma em itens.
inline std::vector<Item> DzLista(const std::string& url, Kind kind, int max) {
    std::vector<Item> out; JVal v;
    if (!HttpJson(url, v)) return out;
    const JVal* data = v.get("data"); if (!data || data->t != JVal::ARR) return out;
    for (auto& x : data->a) {
        if ((int)out.size() >= max) break;
        if (x.t != JVal::OBJ) continue;
        Item it = kind == K_FAIXA ? ItemFaixa(x) : kind == K_ALBUM ? ItemAlbum(x) : kind == K_PLAYLIST ? ItemPlaylist(x) : ItemArtista(x);
        if (!it.titulo.empty()) out.push_back(it);
    }
    return out;
}
// Acha o id do artista no Deezer pelo nome (usado pelo perfil de gostos).
inline std::wstring DzArtistaId(const std::wstring& nome) {
    JVal v;
    if (!HttpJson("https://api.deezer.com/search/artist?limit=1&q=" + OUrlEnc(nome), v)) return L"";
    const JVal* d = v.get("data"); if (!d || d->t != JVal::ARR || d->a.empty()) return L"";
    return DzId(d->a[0]);
}
// Procurar playlists prontas pelo nome (para nao precisar sair do app).
inline std::vector<Item> BuscarPlaylists(const std::wstring& q, int max) {
    if (q.empty()) return {};
    return DzLista("https://api.deezer.com/search/playlist?limit=" + std::to_string(max) + "&q=" + OUrlEnc(q), K_PLAYLIST, max);
}
inline std::vector<Item> BuscarAlbuns(const std::wstring& q, int max) {
    if (q.empty()) return {};
    return DzLista("https://api.deezer.com/search/album?limit=" + std::to_string(max) + "&q=" + OUrlEnc(q), K_ALBUM, max);
}
// Generos do Deezer (aba "explorar"): id + nome + imagem.
inline std::vector<Item> Generos() {
    std::vector<Item> out; JVal v;
    if (!HttpJson("https://api.deezer.com/genre", v)) return out;
    const JVal* d = v.get("data"); if (!d || d->t != JVal::ARR) return out;
    for (auto& x : d->a) {
        if (x.t != JVal::OBJ) continue;
        Item it; it.kind = K_PLAYLIST; it.id = DzId(x);
        it.titulo = Utf8ToWide(JS(x, "name"));
        it.capa = DzImg(x, "picture_small", "picture_medium");
        if (!it.titulo.empty() && it.id != L"0") out.push_back(it);
    }
    return out;
}

// ------------------------------------------------------- sobre o artista ---
// Ficha do artista que esta tocando: foto, quantos fas e quem e parecido.
struct Artista {
    std::wstring nome, capa, link;
    long long fas = 0;
    std::vector<Item> parecidos;
    int estado = 0;   // 0 = procurando, 1 = achou, 2 = nao achou
};
struct ArtEstado { std::mutex m; std::map<std::wstring, Artista> cache; std::map<std::wstring, bool> indo; };
inline ArtEstado& A() { static ArtEstado* a = new ArtEstado(); return *a; }
inline std::wstring ArtistaPrincipal(const std::wstring& artista) {
    std::wstring a = artista; size_t c = a.find_first_of(L",&");
    if (c != std::wstring::npos) a = a.substr(0, c);
    while (!a.empty() && a.back() == L' ') a.pop_back();
    return a;
}
inline Artista SobreArtista(const std::wstring& artista, void (*avisar)()) {
    std::wstring nome = ArtistaPrincipal(artista);
    Artista vazio; vazio.estado = 2;
    if (nome.empty()) return vazio;
    std::wstring k = Min(nome);
    ArtEstado& a = A();
    {
        std::lock_guard<std::mutex> lk(a.m);
        auto it = a.cache.find(k);
        if (it != a.cache.end()) return it->second;
        if (a.indo.count(k)) { Artista p; p.estado = 0; p.nome = nome; return p; }
        a.indo[k] = true;
    }
    std::thread([nome, k, avisar] {
        ArtEstado& a2 = A();
        Artista r; r.nome = nome; r.estado = 2;
        RemixSafe("sobre o artista", [&] {
            JVal v;
            if (!HttpJson("https://api.deezer.com/search/artist?limit=1&q=" + OUrlEnc(nome), v)) return;
            const JVal* d = v.get("data");
            if (!d || d->t != JVal::ARR || d->a.empty()) return;
            const JVal& x = d->a[0];
            Item it = ItemArtista(x);
            r.capa = DzImg(x, "picture_medium", "picture_big");
            if (r.capa.empty()) r.capa = it.capa;
            r.link = it.link;
            r.fas = (long long)x.num("nb_fan", 0);
            r.parecidos = DzLista("https://api.deezer.com/artist/" + WideToUtf8(it.id) + "/related?limit=6", K_ARTISTA, 6);
            r.estado = 1;
        });
        { std::lock_guard<std::mutex> lk(a2.m); a2.cache[k] = r; a2.indo.erase(k); }
        if (avisar) avisar();
    }).detach();
    Artista p; p.estado = 0; p.nome = nome; return p;
}
inline std::wstring FormataFas(long long n) {
    if (n >= 1000000) { wchar_t b[32]; swprintf(b, 32, L"%.1f mi de fãs", (double)n / 1000000.0); return b; }
    if (n >= 1000) return std::to_wstring(n / 1000) + L" mil fãs";
    return n > 0 ? (std::to_wstring(n) + L" fãs") : L"";
}

// -------------------------------------------------------------- cache -----
inline std::wstring CachePath() { return Config::Join(Config::CacheDir(), L"descobrir.cache"); }
inline void Gravar(const Home& h) {
    std::vector<std::wstring> out;
    out.push_back(L"v1");
    out.push_back(L"at=" + Num(h.at));
    for (auto& s : h.fileiras) {
        out.push_back(L"S|" + Limpa(s.titulo) + L"|" + Limpa(s.nota) + L"|" + Num((int)s.kind) + L"|" + Limpa(s.chave));
        for (auto& it : s.itens)
            out.push_back(L"I|" + Num((int)it.kind) + L"|" + Limpa(it.id) + L"|" + Limpa(it.titulo) + L"|" +
                          Limpa(it.sub) + L"|" + Limpa(it.capa) + L"|" + Limpa(it.link) + L"|" + Num(it.dur));
    }
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(Config::CacheDir()), ec);
    WriteAllUtf8Lines(CachePath(), out);
}
inline bool Ler(Home& h) {
    std::vector<std::wstring> lines;
    if (!ReadAllUtf8Lines(CachePath(), lines)) return false;
    if (lines.empty() || lines[0] != L"v1") return false;
    Home r;
    for (auto& ln : lines) {
        if (ln.rfind(L"at=", 0) == 0) { r.at = LeLL(ln.substr(3)); continue; }
        if (ln.empty() || ln[0] == L'v' || ln[0] == L'#') continue;
        std::vector<std::wstring> c = Partes(ln);
        if (c.empty()) continue;
        if (c[0] == L"S" && c.size() >= 4) {
            Shelf s; s.titulo = c[1]; s.nota = c[2]; s.kind = (Kind)_wtoi(c[3].c_str()); if (c.size() > 4) s.chave = c[4];
            r.fileiras.push_back(s);
        } else if (c[0] == L"I" && c.size() >= 8 && !r.fileiras.empty()) {
            Item it; it.kind = (Kind)_wtoi(c[1].c_str()); it.id = c[2]; it.titulo = c[3]; it.sub = c[4];
            it.capa = c[5]; it.link = c[6]; it.dur = _wtoi(c[7].c_str());
            r.fileiras.back().itens.push_back(it);
        }
    }
    if (r.fileiras.empty()) return false;
    h = r; return true;
}

// ------------------------------------------------------------- montagem ---
struct Estado {
    std::mutex m;
    Home home;
    std::atomic<bool> carregando{ false };
    std::atomic<bool> leuCache{ false };
};
inline Estado& St() { static Estado* e = new Estado(); return *e; }

// Monta as fileiras (demora: sempre em thread).
inline Home Montar() {
    Home h; h.at = Agora();
    auto add = [&](const std::wstring& titulo, const std::wstring& nota, const std::wstring& chave, Kind k, std::vector<Item> itens) {
        if (itens.empty()) return;
        Shelf s; s.titulo = titulo; s.nota = nota; s.chave = chave; s.kind = k; s.itens = itens;
        h.fileiras.push_back(s);
    };
    // --- personalizado primeiro: quem a pessoa mais ouve
    std::vector<std::wstring> tops = TopArtistas(4);
    std::vector<Item> lancamentos, parecidos;
    std::map<std::wstring, bool> vistos;   // o Deezer repete o mesmo album em varias edicoes
    auto novo = [&](const Item& it) { std::wstring k = Min(it.titulo) + L"\x01" + Min(it.sub); if (vistos.count(k)) return false; vistos[k] = true; return true; };
    for (size_t i = 0; i < tops.size(); i++) {
        std::wstring id = DzArtistaId(tops[i]);
        if (id.empty()) continue;
        std::string sid = WideToUtf8(id);
        if (i == 0) {
            add(L"O melhor de " + tops[i], L"porque você ouve bastante", L"top-artista", K_FAIXA,
                DzLista("https://api.deezer.com/artist/" + sid + "/top?limit=16", K_FAIXA, 16));
        }
        if (i < 2) {
            for (auto& a : DzLista("https://api.deezer.com/artist/" + sid + "/related?limit=8", K_ARTISTA, 8))
                if (parecidos.size() < 16 && novo(a)) parecidos.push_back(a);
        }
        for (auto& a : DzLista("https://api.deezer.com/artist/" + sid + "/albums?limit=4", K_ALBUM, 4)) {
            // a lista de albuns do artista nao repete o nome dele: completa aqui
            a.sub = a.sub.empty() ? tops[i] : (tops[i] + L"  ·  " + a.sub);
            if (lancamentos.size() < 16 && novo(a)) lancamentos.push_back(a);
        }
    }
    if (!tops.empty()) {
        add(L"Parecido com " + tops[0], L"artistas que combinam com o que você ouve", L"parecidos", K_ARTISTA, parecidos);
        add(L"Dos artistas que você ouve", L"álbuns e singles", L"lancamentos", K_ALBUM, lancamentos);
    }
    // --- o que está bombando
    add(L"Bombando agora", L"as mais tocadas do país", L"chart-faixas", K_FAIXA,
        DzLista("https://api.deezer.com/chart/0/tracks?limit=24", K_FAIXA, 24));
    add(L"Playlists da semana", L"prontas para ouvir", L"chart-playlists", K_PLAYLIST,
        DzLista("https://api.deezer.com/chart/0/playlists?limit=20", K_PLAYLIST, 20));
    add(L"Álbuns em alta", L"", L"chart-albuns", K_ALBUM,
        DzLista("https://api.deezer.com/chart/0/albums?limit=20", K_ALBUM, 20));
    add(L"Artistas do momento", L"", L"chart-artistas", K_ARTISTA,
        DzLista("https://api.deezer.com/chart/0/artists?limit=20", K_ARTISTA, 20));
    if (h.fileiras.empty()) h.err = L"Não consegui buscar as novidades (sem internet?).";
    return h;
}

// Carrega o cache (rapido) e, se estiver velho, atualiza em segundo plano.
// avisar() e chamado na thread quando termina (a casca posta o evento da UI).
inline void Atualizar(bool forcar, void (*avisar)()) {
    Estado& e = St();
    if (!e.leuCache.exchange(true)) {
        Home h;
        if (Ler(h)) { std::lock_guard<std::mutex> lk(e.m); e.home = h; }
    }
    long long idade;
    { std::lock_guard<std::mutex> lk(e.m); idade = Agora() - e.home.at; }
    if (!forcar && idade < 6 * 3600) return;                 // 6 h: novidade nao muda de minuto em minuto
    if (e.carregando.exchange(true)) return;
    std::thread([avisar] {
        Estado& e2 = St();
        Home h;
        RemixSafe("descobrir", [&] { h = Montar(); });
        if (!h.fileiras.empty()) {
            { std::lock_guard<std::mutex> lk(e2.m); e2.home = h; }
            Gravar(h);
        } else {
            std::lock_guard<std::mutex> lk(e2.m); e2.home.err = h.err;
        }
        e2.carregando.store(false);
        if (avisar) avisar();
    }).detach();
}
inline Home Copia() { Estado& e = St(); std::lock_guard<std::mutex> lk(e.m); return e.home; }
inline bool Carregando() { return St().carregando.load(); }

// ------------------------------------------------------------- generos ----
// A aba "Descobrir": os generos do Deezer e, ao entrar em um, as paradas dele.
struct GenEstado {
    std::mutex m;
    std::vector<Item> lista;                 // generos (com imagem)
    std::map<std::wstring, Home> porId;      // id do genero -> fileiras
    std::atomic<bool> carregando{ false };
    std::atomic<bool> pediuLista{ false };
};
inline GenEstado& G() { static GenEstado* g = new GenEstado(); return *g; }

inline std::vector<Item> ListaGeneros() { GenEstado& g = G(); std::lock_guard<std::mutex> lk(g.m); return g.lista; }
inline bool HomeDoGenero(const std::wstring& id, Home& out) {
    GenEstado& g = G(); std::lock_guard<std::mutex> lk(g.m);
    auto it = g.porId.find(id); if (it == g.porId.end()) return false;
    out = it->second; return true;
}
inline Home MontarGenero(const std::wstring& id, const std::wstring& nome) {
    Home h; h.at = Agora();
    std::string sid = WideToUtf8(id);
    auto add = [&](const std::wstring& titulo, Kind k, std::vector<Item> itens) {
        if (itens.empty()) return;
        Shelf s; s.titulo = titulo; s.kind = k; s.chave = L"gen-" + id; s.itens = itens;
        h.fileiras.push_back(s);
    };
    add(L"Bombando em " + nome, K_FAIXA, DzLista("https://api.deezer.com/chart/" + sid + "/tracks?limit=24", K_FAIXA, 24));
    add(L"Playlists de " + nome, K_PLAYLIST, DzLista("https://api.deezer.com/chart/" + sid + "/playlists?limit=20", K_PLAYLIST, 20));
    add(L"Álbuns de " + nome, K_ALBUM, DzLista("https://api.deezer.com/chart/" + sid + "/albums?limit=20", K_ALBUM, 20));
    add(L"Artistas de " + nome, K_ARTISTA, DzLista("https://api.deezer.com/chart/" + sid + "/artists?limit=20", K_ARTISTA, 20));
    if (h.fileiras.empty()) h.err = L"Não consegui buscar esse gênero agora.";
    return h;
}
// Busca a lista de generos (uma vez) e, com id != "", as paradas daquele genero.
inline void AtualizarGeneros(const std::wstring& id, const std::wstring& nome, void (*avisar)()) {
    GenEstado& g = G();
    bool precisaLista = !g.pediuLista.load();
    bool precisaGen = false;
    if (!id.empty()) { std::lock_guard<std::mutex> lk(g.m); precisaGen = !g.porId.count(id); }
    if (!precisaLista && !precisaGen) return;
    if (g.carregando.exchange(true)) return;
    g.pediuLista.store(true);
    std::thread([id, nome, avisar, precisaLista, precisaGen] {
        GenEstado& g2 = G();
        RemixSafe("descobrir generos", [&] {
            if (precisaLista) { std::vector<Item> l = Generos(); std::lock_guard<std::mutex> lk(g2.m); g2.lista = l; }
            if (precisaGen) { Home h = MontarGenero(id, nome); std::lock_guard<std::mutex> lk(g2.m); g2.porId[id] = h; }
        });
        g2.carregando.store(false);
        if (avisar) avisar();
    }).detach();
}
inline bool CarregandoGeneros() { return G().carregando.load(); }

} // namespace desc
