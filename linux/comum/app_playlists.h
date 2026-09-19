#pragma once
// Playlists (compartilhado Windows/Linux). Cada playlist e uma pasta em
// <BaseDir>/playlists/<slug>/ com um playlist.json que guarda so os CAMINHOS das
// musicas (nada e copiado nem apagado). Se um arquivo mudou de lugar, o app tenta
// achar pelo nome + tamanho na biblioteca e corrige o caminho sozinho; se nao
// achar, a faixa fica marcada como ausente (e volta quando o arquivo voltar).
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <set>

// ---- JSON minimo (so o que o formato da playlist precisa) ---------------------
struct JVal {
    enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t=NUL;
    bool b=false; double n=0; std::string s; std::vector<JVal> a; std::vector<std::pair<std::string,JVal>> o;
    const JVal* get(const char* k) const { if(t!=OBJ) return nullptr; for(auto& kv:o) if(kv.first==k) return &kv.second; return nullptr; }
    std::string str(const char* k,const std::string& d="") const { auto v=get(k); return (v&&v->t==STR)?v->s:d; }
    double num(const char* k,double d=0) const { auto v=get(k); return (v&&v->t==NUM)?v->n:d; }
};
struct JParser {
    const std::string& s; size_t i=0;
    explicit JParser(const std::string& src):s(src){}
    void ws(){ while(i<s.size()&&(s[i]==' '||s[i]=='\n'||s[i]=='\r'||s[i]=='\t')) i++; }
    static void putUtf8(std::string& o,unsigned cp){
        if(cp<0x80) o.push_back((char)cp);
        else if(cp<0x800){ o.push_back((char)(0xC0|(cp>>6))); o.push_back((char)(0x80|(cp&0x3F))); }
        else if(cp<0x10000){ o.push_back((char)(0xE0|(cp>>12))); o.push_back((char)(0x80|((cp>>6)&0x3F))); o.push_back((char)(0x80|(cp&0x3F))); }
        else { o.push_back((char)(0xF0|(cp>>18))); o.push_back((char)(0x80|((cp>>12)&0x3F))); o.push_back((char)(0x80|((cp>>6)&0x3F))); o.push_back((char)(0x80|(cp&0x3F))); }
    }
    bool parseStr(JVal& out){
        if(i>=s.size()||s[i]!='"') return false; i++; out.t=JVal::STR; std::string r;
        while(i<s.size()){ char c=s[i++]; if(c=='"'){ out.s=r; return true; }
            if(c=='\\'&&i<s.size()){ char e=s[i++];
                switch(e){ case 'n': r.push_back('\n'); break; case 't': r.push_back('\t'); break; case 'r': r.push_back('\r'); break; case 'b': r.push_back('\b'); break; case 'f': r.push_back('\f'); break;
                case 'u': { if(i+4>s.size()) return false; unsigned cp=(unsigned)strtoul(s.substr(i,4).c_str(),nullptr,16); i+=4;
                    if(cp>=0xD800&&cp<=0xDBFF&&i+6<=s.size()&&s[i]=='\\'&&s[i+1]=='u'){ unsigned lo=(unsigned)strtoul(s.substr(i+2,4).c_str(),nullptr,16); if(lo>=0xDC00&&lo<=0xDFFF){ cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00); i+=6; } }
                    putUtf8(r,cp); break; }
                default: r.push_back(e); } }
            else r.push_back(c); }
        return false;
    }
    bool parse(JVal& out){
        ws(); if(i>=s.size()) return false; char c=s[i];
        if(c=='{'){ out.t=JVal::OBJ; i++; ws(); if(i<s.size()&&s[i]=='}'){ i++; return true; }
            while(i<s.size()){ ws(); JVal k; if(!parseStr(k)) return false; ws(); if(i>=s.size()||s[i]!=':') return false; i++; JVal v; if(!parse(v)) return false; out.o.emplace_back(k.s,std::move(v)); ws();
                if(i<s.size()&&s[i]==','){ i++; continue; } if(i<s.size()&&s[i]=='}'){ i++; return true; } return false; }
            return false; }
        if(c=='['){ out.t=JVal::ARR; i++; ws(); if(i<s.size()&&s[i]==']'){ i++; return true; }
            while(i<s.size()){ JVal v; if(!parse(v)) return false; out.a.push_back(std::move(v)); ws();
                if(i<s.size()&&s[i]==','){ i++; continue; } if(i<s.size()&&s[i]==']'){ i++; return true; } return false; }
            return false; }
        if(c=='"') return parseStr(out);
        if(s.compare(i,4,"true")==0){ out.t=JVal::BOOL; out.b=true; i+=4; return true; }
        if(s.compare(i,5,"false")==0){ out.t=JVal::BOOL; out.b=false; i+=5; return true; }
        if(s.compare(i,4,"null")==0){ out.t=JVal::NUL; i+=4; return true; }
        size_t j=i; while(j<s.size()&&(isdigit((unsigned char)s[j])||s[j]=='-'||s[j]=='+'||s[j]=='.'||s[j]=='e'||s[j]=='E')) j++;
        if(j==i) return false; out.t=JVal::NUM; out.n=atof(s.substr(i,j-i).c_str()); i=j; return true;
    }
};
static std::string JEsc(const std::string& s){
    std::string o="\"";
    for(unsigned char c:s){ switch(c){ case '"': o+="\\\""; break; case '\\': o+="\\\\"; break; case '\n': o+="\\n"; break; case '\r': o+="\\r"; break; case '\t': o+="\\t"; break;
        default: if(c<0x20){ char b[8]; snprintf(b,8,"\\u%04x",c); o+=b; } else o.push_back((char)c); } }
    return o+"\"";
}
static std::string JEscW(const std::wstring& s){ return JEsc(WideToUtf8(s)); }

// ---- modelo ------------------------------------------------------------------------
// Entrada: arquivo local (path) e/ou musica online (url; play = link tocavel ja achado no
// YouTube/SoundCloud; path fica preenchido quando a musica foi baixada).
struct PlEntry { std::wstring path; std::wstring file; unsigned long long size=0; std::wstring title, artist; bool ok=true;
                 std::wstring url, thumb, play; int dur=0; };
// folder = pasta vinculada (sempre atualizada); link = playlist/album online de origem;
// mode = "" (padrao das configuracoes), "stream" ou "download".
struct Playlist { std::wstring slug, name, dir, coverPath, folder, link, mode; std::vector<PlEntry> entries; };
static bool IsUrlText(const std::wstring& s){ return s.rfind(L"http://",0)==0||s.rfind(L"https://",0)==0||s.rfind(L"spotify:",0)==0; }
// miniatura de musica online no cache (nome = hash da URL da imagem)
static std::wstring OnlineThumbFile(const std::wstring& thumbUrl){
    uint64_t h=1469598103934665603ULL; for(wchar_t c:thumbUrl){ h^=(uint64_t)c; h*=1099511628211ULL; }
    wchar_t n[40]; swprintf(n,40,L"%016llx.jpg",(unsigned long long)h);
    return Config::Join(Config::Join(Config::CacheDir(),L"online-thumbs"),n);
}
static std::vector<Playlist> g_playlists;

static std::wstring PlaylistsDir(){ return Config::Join(Config::BaseDir(), L"playlists"); }
static std::wstring Slugify(const std::wstring& name){
    std::wstring s;
    for(wchar_t c:name){ wchar_t l=(wchar_t)towlower(c);
        if((l>=L'a'&&l<=L'z')||(l>=L'0'&&l<=L'9')) s.push_back(l);
        else if(l==L'á'||l==L'à'||l==L'â'||l==L'ã'||l==L'ä') s.push_back(L'a'); else if(l==L'é'||l==L'è'||l==L'ê'||l==L'ë') s.push_back(L'e');
        else if(l==L'í'||l==L'ì'||l==L'î'||l==L'ï') s.push_back(L'i'); else if(l==L'ó'||l==L'ò'||l==L'ô'||l==L'õ'||l==L'ö') s.push_back(L'o');
        else if(l==L'ú'||l==L'ù'||l==L'û'||l==L'ü') s.push_back(L'u'); else if(l==L'ç') s.push_back(L'c'); else if(l==L'ñ') s.push_back(L'n');
        else if(!s.empty()&&s.back()!=L'-') s.push_back(L'-'); }
    while(!s.empty()&&s.back()==L'-') s.pop_back();
    if(s.empty()) s=L"playlist"; if(s.size()>40) s.resize(40);
    return s;
}
static bool ReadFileUtf8(const std::wstring& p,std::string& out){ std::ifstream f(std::filesystem::path(p),std::ios::binary); if(!f) return false; std::stringstream ss; ss<<f.rdbuf(); out=ss.str(); return true; }
static bool WriteFileUtf8(const std::wstring& p,const std::string& data){
    std::error_code ec; std::filesystem::path tmp(p+L".tmp");
    { std::ofstream f(tmp,std::ios::binary|std::ios::trunc); if(!f) return false; f<<data; }
    std::filesystem::rename(tmp,std::filesystem::path(p),ec); if(ec){ std::filesystem::remove(tmp,ec); return false; }
    return true;
}
static void SortPlaylists(){ std::sort(g_playlists.begin(),g_playlists.end(),[](const Playlist& a,const Playlist& b){ return _wcsicmp(a.name.c_str(),b.name.c_str())<0; }); }
static std::wstring PlJsonPath(const Playlist& pl){ return Config::Join(pl.dir,L"playlist.json"); }
static bool SavePlaylist(const Playlist& pl){
    std::error_code ec; std::filesystem::create_directories(std::filesystem::path(pl.dir),ec);
    std::string j="{\n  \"name\": "+JEscW(pl.name)+",\n  \"formato\": 2,\n  \"folder\": "+JEscW(Config::ToPortable(pl.folder))+",\n  \"link\": "+JEscW(pl.link)+",\n  \"mode\": "+JEscW(pl.mode)+",\n  \"tracks\": [\n";
    for(size_t i=0;i<pl.entries.size();++i){ const PlEntry& e=pl.entries[i];
        j+="    {\"path\": "+JEscW(Config::ToPortable(e.path))+", \"file\": "+JEscW(e.file)+", \"size\": "+std::to_string(e.size)+", \"title\": "+JEscW(e.title)+", \"artist\": "+JEscW(e.artist);
        if(!e.url.empty()) j+=", \"url\": "+JEscW(e.url)+", \"play\": "+JEscW(e.play)+", \"thumb\": "+JEscW(e.thumb)+", \"dur\": "+std::to_string(e.dur);
        j+=std::string("}")+(i+1<pl.entries.size()?",":"")+"\n"; }
    j+="  ]\n}\n";
    return WriteFileUtf8(PlJsonPath(pl),j);
}
static bool LoadPlaylistFile(const std::wstring& dir,Playlist& pl){
    std::string data; if(!ReadFileUtf8(Config::Join(dir,L"playlist.json"),data)) return false;
    JVal root; JParser p(data); if(!p.parse(root)||root.t!=JVal::OBJ) return false;
    pl.dir=dir; pl.slug=std::filesystem::path(dir).filename().wstring(); pl.name=Utf8ToWide(root.str("name",WideToUtf8(pl.slug)));
    pl.entries.clear();
    { std::wstring f=Utf8ToWide(root.str("folder")); pl.folder=f.empty()?L"":Config::FromPortable(f); }
    pl.link=Utf8ToWide(root.str("link")); pl.mode=Utf8ToWide(root.str("mode"));
    if(auto tr=root.get("tracks")) if(tr->t==JVal::ARR) for(auto& v:tr->a){ if(v.t!=JVal::OBJ) continue; PlEntry e;
        std::wstring pth=Utf8ToWide(v.str("path")); e.path=pth.empty()?L"":Config::FromPortable(pth); e.file=Utf8ToWide(v.str("file")); if(e.file.empty()&&!e.path.empty()) e.file=std::filesystem::path(e.path).filename().wstring();
        e.size=(unsigned long long)v.num("size",0); e.title=Utf8ToWide(v.str("title")); e.artist=Utf8ToWide(v.str("artist"));
        e.url=Utf8ToWide(v.str("url")); e.play=Utf8ToWide(v.str("play")); e.thumb=Utf8ToWide(v.str("thumb")); e.dur=(int)v.num("dur",0);
        if(e.path.empty()&&e.url.empty()) continue;
        pl.entries.push_back(e); }
    return true;
}
static void RefreshPlaylistCover(Playlist& pl){
    pl.coverPath.clear(); std::error_code ec; auto covers=LoadCustomCovers(); int n=0;
    for(auto& e:pl.entries){ if(++n>25) break;
        if(e.path.empty()||!std::filesystem::exists(std::filesystem::path(e.path),ec)){ if(!e.thumb.empty()){ std::wstring tf=OnlineThumbFile(e.thumb); if(std::filesystem::exists(std::filesystem::path(tf),ec)){ pl.coverPath=tf; return; } } continue; }
        { std::wstring emb; if(art::Lookup(e.path,LowerExt(std::filesystem::path(e.path)),emb)==1){ pl.coverPath=emb; return; } }
        auto it=covers.find(e.path); if(it!=covers.end()&&std::filesystem::exists(std::filesystem::path(it->second),ec)){ pl.coverPath=it->second; return; }
        std::wstring c=FindCoverInFolder(std::filesystem::path(e.path).parent_path()); if(!c.empty()){ pl.coverPath=c; return; } }
    if(!pl.folder.empty()) pl.coverPath=FindCoverInFolder(std::filesystem::path(pl.folder));   // so pasta vinculada: imagem da pasta
}
struct PlCount { size_t total=0, online=0; bool ok=false; };
static std::map<std::wstring,PlCount> g_plCount;   // slug -> faixas de verdade (pasta vinculada + avulsas + online) para os cards
static void RecountPlaylists();
static void LoadPlaylists(){
    g_playlists.clear(); std::error_code ec; std::filesystem::path root(PlaylistsDir());
    if(!std::filesystem::exists(root,ec)) return;
    for(std::filesystem::directory_iterator it(root,std::filesystem::directory_options::skip_permission_denied,ec),end;it!=end;it.increment(ec)){
        if(ec) break; std::error_code e2; if(!it->is_directory(e2)) continue;
        Playlist pl; if(LoadPlaylistFile(it->path().wstring(),pl)){ RefreshPlaylistCover(pl); g_playlists.push_back(std::move(pl)); } }
    SortPlaylists();
    g_plCount.clear(); RecountPlaylists();
}
static int FindPlaylistBySlug(const std::wstring& slug){ for(size_t i=0;i<g_playlists.size();++i) if(g_playlists[i].slug==slug) return (int)i; return -1; }
static int CreatePlaylist(const std::wstring& name){
    std::wstring base=Slugify(name), slug=base; int k=2; while(FindPlaylistBySlug(slug)>=0) slug=base+L"-"+std::to_wstring(k++);
    Playlist pl; pl.slug=slug; pl.name=Config::Trim(name).empty()?L"Playlist":Config::Trim(name); pl.dir=Config::Join(PlaylistsDir(),slug);
    if(!SavePlaylist(pl)) return -1;
    g_playlists.push_back(pl); SortPlaylists();
    return FindPlaylistBySlug(slug);
}
static bool DeletePlaylistDir(int i){ if(i<0||i>=(int)g_playlists.size()) return false; std::error_code ec; std::filesystem::remove_all(std::filesystem::path(g_playlists[(size_t)i].dir),ec); g_playlists.erase(g_playlists.begin()+i); return !ec; }
static bool RenamePlaylist(int i,const std::wstring& name){ if(i<0||i>=(int)g_playlists.size()) return false; std::wstring n=Config::Trim(name); if(n.empty()) return false; g_playlists[(size_t)i].name=n; SavePlaylist(g_playlists[(size_t)i]); return true; }
static unsigned long long FileSizeOf(const std::wstring& p){ std::error_code ec; auto s=std::filesystem::file_size(std::filesystem::path(p),ec); return ec?0ULL:(unsigned long long)s; }
static bool PlaylistAddTrack(int i,const Track& t,bool save=true){
    if(i<0||i>=(int)g_playlists.size()) return false; Playlist& pl=g_playlists[(size_t)i];
    if(IsOnlineTrack(t)){
        for(auto& e:pl.entries) if(e.url==t.url) return false;
        PlEntry e; e.url=t.url; e.title=t.title; e.artist=t.artist; e.dur=t.durSec; pl.entries.push_back(e);
        return save?SavePlaylist(pl):true;
    }
    if(!pl.folder.empty()&&Config::StartsI(Config::NormSep(t.path),Config::NormSep(pl.folder)+REMIX_SEP_STR)) return false;   // ja vem da pasta vinculada
    for(auto& e:pl.entries) if(_wcsicmp(e.path.c_str(),t.path.c_str())==0) return false;   // ja esta na playlist
    PlEntry e; e.path=t.path; e.file=std::filesystem::path(t.path).filename().wstring(); e.size=FileSizeOf(t.path); e.title=t.title; e.artist=t.artist;
    pl.entries.push_back(e); if(pl.coverPath.empty()&&!t.coverPath.empty()) pl.coverPath=t.coverPath;
    return save?SavePlaylist(pl):true;
}
static bool PlaylistRemovePath(int i,const std::wstring& path){
    if(i<0||i>=(int)g_playlists.size()) return false; Playlist& pl=g_playlists[(size_t)i]; size_t before=pl.entries.size();
    pl.entries.erase(std::remove_if(pl.entries.begin(),pl.entries.end(),[&](const PlEntry& e){ return (!e.path.empty()&&_wcsicmp(e.path.c_str(),path.c_str())==0)||(!e.url.empty()&&e.url==path); }),pl.entries.end());
    if(pl.entries.size()==before) return false; SavePlaylist(pl); return true;
}
// arquivo renomeado (newPath) ou apagado (newPath vazio): atualiza todas as playlists
static void PlaylistsRekey(const std::wstring& oldPath,const std::wstring& newPath){
    for(auto& pl:g_playlists){ bool ch=false;
        for(auto& e:pl.entries) if(_wcsicmp(e.path.c_str(),oldPath.c_str())==0){ if(newPath.empty()) e.path.clear(); else { e.path=newPath; e.file=std::filesystem::path(newPath).filename().wstring(); } ch=true; }
        if(ch){ pl.entries.erase(std::remove_if(pl.entries.begin(),pl.entries.end(),[](const PlEntry& e){ return e.path.empty(); }),pl.entries.end()); SavePlaylist(pl); } }
}
// ordem manual dentro da playlist: segue a ordem de 'paths'
static void PlaylistSetOrder(int i,const std::vector<std::wstring>& paths){
    if(i<0||i>=(int)g_playlists.size()) return; Playlist& pl=g_playlists[(size_t)i]; std::vector<PlEntry> out;
    auto same=[](const PlEntry& e,const std::wstring& p){ return (!e.path.empty()&&_wcsicmp(e.path.c_str(),p.c_str())==0)||(!e.url.empty()&&e.url==p); };
    for(auto& p:paths) for(auto& e:pl.entries) if(same(e,p)){ out.push_back(e); break; }
    for(auto& e:pl.entries){ bool has=false; for(auto& o:out) if(o.path==e.path&&o.url==e.url){ has=true; break; } if(!has) out.push_back(e); }
    pl.entries=out; SavePlaylist(pl);
}
static Track TrackFromPath(const std::wstring& path,const std::map<std::wstring,std::wstring>& covers){
    Track t; std::filesystem::path p(path); t.path=path; t.title=p.stem().wstring(); t.fileTime=FileTimeOf(p);
    Id3Info tg=ReadTags(path,LowerExt(p)); if(!tg.title.empty()) t.title=tg.title; if(!tg.artist.empty()) t.artist=tg.artist; if(t.artist.empty()) t.artist=L"Artista desconhecido";
    ResolveTrackCover(t,p,LowerExt(p),covers);
    return t;
}
// Resolve as faixas da playlist: existe -> ok; sumiu -> procura na biblioteca pelo nome
// (+ tamanho) e corrige o caminho; senao conta como ausente.
static std::vector<Track> PlaylistTracks(int i,const std::vector<Track>& lib,int& missing){
    std::vector<Track> out; missing=0; if(i<0||i>=(int)g_playlists.size()) return out;
    Playlist& pl=g_playlists[(size_t)i]; auto covers=LoadCustomCovers(); std::error_code ec; bool changed=false;
    std::set<std::wstring> seen;
    // 1) pasta vinculada: sempre o conteudo atual da pasta
    if(!pl.folder.empty()&&Config::DirExists(pl.folder)){
        for(auto& t:ScanFolder(pl.folder)){ if(seen.insert(t.path).second) out.push_back(std::move(t)); }
    }
    // 2) adicionadas a mao: arquivos e musicas online
    for(auto& e:pl.entries){
        if(!e.path.empty()&&std::filesystem::exists(std::filesystem::path(e.path),ec)){
            e.ok=true;
            if(seen.insert(e.path).second){ Track t=TrackFromPath(e.path,covers); if(t.title.empty()&&!e.title.empty()) t.title=e.title; out.push_back(t); }
            continue;
        }
        if(!e.url.empty()){
            if(!e.path.empty()){ e.path.clear(); e.file.clear(); changed=true; }   // o download sumiu: volta a ser online
            e.ok=true;
            if(!seen.insert(e.url).second) continue;
            Track t; t.path=e.url; t.url=e.url; t.title=e.title.empty()?e.url:e.title; t.artist=e.artist.empty()?L"Online":e.artist; t.durSec=e.dur;
            auto it=covers.find(e.url);
            if(it!=covers.end()&&std::filesystem::exists(std::filesystem::path(it->second),ec)) t.coverPath=it->second;
            else if(!e.thumb.empty()){ std::wstring tf=OnlineThumbFile(e.thumb); if(std::filesystem::exists(std::filesystem::path(tf),ec)) t.coverPath=tf; }
            out.push_back(t);
            continue;
        }
        const Track* found=nullptr;   // arquivo que mudou de lugar: procura na biblioteca pelo nome + tamanho
        for(auto& t:lib){ if(!t.url.empty()) continue; if(_wcsicmp(std::filesystem::path(t.path).filename().wstring().c_str(),e.file.c_str())==0){ if(e.size==0||FileSizeOf(t.path)==e.size){ found=&t; break; } if(!found) found=&t; } }
        if(found){ e.path=found->path; e.ok=true; changed=true; if(seen.insert(e.path).second) out.push_back(TrackFromPath(e.path,covers)); }
        else { e.ok=false; missing++; }
    }
    if(changed) SavePlaylist(pl);
    { PlCount c; c.total=out.size(); for(auto& t:out) if(!t.url.empty()&&t.path==t.url) ++c.online; c.ok=true; g_plCount[pl.slug]=c; }   // conta exata de quem acabou de montar a lista
    return out;
}
// Conta sem ler tags (so lista a pasta): para os cards das playlists que nao estao abertas.
static void RecountPlaylist(int i){
    if(i<0||i>=(int)g_playlists.size()) return;
    const Playlist& pl=g_playlists[(size_t)i]; std::set<std::wstring> seen; size_t on=0; std::error_code ec;
    if(!pl.folder.empty()&&Config::DirExists(pl.folder)) WalkFiles(pl.folder,[&](const std::filesystem::path& p){ if(IsAudioExt(LowerExt(p))) seen.insert(p.wstring()); });
    for(auto& e:pl.entries){
        if(!e.path.empty()&&std::filesystem::exists(std::filesystem::path(e.path),ec)){ seen.insert(e.path); continue; }
        if(!e.url.empty()&&seen.insert(e.url).second) ++on;
    }
    PlCount c; c.total=seen.size(); c.online=on; c.ok=true; g_plCount[pl.slug]=c;
}
static void RecountPlaylists(){ for(size_t i=0;i<g_playlists.size();++i) RecountPlaylist((int)i); }
// Assinatura barata de uma pasta vinculada (quantos arquivos de audio, nomes, tamanhos e datas):
// muda quando entra, sai, renomeia ou regrava musica. Roda fora da thread da UI.
static std::string FolderAudioSignature(const std::wstring& folder){
    unsigned long long h=1469598103934665603ULL, n=0; std::error_code ec;
    auto mix=[&](unsigned long long v){ h^=v; h*=1099511628211ULL; };
    if(!Config::DirExists(folder)) return "sem-pasta";
    WalkFiles(folder,[&](const std::filesystem::path& p){
        if(!IsAudioExt(LowerExt(p))) return;
        ++n; for(wchar_t c:p.wstring()) mix((unsigned long long)c);
        std::error_code e2; mix((unsigned long long)std::filesystem::file_size(p,e2));
        auto t=std::filesystem::last_write_time(p,e2); if(!e2) mix((unsigned long long)t.time_since_epoch().count());
    });
    return std::to_string(n)+":"+std::to_string(h);
}
static bool PlaylistSetFolder(int i,const std::wstring& folder){ if(i<0||i>=(int)g_playlists.size()) return false; g_playlists[(size_t)i].folder=folder; return SavePlaylist(g_playlists[(size_t)i]); }
// adiciona um arquivo local pelo caminho (arquivos escolhidos / pasta copiada)
static bool PlaylistAddFile(int i,const std::wstring& path){
    if(i<0||i>=(int)g_playlists.size()||path.empty()) return false; Playlist& pl=g_playlists[(size_t)i];
    for(auto& e:pl.entries) if(!e.path.empty()&&_wcsicmp(e.path.c_str(),path.c_str())==0) return false;
    PlEntry e; e.path=path; e.file=std::filesystem::path(path).filename().wstring(); e.size=FileSizeOf(path); pl.entries.push_back(e);
    return true;   // quem chama salva no fim (lote)
}
static bool PlaylistAddOnlineEntry(int i,const PlEntry& in){
    if(i<0||i>=(int)g_playlists.size()||in.url.empty()) return false; Playlist& pl=g_playlists[(size_t)i];
    for(auto& e:pl.entries) if(e.url==in.url) return false;
    pl.entries.push_back(in); return true;
}
// metadados/links achados depois (streaming ou download) valem para todas as playlists
static void PlaylistsUpdateOnline(const std::wstring& url,const std::wstring& title,const std::wstring& artist,int dur,const std::wstring& thumb,const std::wstring& play,const std::wstring& localPath){
    for(auto& pl:g_playlists){ bool ch=false;
        for(auto& e:pl.entries){ if(e.url!=url) continue;
            if(!title.empty()&&e.title!=title){ e.title=title; ch=true; }
            if(!artist.empty()&&e.artist!=artist){ e.artist=artist; ch=true; }
            if(dur>0&&e.dur!=dur){ e.dur=dur; ch=true; }
            if(!thumb.empty()&&e.thumb!=thumb){ e.thumb=thumb; ch=true; }
            if(!play.empty()&&e.play!=play){ e.play=play; ch=true; }
            if(!localPath.empty()&&e.path!=localPath){ e.path=localPath; e.file=std::filesystem::path(localPath).filename().wstring(); e.size=FileSizeOf(localPath); ch=true; } }
        if(ch) SavePlaylist(pl); }
}
static const PlEntry* FindOnlineEntry(const std::wstring& url){ for(auto& pl:g_playlists) for(auto& e:pl.entries) if(e.url==url) return &e; return nullptr; }
