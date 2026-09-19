#pragma once
// Musica online na UI (parte C): menus (+ ADICIONAR, nova playlist, pasta da playlist,
// downloads) e as acoes da tela de busca online. Incluido por app_core.h depois da parte B.

static void OpenAddMenu(int pl,int x,int y){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    const Playlist& p=g_playlists[(size_t)pl];
    std::vector<std::wstring> l={L"Marcar músicas da biblioteca...",L"Escolher arquivos de música...",L"Adicionar as músicas de uma pasta...",
        p.folder.empty()?L"Vincular uma pasta (fica sempre atualizada)...":L"Trocar a pasta vinculada...",
        L"Colar link (Spotify, YouTube, Deezer, Apple Music, SoundCloud)...",L"Buscar online (YouTube Music, YouTube, SoundCloud)..."};
    std::vector<int> a={CA_ADD_LIB,CA_ADD_FILES,CA_ADD_FOLDERCOPY,CA_ADD_FOLDERLINK,CA_ADD_LINK,CA_ADD_SEARCH};
    OpenCtxMenuGeneric(x,y,CTX_MENU,pl,l,a,std::vector<bool>(l.size(),false),std::min((int)S(440),g_winW-12));
}
static void OpenNewPlaylistMenu(int x,int y){
    std::vector<std::wstring> l={L"Playlist vazia (dar um nome)...",L"A partir de uma pasta (vinculada)...",L"A partir de um link (Spotify, YouTube, Deezer...)...",L"Buscar músicas online..."};
    std::vector<int> a={CA_NEW_EMPTY,CA_NEW_FOLDER,CA_NEW_LINK,CA_NEW_SEARCH};
    OpenCtxMenuGeneric(x,y,CTX_MENU,-1,l,a,std::vector<bool>(l.size(),false),std::min((int)S(390),g_winW-12));
}
// Botao PASTA com uma playlist aberta: mexe na pasta DA PLAYLIST (antes trocava a biblioteca inteira).
static void OpenPlaylistFolderMenu(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    const Playlist& p=g_playlists[(size_t)pl];
    std::vector<std::wstring> l; std::vector<int> a; std::vector<bool> d; g_menuPaths.clear();
    l.push_back(L"Vincular uma pasta a \""+p.name+L"\"..."); a.push_back(CA_PLF_PICK); d.push_back(false);
    std::vector<std::wstring> cand;
    { auto roots=UserMusicRoots(); if(!roots.empty()) cand.push_back(roots[0]); }   // Musicas do sistema (ex.: ~/Músicas)
    if(!g_cfg.musicFolder.empty()) cand.push_back(g_cfg.musicFolder);
    for(auto& f:g_cfg.recentFolders) cand.push_back(f);
    for(auto& f:cand){
        if(g_menuPaths.size()>=5||!Config::DirExists(f)) continue;
        bool dup=!p.folder.empty()&&_wcsicmp(p.folder.c_str(),f.c_str())==0;
        for(auto& m:g_menuPaths) if(_wcsicmp(m.c_str(),f.c_str())==0) dup=true;
        if(dup) continue;
        g_menuPaths.push_back(f);
        l.push_back(L"Vincular: "+std::filesystem::path(f).filename().wstring()+L"   ("+f+L")"); a.push_back(CA_PLF_RECENT_BASE+(int)g_menuPaths.size()-1); d.push_back(false);
    }
    if(!p.folder.empty()){ l.push_back(L"Desvincular a pasta "+std::filesystem::path(p.folder).filename().wstring()); a.push_back(CA_PLF_UNLINK); d.push_back(true); }
    l.push_back(L"Pasta da BIBLIOTECA (todas as músicas)..."); a.push_back(CA_PLF_LIBRARY); d.push_back(false);
    OpenCtxMenuGeneric(R_folderBtn.left,R_folderBtn.bottom+4,CTX_MENU,pl,l,a,d,std::min((int)S(500),g_winW-12));
}
static void OpenActivityMenu(int x,int y){
    OpenCtxMenuGeneric(x,y,CTX_MENU,-1,{L"Abrir a pasta dos downloads",L"Cancelar downloads"},{CA_ACT_OPENDIR,CA_ACT_CANCEL},{false,true},(int)S(270));
}

// ---- tela de busca online ------------------------------------------------------------------
static int OnlineTargetPl(){ return g_onTargetSlug.empty()?-1:FindPlaylistBySlug(g_onTargetSlug); }
static void OpenOnlineSearch(int targetPl,const std::wstring& query){
    OnlineUI& u=OU(); u.open=true; u.editing=true; u.scroll=0;
    g_onTargetSlug=(targetPl>=0&&targetPl<(int)g_playlists.size())?g_playlists[(size_t)targetPl].slug:L"";
    u.targetPl=OnlineTargetPl();
    u.source=std::max(0,std::min(2,g_cfg.onlineSource));
    g_ctxOpen=false; g_showSettings=false; g_searchFocus=false;
    EnsureToolsAsync();
    if(!query.empty()){ u.query=query; OnlineSearchAsync(); }
    else { std::lock_guard<std::mutex> lk(u.m); if(u.res.empty()&&!u.busy.load()) u.status=L"Digite o nome da música (ou cole um link) e aperte ENTER."; }
}
static bool OnlineResultAt(int i,OTrack& out){ OnlineUI& u=OU(); std::lock_guard<std::mutex> lk(u.m); if(i<0||i>=(int)u.res.size()) return false; out=u.res[(size_t)i]; return true; }
static void OnlinePlayResult(int i){
    OTrack t; if(!OnlineResultAt(i,t)) return;
    g_onlineInfo[t.url]=t;
    int idx=-1; for(size_t k=0;k<g_tracks.size();++k) if(g_tracks[k].path==t.url){ idx=(int)k; break; }
    if(idx<0){ g_tracks.push_back(TrackFromOTrack(t)); idx=(int)g_tracks.size()-1; BuildLayout(); }
    PlayIndex(idx,true);
    if(!t.thumb.empty()) FetchThumbsAsync({{t.url,t.thumb}});
}
static void OnlineDownloadResult(int i){
    OTrack t; if(!OnlineResultAt(i,t)) return;
    g_onlineInfo[t.url]=t;
    int pl=OnlineTargetPl(); std::wstring plName;
    if(pl>=0){ std::vector<OTrack> v{t}; AddOnlineItemsToPlaylist(pl,v); plName=g_playlists[(size_t)pl].name; AfterPlaylistChange(pl); }
    StartDownloads({t},plName);
}
static void OpenPickOnlineMenu(int arg,int x,int y){   // arg = resultado (ou -1 = todos)
    std::vector<std::wstring> l; std::vector<int> a;
    for(size_t k=0;k<g_playlists.size()&&k<14;++k){ l.push_back(g_playlists[k].name); a.push_back(CA_PICKON_BASE+(int)k); }
    l.push_back(L"+ Nova playlist"); a.push_back(CA_PICKON_NEW);
    OpenCtxMenuGeneric(x,y,CTX_PICKON,arg,l,a,std::vector<bool>(l.size(),false),(int)S(290));
}
static void OnlinePickTarget(int act,int arg){
    std::vector<OTrack> items; std::wstring name,link;
    { OnlineUI& u=OU(); std::lock_guard<std::mutex> lk(u.m);
      if(arg<0) items=u.res; else if(arg<(int)u.res.size()) items.push_back(u.res[(size_t)arg]);
      name=u.fromLink?u.linkName:Config::Trim(u.query); link=u.fromLink?u.linkUrl:L""; }
    if(items.empty()) return;
    int pl;
    if(act==CA_PICKON_NEW){
        pl=CreatePlaylistSafe(name.empty()?L"Playlist online":name);
        if(pl<0){ SetStatus(L"Não consegui criar a playlist.",3000); return; }
        if(arg<0&&!link.empty()) g_playlists[(size_t)pl].link=link;
    } else pl=act-CA_PICKON_BASE;
    if(pl<0||pl>=(int)g_playlists.size()) return;
    int n=AddOnlineItemsToPlaylist(pl,items);
    std::wstring nm=g_playlists[(size_t)pl].name;
    if(arg<0){ OU().open=false; OU().editing=false; OpenPlaylistView(pl); }   // "adicionar todas": abre a playlist
    else AfterPlaylistChange(pl);
    SetStatus(std::to_wstring(n)+(n==1?L" música adicionada em \"":L" músicas adicionadas em \"")+nm+L"\".",3000);
}
static void OnlineAddResult(int i,int x,int y){
    int pl=OnlineTargetPl();
    if(pl<0){ OpenPickOnlineMenu(i,x,y); return; }
    OTrack t; if(!OnlineResultAt(i,t)) return;
    std::vector<OTrack> v{t}; int n=AddOnlineItemsToPlaylist(pl,v); AfterPlaylistChange(pl);
    SetStatus(n?L"Adicionada em \""+g_playlists[(size_t)pl].name+L"\".":std::wstring(L"Essa já está na playlist."),2200);
}
static void OnlineAddAll(int x,int y){
    int pl=OnlineTargetPl();
    bool fromLink=false; { std::lock_guard<std::mutex> lk(OU().m); fromLink=OU().fromLink; }
    if(pl>=0) OnlinePickTarget(CA_PICKON_BASE+pl,-1);
    else if(fromLink) OnlinePickTarget(CA_PICKON_NEW,-1);   // link de playlist/album: vira uma playlist nova com o nome dele
    else OpenPickOnlineMenu(-1,x,y);
}

// ---- itens dos menus novos (CTX_MENU, CTX_PICKON e acoes novas de faixa/playlist) -----------
static bool RunMenuAction(int kind,int act,int arg){
    if(kind==CTX_SAIDA){
        int i=act-CA_SAIDA_BASE;
        if(i==0) TrocarSaida(L"");
        else if(i-1>=0&&i-1<(int)g_saidas.size()) TrocarSaida(g_saidas[(size_t)i-1]);
        return true;
    }
    if(kind==CTX_PICKON){ if(act==CA_PICKON_NEW||act>=CA_PICKON_BASE) OnlinePickTarget(act,arg); return true; }
    if(kind==CTX_TRACK){
        if(arg<0||arg>=(int)g_tracks.size()) return false;
        const Track& tr=g_tracks[(size_t)arg];
        if(act==CA_DOWNLOAD){ const Playlist* p=OpenPlaylistPtr(); StartDownloads({OTrackFor(tr)},p?p->name:L""); return true; }
        if(act==CA_OPEN_URL){ PlatformOpenFolder(tr.url.empty()?tr.path:tr.url); return true; }
        return false;
    }
    if(kind==CTX_PLAYLIST){
        if(arg<0||arg>=(int)g_playlists.size()) return false;
        switch(act){
        case CA_PL_FOLDER: SetPendingPl(arg); PlatformPickFolderFor(EV_PICK_PL_FOLDER,0); return true;
        case CA_PL_SYNC: ResolveLinkAsync(g_playlists[(size_t)arg].link,g_playlists[(size_t)arg].slug); SetStatus(L"Sincronizando com o link (só adiciona o que falta)...",4000); return true;
        case CA_PL_DLALL: DownloadPlaylistOnline(arg); return true;
        case CA_PL_MODE: CyclePlaylistMode(arg); return true;
        default: return false;
        }
    }
    if(kind!=CTX_MENU) return false;
    int pl=(arg>=0&&arg<(int)g_playlists.size())?arg:-1;
    switch(act){
    case CA_ADD_LIB: StartPickMode(pl); return true;
    case CA_ADD_FILES: if(pl>=0){ SetPendingPl(pl); PlatformPickAudioFilesAsync(EV_PICK_PL_FILES,0); } return true;
    case CA_ADD_FOLDERCOPY: if(pl>=0){ SetPendingPl(pl); PlatformPickFolderFor(EV_PICK_PL_ADDFOLDER,0); } return true;
    case CA_ADD_FOLDERLINK: case CA_PLF_PICK: if(pl>=0){ SetPendingPl(pl); PlatformPickFolderFor(EV_PICK_PL_FOLDER,0); } return true;
    case CA_ADD_LINK: if(pl>=0) StartPlaylistNameEdit(4,pl); return true;
    case CA_ADD_SEARCH: OpenOnlineSearch(pl,L""); return true;
    case CA_NEW_EMPTY: g_pendingAddPath.clear(); StartPlaylistNameEdit(2,-1); return true;
    case CA_NEW_FOLDER: PlatformPickFolderFor(EV_PICK_NEWPL_FOLDER,0); return true;
    case CA_NEW_LINK: StartPlaylistNameEdit(5,-1); return true;
    case CA_NEW_SEARCH: OpenOnlineSearch(-1,L""); return true;
    case CA_PLF_UNLINK: UnlinkPlaylistFolder(pl); return true;
    case CA_PLF_LIBRARY: EnterLibraryView(); OpenFolderMenu(); return true;
    case CA_ACT_OPENDIR: { std::error_code ec; std::wstring d=OnlineDownloadBase(); std::filesystem::create_directories(std::filesystem::path(d),ec); PlatformOpenFolder(d); return true; }
    case CA_ACT_CANCEL: { int n=CancelAllDownloads(); SetStatus(n?std::to_wstring(n)+L" download(s) cancelado(s).":std::wstring(L"Nenhum download em andamento."),2500); return true; }
    default: break;
    }
    if(act>=CA_PLF_RECENT_BASE&&act<CA_PLF_RECENT_BASE+(int)g_menuPaths.size()){ LinkPlaylistFolder(pl,g_menuPaths[(size_t)(act-CA_PLF_RECENT_BASE)]); return true; }
    return false;
}

// ---- estilo REMIX: cartoes da tela inicial ---------------------------------------------------
// Os quadradinhos do "Início" vem do descobrir (so metadados). Tocar uma musica
// passa pelo mesmo motor online de sempre (yt-dlp), entao vem a musica inteira.
static void RxTocarLocal(const std::vector<std::wstring>& chaves,size_t i){
    if(i>=chaves.size()) return;
    std::wstring chave=chaves[i];
    if(g_view!=0) EnterLibraryView();
    for(size_t k=0;k<g_tracks.size();k++) if(g_tracks[k].path==chave){ PlayIndex((int)k,true); return; }
    SetStatus(L"Essa música não está mais na biblioteca.",2500);
}
static void RxAbrirItem(const desc::Item& it,bool tocar){
    if(it.kind==desc::K_FAIXA){
        OTrack t; t.src=DetectSource(it.link); t.url=it.link; t.title=it.titulo; t.artist=it.sub; t.thumb=it.capa; t.dur=it.dur;
        if(t.url.empty()){ SetStatus(L"Sem link para essa música.",2500); return; }
        g_onlineInfo[t.url]=t;
        int idx=-1; for(size_t k=0;k<g_tracks.size();++k) if(g_tracks[k].path==t.url){ idx=(int)k; break; }
        if(idx<0){ g_tracks.push_back(TrackFromOTrack(t)); idx=(int)g_tracks.size()-1; BuildLayout(); }
        EnsureToolsAsync();
        PlayIndex(idx,true);
        if(!t.thumb.empty()) FetchThumbsAsync({{t.url,t.thumb}});
        return;
    }
    (void)tocar;
    // album/playlist: abre o link na busca online (a mesma tela do "colar link").
    // artista: procura pelo nome, porque link de artista nao e uma lista de musicas.
    if(it.kind==desc::K_ARTISTA) OpenOnlineSearch(-1,it.titulo);
    else OpenOnlineSearch(-1,it.link);
}
static void RxClicarCard(size_t ci,bool tocar){
    if(ci>=g_rxCards.size()) return;
    const RxCard& k=g_rxCards[ci];
    if((size_t)k.fila>=g_rxFilas.size()) return;
    int fonte=g_rxFilas[(size_t)k.fila].fonte;
    if(fonte==-2){   // gênero: abre as paradas dele
        std::vector<desc::Item> gs=desc::ListaGeneros();
        if((size_t)k.item>=gs.size()) return;
        g_rxGenero=gs[(size_t)k.item].id; g_rxGeneroNome=gs[(size_t)k.item].titulo;
        g_rxScroll=0;
        desc::AtualizarGeneros(g_rxGenero,g_rxGeneroNome,RxAvisarNovidades);
        BuildLayout(); return;
    }
    if(fonte==-1){ RxTocarLocal(g_rxRecentesChave,(size_t)k.item); return; }
    if(fonte==-3){ RxTocarLocal(g_rxMisturaChave,(size_t)k.item); return; }
    desc::Home h;
    if(g_rxPag==RXP_DESCOBRIR){ if(!desc::HomeDoGenero(g_rxGenero,h)) return; }
    else h=desc::Copia();
    if((size_t)fonte>=h.fileiras.size()) return;
    const desc::Shelf& s=h.fileiras[(size_t)fonte];
    if((size_t)k.item>=s.itens.size()) return;
    RxAbrirItem(s.itens[(size_t)k.item],tocar);
}
static void RxVerTudo(size_t fi){
    if(fi>=g_rxFilas.size()) return;
    int fonte=g_rxFilas[fi].fonte;
    for(size_t i=0;i<g_rxAbertas.size();i++) if(g_rxAbertas[i]==fonte){ g_rxAbertas.erase(g_rxAbertas.begin()+(long)i); BuildLayout(); return; }
    g_rxAbertas.push_back(fonte); BuildLayout();
}
