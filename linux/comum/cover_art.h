#pragma once
// Capa embutida no proprio arquivo de audio: ID3v2 APIC/PIC (MP3, WAV, AIFF), FLAC
// PICTURE, OGG/Opus METADATA_BLOCK_PICTURE e MP4/M4A 'covr'. As imagens vao para
// <cache>/art/<hash do conteudo>.jpg|png (um album inteiro reaproveita o mesmo
// arquivo) e um indice (index.tsv) guarda o resultado por caminho+tamanho+data, entao
// cada musica so e aberta uma vez. A extracao roda numa thread propria: a varredura
// usa o que ja esta no indice e pede o resto em segundo plano (callback onReady).
// Incluido por playlist.h depois de ShrinkCoverInto (reduz capas grandes para 512 px).
#include "config.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <deque>
#include <mutex>
#include <thread>
#include <functional>
#include <fstream>
#include <filesystem>
#include <cstring>
#include <cstdio>

std::wstring ShrinkCoverInto(const std::wstring& dir, const wchar_t* name, const std::wstring& imgPath);

namespace art {
inline uint32_t BE32(const unsigned char* p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }
inline uint32_t Sync32(const unsigned char* p) { return ((p[0] & 0x7Fu) << 21) | ((p[1] & 0x7Fu) << 14) | ((p[2] & 0x7Fu) << 7) | (p[3] & 0x7Fu); }
inline void Unsync(std::vector<unsigned char>& v) { size_t w = 0; for (size_t i = 0; i < v.size(); ++i) { v[w++] = v[i]; if (v[i] == 0xFF && i + 1 < v.size() && v[i + 1] == 0x00) ++i; } v.resize(w); }
inline bool IsImage(const std::string& d) { return d.size() > 16 && (((unsigned char)d[0] == 0xFF && (unsigned char)d[1] == 0xD8) || memcmp(d.data(), "\x89PNG", 4) == 0); }
inline bool ReadN(std::ifstream& f, std::vector<unsigned char>& v, size_t n) { v.resize(n); f.read((char*)v.data(), (std::streamsize)n); return (size_t)f.gcount() == n; }
static const size_t MAXB = (size_t)64 * 1024 * 1024;

// tag = conteudo do ID3v2 sem os 10 bytes do cabecalho
inline bool FromId3(std::vector<unsigned char> tag, unsigned char ver, unsigned char flags, std::string& out) {
    if (ver < 2 || ver > 4) return false;
    if ((flags & 0x80) && ver < 4) Unsync(tag);
    size_t pos = 0;
    if ((flags & 0x40) && ver >= 3 && tag.size() >= 4) pos += ver == 4 ? Sync32(&tag[0]) : BE32(&tag[0]) + 4;
    int idLen = ver == 2 ? 3 : 4, hdrLen = ver == 2 ? 6 : 10;
    std::string best; int bestType = -1;
    while (pos + hdrLen <= tag.size() && tag[pos] != 0) {
        std::string id((char*)&tag[pos], idLen);
        uint32_t fs; unsigned char ff = 0;
        if (ver == 2) fs = ((uint32_t)tag[pos + 3] << 16) | ((uint32_t)tag[pos + 4] << 8) | tag[pos + 5];
        else { fs = ver == 4 ? Sync32(&tag[pos + 4]) : BE32(&tag[pos + 4]); ff = tag[pos + 9]; }
        pos += hdrLen;
        if (fs == 0 || pos + fs > tag.size()) break;
        bool isPic = (ver == 2 && id == "PIC") || (ver > 2 && id == "APIC");
        bool skip = (ver == 3 && (ff & 0xC0)) || (ver == 4 && (ff & 0x0C));   // comprimido / criptografado
        if (isPic && !skip) {
            std::vector<unsigned char> fr(tag.begin() + pos, tag.begin() + pos + fs);
            if (ver == 4) { if (ff & 0x01) fr.erase(fr.begin(), fr.begin() + std::min<size_t>(4, fr.size())); if ((ff & 0x02) || (flags & 0x80)) Unsync(fr); }
            size_t q = 0;
            if (!fr.empty()) {
                unsigned char enc = fr[q++];
                if (ver == 2) q += 3; else { while (q < fr.size() && fr[q] != 0) ++q; ++q; }
                if (q < fr.size()) {
                    int ptype = fr[q++];
                    if (enc == 1 || enc == 2) { while (q + 1 < fr.size() && !(fr[q] == 0 && fr[q + 1] == 0)) q += 2; q += 2; }
                    else { while (q < fr.size() && fr[q] != 0) ++q; ++q; }
                    if (q < fr.size()) {
                        std::string data((char*)&fr[q], fr.size() - q);
                        if (IsImage(data) && bestType != 3 && (best.empty() || ptype == 3)) { best.swap(data); bestType = ptype; }
                    }
                }
            }
        }
        pos += fs;
    }
    if (best.empty()) return false;
    out.swap(best);
    return true;
}
inline bool FromMp3(const std::wstring& path, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    unsigned char h[10]; f.read((char*)h, 10);
    if (f.gcount() != 10 || memcmp(h, "ID3", 3) != 0) return false;
    uint32_t sz = Sync32(h + 6);
    if (sz == 0 || sz > MAXB) return false;
    std::vector<unsigned char> tag;
    if (!ReadN(f, tag, sz)) return false;
    return FromId3(std::move(tag), h[3], h[5], out);
}
// WAV (RIFF, chunk "id3 "/"ID3 ") e AIFF (FORM, chunk "ID3 ") com ID3v2 dentro
inline bool FromChunks(const std::wstring& path, bool big, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    unsigned char h[12]; f.read((char*)h, 12);
    if (f.gcount() != 12 || memcmp(h, big ? "FORM" : "RIFF", 4) != 0) return false;
    for (int g = 0; g < 512; ++g) {
        unsigned char c[8]; f.read((char*)c, 8);
        if (f.gcount() != 8) break;
        uint32_t len = big ? BE32(c + 4) : (uint32_t)(c[4] | (c[5] << 8) | (c[6] << 16) | ((uint32_t)c[7] << 24));
        if ((memcmp(c, "id3 ", 4) == 0 || memcmp(c, "ID3 ", 4) == 0) && len > 10 && len < MAXB) {
            std::vector<unsigned char> d;
            if (!ReadN(f, d, len) || memcmp(d.data(), "ID3", 3) != 0) return false;
            uint32_t sz = std::min<uint32_t>(Sync32(&d[6]), len - 10);
            std::vector<unsigned char> tag(d.begin() + 10, d.begin() + 10 + sz);
            return FromId3(std::move(tag), d[3], d[5], out);
        }
        f.seekg((std::streamoff)len + (len & 1), std::ios::cur);
        if (!f) break;
    }
    return false;
}
// bloco PICTURE do FLAC (o mesmo formato vai em base64 no METADATA_BLOCK_PICTURE do OGG)
inline bool FromPictureBlock(const unsigned char* d, size_t n, std::string& out, uint32_t* type = nullptr) {
    size_t q = 0;
    auto rd = [&](uint32_t& v) { if (q + 4 > n) return false; v = BE32(d + q); q += 4; return true; };
    uint32_t t, ml, dl, w, hh, dep, col, len;
    if (!rd(t) || !rd(ml) || q + ml > n) return false; q += ml;
    if (!rd(dl) || q + dl > n) return false; q += dl;
    if (!rd(w) || !rd(hh) || !rd(dep) || !rd(col) || !rd(len) || q + len > n) return false;
    std::string data((char*)d + q, len);
    if (!IsImage(data)) return false;
    if (type) *type = t;
    out.swap(data);
    return true;
}
inline bool FromFlac(const std::wstring& path, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    char magic[4]; f.read(magic, 4);
    if (f.gcount() != 4 || memcmp(magic, "fLaC", 4) != 0) return false;
    std::string best;
    for (int g = 0; g < 128; ++g) {
        unsigned char hdr[4]; f.read((char*)hdr, 4);
        if (f.gcount() != 4) break;
        bool last = (hdr[0] & 0x80) != 0; int type = hdr[0] & 0x7F;
        uint32_t len = ((uint32_t)hdr[1] << 16) | ((uint32_t)hdr[2] << 8) | hdr[3];
        if (type == 6 && len < MAXB) {
            std::vector<unsigned char> d;
            if (!ReadN(f, d, len)) break;
            std::string pic; uint32_t pt = 0;
            if (FromPictureBlock(d.data(), d.size(), pic, &pt) && (best.empty() || pt == 3)) { best.swap(pic); if (pt == 3) break; }
        } else f.seekg(len, std::ios::cur);
        if (last || !f) break;
    }
    if (best.empty()) return false;
    out.swap(best);
    return true;
}
inline std::string B64(const std::string& s) {
    static const std::string tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string o; int val = 0, bits = -8;
    for (unsigned char c : s) {
        if (c == '=') break;
        size_t p = tbl.find((char)c);
        if (p == std::string::npos) continue;
        val = (val << 6) + (int)p; bits += 6;
        if (bits >= 0) { o.push_back((char)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return o;
}
inline bool FromOgg(const std::wstring& path, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    std::vector<unsigned char> packet; int packets = 0;
    for (int page = 0; page < 1024; ++page) {
        unsigned char h[27]; f.read((char*)h, 27);
        if (f.gcount() != 27 || memcmp(h, "OggS", 4) != 0) return false;
        int nseg = h[26];
        std::vector<unsigned char> seg(nseg); f.read((char*)seg.data(), nseg);
        if (f.gcount() != nseg) return false;
        for (int i = 0; i < nseg; ++i) {
            int L = seg[i];
            size_t before = packet.size();
            if (before + L > MAXB) return false;
            packet.resize(before + L);
            if (L) { f.read((char*)&packet[before], L); if (f.gcount() != L) return false; }
            if (L == 255) continue;
            size_t pos = 0;
            if (packet.size() >= 7 && packet[0] == 3 && memcmp(&packet[1], "vorbis", 6) == 0) pos = 7;
            else if (packet.size() >= 8 && memcmp(packet.data(), "OpusTags", 8) == 0) pos = 8;
            if (pos) {
                auto u32 = [&](size_t p) -> uint32_t { return p + 4 > packet.size() ? 0 : (uint32_t)(packet[p] | (packet[p + 1] << 8) | (packet[p + 2] << 16) | ((uint32_t)packet[p + 3] << 24)); };
                uint32_t vl = u32(pos); pos += 4 + vl;
                uint32_t n = u32(pos); pos += 4;
                for (uint32_t k = 0; k < n && pos + 4 <= packet.size(); ++k) {
                    uint32_t len = u32(pos); pos += 4;
                    if (pos + len > packet.size()) break;
                    const char* kv = (const char*)&packet[pos];
                    const char key[] = "METADATA_BLOCK_PICTURE=";
                    if (len > sizeof(key) && strncasecmp(kv, key, sizeof(key) - 1) == 0) {
                        std::string raw = B64(std::string(kv + sizeof(key) - 1, len - (sizeof(key) - 1)));
                        if (FromPictureBlock((const unsigned char*)raw.data(), raw.size(), out)) return true;
                    }
                    pos += len;
                }
                return false;
            }
            packet.clear();
            if (++packets > 4) return false;
        }
    }
    return false;
}
// MP4: moov / udta / meta(4 bytes de versao) / ilst / covr / data(8 bytes de tipo)
inline bool FindCovr(const unsigned char* d, size_t n, std::string& out, int depth) {
    size_t off = 0;
    while (off + 8 <= n) {
        uint64_t sz = BE32(d + off); size_t hl = 8;
        if (sz == 1) { if (off + 16 > n) break; sz = ((uint64_t)BE32(d + off + 8) << 32) | BE32(d + off + 12); hl = 16; }
        else if (sz == 0) sz = n - off;
        if (sz < hl || off + sz > n) break;
        const unsigned char* body = d + off + hl; size_t bl = (size_t)sz - hl;
        if (memcmp(d + off + 4, "covr", 4) == 0) {
            size_t o2 = 0;
            while (o2 + 16 <= bl) {
                uint32_t s2 = BE32(body + o2);
                if (s2 < 16 || o2 + s2 > bl) break;
                if (memcmp(body + o2 + 4, "data", 4) == 0) { std::string img((char*)body + o2 + 16, s2 - 16); if (IsImage(img)) { out.swap(img); return true; } }
                o2 += s2;
            }
        } else if (depth < 6 && (memcmp(d + off + 4, "udta", 4) == 0 || memcmp(d + off + 4, "ilst", 4) == 0 || memcmp(d + off + 4, "moov", 4) == 0)) {
            if (FindCovr(body, bl, out, depth + 1)) return true;
        } else if (depth < 6 && memcmp(d + off + 4, "meta", 4) == 0) {
            if ((bl > 4 && FindCovr(body + 4, bl - 4, out, depth + 1)) || FindCovr(body, bl, out, depth + 1)) return true;
        }
        off += (size_t)sz;
    }
    return false;
}
inline bool FromMp4(const std::wstring& path, std::string& out) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end); uint64_t fsz = (uint64_t)f.tellg();
    uint64_t off = 0;
    for (int g = 0; g < 256 && off + 8 <= fsz; ++g) {
        f.seekg((std::streamoff)off);
        unsigned char h[16]; f.read((char*)h, 8);
        if (f.gcount() != 8) break;
        uint64_t sz = BE32(h); uint64_t hl = 8;
        if (sz == 1) { f.read((char*)h + 8, 8); sz = ((uint64_t)BE32(h + 8) << 32) | BE32(h + 12); hl = 16; }
        else if (sz == 0) sz = fsz - off;
        if (sz < hl) break;
        if (memcmp(h + 4, "moov", 4) == 0) {
            if (sz - hl > MAXB) return false;
            std::vector<unsigned char> mv;
            f.seekg((std::streamoff)(off + hl));
            if (!ReadN(f, mv, (size_t)(sz - hl))) return false;
            return FindCovr(mv.data(), mv.size(), out, 0);
        }
        off += sz;
    }
    return false;
}
inline bool CanHaveArt(const std::wstring& e) {
    return e == L".mp3" || e == L".aac" || e == L".flac" || e == L".ogg" || e == L".oga" || e == L".opus" || e == L".m4a" || e == L".mp4" || e == L".m4b" || e == L".alac" || e == L".wav" || e == L".aif" || e == L".aiff";
}
inline bool Extract(const std::wstring& path, const std::wstring& e, std::string& out) {
    if (e == L".mp3" || e == L".aac") return FromMp3(path, out);
    if (e == L".flac") return FromFlac(path, out);
    if (e == L".ogg" || e == L".oga" || e == L".opus") return FromOgg(path, out);
    if (e == L".m4a" || e == L".mp4" || e == L".m4b" || e == L".alac") return FromMp4(path, out);
    if (e == L".wav") return FromChunks(path, false, out);
    if (e == L".aif" || e == L".aiff") return FromChunks(path, true, out);
    return false;
}

// ---- cache + indice + thread de extracao ----------------------------------------
struct State {
    std::mutex m;
    bool loaded = false, worker = false;
    std::map<std::wstring, std::wstring> idx;   // chave -> nome do arquivo em art/ ou "-" (sem capa)
    std::deque<std::pair<std::wstring, std::wstring>> q;   // (caminho, extensao)
    std::set<std::wstring> queued;
    std::function<void(const std::wstring&, const std::wstring&)> onReady;   // (musica, imagem) -> chamado na thread
};
inline State& St() { static State* s = new State(); return *s; }
inline std::wstring Dir() { return Config::Join(Config::CacheDir(), L"art"); }
inline std::wstring Key(const std::wstring& path) {
    std::error_code ec;
    std::filesystem::path p(path);
    auto sz = std::filesystem::file_size(p, ec); if (ec) sz = 0;
    ec.clear();
    auto t = std::filesystem::last_write_time(p, ec);
    long long tt = ec ? 0LL : (long long)t.time_since_epoch().count();
    return path + L"|" + std::to_wstring((unsigned long long)sz) + L"|" + std::to_wstring(tt);
}
inline void LoadIndexLocked() {
    if (St().loaded) return;
    St().loaded = true;
    std::vector<std::wstring> ls;
    if (!ReadAllUtf8Lines(Config::Join(Dir(), L"index.tsv"), ls)) return;
    for (auto& l : ls) { size_t tab = l.rfind(L'\t'); if (tab != std::wstring::npos) St().idx[l.substr(0, tab)] = l.substr(tab + 1); }
}
inline void AppendIndexLocked(const std::wstring& key, const std::wstring& val) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(Dir()), ec);
    std::ofstream o(std::filesystem::path(Config::Join(Dir(), L"index.tsv")), std::ios::binary | std::ios::app);
    o << WideToUtf8(key + L"\t" + val) << "\n";
}
inline uint64_t Fnv(const std::string& s) { uint64_t h = 1469598103934665603ULL; for (unsigned char c : s) { h ^= c; h *= 1099511628211ULL; } return h; }
inline void Work() {
    for (;;) {
        std::pair<std::wstring, std::wstring> job;
        {
            std::lock_guard<std::mutex> lk(St().m);
            if (St().q.empty()) { St().worker = false; return; }
            job = St().q.front(); St().q.pop_front();
        }
        std::string img; bool got = false;
        try { got = Extract(job.first, job.second, img); } catch (...) { got = false; }
        std::wstring fileName;
        try {
        if (got) {
            std::error_code ec;
            std::wstring dir = Dir();
            std::filesystem::create_directories(std::filesystem::path(dir), ec);
            wchar_t name[32]; swprintf(name, 32, L"%016llx", (unsigned long long)Fnv(img));
            for (const wchar_t* e : { L".jpg", L".png" })
                if (std::filesystem::exists(std::filesystem::path(Config::Join(dir, std::wstring(name) + e)), ec)) { fileName = std::wstring(name) + e; break; }
            if (fileName.empty()) {
                bool png = memcmp(img.data(), "\x89PNG", 4) == 0;
                std::wstring raw = Config::Join(dir, std::wstring(name) + (png ? L".raw.png" : L".raw.jpg"));
                { std::ofstream o(std::filesystem::path(raw), std::ios::binary | std::ios::trunc); o.write(img.data(), (std::streamsize)img.size()); }
                std::wstring shr;
                try { shr = ShrinkCoverInto(dir, name, raw); } catch (...) { shr.clear(); }
                if (!shr.empty()) { std::filesystem::remove(std::filesystem::path(raw), ec); fileName = std::filesystem::path(shr).filename().wstring(); }
                else {
                    fileName = std::wstring(name) + (png ? L".png" : L".jpg");
                    ec.clear(); std::filesystem::rename(std::filesystem::path(raw), std::filesystem::path(Config::Join(dir, fileName)), ec);
                    if (ec) { std::filesystem::remove(std::filesystem::path(raw), ec); got = false; }
                }
            }
        }
        } catch (...) { got = false; PlatformLog("aviso: falha ao gravar uma capa embutida"); }
        std::function<void(const std::wstring&, const std::wstring&)> cb;
        {
            std::lock_guard<std::mutex> lk(St().m);
            std::wstring key = Key(job.first), val = got ? fileName : L"-";
            St().idx[key] = val; AppendIndexLocked(key, val);
            St().queued.erase(job.first);
            cb = St().onReady;
        }
        if (got && cb) cb(job.first, Config::Join(Dir(), fileName));
    }
}
// 1 = achou (out = imagem), 0 = sem capa embutida, -1 = ainda nao sabe (vai para a fila)
inline int Lookup(const std::wstring& path, const std::wstring& ext, std::wstring& out) {
    if (!CanHaveArt(ext)) return 0;
    std::wstring key = Key(path);
    std::lock_guard<std::mutex> lk(St().m);
    LoadIndexLocked();
    auto it = St().idx.find(key);
    if (it != St().idx.end()) {
        if (it->second == L"-") return 0;
        std::wstring full = Config::Join(Dir(), it->second);
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(full), ec)) { out = full; return 1; }
        St().idx.erase(it);
    }
    if (St().queued.insert(path).second) {
        St().q.push_back({ path, ext });
        if (!St().worker) { St().worker = true; std::thread(Work).detach(); }
    }
    return -1;
}
} // namespace art
