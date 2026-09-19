#pragma once
// Musica online na UI (parte A): tocar por streaming, metadados, miniaturas e capas
// embutidas que chegam das threads. Incluido por app_core.h logo depois de OnTranscoded.
static void OpenPlaylistView(int pi);
static int CreatePlaylistSafe(const std::wstring& name);
static int AddOnlineItemsToPlaylist(int pl,const std::vector<OTrack>& items);
static void OnStreamFail(const std::wstring& msg,int id);
static void OnStreamReady(int id,int durSec);

// procura yt-dlp/ffmpeg/node em segundo plano (so quando o usuario chega perto do online)
static void EnsureToolsAsync(){
    if(OT().probed.load()||OT().probing.load()) return;
    std::thread([]{ RemixSafe("procurar yt-dlp/ffmpeg",[]{ ProbeOnlineTools(); }); AppPost(EV_TOOLS_READY); }).detach();
}
// pasta dos downloads: a das configuracoes ou <pasta Musicas>/Remix Online
static std::wstring OnlineDownloadBase(){
    if(!g_cfg.downloadFolder.empty()) return g_cfg.downloadFolder;
    std::wstring base=g_cfg.musicFolder;
    if(base.empty()){ auto roots=UserMusicRoots(); if(!roots.empty()) base=roots[0]; }
    if(base.empty()) base=Config::BaseDir();
    return Config::Join(base,L"Remix Online");
}
static std::wstring OnlineDownloadBaseCached(){   // para desenhar (nao le user-dirs a cada quadro)
    static std::wstring v; static ULONGLONG at=0; ULONGLONG now=GetTickCount64();
    if(at==0||now-at>1500){ v=OnlineDownloadBase(); at=now; }
    return v;
}
// baixar: junta as faixas e, se o usuario pediu (g_cfg.askDlFolder), pergunta a pasta antes.
// A escolha vira a pasta padrao (a proxima ja abre nela). O download enquanto toca (modo
// "download" ao dar play) nao pergunta: usa a pasta padrao para nao atrapalhar.
struct PendingDownloads { std::vector<OTrack> items; std::wstring plName; };
static PendingDownloads g_pendingDl;
static void QueueDownloadsInto(const std::vector<OTrack>& items,const std::wstring& plName,const std::wstring& base){
    int n=0; for(auto& t:items){ if(t.url.empty()) continue; QueueDownload(t,plName,base,g_cfg.onlineFormat); ++n; }
    std::wstring dst=plName.empty()?base:Config::Join(base,SafeFileName(plName));
    SetStatus(n?std::to_wstring(n)+(n==1?L" na fila de download  →  ":L" músicas na fila de download  →  ")+dst:std::wstring(L"Nada para baixar."),4500);
}
static void StartDownloads(std::vector<OTrack> items,const std::wstring& plName){
    items.erase(std::remove_if(items.begin(),items.end(),[](const OTrack& t){ return t.url.empty(); }),items.end());
    if(items.empty()){ SetStatus(L"Nada para baixar.",3000); return; }
    if(g_cfg.askDlFolder){ g_pendingDl={items,plName}; SetStatus(L"Escolha a pasta para baixar…",3000); PlatformPickFolderFor(EV_PICK_DLONCE,0); }
    else QueueDownloadsInto(items,plName,OnlineDownloadBase());
}
// "N faixas" conta tudo o que a playlist mostra (pasta vinculada + avulsas + online, sem repetir);
// "· M online" diz quantas dessas ainda sao online (nao baixadas).
static std::wstring PlaylistCardSubtitle(int pi){
    const Playlist& p=g_playlists[(size_t)pi]; size_t n=0,on=0;
    auto it=g_plCount.find(p.slug);
    if(it!=g_plCount.end()&&it->second.ok){ n=it->second.total; on=it->second.online; }
    else { for(auto& e:p.entries) if(!e.url.empty()&&e.path.empty()) ++on; n=p.entries.size(); }
    std::wstring s=std::to_wstring(n)+(n==1?L" faixa":L" faixas");
    if(on) s+=L" · "+std::to_wstring(on)+L" online";
    if(!p.folder.empty()) s+=L" · pasta vinculada";
    return s;
}
static const Playlist* OpenPlaylistPtr(){ return (g_view==2&&g_openPl>=0&&g_openPl<(int)g_playlists.size())?&g_playlists[(size_t)g_openPl]:nullptr; }
static std::wstring OnlineModeNow(){ const Playlist* p=OpenPlaylistPtr(); return (p&&!p->mode.empty())?p->mode:g_cfg.onlineMode; }
// junta o que se sabe de uma musica online: faixa em tela, busca/streaming (g_onlineInfo) e playlists
static OTrack OTrackFor(const Track& tr){
    OTrack t; t.url=tr.url.empty()?tr.path:tr.url;
    t.title=(tr.title==t.url)?L"":tr.title; t.artist=(tr.artist==L"Online")?L"":tr.artist; t.dur=tr.durSec; t.src=DetectSource(t.url);
    auto it=g_onlineInfo.find(t.url);
    if(it!=g_onlineInfo.end()){ const OTrack& o=it->second;
        if(!o.play.empty()) t.play=o.play;
        if(!o.thumb.empty()) t.thumb=o.thumb;
        if(!o.album.empty()) t.album=o.album;
        if(t.dur<=0) t.dur=o.dur;
        if(t.title.empty()) t.title=o.title;
        if(t.artist.empty()) t.artist=o.artist; }
    if(const PlEntry* e=FindOnlineEntry(t.url)){ if(t.play.empty()) t.play=e->play; if(t.thumb.empty()) t.thumb=e->thumb; if(t.dur<=0) t.dur=e->dur; }
    if(t.play.empty()&&!NeedsMatch(t.src)) t.play=t.url;
    return t;
}
static PlEntry EntryFromOTrack(const OTrack& t){ PlEntry e; e.url=t.url; e.play=t.play; e.title=t.title; e.artist=t.artist; e.thumb=t.thumb; e.dur=t.dur; return e; }
static Track TrackFromOTrack(const OTrack& t){
    Track tr; tr.path=t.url; tr.url=t.url; tr.title=t.title.empty()?t.url:t.title; tr.artist=t.artist.empty()?L"Online":t.artist; tr.durSec=t.dur;
    if(!t.thumb.empty()){ std::wstring f=OnlineThumbFile(t.thumb); std::error_code ec; if(std::filesystem::exists(std::filesystem::path(f),ec)) tr.coverPath=f; }
    return tr;
}
// miniaturas em segundo plano (uma por vez); cada uma avisa a UI com EV_ONLINE_THUMB
static void FetchThumbsAsync(std::vector<std::pair<std::wstring,std::wstring>> v){
    if(v.empty()) return;
    if(v.size()>400) v.resize(400);
    std::thread([v]{ for(auto& p:v){ if(p.second.empty()) continue; std::wstring f; RemixSafe("miniatura online",[&]{ f=FetchThumb(p.second); }); if(!f.empty()) AppPost(EV_ONLINE_THUMB,OPack({p.first,f})); } }).detach();
}
static std::set<std::wstring> g_thumbReady;   // miniaturas online ja no disco (a tela de busca so desenha estas)
static int g_onlineFailRow=0;   // falhas seguidas (pula no maximo 3 indisponiveis)
// PlayIndex de uma musica online. Sem autoplay so seleciona: o streaming comeca no play.
// Se a musica ja esta num canal da fila (carregando ou pronta), usa esse canal: toca na hora.
static void PlayOnlineIndex(int idx,bool autoplay,unsigned gen){
    (void)gen;
    const Track& tr=g_tracks[(size_t)idx];
    ClearWave(); g_converting=false;
    OTrack t=OTrackFor(tr);
    g_lastOnlineUrl=t.url; g_lastOnlineTitle=tr.title;
    if(!autoplay) return;
    g_pendingAutoplay=true; g_queueTick=0;
    auto j=FindStreamUrl(t.url);
    if(j&&j->phase.load()==5){
        if(GetTickCount64()-j->failAt.load()<90000){   // o canal da fila acabou de falhar nessa musica: nao espera de novo
            std::wstring e; { std::lock_guard<std::mutex> lk(j->im); e=j->err; }
            j->prefetch=false; g_curStreamId=j->id; g_curStreamOpen=false;
            OnStreamFail(e.empty()?std::wstring(L"Não consegui tocar essa música."):e,j->id);
            return;
        }
        DropStream(j); j=nullptr;
    }
    if(j){ bool reusable; { std::lock_guard<std::mutex> lk(j->st->m); reusable=j->st->baseFrame==0||j->st->canRestart; } if(!reusable){ DropStream(j); j=nullptr; } }
    bool fromQueue=j!=nullptr;
    if(!j) j=StartStreamJob(t,false);
    j->prefetch=false;
    g_curStreamId=j->id; g_curStreamOpen=false;
    if(j->ready.load()) OnStreamReady(j->id,j->durSec.load());   // o canal ja achou o audio: abre agora
    if(!g_curStreamOpen) g_converting=true;
    if(OnlineModeNow()==L"download"){
        const Playlist* p=OpenPlaylistPtr();
        QueueDownload(t,p?p->name:L"",OnlineDownloadBase(),g_cfg.onlineFormat);
        SetStatus(L"Tocando por streaming e baixando: "+tr.title,3500);
    } else if(fromQueue&&g_curStreamOpen) SetStatus(L"Tocando na hora (já estava carregada na fila): "+tr.title,2500);
    else if(fromQueue) SetStatus(L"Já estava carregando na fila: "+tr.title,2500);
    else SetStatus(L"Conectando: "+tr.title+L"   (streaming só na memória)",3500);
}
static void OnStreamReady(int id,int durSec){
    if(id<=0||id!=g_curStreamId||g_curStreamOpen) return;   // canal da fila: fica pronto esperando a vez
    auto j=FindStreamId(id); if(!j) return;
    if(g_player.OpenStream(j->st,(unsigned)std::max(0,durSec))){
        g_curStreamOpen=true; g_converting=false; g_onlineFailRow=0; ApplyVolume();
        if(g_resumeMs){ g_player.SeekMs(g_resumeMs); g_resumeMs=0; }   // voltou de um stem para a completa
        if(g_pendingAutoplay) g_player.Play();
    } else {
        g_converting=false; DropStream(j); g_curStreamId=0;
        SetStatus(Player::HasAudio()?L"Não consegui abrir o streaming.":L"Sem saída de áudio (nenhum dispositivo de som encontrado).",3500);
    }
}
// ---- fila de streaming: a musica atual + as proximas 2 online, cada uma no seu canal ----
static const int STREAM_QUEUE_NEXT=2;
static std::vector<int> UpcomingIndices(int k){   // proximas na ordem de reproducao (lista ou fila do aleatorio)
    std::vector<int> v; int n=(int)g_tracks.size();
    if(g_current<0||g_current>=n||k<=0) return v;
    if(g_cfg.shuffle){
        if((int)g_shufQueue.size()!=n||g_shufPos<0) return v;
        for(int i=1;i<=k&&g_shufPos+i<n;i++) v.push_back(g_shufQueue[(size_t)(g_shufPos+i)]);
    } else for(int i=1;i<=k&&g_current+i<n;i++) v.push_back(g_current+i);
    return v;
}
static void UpdateStreamQueue(){
    ULONGLONG now=GetTickCount64();
    if(g_queueTick&&now-g_queueTick<400) return;
    g_queueTick=now;
    auto cur=FindStreamId(g_curStreamId);
    if(g_curStreamId&&!cur){ g_curStreamId=0; g_curStreamOpen=false; }
    bool active=g_current>=0&&g_current<(int)g_tracks.size()&&(g_player.playing||g_converting);
    std::vector<int> next=active?UpcomingIndices(STREAM_QUEUE_NEXT):std::vector<int>();
    std::vector<std::wstring> urls;
    for(int i:next){ const Track& tr=g_tracks[(size_t)i]; if(IsOnlineTrack(tr)&&std::find(urls.begin(),urls.end(),tr.path)==urls.end()) urls.push_back(tr.path); }
    if(active||!g_player.loaded) PruneStreams(urls,cur?cur->id:0);   // pausado: a fila fica como esta
    if(!active) return;
    bool prevOk=!cur||cur->phase.load()>=2;   // em cascata: cada canal comeca quando o anterior ja achou o audio
    for(auto& url:urls){
        auto j=FindStreamUrl(url);
        if(j&&cur&&j->id==cur->id) continue;   // a mesma musica ja e a atual
        if(!j){
            if(!prevOk||StreamCount()>=(size_t)(1+STREAM_QUEUE_NEXT)) break;
            const Track* tr=nullptr; for(int i:next) if(g_tracks[(size_t)i].path==url){ tr=&g_tracks[(size_t)i]; break; }
            if(!tr) break;
            j=StartStreamJob(OTrackFor(*tr),true);
        } else j->prefetch=true;
        prevOk=j->phase.load()>=2;
    }
}
// estado dos canais para desenhar (lido uma vez por quadro)
static std::vector<StreamInfo> g_streamSnap;
static const StreamInfo* FindSnap(const std::wstring& url){ for(auto& c:g_streamSnap) if(c.url==url) return &c; return nullptr; }
static float StreamBufFrac(const StreamInfo& c){ if(c.phase==4) return 1.f; return c.len?std::min(1.f,(float)c.end/(float)c.len):0.f; }
static bool StreamQueuedReady(const StreamInfo& c){ return c.phase==4||c.phase==6||(c.phase==3&&c.end>=(uint64_t)15*c.rate); }
static std::wstring StreamTagLabel(const StreamInfo& c,bool wide){
    bool cur=c.id==g_curStreamId;
    if(c.phase==5) return wide?(cur?L"FALHOU":L"FILA: FALHOU"):L"ERRO";
    if(c.phase<=2) return wide?(cur?(c.phase==2?L"CONECTANDO":L"BUSCANDO"):(c.phase==2?L"FILA: CONECTANDO":L"FILA: BUSCANDO")):L"FILA";
    if(cur) return L"TOCANDO";
    return wide?(StreamQueuedReady(c)?L"FILA: PRONTA":L"FILA: CARREGANDO"):(StreamQueuedReady(c)?L"PRONTA":L"FILA");
}
static std::wstring StreamRowText(const StreamInfo& c){
    bool cur=c.id==g_curStreamId; int sec=(int)(c.end/std::max<uint32_t>(1,c.rate));
    std::wstring p=cur?L"tocando":L"na fila";
    if(c.phase==5) return p+L" · falhou";
    if(c.phase<=1) return p+L" · buscando o áudio...";
    if(c.phase==2) return p+L" · conectando...";
    if(cur) return p+L" · "+std::to_wstring((int)(StreamBufFrac(c)*100))+L"% carregado";
    return p+(StreamQueuedReady(c)?L" · pronta (":L" · carregando (")+std::to_wstring(sec)+L" s)";
}
// titulo/artista/duracao e o link tocavel achado: valem para a lista, a playlist e a proxima vez
static void OnStreamMeta(const std::wstring& s,int id){
    auto v=OUnpack(s); if(v.size()<7) return;
    const std::wstring url=v[0]; int dur=_wtoi(v[5].c_str());
    OTrack& o=g_onlineInfo[url]; o.url=url;
    if(!v[1].empty()) o.title=v[1];
    if(!v[2].empty()) o.artist=v[2];
    if(!v[3].empty()) o.album=v[3];
    if(!v[4].empty()) o.thumb=v[4];
    if(dur>0) o.dur=dur;
    if(!v[6].empty()) o.play=v[6];
    auto upd=[&](Track& t){
        if(t.path!=url) return;
        if(!v[1].empty()&&(t.title.empty()||t.title==url)) t.title=v[1];
        if(!v[2].empty()&&(t.artist.empty()||t.artist==L"Online")) t.artist=v[2];
        if(dur>0) t.durSec=dur;
    };
    for(auto& t:g_tracks) upd(t);
    for(auto& t:g_libTracks) upd(t);
    if(g_nowPlayingValid) upd(g_nowPlaying);
    const PlEntry* e=FindOnlineEntry(url);
    PlaylistsUpdateOnline(url,(e&&!e->title.empty())?L"":v[1],(e&&!e->artist.empty())?L"":v[2],dur,v[4],v[6],L"");
    if(id==g_curStreamId&&g_nowPlayingValid&&g_nowPlaying.path==url) g_lastOnlineTitle=g_nowPlaying.title;
    if(id==g_curStreamId&&dur>0&&g_player.loaded&&g_player.IsStream()) g_player.SetStreamLengthMs((DWORD)(dur)*1000);   // duracao real chegou: corrige lenFrames/cachedLenMs (a onda usa isso)
}
static void OnOnlineThumb(const std::wstring& s){
    auto v=OUnpack(s); if(v.size()<2||v[1].empty()) return;
    const std::wstring url=v[0], file=v[1];
    g_thumbReady.insert(file);
    for(auto& t:g_tracks) if(t.path==url&&t.coverPath.empty()) t.coverPath=file;
    for(auto& t:g_libTracks) if(t.path==url&&t.coverPath.empty()) t.coverPath=file;
    if(g_nowPlayingValid&&g_nowPlaying.path==url&&g_nowPlaying.coverPath.empty()){ g_nowPlaying.coverPath=file; PlatformLoadCover(file); }
    for(auto& pl:g_playlists) if(pl.coverPath.empty()) for(auto& e:pl.entries) if(e.url==url){ pl.coverPath=file; break; }
}
// capa embutida extraida (mp3/m4a/flac/ogg/opus...): tem prioridade sobre a imagem da pasta
static std::map<std::wstring,std::wstring> g_customCoversCache; static ULONGLONG g_customCoversAt=0;
static void OnArtReady(const std::wstring& s){
    auto v=OUnpack(s); if(v.size()<2||v[1].empty()) return;
    const std::wstring path=v[0], art=v[1];
    ULONGLONG now=GetTickCount64();
    if(g_customCoversAt==0||now-g_customCoversAt>3000){ g_customCoversCache=LoadCustomCovers(); g_customCoversAt=now; }
    if(g_customCoversCache.count(path)) return;   // capa escolhida pelo usuario continua valendo
    for(auto& t:g_tracks) if(t.path==path) t.coverPath=art;
    for(auto& t:g_libTracks) if(t.path==path) t.coverPath=art;
    if(g_nowPlayingValid&&g_nowPlaying.path==path&&g_nowPlaying.coverPath!=art){ g_nowPlaying.coverPath=art; PlatformLoadCover(art); }
    for(auto& pl:g_playlists) if(pl.coverPath.empty()) for(auto& e:pl.entries) if(_wcsicmp(e.path.c_str(),path.c_str())==0){ pl.coverPath=art; break; }
}
