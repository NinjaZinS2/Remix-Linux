#pragma once
// Musica online, parte 1: ferramentas externas, busca, links de plataformas e o
// "casamento" de musicas no YouTube Music. Nada aqui toca audio nem mexe na tela.
//  - CLI de midia configurada pela pessoa: YouTube, YouTube Music, SoundCloud e afins.
//    Algumas CLIs precisam de um runtime JavaScript recente (deno, node ou bun).
//  - Spotify: pagina publica "embed" (so os metadados).
//  - Deezer e Apple Music: APIs publicas (api.deezer.com e itunes.apple.com).
//  Spotify, Deezer e Apple nao sao baixados direto deles (exigiria quebrar a protecao
//  dos arquivos): titulo + artista sao procurados no YouTube Music, pelo titulo e artista.
#include "platform.h"
#include "config.h"
#include "playlist.h"
#include "app_playlists.h"
#include "app_proc.h"
#include "fonte_externa.h"
#include <memory>
#include <map>
#include <set>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <cstring>
#include <climits>
#ifndef _WIN32
#include <sys/stat.h>
#endif

bool PlatformHttpGet(const std::string& url, std::string& body);   // casca: libcurl (Linux) / WinHTTP (Windows)

enum OSrc : int { OS_UNKNOWN = 0, OS_YOUTUBE, OS_YTMUSIC, OS_SOUNDCLOUD, OS_SPOTIFY, OS_DEEZER, OS_APPLE, OS_BANDCAMP, OS_GPM, OS_OTHER };
struct OTrack { std::wstring url, play, title, artist, album, thumb; int dur = 0; OSrc src = OS_UNKNOWN; };

inline std::wstring OLower(std::wstring s) { for (auto& c : s) c = (wchar_t)towlower(c); return s; }
inline bool OHas(const std::wstring& s, const wchar_t* sub) { return s.find(sub) != std::wstring::npos; }
inline OSrc DetectSource(const std::wstring& u0) {
    std::wstring u = OLower(u0);
    if (OHas(u, L"music.youtube.com")) return OS_YTMUSIC;
    if (OHas(u, L"youtube.com") || OHas(u, L"youtu.be")) return OS_YOUTUBE;
    if (OHas(u, L"soundcloud.com")) return OS_SOUNDCLOUD;
    if (OHas(u, L"open.spotify.com") || u.rfind(L"spotify:", 0) == 0 || OHas(u, L"spotify.link")) return OS_SPOTIFY;
    if (OHas(u, L"deezer.com") || OHas(u, L"deezer.page.link") || OHas(u, L"dzr.page.link")) return OS_DEEZER;
    if (OHas(u, L"music.apple.com") || OHas(u, L"itunes.apple.com")) return OS_APPLE;
    if (OHas(u, L"bandcamp.com")) return OS_BANDCAMP;
    if (OHas(u, L"play.google.com/music") || OHas(u, L"music.google.com")) return OS_GPM;
    if (u.rfind(L"http://", 0) == 0 || u.rfind(L"https://", 0) == 0) return OS_OTHER;
    return OS_UNKNOWN;
}
inline const wchar_t* SourceName(OSrc s) {
    switch (s) {
    case OS_YOUTUBE: return L"YouTube"; case OS_YTMUSIC: return L"YouTube Music"; case OS_SOUNDCLOUD: return L"SoundCloud";
    case OS_SPOTIFY: return L"Spotify"; case OS_DEEZER: return L"Deezer"; case OS_APPLE: return L"Apple Music";
    case OS_BANDCAMP: return L"Bandcamp"; case OS_GPM: return L"Google Play Música"; default: return L"Online";
    }
}
inline bool NeedsMatch(OSrc s) { return s == OS_SPOTIFY || s == OS_DEEZER || s == OS_APPLE; }

// ---- ferramentas ------------------------------------------------------------------
// A camada que fala com o programa externo mora em fonte_externa.h: regras,
// contrato, deteccao e as duas unicas funcoes que executam o processo
// (fonte::Rodar e fonte::Abrir). Daqui para baixo e so montar comando e ler JSON.

inline std::wstring OErr(const CapResult& r, const wchar_t* fallback) {
    if (!r.started) return L"Não consegui executar a ferramenta externa.";
    if (r.timedOut) return L"Demorou demais (tempo esgotado).";
    std::string e = r.err;
    size_t p = e.rfind("ERROR");
    if (p != std::string::npos) {
        std::string l = e.substr(p);
        size_t n = l.find_first_of("\r\n"); if (n != std::string::npos) l = l.substr(0, n);
        if (l.size() > 170) l = l.substr(0, 170) + "...";
        return Utf8ToWide(l);
    }
    return fallback;
}

// ---- JSON / URL ------------------------------------------------------------------
inline bool OParse(const std::string& s, JVal& v) {
    size_t b = s.find_first_of("{[");
    if (b == std::string::npos) return false;
    std::string body = s.substr(b);
    JParser p(body);
    return p.parse(v);
}
inline std::string JS(const JVal& o, const char* k) {
    const JVal* v = o.get(k); if (!v) return "";
    if (v->t == JVal::STR) return v->s;
    if (v->t == JVal::NUM) { char b[32]; snprintf(b, 32, "%.0f", v->n); return b; }
    return "";
}
inline std::string OUrlEnc(const std::wstring& w) {
    std::string s = WideToUtf8(w), o; char b[4];
    for (unsigned char c : s) { if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') o.push_back((char)c); else { snprintf(b, 4, "%%%02X", c); o += b; } }
    return o;
}
inline std::wstring StripTopic(std::wstring a) { const std::wstring t = L" - Topic"; if (a.size() > t.size() && a.compare(a.size() - t.size(), t.size(), t) == 0) a.erase(a.size() - t.size()); return a; }
inline void InfoToTrack(const JVal& e, OTrack& t, OSrc def) {
    std::string url = JS(e, "webpage_url"); if (url.empty()) url = JS(e, "url");
    std::string id = JS(e, "id");
    if (url.empty() && !id.empty() && (def == OS_YOUTUBE || def == OS_YTMUSIC)) url = "https://www.youtube.com/watch?v=" + id;
    t.url = Utf8ToWide(url); t.play = t.url;
    std::string title = JS(e, "track"); if (title.empty()) title = JS(e, "title");
    std::string ar = JS(e, "artist");
    if (ar.empty()) { const JVal* a = e.get("artists"); if (a && a->t == JVal::ARR && !a->a.empty() && a->a[0].t == JVal::STR) ar = a->a[0].s; }
    if (ar.empty()) ar = JS(e, "creator"); if (ar.empty()) ar = JS(e, "channel"); if (ar.empty()) ar = JS(e, "uploader");
    t.title = Utf8ToWide(title); t.artist = StripTopic(Utf8ToWide(ar)); t.album = Utf8ToWide(JS(e, "album"));
    t.dur = (int)e.num("duration", 0);
    std::string th = JS(e, "thumbnail");
    if (th.empty()) { const JVal* ts = e.get("thumbnails"); if (ts && ts->t == JVal::ARR && !ts->a.empty()) th = JS(ts->a.back(), "url"); }
    t.thumb = Utf8ToWide(th);
    t.src = DetectSource(t.url); if (t.src == OS_UNKNOWN || t.src == OS_OTHER) t.src = def;
}


// ---- busca: 0 = YouTube Music, 1 = YouTube, 2 = SoundCloud ----------------------------
inline bool OnlineSearch(const std::wstring& q, int where, std::vector<OTrack>& out, std::wstring& err, const std::atomic<bool>* cancel, int limit = 20,
                         std::function<void(const OTrack&)> onItem = nullptr) {
    fonte::Garantir();
    if (!fonte::Configurada()) { err = fonte::MsgFalta(1); return false; }
    auto a = fonte::Cmd(); a.push_back(L"-j"); a.push_back(L"--flat-playlist");   // -j: uma linha JSON por resultado (a tela mostra conforme chegam)
    OSrc def = OS_YOUTUBE; std::wstring n = std::to_wstring(limit);
    if (where == 0) { a.push_back(L"--playlist-items"); a.push_back(L"1:" + n); a.push_back(L"https://music.youtube.com/search?q=" + Utf8ToWide(OUrlEnc(q)) + L"#songs"); def = OS_YTMUSIC; }
    else if (where == 2) { a.push_back(L"scsearch" + n + L":" + q); def = OS_SOUNDCLOUD; }
    else a.push_back(L"ytsearch" + n + L":" + q);
    CapResult r = fonte::Rodar(a, 90000, cancel, [&](const std::string& ln) {
        if (ln.empty() || ln[0] != '{') return;
        JVal e; if (!OParse(ln, e) || e.t != JVal::OBJ) return;
        OTrack t; InfoToTrack(e, t, def); if (t.url.empty()) return;
        out.push_back(t); if (onItem) onItem(t);
    });
    if (r.canceled) return false;
    if (out.empty()) err = r.code == 0 ? std::wstring(L"Nada encontrado.") : OErr(r, L"A busca falhou.");
    return !out.empty();
}
// Procura PLAYLISTS no YouTube (a busca normal so traz musica solta). O filtro
// "sp=EgIQAw%3D%3D" da pagina de resultados e o "somente playlists"; a CLI le
// essa pagina e devolve uma entrada por playlist.
struct OLista { std::wstring titulo, sub, capa, link; int n = 0; };
inline bool BuscarPlaylistsYoutube(const std::wstring& q, std::vector<OLista>& out, int limit, const std::atomic<bool>* cancel) {
    fonte::Garantir();
    if (!fonte::Configurada() || q.empty()) return false;
    auto a = fonte::Cmd();
    a.push_back(L"-J"); a.push_back(L"--flat-playlist");
    a.push_back(L"--playlist-items"); a.push_back(L"1:" + std::to_wstring(limit));
    a.push_back(L"https://www.youtube.com/results?search_query=" + Utf8ToWide(OUrlEnc(q)) + L"&sp=EgIQAw%253D%253D");
    std::string all;
    CapResult r = fonte::Rodar(a, 60000, cancel, [&](const std::string& ln) { all += ln; });
    if (r.canceled || all.empty()) return false;
    JVal v; if (!OParse(all, v) || v.t != JVal::OBJ) return false;
    const JVal* ent = v.get("entries");
    if (!ent || ent->t != JVal::ARR) return false;
    for (auto& e : ent->a) {
        if ((int)out.size() >= limit) break;
        if (e.t != JVal::OBJ) continue;
        OLista L;
        L.titulo = Utf8ToWide(JS(e, "title"));
        L.link = Utf8ToWide(JS(e, "url"));
        if (L.link.empty()) { std::string id = JS(e, "id"); if (!id.empty()) L.link = L"https://www.youtube.com/playlist?list=" + Utf8ToWide(id); }
        L.n = (int)e.num("playlist_count", 0);
        std::wstring canal = Utf8ToWide(JS(e, "channel")); if (canal.empty()) canal = Utf8ToWide(JS(e, "uploader"));
        L.sub = L.n > 0 ? (std::to_wstring(L.n) + L" músicas" + (canal.empty() ? L"" : L"  ·  " + canal)) : (canal.empty() ? std::wstring(L"YouTube") : canal);
        if (const JVal* th = e.get("thumbnails"); th && th->t == JVal::ARR && !th->a.empty()) L.capa = Utf8ToWide(JS(th->a.back(), "url"));
        if (!L.titulo.empty() && !L.link.empty() && L.link.find(L"list=") != std::wstring::npos) out.push_back(L);
    }
    return !out.empty();
}
// Acha a musica tocavel (YouTube Music; senao YouTube pela duracao mais parecida).
inline bool MatchOnYouTube(OTrack& t, std::wstring& err, const std::atomic<bool>* cancel) {
    if (!t.play.empty() && !NeedsMatch(DetectSource(t.play))) return true;
    std::wstring base = t.artist.empty() ? t.title : (t.artist + L" " + t.title);
    std::vector<OTrack> res; std::wstring e1;
    if (OnlineSearch(base, 0, res, e1, cancel, 5) && !res.empty()) { t.play = res[0].url; return true; }
    if (cancel && cancel->load()) return false;
    res.clear();
    if (OnlineSearch(base + L" audio", 1, res, e1, cancel, 8) && !res.empty()) {
        size_t best = 0;
        if (t.dur > 0) { int bd = INT_MAX; for (size_t i = 0; i < res.size(); ++i) { int d = res[i].dur > 0 ? std::abs(res[i].dur - t.dur) : 9999; if (d < bd) { bd = d; best = i; } } }
        t.play = res[best].url;
        return true;
    }
    err = L"Não achei \"" + t.title + L"\" no YouTube Music.";
    return false;
}

// ---- links de playlists / albuns / musicas ---------------------------------------------
struct OResolved { std::vector<OTrack> items; std::wstring name, err; OSrc src = OS_UNKNOWN; };
inline bool HttpJson(const std::string& url, JVal& v) { std::string body; return PlatformHttpGet(url, body) && OParse(body, v) && v.t == JVal::OBJ; }
inline std::string DigitsAfter(const std::string& s, size_t pos) { std::string d; while (pos < s.size() && isdigit((unsigned char)s[pos])) d.push_back(s[pos++]); return d; }
inline void ResolveDeezer(std::wstring url, OResolved& r) {
    std::string u = WideToUtf8(url), kind, id;
    auto scan = [&](const std::string& s) {
        for (const char* k : { "/track/", "/album/", "/playlist/" }) {
            size_t p = s.find(k);
            if (p != std::string::npos && s.find("deezer.com") != std::string::npos) { std::string d = DigitsAfter(s, p + strlen(k)); if (!d.empty()) { kind = std::string(k).substr(1, strlen(k) - 2); id = d; return; } }
        }
    };
    scan(u);
    if (id.empty()) { std::string body; if (PlatformHttpGet(u, body)) scan(body); }   // link curto: a pagina final tem o link completo
    if (id.empty()) { r.err = L"Link do Deezer não reconhecido (use o link de música, álbum ou playlist)."; return; }
    JVal v;
    auto addTrack = [&](const JVal& x, const std::wstring& album, const std::wstring& cover) {
        OTrack t; t.src = OS_DEEZER; t.url = Utf8ToWide(JS(x, "link")); if (t.url.empty()) t.url = L"https://www.deezer.com/track/" + Utf8ToWide(JS(x, "id"));
        t.title = Utf8ToWide(JS(x, "title")); t.dur = (int)x.num("duration", 0);
        if (const JVal* a = x.get("artist")) t.artist = Utf8ToWide(JS(*a, "name"));
        t.album = album; t.thumb = cover;
        if (const JVal* al = x.get("album")) { if (t.album.empty()) t.album = Utf8ToWide(JS(*al, "title")); std::wstring c = Utf8ToWide(JS(*al, "cover_medium")); if (!c.empty()) t.thumb = c; }
        if (!t.title.empty()) r.items.push_back(t);
    };
    if (kind == "track") { if (!HttpJson("https://api.deezer.com/track/" + id, v) || v.get("error")) { r.err = L"O Deezer não respondeu para essa música."; return; } addTrack(v, L"", L""); r.name = r.items.empty() ? L"" : r.items[0].title; return; }
    if (!HttpJson("https://api.deezer.com/" + kind + "/" + id, v) || v.get("error")) { r.err = L"O Deezer não respondeu (link privado ou fora do ar?)."; return; }
    r.name = Utf8ToWide(JS(v, "title"));
    std::wstring cover = Utf8ToWide(JS(v, kind == "album" ? "cover_medium" : "picture_medium")), album = kind == "album" ? r.name : L"";
    for (int index = 0; index < 5000; index += 100) {
        JVal page;
        if (!HttpJson("https://api.deezer.com/" + kind + "/" + id + "/tracks?limit=100&index=" + std::to_string(index), page)) break;
        const JVal* data = page.get("data"); if (!data || data->t != JVal::ARR || data->a.empty()) break;
        for (auto& x : data->a) if (x.t == JVal::OBJ) addTrack(x, album, cover);
        if (data->a.size() < 100) break;
    }
    if (r.items.empty()) r.err = L"Nenhuma música encontrada nesse link do Deezer.";
}
inline void ResolveApple(const std::wstring& url, OResolved& r) {
    std::string u = WideToUtf8(url), cc = "us";
    size_t h = u.find(".apple.com/");
    if (h != std::string::npos) { size_t s = h + 11; size_t e = u.find('/', s); if (e != std::string::npos && e - s == 2) cc = u.substr(s, 2); }
    auto pathId = [&]() -> std::string { std::string p = u; size_t q = p.find('?'); if (q != std::string::npos) p = p.substr(0, q); while (!p.empty() && p.back() == '/') p.pop_back(); size_t sl = p.rfind('/'); return sl == std::string::npos ? "" : p.substr(sl + 1); };
    std::string trackId; { size_t i = u.find("?i="); if (i == std::string::npos) i = u.find("&i="); if (i != std::string::npos) trackId = DigitsAfter(u, i + 3); }
    auto fromItunes = [&](const JVal& x) {
        if (JS(x, "wrapperType") != "track") return;
        OTrack t; t.src = OS_APPLE; t.title = Utf8ToWide(JS(x, "trackName")); t.artist = Utf8ToWide(JS(x, "artistName")); t.album = Utf8ToWide(JS(x, "collectionName"));
        t.dur = (int)(x.num("trackTimeMillis", 0) / 1000); t.url = Utf8ToWide(JS(x, "trackViewUrl"));
        std::string art = JS(x, "artworkUrl100"); size_t k = art.find("100x100"); if (k != std::string::npos) art.replace(k, 7, "600x600");
        t.thumb = Utf8ToWide(art);
        if (!t.title.empty()) r.items.push_back(t);
    };
    if (u.find("/playlist/") != std::string::npos) {
        std::string body;
        if (PlatformHttpGet(u, body)) {
            size_t p = 0;
            while ((p = body.find("application/ld+json", p)) != std::string::npos) {
                size_t b = body.find('>', p), e = body.find("</script>", p);
                if (b == std::string::npos || e == std::string::npos) break;
                JVal v;
                if (OParse(body.substr(b + 1, e - b - 1), v) && v.t == JVal::OBJ) {
                    if (r.name.empty()) r.name = Utf8ToWide(JS(v, "name"));
                    if (const JVal* tr = v.get("track")) if (tr->t == JVal::ARR) for (auto& x : tr->a) {
                        if (x.t != JVal::OBJ) continue;
                        OTrack t; t.src = OS_APPLE; t.title = Utf8ToWide(JS(x, "name")); t.url = Utf8ToWide(JS(x, "url"));
                        if (const JVal* ba = x.get("byArtist")) { if (ba->t == JVal::OBJ) t.artist = Utf8ToWide(JS(*ba, "name")); else if (ba->t == JVal::ARR && !ba->a.empty()) t.artist = Utf8ToWide(JS(ba->a[0], "name")); }
                        std::string d = JS(x, "duration"); int mm = 0, ss = 0; if (sscanf(d.c_str(), "PT%dM%dS", &mm, &ss) >= 1) t.dur = mm * 60 + ss;
                        if (!t.title.empty()) r.items.push_back(t);
                    }
                }
                p = e;
            }
        }
        if (r.items.empty()) r.err = L"Não consegui ler essa playlist da Apple Music (playlists pessoais não são públicas). Use o link de um álbum ou música.";
        return;
    }
    std::string lookupId = !trackId.empty() ? trackId : DigitsAfter(pathId(), 0);
    if (lookupId.empty()) { std::string pid = pathId(); size_t k = pid.find_first_of("0123456789"); if (k != std::string::npos) lookupId = DigitsAfter(pid, k); }
    if (lookupId.empty()) { r.err = L"Link da Apple Music não reconhecido."; return; }
    bool album = trackId.empty() && u.find("/album/") != std::string::npos;
    JVal v;
    if (!HttpJson("https://itunes.apple.com/lookup?id=" + lookupId + "&country=" + cc + (album ? "&entity=song&limit=200" : ""), v)) { r.err = L"A Apple Music não respondeu."; return; }
    if (const JVal* res = v.get("results")) if (res->t == JVal::ARR) for (auto& x : res->a) { if (x.t != JVal::OBJ) continue; if (JS(x, "wrapperType") == "collection") r.name = Utf8ToWide(JS(x, "collectionName")); fromItunes(x); }
    if (r.name.empty() && !r.items.empty()) r.name = album ? r.items[0].album : r.items[0].title;
    if (r.items.empty()) r.err = L"Nenhuma música encontrada nesse link da Apple Music.";
}
// Spotify sem API: a pagina publica "embed" (open.spotify.com/embed/<tipo>/<id>) traz o JSON
// __NEXT_DATA__ com nome, faixas (titulo, artistas, duracao) e capa. Rapido e sem chave.
inline bool SpotifyTypeId(const std::wstring& url, std::string& type, std::string& id) {
    std::string u = WideToUtf8(url);
    if (u.rfind("spotify:", 0) == 0) { size_t c = u.find(':', 8); if (c == std::string::npos) return false; type = u.substr(8, c - 8); id = u.substr(c + 1); }
    else {
        size_t h = u.find("open.spotify.com/"); if (h == std::string::npos) return false;
        std::string p = u.substr(h + 17);
        size_t q = p.find_first_of("?#"); if (q != std::string::npos) p = p.substr(0, q);
        if (p.rfind("intl-", 0) == 0) { size_t sl = p.find('/'); if (sl == std::string::npos) return false; p = p.substr(sl + 1); }
        if (p.rfind("embed/", 0) == 0) p = p.substr(6);
        size_t sl = p.find('/'); if (sl == std::string::npos) return false;
        type = p.substr(0, sl); id = p.substr(sl + 1);
        size_t e = id.find('/'); if (e != std::string::npos) id = id.substr(0, e);
    }
    for (char c : id) if (!isalnum((unsigned char)c)) return false;
    return !id.empty() && (type == "track" || type == "album" || type == "playlist" || type == "artist");
}
inline std::wstring SpotifyClean(const std::string& s) { std::wstring w = Utf8ToWide(s); for (auto& c : w) if (c == 0xA0) c = L' '; return w; }
inline bool ResolveSpotifyEmbed(const std::wstring& url, OResolved& r) {
    std::string type, id;
    if (!SpotifyTypeId(url, type, id)) return false;
    std::string body;
    if (!PlatformHttpGet("https://open.spotify.com/embed/" + type + "/" + id, body)) return false;
    const std::string tag = "<script id=\"__NEXT_DATA__\" type=\"application/json\">";
    size_t a = body.find(tag); if (a == std::string::npos) return false; a += tag.size();
    size_t b = body.find("</script>", a); if (b == std::string::npos) return false;
    JVal root; if (!OParse(body.substr(a, b - a), root) || root.t != JVal::OBJ) return false;
    const JVal* ent = &root;
    for (const char* k : { "props", "pageProps", "state", "data", "entity" }) { ent = ent->get(k); if (!ent || ent->t != JVal::OBJ) return false; }
    std::string cover; double best = -1;
    if (const JVal* vi = ent->get("visualIdentity")) if (const JVal* im = vi->get("image")) if (im->t == JVal::ARR)
        for (auto& x : im->a) { double w = x.num("maxWidth", 0); if (w > best) { best = w; cover = JS(x, "url"); } }
    std::wstring name = SpotifyClean(JS(*ent, "name"));
    if (const JVal* tl = ent->get("trackList")) if (tl->t == JVal::ARR) for (auto& x : tl->a) {
        if (x.t != JVal::OBJ) continue;
        std::string uri = JS(x, "uri"); if (uri.rfind("spotify:track:", 0) != 0) continue;   // episodios de podcast ficam de fora
        OTrack t; t.src = OS_SPOTIFY; t.url = L"https://open.spotify.com/track/" + Utf8ToWide(uri.substr(14));
        t.title = SpotifyClean(JS(x, "title")); t.artist = SpotifyClean(JS(x, "subtitle")); t.dur = (int)(x.num("duration", 0) / 1000);
        if (type == "album") { t.album = name; t.thumb = Utf8ToWide(cover); }
        if (!t.title.empty()) r.items.push_back(t);
    }
    if (type == "track" && r.items.empty()) {
        OTrack t; t.src = OS_SPOTIFY; t.url = L"https://open.spotify.com/track/" + Utf8ToWide(id);
        t.title = name; t.dur = (int)(ent->num("duration", 0) / 1000); t.thumb = Utf8ToWide(cover);
        if (const JVal* as = ent->get("artists")) if (as->t == JVal::ARR) for (auto& x : as->a) { std::wstring n = SpotifyClean(JS(x, "name")); if (n.empty()) continue; if (!t.artist.empty()) t.artist += L", "; t.artist += n; }
        if (!t.title.empty()) r.items.push_back(t);
    }
    if (r.items.empty()) return false;
    r.name = type == "track" ? r.items[0].title : name;
    r.src = OS_SPOTIFY;
    return true;
}
inline void ResolveSpotify(const std::wstring& url, OResolved& r, const std::atomic<bool>* cancel) {
    if (ResolveSpotifyEmbed(url, r)) return;
    r.items.clear(); r.name.clear(); r.err.clear();
}
inline OResolved ResolveLink(const std::wstring& url0, const std::atomic<bool>* cancel) {
    OResolved r;
    std::wstring url = Config::Trim(url0);
    r.src = DetectSource(url);
    switch (r.src) {
    case OS_GPM: r.err = L"O Google Play Música foi encerrado em 2020 (virou o YouTube Music): use um link do YouTube Music."; return r;
    case OS_UNKNOWN: r.err = L"Isso não parece um link."; return r;
    case OS_SPOTIFY: ResolveSpotify(url, r, cancel); return r;
    case OS_DEEZER: ResolveDeezer(url, r); return r;
    case OS_APPLE: ResolveApple(url, r); return r;
    default: break;
    }
    fonte::Garantir();
    if (!fonte::Configurada()) { r.err = fonte::MsgFalta(2); return r; }
    auto a = fonte::Cmd(); a.push_back(L"-J"); a.push_back(L"--flat-playlist"); a.push_back(L"--"); a.push_back(url);   // "--": link nunca vira opcao
    CapResult cr = fonte::Rodar(a, 300000, cancel);
    JVal root;
    if (!OParse(cr.out, root) || root.t != JVal::OBJ) { r.err = OErr(cr, L"Não consegui ler esse link."); return r; }
    if (JS(root, "_type") == "playlist" || root.get("entries")) {
        r.name = Utf8ToWide(JS(root, "title"));
        if (const JVal* ents = root.get("entries")) if (ents->t == JVal::ARR)
            for (auto& e : ents->a) { if (e.t != JVal::OBJ) continue; OTrack t; InfoToTrack(e, t, r.src); if (!t.url.empty()) r.items.push_back(t); }
    } else {
        OTrack t; InfoToTrack(root, t, r.src);
        if (!t.url.empty()) { r.items.push_back(t); r.name = t.title; }
    }
    if (r.items.empty()) r.err = L"Nenhuma música encontrada nesse link.";
    return r;
}

// ---- URL do audio para o streaming --------------------------------------------------
struct MediaInfo { std::string url; std::vector<std::string> headers; int dur = 0; std::wstring title, artist, album, thumb; std::string raw; };   // raw = JSON da CLI (download reaproveita)
inline bool GetMediaInfoFresh(const std::wstring& play, MediaInfo& mi, std::wstring& err, const std::atomic<bool>* cancel) {
    auto a = fonte::Cmd();
    for (const wchar_t* x : { L"-J", L"--no-playlist", L"-f", L"bestaudio/best" }) a.push_back(x);
    a.push_back(L"--"); a.push_back(play);   // o link pode vir de terceiros (playlist, celular): "--" impede virar opcao da CLI
    CapResult r = fonte::Rodar(a, 120000, cancel);
    JVal root;
    if (!OParse(r.out, root) || root.t != JVal::OBJ) { err = OErr(r, L"Não consegui abrir essa música."); return false; }
    mi.url = JS(root, "url");
    const JVal* hdr = root.get("http_headers");
    if (mi.url.empty()) if (const JVal* rf = root.get("requested_formats")) if (rf->t == JVal::ARR)
        for (auto& f : rf->a) { if (f.t != JVal::OBJ) continue; if (mi.url.empty() || JS(f, "vcodec") == "none") { mi.url = JS(f, "url"); hdr = f.get("http_headers"); } }
    if (hdr && hdr->t == JVal::OBJ) for (auto& kv : hdr->o) if (kv.second.t == JVal::STR) mi.headers.push_back(kv.first + ": " + kv.second.s);
    OTrack t; InfoToTrack(root, t, DetectSource(play));
    mi.dur = t.dur; mi.title = t.title; mi.artist = t.artist; mi.album = t.album; mi.thumb = t.thumb;
    if (r.out.size() < (size_t)4 * 1024 * 1024) mi.raw = r.out;
    return true;   // url vazia = usa o modo pipe (CLI de midia -o - | ffmpeg)
}
// ---- Cache da extracao ----------------------------------------------------------------
// A extracao do CLI de midia e o que mais atrasa o comeco (~3 s). O resultado vale ~25 min e e usado
// por todo mundo: o player do PC, o celular (Host) e o download (--load-info-json). Dois pedidos
// da mesma musica ao mesmo tempo esperam um so CLI de midia.
struct MediaCacheT { std::mutex m; std::condition_variable cv; std::map<std::wstring, std::pair<MediaInfo, long long>> done; std::map<std::wstring, int> busy; };
inline MediaCacheT& MCache() { static MediaCacheT* c = new MediaCacheT(); return *c; }
inline long long MonoSec() { return (long long)std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
static const long long MEDIA_CACHE_SEC = 25 * 60;
inline bool GetMediaInfoCached(const std::wstring& play, MediaInfo& mi) {
    std::lock_guard<std::mutex> lk(MCache().m);
    auto it = MCache().done.find(play);
    if (it == MCache().done.end() || MonoSec() - it->second.second >= MEDIA_CACHE_SEC) return false;
    mi = it->second.first; return true;
}
inline void ForgetMediaInfo(const std::wstring& play) { std::lock_guard<std::mutex> lk(MCache().m); MCache().done.erase(play); }   // o link direto falhou: a proxima vez extrai de novo
inline bool GetMediaInfo(const std::wstring& play, MediaInfo& mi, std::wstring& err, const std::atomic<bool>* cancel) {
    {
        std::unique_lock<std::mutex> lk(MCache().m);
        for (;;) {
            auto it = MCache().done.find(play);
            if (it != MCache().done.end() && MonoSec() - it->second.second < MEDIA_CACHE_SEC) { mi = it->second.first; return true; }
            if (!MCache().busy.count(play)) break;
            MCache().cv.wait_for(lk, std::chrono::milliseconds(150));   // outro pedido ja esta extraindo esta musica
            if (cancel && cancel->load()) return false;
        }
        MCache().busy[play] = 1;
    }
    bool ok = false;
    try { ok = GetMediaInfoFresh(play, mi, err, cancel); } catch (...) { ok = false; }
    {
        std::lock_guard<std::mutex> lk(MCache().m);
        MCache().busy.erase(play);
        if (ok) {
            if (MCache().done.size() >= 40) {   // o JSON do YouTube pode ter centenas de KB: guarda so as mais recentes
                auto old = MCache().done.begin();
                for (auto i = MCache().done.begin(); i != MCache().done.end(); ++i) if (i->second.second < old->second.second) old = i;
                MCache().done.erase(old);
            }
            MCache().done[play] = { mi, MonoSec() };
        }
    }
    MCache().cv.notify_all();
    return ok;
}
// Pre-extrai em segundo plano (primeiros resultados de uma busca): tocar depois comeca na hora.
// Uma busca nova cancela a anterior; no maximo 2 CLI de midia ao mesmo tempo por lote.
struct PreResolveT { std::mutex m; std::shared_ptr<std::atomic<bool>> cur; };
inline PreResolveT& PreRes() { static PreResolveT* p = new PreResolveT(); return *p; }
inline void PreResolve(const std::vector<std::wstring>& plays) {
    auto list = std::make_shared<std::vector<std::wstring>>();
    for (auto& p : plays) if (!p.empty() && !NeedsMatch(DetectSource(p)) && (p.rfind(L"https://", 0) == 0 || p.rfind(L"http://", 0) == 0)) list->push_back(p);
    if (list->empty()) return;
    auto cancel = std::make_shared<std::atomic<bool>>(false);
    { std::lock_guard<std::mutex> lk(PreRes().m); if (PreRes().cur) *PreRes().cur = true; PreRes().cur = cancel; }
    auto next = std::make_shared<std::atomic<size_t>>(0);
    for (int w = 0; w < 2; ++w) std::thread([list, next, cancel] {
        for (;;) {
            if (cancel->load()) return;
            size_t i = (*next)++;
            if (i >= list->size()) return;
            MediaInfo mi; if (GetMediaInfoCached((*list)[i], mi)) continue;
            std::wstring e;
            try { GetMediaInfo((*list)[i], mi, e, cancel.get()); } catch (...) {}
        }
    }).detach();
}
// ---- Miniaturas ------------------------------------------------------------------------
// Forma do link: http(s), sem usuario/senha ("https://ytimg.com@outro/"), sem barra invertida,
// sem IP literal e sem porta estranha. strictHttps = so https e porta 443.
inline bool ThumbUrlShapeOk(const std::string& u, bool strictHttps, std::string* hostOut = nullptr) {
    if (u.size() > 2048) return false;
    size_t sch = u.rfind("https://", 0) == 0 ? 8 : (!strictHttps && u.rfind("http://", 0) == 0 ? 7 : 0);
    if (!sch) return false;
    for (unsigned char c : u) if (c < 0x21 || c == 0x7F || c == '\\') return false;
    size_t e = u.find_first_of("/?#", sch); std::string auth = u.substr(sch, e == std::string::npos ? std::string::npos : e - sch);
    if (auth.empty() || auth.find_first_of("@%[]") != std::string::npos) return false;
    size_t c = auth.find(':');
    if (c != std::string::npos) { std::string port = auth.substr(c + 1); if (port != (sch == 8 ? "443" : "80")) return false; auth = auth.substr(0, c); }
    bool num = true; for (auto& ch : auth) { ch = (char)tolower((unsigned char)ch); if (!(isalnum((unsigned char)ch) || ch == '-' || ch == '.')) return false; if (!(isdigit((unsigned char)ch) || ch == '.')) num = false; }
    if (auth.empty() || num || auth.front() == '.' || auth.back() == '.' || auth.find('.') == std::string::npos) return false;
    if (hostOut) *hostOut = auth;
    return true;
}
// Miniaturas que o Host busca a pedido do celular: so https e so CDNs conhecidos.
inline bool ThumbUrlAllowed(const std::wstring& w) {
    std::string host; if (!ThumbUrlShapeOk(WideToUtf8(w), true, &host)) return false;
    static const char* ok[] = { "ytimg.com", "googleusercontent.com", "ggpht.com", "sndcdn.com", "scdn.co", "spotifycdn.com", "dzcdn.net", "mzstatic.com" };
    for (const char* d : ok) { std::string dd = d; if (host == dd || (host.size() > dd.size() && host.compare(host.size() - dd.size(), dd.size(), dd) == 0 && host[host.size() - dd.size() - 1] == '.')) return true; }
    return false;
}
bool PlatformHttpGetNoRedirect(const std::string& url, std::string& body);   // casca: como PlatformHttpGet, mas sem seguir redirecionamento
// Miniatura no cache: sempre JPEG quadrado (corte central, ate 512 px). O YouTube entrega
// WebP (mesmo com ".jpg" no nome), que nem o raylib nem o GDI+ abrem: o ffmpeg converte.
inline bool LooksWebp(const char* h, size_t n) { return n >= 12 && memcmp(h, "RIFF", 4) == 0 && memcmp(h + 8, "WEBP", 4) == 0; }
inline bool FileLooksWebp(const std::wstring& p) { std::ifstream f(std::filesystem::path(p), std::ios::binary); char h[12]; f.read(h, 12); return LooksWebp(h, (size_t)f.gcount()); }
// So imagem de verdade vai para o ffmpeg, e com o formato fixo (nada de playlist/manifesto).
inline const wchar_t* ImageDemuxer(const std::string& b) {
    if (b.size() >= 3 && (unsigned char)b[0] == 0xFF && (unsigned char)b[1] == 0xD8 && (unsigned char)b[2] == 0xFF) return L"jpeg_pipe";
    if (b.size() >= 8 && memcmp(b.data(), "\x89PNG\r\n\x1a\n", 8) == 0) return L"png_pipe";
    if (LooksWebp(b.data(), b.size())) return L"webp_pipe";
    if (b.size() >= 6 && (memcmp(b.data(), "GIF87a", 6) == 0 || memcmp(b.data(), "GIF89a", 6) == 0)) return L"gif";
    return nullptr;
}
struct ThumbFlights { std::mutex m; std::condition_variable cv; std::map<std::wstring, int> busy; std::atomic<unsigned> seq{ 0 }; };
inline ThumbFlights& TFl() { static ThumbFlights* t = new ThumbFlights(); return *t; }
// strict = pedido vindo do celular (Host): so CDNs conhecidos e sem seguir redirecionamento.
inline std::wstring FetchThumb(const std::wstring& thumbUrl, bool strict = false) {
    if (thumbUrl.empty()) return L"";
    if (strict ? !ThumbUrlAllowed(thumbUrl) : !ThumbUrlShapeOk(WideToUtf8(thumbUrl), false)) return L"";
    std::wstring fin = OnlineThumbFile(thumbUrl);
    {   // a mesma capa pedida por duas threads (celular + tela do PC): a segunda espera a primeira
        std::unique_lock<std::mutex> lk(TFl().m);
        if (!TFl().cv.wait_for(lk, std::chrono::seconds(45), [&] { return !TFl().busy.count(fin); })) return L"";
        TFl().busy[fin] = 1;
    }
    struct Release { std::wstring f; ~Release() { { std::lock_guard<std::mutex> lk(TFl().m); TFl().busy.erase(f); } TFl().cv.notify_all(); } } release{ fin };
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(fin), ec) && !FileLooksWebp(fin)) return fin;
    std::string body;
    if (!(strict ? PlatformHttpGetNoRedirect(WideToUtf8(thumbUrl), body) : PlatformHttpGet(WideToUtf8(thumbUrl), body)) || body.size() < 200) return L"";
    const wchar_t* demux = ImageDemuxer(body);
    if (!demux) return L"";   // nao e imagem: descarta
    std::filesystem::path fp(fin);
    std::filesystem::create_directories(fp.parent_path(), ec);
    std::wstring uniq = L"." + std::to_wstring(++TFl().seq);
    std::wstring tmp = fin + uniq + L".tmp";
    { std::ofstream o(std::filesystem::path(tmp), std::ios::binary | std::ios::trunc); o.write(body.data(), (std::streamsize)body.size()); }
    fonte::Garantir();
    if (fonte::FfmpegOk()) {
        std::wstring part = fin + uniq + L".part.jpg";
        CapResult r = RunCapture({ fonte::Ffmpeg(), L"-nostdin", L"-loglevel", L"error", L"-y", L"-protocol_whitelist", L"file", L"-f", demux, L"-i", tmp,
                                   L"-vf", L"crop='min(iw,ih)':'min(iw,ih)',scale='trunc(min(512,iw)/2)*2':-2", L"-frames:v", L"1", L"-q:v", L"3", part }, 30000);
        std::filesystem::remove(std::filesystem::path(tmp), ec);
        if (r.code == 0 && std::filesystem::exists(std::filesystem::path(part), ec)) {
            std::filesystem::remove(fp, ec); ec.clear();
            std::filesystem::rename(std::filesystem::path(part), fp, ec);
            if (!ec) return fin;
        }
        std::filesystem::remove(std::filesystem::path(part), ec);
        return L"";
    }
    if (LooksWebp(body.data(), body.size())) { std::filesystem::remove(std::filesystem::path(tmp), ec); return L""; }   // sem ffmpeg nao da para mostrar WebP
    std::wstring shr;
    try { shr = ShrinkCoverInto(fp.parent_path().wstring(), fp.stem().wstring().c_str(), tmp); } catch (...) { shr.clear(); }
    if (!shr.empty() && shr != fin) { std::filesystem::remove(std::filesystem::path(tmp), ec); std::filesystem::remove(fp, ec); ec.clear(); std::filesystem::rename(std::filesystem::path(shr), fp, ec); return fin; }
    if (!shr.empty()) { std::filesystem::remove(std::filesystem::path(tmp), ec); return fin; }
    std::filesystem::remove(fp, ec); ec.clear();
    std::filesystem::rename(std::filesystem::path(tmp), fp, ec);
    return ec ? L"" : fin;
}
