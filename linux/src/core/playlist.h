#pragma once
#include "platform.h"
#ifdef _WIN32
#include <gdiplus.h>
#include <shlobj.h>
#endif
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <memory>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include "config.h"

struct Track {
    std::wstring path;
    std::wstring title;
    std::wstring artist;    // lido das tags (ID3 / Vorbis comments), vazio se nao encontrado
    std::wstring coverPath; // pode ser vazio
    long long fileTime = 0; // data de modificacao (para ordenar por data)
    std::wstring url;       // musica online (playlist): path == url enquanto nao for baixada
    int durSec = 0;         // duracao conhecida pelos metadados (online)
};
inline bool IsOnlineTrack(const Track& t) { return !t.url.empty() && t.path == t.url; }

// ------------------------------------------------------- formatos ----------
// Nativos: decodificados pelo proprio player. "Extras": so entram na biblioteca
// quando ha um conversor externo (ffmpeg) disponivel — a casca liga a flag.
inline bool& ExtraFormatsEnabled(){ static bool v = false; return v; }
inline bool IsNativeAudioExt(const std::wstring& e){
    // miniaudio (Windows e Linux): mp3, wav, flac e ogg sem depender do ffmpeg
    return e == L".mp3" || e == L".wav" || e == L".flac" || e == L".ogg";
}
inline bool IsExtraAudioExt(const std::wstring& e){
    return e == L".m4a" || e == L".aac" || e == L".opus" || e == L".oga" || e == L".wma" || e == L".aiff" || e == L".aif" ||
           e == L".ape" || e == L".wv" || e == L".mka" || e == L".mp4" || e == L".webm" || e == L".m4b" || e == L".alac" || e == L".mpc";
}
inline bool IsAudioExt(const std::wstring& e){ return IsNativeAudioExt(e) || (ExtraFormatsEnabled() && IsExtraAudioExt(e)); }
inline std::wstring LowerExt(const std::filesystem::path& p){ auto e = p.extension().wstring(); for (auto& c : e) c = towlower(c); return e; }

// ------------------------------------------------------------ tags ID3 ----
// Le o artista (e opcionalmente titulo) de um MP3 via ID3v2 (TPE1/TP1) com
// fallback pra ID3v1 (ultimos 128 bytes do arquivo). Sem libs externas.

inline std::wstring DecodeID3Text(BYTE encoding, const std::vector<BYTE>& raw){
    if (raw.empty()) return L"";
    if (encoding == 0 || encoding == 3){
        // ISO-8859-1 (0) ou UTF-8 (3)
        std::string s(raw.begin(), raw.end());
        while (!s.empty() && s.back()=='\0') s.pop_back();
        if (s.empty()) return L"";
        if (encoding == 3){
            size_t z = s.find('\0');
            if (z != std::string::npos) s.resize(z); // termina no primeiro NUL
            return Utf8ToWide(s);
        } else {
            std::wstring w; w.reserve(s.size());
            for (unsigned char c : s) w.push_back((wchar_t)c); // ISO-8859-1 -> Unicode direto
            return w;
        }
    } else {
        // 1 = UTF-16 com BOM, 2 = UTF-16BE sem BOM
        size_t n = raw.size();
        size_t start = 0;
        bool swap = (encoding == 2);
        if (n >= 2 && raw[0]==0xFF && raw[1]==0xFE) { start = 2; swap = false; }
        else if (n >= 2 && raw[0]==0xFE && raw[1]==0xFF) { start = 2; swap = true; }
        std::wstring w;
        uint32_t hi = 0; // par substituto pendente (so importa onde wchar_t e 32 bits)
        for (size_t i = start; i+1 < n; i += 2){
            unsigned char b0 = raw[i], b1 = raw[i+1];
            uint32_t ch = swap ? (uint32_t)((b0<<8)|b1) : (uint32_t)((b1<<8)|b0);
            if (ch == 0) break;
            if (sizeof(wchar_t) == 4) {
                if (ch >= 0xD800 && ch <= 0xDBFF) { hi = ch; continue; }
                if (ch >= 0xDC00 && ch <= 0xDFFF) {
                    if (hi) { w.push_back((wchar_t)(0x10000 + ((hi - 0xD800) << 10) + (ch - 0xDC00))); hi = 0; }
                    continue;
                }
                hi = 0;
            }
            w.push_back((wchar_t)ch);
        }
        return w;
    }
}

struct Id3Info { std::wstring artist, title; };

inline Id3Info ReadId3Tags(const std::wstring& path){
    Id3Info info;
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f.good()) return info;

    // ---- ID3v2 (no comeco do arquivo) ----
    BYTE hdr[10];
    f.read((char*)hdr, 10);
    if (f.gcount() == 10 && hdr[0]=='I' && hdr[1]=='D' && hdr[2]=='3'){
        BYTE verMajor = hdr[3];
        bool unsynch = (hdr[5] & 0x80) != 0;
        bool extHdr  = (hdr[5] & 0x40) != 0;
        uint32_t tagSize = ((hdr[6]&0x7F)<<21) | ((hdr[7]&0x7F)<<14) | ((hdr[8]&0x7F)<<7) | (hdr[9]&0x7F);
        std::vector<BYTE> tag(tagSize);
        f.read((char*)tag.data(), tagSize);
        size_t pos = 0;
        if (extHdr && pos+4 <= tag.size()){
            uint32_t extSize = verMajor>=4
                ? (((tag[0]&0x7F)<<21)|((tag[1]&0x7F)<<14)|((tag[2]&0x7F)<<7)|(tag[3]&0x7F))
                : ((tag[0]<<24)|(tag[1]<<16)|(tag[2]<<8)|tag[3]);
            pos += (verMajor>=3 ? 4 : 0) + extSize;
        }
        int idLen = (verMajor==2) ? 3 : 4;
        while (pos + idLen + (verMajor==2?3:6) <= tag.size()){
            std::string fid((char*)&tag[pos], idLen);
            pos += idLen;
            uint32_t fsize;
            if (verMajor==2){
                fsize = (tag[pos]<<16)|(tag[pos+1]<<8)|tag[pos+2];
                pos += 3;
            } else if (verMajor>=4){
                fsize = ((tag[pos]&0x7F)<<21)|((tag[pos+1]&0x7F)<<14)|((tag[pos+2]&0x7F)<<7)|(tag[pos+3]&0x7F);
                pos += 6; // 4 size + 2 flags
            } else {
                fsize = (tag[pos]<<24)|(tag[pos+1]<<16)|(tag[pos+2]<<8)|tag[pos+3];
                pos += 6; // 4 size + 2 flags
            }
            if (fsize == 0 || pos + fsize > tag.size()) break;
            bool isArtist = (fid=="TPE1"||fid=="TP1");
            bool isTitle  = (fid=="TIT2"||fid=="TT2");
            if ((isArtist && info.artist.empty()) || (isTitle && info.title.empty())){
                BYTE enc = tag[pos];
                std::vector<BYTE> raw(tag.begin()+pos+1, tag.begin()+pos+fsize);
                std::wstring txt = DecodeID3Text(enc, raw);
                if (isArtist) info.artist = txt; else info.title = txt;
            }
            pos += fsize;
            if (!info.artist.empty() && !info.title.empty()) break;
        }
        (void)unsynch;
    }

    // ---- ID3v1 (ultimos 128 bytes) -- so usa se ID3v2 nao deu artista ----
    if (info.artist.empty()){
        f.clear();
        f.seekg(0, std::ios::end);
        std::streamoff fileLen = f.tellg();
        if (fileLen >= 128){
            f.seekg(fileLen - 128);
            char tag[128];
            f.read(tag, 128);
            if (f.gcount()==128 && tag[0]=='T' && tag[1]=='A' && tag[2]=='G'){
                std::string artist(tag+33, 30);
                size_t end = artist.find_last_not_of(" \0", std::string::npos, 2);
                artist = (end==std::string::npos) ? "" : artist.substr(0, end+1);
                if (!artist.empty()){
                    std::wstring w; w.reserve(artist.size());
                    for (unsigned char c : artist) w.push_back((wchar_t)c);
                    info.artist = w;
                }
            }
        }
    }
    return info;
}

// ---- Vorbis comments (FLAC / OGG Vorbis / Opus) ----
inline void ParseVorbisComments(const std::vector<BYTE>& d, size_t pos, Id3Info& info){
    auto u32 = [&](size_t p) -> uint32_t {
        if (p + 4 > d.size()) return 0;
        return (uint32_t)d[p] | ((uint32_t)d[p+1] << 8) | ((uint32_t)d[p+2] << 16) | ((uint32_t)d[p+3] << 24);
    };
    uint32_t vlen = u32(pos); pos += 4;
    if (pos + vlen > d.size()) return;
    pos += vlen;
    uint32_t n = u32(pos); pos += 4;
    for (uint32_t i = 0; i < n && pos + 4 <= d.size(); ++i){
        uint32_t len = u32(pos); pos += 4;
        if (pos + len > d.size()) break;
        std::string kv((const char*)&d[pos], len); pos += len;
        size_t eq = kv.find('=');
        if (eq == std::string::npos) continue;
        std::string key = kv.substr(0, eq);
        for (auto& c : key) c = (char)toupper((unsigned char)c);
        std::string val = kv.substr(eq + 1);
        if (key == "TITLE" && info.title.empty()) info.title = Utf8ToWide(val);
        else if (key == "ARTIST" && info.artist.empty()) info.artist = Utf8ToWide(val);
        if (!info.title.empty() && !info.artist.empty()) break;
    }
}
inline Id3Info ReadFlacTags(const std::wstring& path){
    Id3Info info;
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f.good()) return info;
    char magic[4]; f.read(magic, 4);
    if (f.gcount() != 4 || memcmp(magic, "fLaC", 4) != 0) return info;
    for (int guard = 0; guard < 64; ++guard){
        BYTE hdr[4]; f.read((char*)hdr, 4);
        if (f.gcount() != 4) break;
        bool last = (hdr[0] & 0x80) != 0; int type = hdr[0] & 0x7F;
        uint32_t len = ((uint32_t)hdr[1] << 16) | ((uint32_t)hdr[2] << 8) | hdr[3];
        if (type == 4){
            if (len > 8u * 1024u * 1024u) break;
            std::vector<BYTE> d(len); f.read((char*)d.data(), len);
            ParseVorbisComments(d, 0, info);
            break;
        }
        f.seekg(len, std::ios::cur);
        if (last) break;
    }
    return info;
}
// OGG: reconstroi os primeiros pacotes da stream (juntando paginas) ate achar o
// cabecalho de comentarios do Vorbis ("\x03vorbis") ou do Opus ("OpusTags").
inline Id3Info ReadOggTags(const std::wstring& path){
    Id3Info info;
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f.good()) return info;
    std::vector<BYTE> packet; int packets = 0; bool done = false;
    for (int page = 0; page < 64 && !done; ++page){
        BYTE h[27]; f.read((char*)h, 27);
        if (f.gcount() != 27 || memcmp(h, "OggS", 4) != 0) break;
        int nseg = h[26];
        std::vector<BYTE> seg(nseg); f.read((char*)seg.data(), nseg);
        if (f.gcount() != nseg) break;
        for (int i = 0; i < nseg && !done; ++i){
            int L = seg[i];
            size_t before = packet.size();
            if (before + L > 16u * 1024u * 1024u) { done = true; break; }
            packet.resize(before + L);
            if (L){ f.read((char*)&packet[before], L); if (f.gcount() != L) { done = true; break; } }
            if (L < 255){ // fim de pacote
                if (packet.size() >= 7 && packet[0] == 3 && memcmp(&packet[1], "vorbis", 6) == 0){ ParseVorbisComments(packet, 7, info); done = true; }
                else if (packet.size() >= 8 && memcmp(packet.data(), "OpusTags", 8) == 0){ ParseVorbisComments(packet, 8, info); done = true; }
                packet.clear();
                if (++packets > 3) done = true;
            }
        }
    }
    return info;
}
// MP4/M4A: moov/udta/meta/ilst com \xA9nam (titulo), \xA9ART (artista) e aART (artista do album).
// So le o 'udta' (pequeno); as tabelas de amostras do moov sao puladas.
inline uint32_t Mp4BE32(const unsigned char* p){ return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3]; }
inline void Mp4WalkTags(const unsigned char* d, size_t n, Id3Info& info, std::wstring& albumArtist, int depth){
    size_t off=0;
    while(off+8<=n){
        uint64_t sz=Mp4BE32(d+off); size_t hl=8;
        if(sz==1){ if(off+16>n) break; sz=((uint64_t)Mp4BE32(d+off+8)<<32)|Mp4BE32(d+off+12); hl=16; }
        else if(sz==0) sz=n-off;
        if(sz<hl||off+sz>n) break;
        const unsigned char* b=d+off+hl; size_t bl=(size_t)sz-hl; const char* ty=(const char*)d+off+4;
        auto text=[&]()->std::wstring{ size_t o=0; while(o+16<=bl){ uint32_t s2=Mp4BE32(b+o); if(s2<16||o+s2>bl) break; if(memcmp(b+o+4,"data",4)==0) return Utf8ToWide(std::string((const char*)b+o+16,s2-16)); o+=s2; } return L""; };
        if(memcmp(ty,"\xA9" "nam",4)==0){ if(info.title.empty()) info.title=text(); }
        else if(memcmp(ty,"\xA9" "ART",4)==0){ if(info.artist.empty()) info.artist=text(); }
        else if(memcmp(ty,"aART",4)==0){ if(albumArtist.empty()) albumArtist=text(); }
        else if(depth<6&&(memcmp(ty,"udta",4)==0||memcmp(ty,"ilst",4)==0)) Mp4WalkTags(b,bl,info,albumArtist,depth+1);
        else if(depth<6&&memcmp(ty,"meta",4)==0&&bl>4) Mp4WalkTags(b+4,bl-4,info,albumArtist,depth+1);
        off+=(size_t)sz;
    }
}
inline Id3Info ReadMp4Tags(const std::wstring& path){
    Id3Info info; std::wstring aart;
    std::ifstream f(std::filesystem::path(path),std::ios::binary); if(!f) return info;
    f.seekg(0,std::ios::end); uint64_t fsz=(uint64_t)f.tellg();
    auto header=[&](uint64_t off,uint64_t end,uint64_t& sz,uint64_t& hl,char* ty)->bool{
        if(off+8>end) return false;
        f.seekg((std::streamoff)off); unsigned char h[16]; f.read((char*)h,8); if(f.gcount()!=8) return false;
        sz=Mp4BE32(h); hl=8; memcpy(ty,h+4,4);
        if(sz==1){ f.read((char*)h+8,8); if(f.gcount()!=8) return false; sz=((uint64_t)Mp4BE32(h+8)<<32)|Mp4BE32(h+12); hl=16; }
        else if(sz==0) sz=end-off;
        return sz>=hl&&off+sz<=end;
    };
    uint64_t off=0,sz=0,hl=0; char ty[4];
    for(int g=0; g<64&&header(off,fsz,sz,hl,ty); ++g, off+=sz){
        if(memcmp(ty,"moov",4)!=0) continue;
        uint64_t end=off+sz, c=off+hl, csz=0, chl=0; char cty[4];
        for(int k=0; k<512&&header(c,end,csz,chl,cty); ++k, c+=csz){
            if(memcmp(cty,"udta",4)!=0&&memcmp(cty,"meta",4)!=0) continue;
            if(csz-chl>(uint64_t)16*1024*1024) continue;
            std::vector<unsigned char> buf((size_t)(csz-chl));
            f.seekg((std::streamoff)(c+chl)); f.read((char*)buf.data(),(std::streamsize)buf.size());
            if((uint64_t)f.gcount()!=buf.size()) break;
            if(memcmp(cty,"meta",4)==0){ if(buf.size()>4) Mp4WalkTags(buf.data()+4,buf.size()-4,info,aart,1); }
            else Mp4WalkTags(buf.data(),buf.size(),info,aart,1);
        }
        break;
    }
    if(info.artist.empty()) info.artist=aart;
    return info;
}
inline Id3Info ReadTags(const std::wstring& path, const std::wstring& extLower){
    if (extLower == L".mp3") return ReadId3Tags(path);
    if (extLower == L".m4a" || extLower == L".mp4" || extLower == L".m4b" || extLower == L".alac") return ReadMp4Tags(path);
    if (extLower == L".flac") return ReadFlacTags(path);
    if (extLower == L".ogg" || extLower == L".oga" || extLower == L".opus") return ReadOggTags(path);
    return Id3Info{};
}
inline long long FileTimeOf(const std::filesystem::path& p){
    std::error_code ec;
    auto ft = std::filesystem::last_write_time(p, ec);
    if (ec) return 0;
    return (long long)std::chrono::duration_cast<std::chrono::seconds>(ft.time_since_epoch()).count();
}

inline std::wstring FindCoverInFolder(const std::filesystem::path& folder) {
    const wchar_t* names[] = { L"cover.jpg", L"cover.png", L"folder.jpg", L"capa.jpg", L"capa.png" };
    std::error_code ec;
    for (auto n : names) {
        auto p = folder / n;
        if (std::filesystem::exists(p, ec)) return p.wstring();
    }
    // Fallback: qualquer imagem solta na pasta serve (musica nova adicionada
    // herda a capa que o usuario deixou na pasta, com qualquer nome).
    std::wstring best;
    for (std::filesystem::directory_iterator it(folder, std::filesystem::directory_options::skip_permission_denied, ec), end; !ec && it != end; it.increment(ec)) {
        const std::filesystem::directory_entry& e = *it;   // incremento com error_code: o range-for lancava excecao
        auto ext = e.path().extension().wstring();
        for (auto& c : ext) c = towlower(c);
        if (ext == L".jpg" || ext == L".jpeg" || ext == L".png" || ext == L".bmp" || ext == L".webp") {
            std::wstring name = e.path().filename().wstring();
            bool hidden = !name.empty() && name[0] == L'.';
            if (!hidden && best.empty()) best = e.path().wstring();
        }
    }
    return best;
}

// covers.ini: mapeia caminho_da_musica=caminho_da_imagem (capas escolhidas manualmente)
// Os caminhos sao gravados em formato portatil (relativos a BaseDir quando
// possivel) e resolvidos na leitura, entao sobrevivem a mudanca de pasta/maquina.
inline std::map<std::wstring, std::wstring> LoadCustomCovers() {
    std::map<std::wstring, std::wstring> m;
    std::vector<std::wstring> ls;
    if (!ReadAllUtf8Lines(Config::CoversPath(), ls)) return m;
    for (auto& line : ls) {
        size_t eq = line.find_last_of(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = Config::FromPortable(line.substr(0, eq));
        std::wstring v = Config::FromPortable(line.substr(eq + 1));
        if (!k.empty() && !v.empty()) m[k] = v;
    }
    return m;
}

// ------------------------------------------------ capas otimizadas ----
// Toda capa escolhida pelo usuario e reencodada para caber em COVER_MAX_DIM
// antes de ir para assets/covers. A maior exibicao do player (~300 px * escala
// de UI) fica bem abaixo disso, entao nao ha perda visivel — e uma foto de
// celular de 4 MB vira ~100 KB em disco.
static const int COVER_MAX_DIM = 512;
static const ULONG COVER_JPEG_QUALITY = 88;

#ifdef _WIN32
inline int FindEncoderClsid(const WCHAR* mime, CLSID* out) {
    UINT n = 0, sz = 0;
    if (Gdiplus::GetImageEncodersSize(&n, &sz) != Gdiplus::Ok || !n || !sz) return -1;
    std::vector<BYTE> buf(sz);
    auto* enc = reinterpret_cast<Gdiplus::ImageCodecInfo*>(buf.data());
    if (Gdiplus::GetImageEncoders(n, sz, enc) != Gdiplus::Ok) return -1;
    for (UINT i = 0; i < n; ++i)
        if (wcscmp(enc[i].MimeType, mime) == 0) { *out = enc[i].Clsid; return (int)i; }
    return -1;
}

// Reencodada a imagem ja redimensionada (mantendo proporcao) no mime pedido.
inline bool SaveResized(Gdiplus::Image* img, const std::wstring& dest, const WCHAR* mime) {
    UINT w = img->GetWidth(), h = img->GetHeight();
    if (!w || !h) return false;
    double sc = (double)COVER_MAX_DIM / (double)(w > h ? w : h);
    UINT nw = (UINT)std::max(1.0, std::floor(w * sc));
    UINT nh = (UINT)std::max(1.0, std::floor(h * sc));
    bool png = (wcscmp(mime, L"image/png") == 0);
    CLSID clsid;
    if (FindEncoderClsid(mime, &clsid) < 0) return false;
    Gdiplus::Bitmap out(nw, nh, png ? PixelFormat32bppARGB : PixelFormat24bppRGB);
    if (out.GetLastStatus() != Gdiplus::Ok) return false;
    Gdiplus::Graphics* g = Gdiplus::Graphics::FromImage(&out);
    if (!g) return false;
    g->SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    g->SetSmoothingMode(Gdiplus::SmoothingModeHighQuality);
    g->SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    g->Clear(png ? Gdiplus::Color(0, 0, 0, 0) : Gdiplus::Color(255, 255, 255, 255));
    g->DrawImage(img, Gdiplus::Rect(0, 0, (INT)nw, (INT)nh), 0, 0, (INT)w, (INT)h, Gdiplus::UnitPixel);
    Gdiplus::Status st = g->GetLastStatus();
    delete g;
    if (st != Gdiplus::Ok) return false;
    if (!png) {
        Gdiplus::EncoderParameters ep;
        ep.Count = 1;
        ep.Parameter[0].Guid = Gdiplus::EncoderQuality;
        ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
        ep.Parameter[0].NumberOfValues = 1;
        ULONG q = COVER_JPEG_QUALITY;
        ep.Parameter[0].Value = &q;
        return out.Save(dest.c_str(), &clsid, &ep) == Gdiplus::Ok;
    }
    return out.Save(dest.c_str(), &clsid, nullptr) == Gdiplus::Ok;
}

// Tenta gravar imgPath redimensionada dentro de dir com nome-base "name".
// A extensao acompanha o conteudo: .png se tiver transparencia, .jpg se opaca.
// Retorna o caminho gravado, ou L"" quando nao vale a pena (imagem pequena)
// ou algo falhou (o chamador entao usa o fallback de copia crua).
inline std::wstring ShrinkCoverInto(const std::wstring& dir, const wchar_t* name,
                                    const std::wstring& imgPath) {
    std::unique_ptr<Gdiplus::Image> img(new Gdiplus::Image(imgPath.c_str()));
    if (img->GetLastStatus() != Gdiplus::Ok) return L"";
    UINT w = img->GetWidth(), h = img->GetHeight();
    if (!w || !h || (w <= (UINT)COVER_MAX_DIM && h <= (UINT)COVER_MAX_DIM)) return L"";
    bool alpha = (img->GetPixelFormat() & PixelFormatAlpha) != 0;
    const WCHAR* mime = alpha ? L"image/png" : L"image/jpeg";
    std::wstring dest = dir + L"\\" + name + (alpha ? L".png" : L".jpg");
    return SaveResized(img.get(), dest, mime) ? dest : L"";
}
#else
// Linux: implementadas na casca (linux/main_linux.cpp) com o raylib/stb_image.
std::wstring ShrinkCoverInto(const std::wstring& dir, const wchar_t* name, const std::wstring& imgPath);
int MigrateOversizedCovers();
#endif

inline std::wstring EnsureInternalCover(const std::wstring& trackPath, const std::wstring& imgPath) {
    if (trackPath.empty() || imgPath.empty()) return L"";
    namespace fs = std::filesystem;
    std::wstring dir = Config::CoversDir();
    std::error_code ec;
    fs::create_directories(fs::path(dir), ec);
    if (ec) return L"";
    size_t h1 = std::hash<std::wstring>{}(trackPath);
    size_t h2 = std::hash<std::wstring>{}(imgPath);
    wchar_t name[80];
    swprintf(name, 80, L"%016llX_%016llX", (unsigned long long)h1, (unsigned long long)h2);

    // 1a) tenta gravar ja otimizada (.jpg se opaca, .png se tem transparencia)
    std::wstring shrunk = ShrinkCoverInto(dir, name, imgPath);
    if (!shrunk.empty()) {
        // limpa possivel versao antiga com a outra extensao
        const wchar_t* all[2] = { L".jpg", L".png" };
        const wchar_t* cur = shrunk.ends_with(L".png") ? L".png" : L".jpg";
        for (auto ext : all)
            if (ext != cur) fs::remove(fs::path(Config::Join(dir, std::wstring(name) + ext)), ec);
        return shrunk;
    }
    // 2) fallback: copia crua, comportamento antigo
    std::wstring ext = fs::path(imgPath).extension().wstring();
    if (ext.empty()) ext = L".jpg";
    std::wstring dest = Config::Join(dir, std::wstring(name) + ext);
    ec.clear();
    bool ok = fs::copy_file(fs::path(imgPath), fs::path(dest), fs::copy_options::overwrite_existing, ec);
    if (!ok || ec) return L"";
    return dest;
}

inline std::wstring SaveCustomCover(const std::wstring& trackPath, const std::wstring& imgPath) {
    auto internal = EnsureInternalCover(trackPath, imgPath);
    if (internal.empty()) return L"";
    auto m = LoadCustomCovers();
    m[trackPath] = internal;
    std::vector<std::wstring> ls;
    for (auto& kv : m)
        ls.push_back(Config::ToPortable(kv.first) + L"=" + Config::ToPortable(kv.second));
    WriteAllUtf8Lines(Config::CoversPath(), ls);
    return internal;
}

#include "cover_art.h"
// Capa de uma faixa: escolhida pelo usuario > embutida no arquivo > imagem da pasta.
// A embutida desconhecida entra na fila de extracao (thread) e chega depois.
inline void ResolveTrackCover(Track& t, const std::filesystem::path& p, const std::wstring& ext, const std::map<std::wstring, std::wstring>& customCovers) {
    std::error_code ec;
    auto itc = customCovers.find(t.path);
    if (itc != customCovers.end() && std::filesystem::exists(std::filesystem::path(itc->second), ec)) { t.coverPath = itc->second; return; }
    std::wstring emb;
    if (art::Lookup(t.path, ext, emb) == 1) { t.coverPath = emb; return; }
    t.coverPath = FindCoverInFolder(p.parent_path());
}

#ifdef _WIN32
// Reencoda capas antigas maiores que COVER_MAX_DIM que ja estao em
// assets\covers. O nome/extensao de cada arquivo e PRESERVADO porque
// covers.ini aponta para eles; o original fica de backup em _originais/
// (apague essa pasta quando quiser liberar o espaco). Retorna quantas mudaram.
inline int MigrateOversizedCovers() {
    namespace fs = std::filesystem;
    std::wstring dir = Config::CoversDir();
    std::error_code ec;
    if (!fs::exists(dir, ec) || ec) return 0;
    std::wstring bakDir = dir + L"\\_originais";

    std::vector<fs::path> files;
    for (fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (ec) break;
        { std::error_code e2; if (!it->is_regular_file(e2)) continue; }
        auto ext = it->path().extension().wstring();
        for (auto& c : ext) c = towlower(c);
        if (ext == L".png" || ext == L".jpg" || ext == L".jpeg") files.push_back(it->path());
    }

    int changed = 0;
    for (auto& p : files) {
        // Le o arquivo para memoria e decodifica de la: Image(path) deixaria um
        // handle aberto no PNG/JPG e o MoveFileW do backup falharia com erro 32.
        std::ifstream f(p.c_str(), std::ios::binary);
        if (!f.good()) continue;
        std::vector<BYTE> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        f.close();
        if (data.empty()) continue;

        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, data.size());
        if (!hg) continue;
        void* pv = GlobalLock(hg);
        if (!pv) { GlobalFree(hg); continue; }
        memcpy(pv, data.data(), data.size());
        GlobalUnlock(hg);
        IStream* stm = nullptr;
        if (CreateStreamOnHGlobal(hg, TRUE, &stm) != S_OK) { GlobalFree(hg); continue; }

        auto lext = p.extension().wstring();
        for (auto& c : lext) c = towlower(c);

        Gdiplus::Image img(stm);
        if (img.GetLastStatus() == Gdiplus::Ok) {
            UINT w = img.GetWidth(), h = img.GetHeight();
            if (w && h && (w > (UINT)COVER_MAX_DIM || h > (UINT)COVER_MAX_DIM)) {
                const WCHAR* mime = (lext == L".png") ? L"image/png" : L"image/jpeg";
                fs::create_directories(bakDir, ec);
                std::wstring orig = bakDir + L"\\" + p.filename().wstring();
                SetFileAttributesW(p.c_str(), FILE_ATTRIBUTE_NORMAL);
                if (MoveFileW(p.c_str(), orig.c_str())) {
                    if (SaveResized(&img, p.wstring(), mime)) ++changed;
                    else MoveFileW(orig.c_str(), p.c_str()); // falhou: devolve o original
                } // arquivo travado/em uso: pula
            }
        }
        stm->Release(); // libera o HGLOBAL (fDeleteOnRelease)
    }
    return changed;
}
#endif

// artists.ini: mapeia caminho_da_musica=nome_do_artista (renomeado pelo usuario)
// Chaves em formato portatil, igual covers.ini.
inline std::map<std::wstring, std::wstring> LoadCustomArtists() {
    std::map<std::wstring, std::wstring> m;
    std::vector<std::wstring> ls;
    if (!ReadAllUtf8Lines(Config::ArtistsPath(), ls)) return m;
    for (auto& line : ls) {
        size_t eq = line.find_last_of(L'=');
        if (eq == std::wstring::npos) continue;
        auto k = Config::FromPortable(line.substr(0, eq)), v = line.substr(eq + 1);
        if (!v.empty() && !k.empty()) m[k] = v;
    }
    return m;
}

inline void SaveCustomArtist(const std::wstring& trackPath, const std::wstring& artist) {
    if (trackPath.empty()) return;
    auto m = LoadCustomArtists();
    if (artist.empty()) m.erase(trackPath); else m[trackPath] = artist;
    std::vector<std::wstring> ls;
    for (auto& kv : m) if (!kv.second.empty())
        ls.push_back(Config::ToPortable(kv.first) + L"=" + kv.second);
    WriteAllUtf8Lines(Config::ArtistsPath(), ls);
}

// DFS manual usando apenas APIs com error_code: nao lanca excecao e nao
// entra em junctions/reparse points (ex.: C:\Arquivos de Programas), que
// derrubam o recursive_directory_iterator do GCC 16. No Linux, links
// simbolicos e pastas ocultas (.cache, .local, .steam...) tambem sao pulados.
template <typename F>
inline void WalkFiles(const std::wstring& root, F&& visit) {
    std::vector<std::wstring> dirs{root};
    while (!dirs.empty()) {
        std::wstring cur = dirs.back(); dirs.pop_back();
        std::error_code ec;
        std::filesystem::directory_iterator it(std::filesystem::path(cur), std::filesystem::directory_options::skip_permission_denied, ec), end;
        if (ec) { ec.clear(); continue; }
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); break; }
            std::error_code ec2;
            const std::filesystem::directory_entry& e = *it;
            auto st = e.symlink_status(ec2);
            if (ec2) { ec2.clear(); continue; }
            const auto ty = st.type();
            if (ty == std::filesystem::file_type::directory) {
                std::wstring name = e.path().filename().wstring();
#ifdef _WIN32
                DWORD attr = GetFileAttributesW(e.path().c_str());
                if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
                if (name == L"Windows" || name == L"Program Files" || name == L"Program Files (x86)" ||
                    name == L"ProgramData" || name == L"$Recycle.Bin" || name == L"System Volume Information" ||
                    name == L"AppData" || name == L"node_modules" || name == L".git" || name == L"SteamLibrary" || name == L"steamapps") continue;
#else
                if (!name.empty() && name[0] == L'.') continue;
                if (name == L"node_modules" || name == L"lost+found" || name == L"proc" || name == L"sys" || name == L"dev" ||
                    name == L"steamapps" || name == L"SteamLibrary" || name == L"Steam") continue;
#endif
                dirs.push_back(e.path().wstring());
            } else if (ty == std::filesystem::file_type::regular) {
                visit(e.path());
            }
        }
    }
}

// Repara associacoes antigas salvas com caminhos absolutos de uma pasta antiga
// (ex.: o app foi movido de lugar). Para cada entrada apontando para arquivo
// inexistente, procura um arquivo de mesmo nome dentro da BaseDir
// (musicas na pasta de musica, capas em assets/covers) e regrava os .inis em
// formato portatil. Nao faz nada se tudo ja existe/esta portatil.

inline bool FileMissing(const std::wstring& p) {
    if (p.empty()) return false;
#ifdef _WIN32
    DWORD attr = GetFileAttributesW(p.c_str());
    return attr == INVALID_FILE_ATTRIBUTES;
#else
    std::error_code ec;
    return !std::filesystem::exists(std::filesystem::path(p), ec);
#endif
}

inline void RewritePortableIni(const std::wstring& path,
                               const std::vector<std::pair<std::wstring, std::wstring>>& rows) {
    std::vector<std::wstring> ls;
    for (auto& kv : rows) ls.push_back(Config::ToPortable(kv.first) + L"=" + Config::ToPortable(kv.second));
    WriteAllUtf8Lines(path, ls);
}

// Indice nome-de-arquivo (minusculo) -> caminho, construido numa unica
// varredura da raiz. Usado para relocalizar arquivos apos o app mudar de pasta.
struct NameIndex {
    std::map<std::wstring, std::wstring> m;
    bool built = false;
    std::wstring root;
    NameIndex() : root(L"") {}
    explicit NameIndex(const std::wstring& r) : root(r) {}
    void Build() {
        if (built) return;
        built = true;
        if (root.empty()) return;
        WalkFiles(root, [&](const std::filesystem::path& p) {
            std::wstring low = p.filename().wstring();
            for (auto& c : low) c = towlower(c);
            if (!m.count(low)) m[low] = p.wstring();
        });
    }
    std::wstring Find(const std::wstring& fileName) {
        Build();
        std::wstring low = fileName;
        for (auto& c : low) c = towlower(c);
        auto it = m.find(low);
        return it == m.end() ? L"" : it->second;
    }
};

inline void MigrateIniAssociations(const std::wstring& musicRootIn) {
    namespace fs = std::filesystem;
    std::wstring musicRoot = musicRootIn;
    if (musicRoot.empty() || !Config::DirExists(musicRoot)) {
        std::wstring cand = Config::Join(Config::BaseDir(), L"Musica");
        musicRoot = Config::DirExists(cand) ? cand : Config::BaseDir();
    }
    std::wstring coversDir = Config::CoversDir();
    NameIndex musicIdx(musicRoot), coverIdx(coversDir);

    // Politica de reancoragem: se o caminho salvo esta fora da pasta do app e
    // existe arquivo de mesmo nome dentro do app (ou o caminho salvou sumiu),
    // aponta pro arquivo local em formato portatil.
    auto reanchor = [&](std::wstring& path, NameIndex& idx) -> bool {
        if (path.empty()) return false;
        bool inside = Config::IsInsideExeDir(path);
        if (inside && !FileMissing(path)) return false; // ja correto
        std::wstring cand = idx.Find(fs::path(path).filename().wstring());
        if (!cand.empty() && _wcsicmp(cand.c_str(), path.c_str()) != 0) { path = cand; return true; }
        return false;
    };
    auto needsPortable = [](const std::wstring& p) {
        return !p.empty() && Config::IsAbsPath(p) && Config::IsInsideExeDir(p);
    };

    // --- covers.ini: chave e valor sao arquivos ---
    {
        auto m = LoadCustomCovers();
        if (!m.empty()) {
            bool changed = false;
            std::map<std::wstring, std::wstring> fix;
            for (auto& kv : m) {
                std::wstring k = kv.first, v = kv.second;
                if (reanchor(k, musicIdx)) changed = true;
                if (reanchor(v, coverIdx)) changed = true;
                if (needsPortable(k) || needsPortable(v)) changed = true;
                fix[k] = v;
            }
            if (changed) {
                std::vector<std::pair<std::wstring, std::wstring>> rows(fix.begin(), fix.end());
                RewritePortableIni(Config::CoversPath(), rows);
            }
        }
    }

    // --- artists.ini: so a chave e arquivo ---
    {
        auto m = LoadCustomArtists();
        if (!m.empty()) {
            bool changed = false;
            std::map<std::wstring, std::wstring> fix;
            for (auto& kv : m) {
                std::wstring k = kv.first;
                if (reanchor(k, musicIdx)) changed = true;
                if (needsPortable(k)) changed = true;
                fix[k] = kv.second;
            }
            if (changed) {
                std::vector<std::pair<std::wstring, std::wstring>> rows(fix.begin(), fix.end());
                RewritePortableIni(Config::ArtistsPath(), rows);
            }
        }
    }
}

// Pastas onde a varredura "padrao" procura musica.
inline std::vector<std::wstring> UserMusicRoots() {
    std::vector<std::wstring> roots;
#ifdef _WIN32
    const KNOWNFOLDERID ids[] = { FOLDERID_Music, FOLDERID_Downloads, FOLDERID_Documents, FOLDERID_Desktop };
    for (auto& id : ids) {
        PWSTR p = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(id, 0, NULL, &p)) && p) { roots.push_back(p); CoTaskMemFree(p); }
    }
    wchar_t drives[512] = {0}; DWORD n = GetLogicalDriveStringsW(511, drives);
    for (DWORD i = 0; i < n;) {
        std::wstring root = &drives[i];
        if (GetDriveTypeW(root.c_str()) == DRIVE_REMOVABLE) roots.push_back(root);
        i += (DWORD)wcslen(&drives[i]) + 1;
    }
#else
    const char* home = std::getenv("HOME");
    const char* user = std::getenv("USER");
    std::string h = home ? home : "";
    // ~/.config/user-dirs.dirs (XDG) traz os nomes localizados (Musicas, Transferencias...)
    std::map<std::string, std::string> xdg;
    {
        std::vector<std::wstring> ls;
        if (!h.empty() && ReadAllUtf8Lines(Utf8ToWide(h + "/.config/user-dirs.dirs"), ls)) {
            for (auto& wl : ls) {
                std::string l = WideToUtf8(wl);
                if (l.rfind("XDG_", 0) != 0) continue;
                size_t eq = l.find('='); if (eq == std::string::npos) continue;
                std::string k = l.substr(0, eq), v = l.substr(eq + 1);
                if (v.size() >= 2 && v.front() == '"') v = v.substr(1, v.size() - 2);
                size_t hp = v.find("$HOME"); if (hp != std::string::npos) v.replace(hp, 5, h);
                xdg[k] = v;
            }
        }
    }
    auto add = [&](const char* key, const char* fallback) {
        auto it = xdg.find(key);
        std::string p = (it != xdg.end() && !it->second.empty()) ? it->second : (h + "/" + fallback);
        roots.push_back(Utf8ToWide(p));
    };
    add("XDG_MUSIC_DIR", "Music"); add("XDG_DOWNLOAD_DIR", "Downloads"); add("XDG_DOCUMENTS_DIR", "Documents"); add("XDG_DESKTOP_DIR", "Desktop");
    if (user && *user) { roots.push_back(Utf8ToWide(std::string("/media/") + user)); roots.push_back(Utf8ToWide(std::string("/run/media/") + user)); }
#endif
    // remove duplicadas e pastas que sao subpastas de outra da lista
    std::vector<std::wstring> out;
    for (auto& r : roots) {
        std::wstring n = Config::NormSep(r); while (n.size() > 1 && n.back() == REMIX_SEP) n.pop_back();
        bool dup = false;
        for (auto& o : out) if (o == n || Config::StartsI(n, o)) { dup = true; break; }
        if (!dup) out.push_back(n);
    }
    return out;
}

inline std::vector<Track> ScanComputerMusic() {
    std::vector<Track> tracks;
    std::error_code ec;
    auto customCovers = LoadCustomCovers();
    auto add = [&](const std::filesystem::path& p) {
        auto ext = LowerExt(p);
        if (!IsAudioExt(ext)) return;
        Track t; t.path=p.wstring(); t.title=p.stem().wstring(); t.fileTime=FileTimeOf(p);
        { auto tg=ReadTags(t.path, ext); if(!tg.title.empty()) t.title=tg.title; if(!tg.artist.empty()) t.artist=tg.artist; }
        if (t.artist.empty()) t.artist=L"Artista desconhecido";
        ResolveTrackCover(t, p, ext, customCovers);
        tracks.push_back(std::move(t));
    };
    // "Padrao": so as pastas do usuario (Musicas, Downloads, Documentos, Area de
    // trabalho) + midias removiveis. Varrer o PC inteiro demorava demais e trazia
    // audio de jogos/programas.
    std::vector<std::wstring> roots = UserMusicRoots();
    for (auto& r : roots) if (Config::DirExists(r)) WalkFiles(r, add);
    std::sort(tracks.begin(),tracks.end(),[](const Track&a,const Track&b){return _wcsicmp(a.title.c_str(),b.title.c_str())<0;});
    return tracks;
}

inline std::vector<Track> ScanFolder(const std::wstring& folder) {
    std::vector<Track> tracks;
    std::error_code ec;
    if (folder.empty() || !std::filesystem::exists(std::filesystem::path(folder), ec)) return tracks;

    auto customCovers = LoadCustomCovers();

    WalkFiles(folder, [&](const std::filesystem::path& p) {
        auto ext = LowerExt(p);
        if (!IsAudioExt(ext)) return;
        Track t;
        t.path = p.wstring();
        t.title = p.stem().wstring();
        t.fileTime = FileTimeOf(p);
        {
            Id3Info tg = ReadTags(t.path, ext);
            if (!tg.title.empty())  t.title  = tg.title;
            if (!tg.artist.empty()) t.artist = tg.artist;
        }
        if (t.artist.empty()) t.artist = L"Artista desconhecido";
        ResolveTrackCover(t, p, ext, customCovers);
        tracks.push_back(std::move(t));
    });
    std::sort(tracks.begin(), tracks.end(), [](const Track& a, const Track& b) { return a.title < b.title; });
    return tracks;
}

// ------------------------------------------------- ordem da playlist -------
// order.ini: um caminho (portatil) por linha, na ordem escolhida pelo usuario.
inline std::vector<std::wstring> LoadCustomOrder() {
    std::vector<std::wstring> v, ls;
    if (!ReadAllUtf8Lines(Config::OrderPath(), ls)) return v;
    for (auto& l : ls) { auto t = Config::Trim(l); if (!t.empty()) v.push_back(Config::FromPortable(t)); }
    return v;
}
inline void SaveCustomOrder(const std::vector<Track>& tracks) {
    std::vector<std::wstring> ls;
    for (auto& t : tracks) ls.push_back(Config::ToPortable(t.path));
    WriteAllUtf8Lines(Config::OrderPath(), ls);
}
inline std::wstring LowerW(std::wstring s) { for (auto& c : s) c = towlower(c); return s; }
// mode: title | artist | file | date | manual. No manual, 'desc' e ignorado e
// faixas fora do order.ini vao para o fim (por titulo).
inline void SortTracks(std::vector<Track>& t, const std::wstring& mode, bool desc) {
    if (mode == L"manual") {
        auto order = LoadCustomOrder();
        std::map<std::wstring, size_t> pos;
        for (size_t i = 0; i < order.size(); ++i) pos.emplace(order[i], i);
        std::stable_sort(t.begin(), t.end(), [&](const Track& a, const Track& b) {
            auto ia = pos.find(a.path), ib = pos.find(b.path);
            size_t ra = ia == pos.end() ? (size_t)-1 : ia->second, rb = ib == pos.end() ? (size_t)-1 : ib->second;
            if (ra != rb) return ra < rb;
            return LowerW(a.title) < LowerW(b.title);
        });
        return;
    }
    auto less = [&](const Track& a, const Track& b) -> bool {
        if (mode == L"artist") { int c = LowerW(a.artist).compare(LowerW(b.artist)); if (c != 0) return c < 0; return LowerW(a.title) < LowerW(b.title); }
        if (mode == L"file")   { return LowerW(a.path) < LowerW(b.path); }
        if (mode == L"date")   { if (a.fileTime != b.fileTime) return a.fileTime > b.fileTime; return LowerW(a.title) < LowerW(b.title); }
        return LowerW(a.title) < LowerW(b.title);
    };
    std::stable_sort(t.begin(), t.end(), [&](const Track& a, const Track& b) { return desc ? less(b, a) : less(a, b); });
}
