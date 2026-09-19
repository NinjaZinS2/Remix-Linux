#pragma once
// Musica online na UI (parte B): playlists (criar, vincular pasta, adicionar arquivos e
// links), downloads concluidos e o modo "marcar musicas da biblioteca".
// Incluido por app_core.h antes de ConfirmYes.
static std::wstring g_pendingPlSlug;           // playlist que espera a resposta de um dialogo
static std::vector<std::wstring> g_menuPaths;  // pastas dos itens "Vincular: <pasta>"
static std::wstring g_onTargetSlug;            // busca online aberta para esta playlist ("" = nenhuma)
static std::set<std::wstring> g_pickHave; static std::wstring g_pickFolder;   // ja estao na playlist (modo marcar)

static int CreatePlaylistSafe(const std::wstring& name){   // CreatePlaylist reordena a lista: mantem os indices em uso
    std::wstring o=(g_openPl>=0&&g_openPl<(int)g_playlists.size())?g_playlists[(size_t)g_openPl].slug:L"";
    std::wstring k=(g_pickPl>=0&&g_pickPl<(int)g_playlists.size())?g_playlists[(size_t)g_pickPl].slug:L"";
    int pi=CreatePlaylist(name);
    if(!o.empty()) g_openPl=FindPlaylistBySlug(o);
    if(!k.empty()) g_pickPl=FindPlaylistBySlug(k);
    return pi;
}
static int PendingPl(){ return g_pendingPlSlug.empty()?-1:FindPlaylistBySlug(g_pendingPlSlug); }
static void SetPendingPl(int pl){ g_pendingPlSlug=(pl>=0&&pl<(int)g_playlists.size())?g_playlists[(size_t)pl].slug:L""; }
static void RefreshOpenPlaylist(){   // rele a playlist aberta sem perder a rolagem
    if(g_view!=2||g_openPl<0||g_openPl>=(int)g_playlists.size()) return;
    int sc=g_listScroll; OpenPlaylistView(g_openPl); g_listScroll=sc; BuildLayout();
}
static void AfterPlaylistChange(int pl){ if(g_view==2&&g_openPl==pl) RefreshOpenPlaylist(); else { RecountPlaylist(pl); BuildLayout(); } }
// Pasta vinculada "sempre atualizada" de verdade: o monitor do sistema so olha a pasta da biblioteca, entao
// a cada ~3 s uma thread compara a assinatura das pastas vinculadas (a da playlist aberta, ou todas nos
// cards). Mudou: a playlist aberta e relida (mantendo a rolagem) e os cards recontam.
struct LinkedFolderPoll { std::mutex m; std::map<std::wstring,std::string> result, known; std::atomic<bool> busy{false}; ULONGLONG last=0; };
static LinkedFolderPoll& LFP(){ static LinkedFolderPoll* p=new LinkedFolderPoll(); return *p; }
static void PollLinkedFolders(){
    auto& lp=LFP();
    std::map<std::wstring,std::string> got;
    { std::lock_guard<std::mutex> lk(lp.m); got.swap(lp.result); }
    for(auto& kv:got){
        auto k=lp.known.find(kv.first);
        bool changed=k!=lp.known.end()&&k->second!=kv.second;
        lp.known[kv.first]=kv.second;
        if(!changed) continue;
        for(size_t i=0;i<g_playlists.size();++i){
            if(g_playlists[i].folder!=kv.first) continue;
            if(g_view==2&&g_openPl==(int)i) RefreshOpenPlaylist(); else RecountPlaylist((int)i);
        }
        g_hostFolderGen++;   // o Host (se ligado) republica no proximo HostTick
        BuildLayout();
    }
    ULONGLONG now=GetTickCount64(); if(lp.busy.load()||now-lp.last<3000) return;
    std::vector<std::wstring> folders;
    auto add=[&](const std::wstring& f){ if(!f.empty()&&std::find(folders.begin(),folders.end(),f)==folders.end()) folders.push_back(f); };
    bool olhando=false;
    if(g_view==2&&g_openPl>=0&&g_openPl<(int)g_playlists.size()){ add(g_playlists[(size_t)g_openPl].folder); olhando=!folders.empty(); }
    else if(g_view==1){ for(auto& p:g_playlists) add(p.folder); olhando=!folders.empty(); }
    if(HostRunningNow()) for(auto& p:g_playlists) if(!p.folder.empty()&&!HostTargetsOf(p.slug).empty()) add(p.folder);   // hosteada no celular
    if(folders.empty()){ lp.last=now; return; }
    if(now-lp.last<(ULONGLONG)(olhando?3000:10000)) return;
    lp.last=now; lp.busy=true;
    std::thread([folders]{
        std::map<std::wstring,std::string> r;
        RemixSafe("pasta vinculada",[&]{ for(auto& f:folders) r[f]=FolderAudioSignature(f); });
        auto& q=LFP(); { std::lock_guard<std::mutex> lk(q.m); for(auto& kv:r) q.result[kv.first]=kv.second; }
        q.busy=false; AppPost(EV_REDRAW);
    }).detach();
}
static int AddOnlineItemsToPlaylist(int pl,const std::vector<OTrack>& items){
    if(pl<0||pl>=(int)g_playlists.size()) return 0;
    int added=0; std::vector<std::pair<std::wstring,std::wstring>> th;
    for(auto& t:items){
        if(t.url.empty()) continue;
        g_onlineInfo[t.url]=t;
        if(PlaylistAddOnlineEntry(pl,EntryFromOTrack(t))){ ++added; if(!t.thumb.empty()) th.push_back({t.url,t.thumb}); }
    }
    SavePlaylist(g_playlists[(size_t)pl]);
    FetchThumbsAsync(th);
    return added;
}
// download pronto na pasta padrao: aparece na biblioteca sem varrer o PC de novo
static void AddDownloadedToLibrary(const std::wstring& path){
    if(!g_cfg.musicFolder.empty()) return;   // pasta personalizada: o monitor da pasta atualiza sozinho
    if(g_view!=0&&!g_libCached) return;
    std::vector<Track>& lib=(g_view==0)?g_tracks:g_libTracks;
    for(auto& t:lib) if(_wcsicmp(t.path.c_str(),path.c_str())==0) return;
    lib.push_back(TrackFromPath(path,LoadCustomCovers()));
    if(g_view==0){ ApplySort(); BuildLayout(); }
}
static void OnDownloadJob(int id){
    auto j=FindJob(id); if(!j) return;
    int st=j->status.load();
    if(st==2){
        std::wstring fin; { std::lock_guard<std::mutex> lk(j->m); fin=j->finalPath; }
        const OTrack& t=j->t;
        PlaylistsUpdateOnline(t.url,L"",L"",t.dur,t.thumb,t.play,fin);   // a entrada da playlist passa a ser o arquivo
        for(auto& tr:g_tracks) if(tr.path==t.url){ tr.path=fin; tr.url=t.url; }
        for(auto& tr:g_libTracks) if(tr.path==t.url){ tr.path=fin; tr.url=t.url; }
        if(g_nowPlayingValid&&g_nowPlaying.path==t.url){ g_nowPlaying.path=fin; g_nowPlaying.url=t.url; }
        AddDownloadedToLibrary(fin);
        for(size_t i=0;i<g_playlists.size();++i){ bool has=false; for(auto& e:g_playlists[i].entries) if(e.url==t.url){ has=true; break; } if(has) RecountPlaylist((int)i); }   // online virou arquivo: o card reconta
        SetStatus(L"Baixada: "+(t.title.empty()?std::filesystem::path(fin).filename().wstring():t.title)+L"   →   "+std::filesystem::path(fin).parent_path().wstring(),4500);
    } else if(st==3){
        std::wstring e; { std::lock_guard<std::mutex> lk(j->m); e=j->err; }
        SetStatus(L"O download falhou ("+j->t.title+L"): "+e,7000);
    }
}
static void OnLinkResolved(const std::wstring& s,int id){
    OResolved rr; if(!TakeResolved(id,rr)) return;
    auto v=OUnpack(s); std::wstring slug=v.size()>0?v[0]:L"", url=v.size()>1?v[1]:L"";
    if(rr.items.empty()){ SetStatus(rr.err.empty()?L"Não achei músicas nesse link.":rr.err,6000); return; }
    int pl=slug.empty()?-1:FindPlaylistBySlug(slug); bool created=false;
    if(pl<0){
        pl=CreatePlaylistSafe(rr.name.empty()?L"Playlist online":rr.name);
        if(pl<0){ SetStatus(L"Não consegui criar a playlist.",3000); return; }
        g_playlists[(size_t)pl].link=url; created=true;
    }
    int added=AddOnlineItemsToPlaylist(pl,rr.items);
    std::wstring name=g_playlists[(size_t)pl].name;
    if(created) OpenPlaylistView(pl); else AfterPlaylistChange(pl);
    SetStatus(std::to_wstring(added)+(added==1?L" música adicionada em \"":L" músicas adicionadas em \"")+name+L"\""+(added<(int)rr.items.size()?L" (as outras já estavam lá)":L"")+L".",4500);
}
static void OnStreamFail(const std::wstring& msg,int id){
    if(id<=0||id!=g_curStreamId) return;   // canal da fila: o erro fica guardado e vale quando chegar a vez dele
    g_converting=false; g_curStreamOpen=false; SetStatus(msg,7000);
    bool tools=fonte::Configurada()&&fonte::FfmpegOk();
    bool hasNext=g_cfg.shuffle||g_current+1<(int)g_tracks.size();
    if(g_pendingAutoplay&&tools&&g_cfg.autoplay&&g_tracks.size()>1&&hasNext&&++g_onlineFailRow<3) NextTrack();   // pula a indisponivel
}

// ---- marcar musicas da biblioteca para a playlist ------------------------------------
static bool PickAlreadyIn(const Track& t){
    if(g_pickHave.count(t.path)) return true;
    return !g_pickFolder.empty()&&Config::StartsI(Config::NormSep(t.path),g_pickFolder);
}
static void StartPickMode(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    std::wstring slug=g_playlists[(size_t)pl].slug;
    g_searchBuf.clear(); g_searchFocus=false;
    EnterLibraryView();
    g_pickPl=FindPlaylistBySlug(slug); g_pickMode=g_pickPl>=0; g_pickSel.clear(); g_pickHave.clear(); g_pickFolder.clear();
    if(!g_pickMode) return;
    const Playlist& p=g_playlists[(size_t)g_pickPl];
    for(auto& e:p.entries) if(!e.path.empty()) g_pickHave.insert(e.path);
    if(!p.folder.empty()) g_pickFolder=Config::NormSep(p.folder)+REMIX_SEP_STR;
    g_listScroll=0; BuildLayout();
    if(g_tracks.empty()) SetStatus(SS().busy?L"A biblioteca ainda está carregando...":L"Biblioteca vazia: escolha a pasta das músicas no botão PASTA (ou use Escolher arquivos).",4500);
    else SetStatus(L"Clique nas músicas para marcar e depois em CONCLUIR.",4000);
}
static void TogglePick(int i){
    if(i<0||i>=(int)g_tracks.size()) return;
    const Track& t=g_tracks[(size_t)i];
    if(PickAlreadyIn(t)){ SetStatus(L"Essa já está na playlist.",1500); return; }
    if(!g_pickSel.erase(t.path)) g_pickSel.insert(t.path);
}
static void FinishPickMode(bool apply){
    int pl=g_pickPl; std::set<std::wstring> sel; sel.swap(g_pickSel);
    g_pickMode=false; g_pickPl=-1; g_pickHave.clear(); g_pickFolder.clear();
    if(pl<0||pl>=(int)g_playlists.size()){ BuildLayout(); return; }
    int added=0;
    if(apply&&!sel.empty()){
        for(auto& t:g_tracks) if(sel.count(t.path)&&PlaylistAddTrack(pl,t,false)) ++added;
        SavePlaylist(g_playlists[(size_t)pl]);
        if(g_playlists[(size_t)pl].coverPath.empty()) RefreshPlaylistCover(g_playlists[(size_t)pl]);
    }
    g_searchBuf.clear(); g_searchFocus=false;
    OpenPlaylistView(pl);
    if(apply) SetStatus(added?std::to_wstring(added)+(added==1?L" música adicionada.":L" músicas adicionadas."):L"Nenhuma música nova marcada.",2800);
}

// ---- pasta vinculada / arquivos / pasta copiada ------------------------------------------
static void LinkPlaylistFolder(int pl,const std::wstring& folder){
    if(pl<0||pl>=(int)g_playlists.size()||folder.empty()) return;
    if(!Config::DirExists(folder)){ SetStatus(L"Pasta não encontrada.",2500); return; }
    PlaylistSetFolder(pl,folder);
    RefreshPlaylistCover(g_playlists[(size_t)pl]);
    AddRecentFolder(folder); g_cfg.Save();
    std::wstring name=g_playlists[(size_t)pl].name;
    if(g_view==2&&g_openPl==pl) RefreshOpenPlaylist(); else OpenPlaylistView(pl);
    SetStatus(L"\""+name+L"\" agora mostra a pasta "+std::filesystem::path(folder).filename().wstring()+L" (a biblioteca não mudou).",4500);
}
static void UnlinkPlaylistFolder(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    PlaylistSetFolder(pl,L""); RefreshPlaylistCover(g_playlists[(size_t)pl]);
    AfterPlaylistChange(pl);
    SetStatus(L"Pasta desvinculada (as músicas adicionadas à mão continuam).",3000);
}
static void OnPickedPlaylistFolder(const std::wstring& folder){ if(!folder.empty()) LinkPlaylistFolder(PendingPl(),folder); }
static void OnPickedFiles(const std::wstring& s,bool fromFolder){
    int pl=PendingPl(); if(pl<0) return;
    if(s.empty()){ if(fromFolder) SetStatus(L"Nenhuma música nessa pasta.",2500); return; }
    int added=0,total=0; size_t st=0; std::error_code ec;
    for(;;){
        size_t e=s.find(L'\n',st);
        std::wstring p=s.substr(st,e==std::wstring::npos?std::wstring::npos:e-st);
        if(!p.empty()&&p.back()==L'\r') p.pop_back();
        if(!p.empty()&&std::filesystem::is_regular_file(std::filesystem::path(p),ec)){ ++total; if(PlaylistAddFile(pl,p)) ++added; }
        if(e==std::wstring::npos) break;
        st=e+1;
    }
    Playlist& P=g_playlists[(size_t)pl]; SavePlaylist(P);
    if(P.coverPath.empty()) RefreshPlaylistCover(P);
    AfterPlaylistChange(pl);
    if(total==0) SetStatus(L"Nenhum arquivo de música escolhido.",2500);
    else SetStatus(std::to_wstring(added)+(added==1?L" música adicionada":L" músicas adicionadas")+(added<total?L" (as outras já estavam na playlist)":L"")+L".",3000);
}
static void OnPickedAddFolder(const std::wstring& folder){   // guarda o caminho de cada musica da pasta (uma vez)
    if(folder.empty()) return;
    SetStatus(L"Lendo a pasta...",2000);
    std::thread([folder]{
        std::vector<Track> v; RemixSafe("ler pasta",[&]{ v=ScanFolder(folder); });
        std::sort(v.begin(),v.end(),[](const Track& a,const Track& b){ return a.path<b.path; });
        std::wstring all; for(auto& t:v){ if(!all.empty()) all.push_back(L'\n'); all+=t.path; }
        AppPost(EV_PICK_PL_FILES,all,1);
    }).detach();
}
static void OnPickedNewPlaylistFolder(const std::wstring& folder){
    if(folder.empty()) return;
    std::wstring nm=std::filesystem::path(folder).filename().wstring(); if(nm.empty()) nm=folder;
    int pl=CreatePlaylistSafe(nm);
    if(pl<0){ SetStatus(L"Não consegui criar a playlist.",3000); return; }
    LinkPlaylistFolder(pl,folder);
}
static void DownloadPlaylistOnline(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    const Playlist& p=g_playlists[(size_t)pl]; std::vector<OTrack> items;
    for(auto& e:p.entries){
        if(e.url.empty()||!e.path.empty()) continue;
        OTrack t; t.url=e.url; t.play=e.play; t.title=e.title; t.artist=e.artist; t.thumb=e.thumb; t.dur=e.dur; t.src=DetectSource(e.url);
        if(t.play.empty()&&!NeedsMatch(t.src)) t.play=t.url;
        items.push_back(t);
    }
    StartDownloads(items,p.name);
}
static void CyclePlaylistMode(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    Playlist& p=g_playlists[(size_t)pl];
    p.mode=p.mode.empty()?L"stream":(p.mode==L"stream"?L"download":L"");
    SavePlaylist(p);
    if(p.mode.empty()) SetStatus(L"\""+p.name+L"\": músicas online seguem as configurações ("+(g_cfg.onlineMode==L"download"?L"baixar":L"streaming")+L").",3500);
    else SetStatus(L"\""+p.name+(p.mode==L"stream"?L"\": músicas online tocam por streaming (só na memória).":L"\": músicas online são baixadas ao tocar."),3500);
}
