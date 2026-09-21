#pragma once
// Entrada compartilhada (Windows e Linux): hit-test e reacao a mouse/teclado.
// A plataforma traduz WM_* / raylib para estas funcoes e depois redesenha.
#include "app_core.h"
#include "app_layout.h"
#include <stdexcept>

static bool IsEqId(int id){ return id>=Z_EQ_BASE&&id<Z_EQ_BASE+8; }
static const int Z_TRACK_RANGE=1000000, Z_EMPTY_ACTION=802;
static bool AnyOverlay(){ return g_fxp.open||g_spadP.open||g_dcP.open||host::PU().open||g_confirmOpen||g_ctxOpen||g_folderMenuOpen||WP().open||g_imgMenuOpen||g_editArtist||OU().open; }

static int HitTest(int x,int y){
    if(g_confirmOpen){ if(PtIn(R_confirmYes,x,y)) return Z_CONFIRM_YES; if(PtIn(R_confirmNo,x,y)) return Z_CONFIRM_NO; return -1; }
    if(g_ctxOpen){ for(size_t i=0;i<R_ctxItems.size();++i) if(PtIn(R_ctxItems[i],x,y)) return Z_CTX_ITEM_BASE+(int)i; return -1; }
    if(g_folderMenuOpen){ for(size_t i=0;i<R_folderItems.size();++i) if(PtIn(R_folderItems[i],x,y)) return Z_FOLDER_ITEM_BASE+(int)i; return -1; }
    if(g_editArtist) return -1;
    if(g_spadP.open){ BuildSpadPanel(g_winW,g_winH); int z=PanelHit(g_spadP,x,y); return z>0?z:-1; }   // SOUNDPAD
    if(g_dcP.open){ BuildDcPanel(g_winW,g_winH); int z=PanelHit(g_dcP,x,y); return z>0?z:-1; }         // DISCORD
    if(g_fxp.open){   // painel EFEITOS
        FxPanelUI& p=g_fxp; LayoutFxPanel(g_winW,g_winH);
        if(PtIn(p.btnClose,x,y)) return Z_FX_CLOSE;
        if(PtIn(p.btnClear,x,y)) return Z_FX_CLEAR;
        if(StemJobActive()&&PtIn(p.btnCancel,x,y)) return Z_FX_CANCEL;
        for(int i=0;i<5;i++) if(PtIn(p.fx[i],x,y)) return Z_FX_BASE+i;
        for(int i=0;i<6;i++) if(PtIn(p.stem[i],x,y)) return Z_STEM_BASE+i;
        return -1;
    }
    if(host::PU().open){   // painel HOST
        host::PanelUI& p=host::PU(); LayoutHostPanel(g_winW,g_winH);
        if(PtIn(p.btnClose,x,y)) return Z_HOST_CLOSE;
        if(PtIn(p.btnToggle,x,y)) return Z_HOST_TOGGLE; if(PtIn(p.btnTunnel,x,y)) return Z_HOST_TUNNEL; if(PtIn(p.btnNewLink,x,y)) return Z_HOST_NEWLINK; if(PtIn(p.btnHtml,x,y)) return Z_HOST_HTML; if(PtIn(p.btnPasta,x,y)) return Z_HOST_PASTA;
        if(PtIn(p.btnPort,x,y)) return Z_HOST_PORT; if(PtIn(p.btnPin,x,y)) return Z_HOST_PIN; if(PtIn(p.btnName,x,y)) return Z_HOST_NAME; if(PtIn(p.btnLan,x,y)) return Z_HOST_LAN; if(PtIn(p.btnIpv6,x,y)) return Z_HOST_IPV6;
        if(PtIn(p.btnCopyTun,x,y)) return Z_HOST_COPYTUN; if(PtIn(p.btnCopyLan,x,y)) return Z_HOST_COPYLAN; if(PtIn(p.btnQrMode,x,y)) return Z_HOST_QRMODE; if(PtIn(p.btnQrNew,x,y)) return Z_HOST_QRNEW;
        if(PtIn(p.btnOnline,x,y)) return Z_HOST_ONLINE; if(PtIn(p.btnQrConfirm,x,y)) return Z_HOST_QRCONF;
        if(PtIn(p.list,x,y)){
            for(size_t i=0;i<p.accept.size();i++){ if(PtIn(p.accept[i],x,y)) return Z_HOST_ACCEPT_BASE+(int)i; if(PtIn(p.deny[i],x,y)) return Z_HOST_DENY_BASE+(int)i; }
            for(size_t i=0;i<p.revoke.size();i++){ if(PtIn(p.revoke[i],x,y)) return Z_HOST_REVOKE_BASE+(int)i; if(i<p.devLib.size()&&PtIn(p.devLib[i],x,y)) return Z_HOST_DEVLIB_BASE+(int)i; if(i<p.devLink.size()&&PtIn(p.devLink[i],x,y)) return Z_HOST_DEVLINK_BASE+(int)i; }
            for(size_t i=0;i<p.dplOk.size();i++) if(p.dplOk[i].right>p.dplOk[i].left&&PtIn(p.dplOk[i],x,y)) return Z_HOST_DPLOK_BASE+(int)i;
            for(size_t i=0;i<p.plHost.size();i++){ if(PtIn(p.plHost[i],x,y)) return Z_HOST_PL_BASE+(int)i; for(size_t j=0;j<p.plDev[i].size();j++) if(PtIn(p.plDev[i][j],x,y)) return Z_HOST_PLDEV_BASE+(int)i*20+(int)j; }
        }
        return -1;
    }
    if(OU().open){   // tela de busca online
        OnlineUI& u=OU();
        if(PtIn(u.btnClose,x,y)) return Z_ON_CLOSE;
        if(PtIn(u.qbox,x,y)) return Z_ON_QBOX;
        if(PtIn(u.btnSearch,x,y)) return Z_ON_SEARCH;
        for(int i=0;i<3;i++) if(u.src[i].right>u.src[i].left&&PtIn(u.src[i],x,y)) return Z_ON_SRC_BASE+i;
        for(int i=0;i<3;i++) if(PtIn(u.tab[i],x,y)) return Z_ON_TAB_BASE+i;
        if(PtIn(R_onList,x,y)) for(size_t i=0;i<u.rows.size()&&i<500;++i){
            if(u.rows[i].right<=u.rows[i].left) continue;
            if(PtIn(u.bPlay[i],x,y)) return Z_ON_PLAY_BASE+(int)i;
            if(PtIn(u.bDl[i],x,y)) return Z_ON_DL_BASE+(int)i;
            if(PtIn(u.bAdd[i],x,y)) return Z_ON_ADD_BASE+(int)i;
            if(PtIn(u.rows[i],x,y)) return Z_ON_PLAY_BASE+(int)i;
        }
        if(u.btnAddAll.right>u.btnAddAll.left&&PtIn(u.btnAddAll,x,y)) return Z_ON_ADDALL;
        if(u.btnCfg.right>u.btnCfg.left&&PtIn(u.btnCfg,x,y)) return Z_ON_CFG;
        return -1;
    }
    {
        WebPick& wb=WP();
        if(wb.open){
            if(PtIn(wb.btnClose,x,y)) return Z_WEB_CLOSE;
            if(PtIn(wb.qbox,x,y)) return Z_WEB_QBOX;
            if(PtIn(wb.btnSearch,x,y)) return Z_WEB_SEARCH;
            for(size_t i=0;i<wb.cells.size();++i) if(PtIn(wb.cells[i],x,y)) return Z_WEB_CELL_BASE+(int)i;
            if(PtIn(wb.btnUse,x,y)) return Z_WEB_USE;
            if(PtIn(wb.btnCancel,x,y)) return Z_WEB_CANCEL;
            return -1;
        }
        if(g_imgMenuOpen){
            if(PtIn(R_imgLocal,x,y)) return Z_IMG_LOCAL;
            if(PtIn(R_imgWeb,x,y)) return Z_IMG_WEB;
            return -1;
        }
    }
    if(g_showSettings){
        if(PtIn(R_settingsClose,x,y)) return Z_SETTINGS_CLOSE;   // o X fica fixo no canto; o resto rola
        y+=g_setScroll;
        if(PtIn(R_settingsDefault,x,y)) return Z_SETTINGS_DEFAULT;
        if(PtIn(R_settingsCustom,x,y)) return Z_SETTINGS_CUSTOM;
        if(PtIn(R_settingsModeSquare,x,y)) return Z_SETTINGS_MODE_SQUARE;
        if(PtIn(R_settingsModeCd,x,y)) return Z_SETTINGS_MODE_CD;
        if(PtIn(R_settingsModeVertical,x,y)) return Z_SETTINGS_MODE_VERTICAL;
        for(int k=0;k<UI_STYLE_COUNT;k++) if(PtIn(R_settingsStyle[k],x,y)) return Z_SETTINGS_STYLE_BASE+k;
        if(PtIn(R_setSpad,x,y)) return Z_SET_SPAD; if(PtIn(R_setDc,x,y)) return Z_SET_DC;
        if(PtIn(R_setHostOn,x,y)) return Z_SET_HOST_ON; if(PtIn(R_setHostPanel,x,y)) return Z_SET_HOST_PANEL; if(PtIn(R_setHostPort,x,y)) return Z_SET_HOST_PORT; if(PtIn(R_setHostPin,x,y)) return Z_SET_HOST_PIN;
        if(PtIn(R_setHostName,x,y)) return Z_SET_HOST_NAME; if(PtIn(R_setHostTunnel,x,y)) return Z_SET_HOST_TUNNEL; if(PtIn(R_setHostLan,x,y)) return Z_SET_HOST_LAN;
        if(PtIn(R_setCli,x,y)) return Z_SET_CLI;
        if(PtIn(R_setCliBuscar,x,y)) return Z_SET_CLI_BUSCAR;
        if(PtIn(R_setNovidades,x,y)) return Z_SET_NOVIDADES;
        if(PtIn(R_setSep,x,y)) return Z_SET_SEP;
        if(PtIn(R_setSepBuscar,x,y)) return Z_SET_SEP_BUSCAR;
        if(PtIn(R_setStemsCpu,x,y)) return Z_SET_STEMSCPU;
        if(PtIn(R_setHostCopyTun,x,y)) return Z_SET_HOST_COPYTUN; if(PtIn(R_setHostCopyLan,x,y)) return Z_SET_HOST_COPYLAN; if(PtIn(R_setHostOnline,x,y)) return Z_SET_HOST_ONLINE;
        if(PtIn(R_setHostQrConf,x,y)) return Z_SET_HOST_QRCONF; if(PtIn(R_setHostIpv6,x,y)) return Z_SET_HOST_IPV6;
        for(auto&s:g_setSliders) if(PtIn(s.hit,x,y)) return s.id;
        if(PtIn(R_setEffect,x,y)) return Z_LED_EFFECT;
        if(PtIn(R_setParticles,x,y)) return Z_PARTICLES_TOGGLE;
        if(PtIn(R_setGlitch,x,y)) return Z_GLITCH_TOGGLE;
        if(PtIn(R_setPerf,x,y)) return Z_PERF_TOGGLE;
        if(PtIn(R_setBgClose,x,y)) return Z_BG_TOGGLE;
        if(PtIn(R_setSysMedia,x,y)) return Z_SYSMEDIA_TOGGLE;
        if(PtIn(R_setQuit,x,y)) return Z_QUIT_BTN;
        if(PtIn(R_setOnMode,x,y)) return Z_SET_ON_MODE;
        if(PtIn(R_setOnFmt,x,y)) return Z_SET_ON_FMT;
        if(PtIn(R_setOnSrc,x,y)) return Z_SET_ON_SRC;
        if(PtIn(R_setOnFolder,x,y)) return Z_SET_ON_FOLDER;
        if(PtIn(R_setOnRecheck,x,y)) return Z_SET_ON_RECHECK;
        for(int a=0;a<HK_COUNT;a++){ if(PtIn(R_hkKey[a],x,y)) return Z_HK_KEY_BASE+a; if(PtIn(R_hkScope[a],x,y)) return Z_HK_SCOPE_BASE+a; }
        if(PtIn(R_hkReset,x,y)) return Z_HK_RESET;
        if(PtIn(R_setRunnerToggle,x,y)) return Z_RUNNER_TOGGLE;
        if(PtIn(R_setAutoColor,x,y)) return Z_AUTOCOLOR_TOGGLE;
        if(PtIn(R_setWallChoose,x,y)) return Z_SET_WALL_CHOOSE;
        if(PtIn(R_setWallClear,x,y)) return Z_CLR_WALLPAPER;
        if(PtIn(R_setCoverBlur,x,y)) return Z_COVERBLUR_TOGGLE;
        if(PtIn(R_setAutoplay,x,y)) return Z_SET_AUTOPLAY;
        if(PtIn(R_setSort,x,y)) return Z_SET_SORT;
        if(PtIn(R_setSortDir,x,y)) return Z_SET_SORTDIR;
        if(PtIn(R_setEqOn,x,y)) return Z_EQ_ON;
        if(PtIn(R_setEqReset,x,y)) return Z_EQ_RESET;
        for(size_t i=0;i<R_themeCirclesSettings.size();++i)if(PtIn(R_themeCirclesSettings[i],x,y))return Z_THEME_BASE+(int)i;
        for(size_t i=0;i<R_runColors.size();++i) if(PtIn(R_runColors[i],x,y)) return Z_RUNNER_COLOR_BASE+(int)i;
        for(size_t i=0;i<R_playColors.size();++i) if(PtIn(R_playColors[i],x,y)) return Z_BTN_PLAY_BASE+(int)i;
        for(size_t i=0;i<R_navColors.size();++i) if(PtIn(R_navColors[i],x,y)) return Z_BTN_NAV_BASE+(int)i;
        for(size_t i=0;i<R_partColors.size();++i) if(PtIn(R_partColors[i],x,y)) return Z_PART_COLOR_BASE+(int)i;
        for(size_t i=0;i<R_ledColors.size();++i) if(PtIn(R_ledColors[i],x,y)) return Z_LED_COLOR_BASE+(int)i;
        return -1;
    }
    if(R_close.right>R_close.left&&PtIn(R_close,x,y)) return Z_CLOSE;
    if(R_min.right>R_min.left&&PtIn(R_min,x,y)) return Z_MIN;
    if(PtIn(R_gear,x,y))return Z_GEAR;
    if(R_activity.right>R_activity.left&&PtIn(R_activity,x,y))return Z_ACTIVITY;
    if(R_autoTgl.right>R_autoTgl.left&&PtIn(R_autoTgl,x,y))return Z_AUTOPLAY;
    if(R_volIcon.right>R_volIcon.left&&PtIn(R_volIcon,x,y))return Z_VOL_ICON;
    if(g_cfg.displayMode==L"vertical"){
        if(g_current>=0&&PtIn(R_verticalCoverButton,x,y))return Z_COVER_BASE+g_current;
        if(g_current>=0&&PtIn(R_pencilVert,x,y))return Z_ARTIST_EDIT;
        if(PtIn(R_shapeTgl,x,y))return Z_SHAPE_TOGGLE;
        if(PtIn(R_play,x,y))return Z_PLAYPAUSE;
        if(PtIn(R_prev,x,y))return Z_PREV;
        if(PtIn(R_next,x,y))return Z_NEXT;
        if(PtIn(R_shuffle,x,y))return Z_SHUFFLE;
        if(PtIn(R_repeat,x,y))return Z_REPEAT;
        if(PtIn(R_seek,x,y))return Z_WAVESEEK;
        RECT sline={R_seek.left,R_seek.bottom+2,R_seek.right,(LONG)(R_seek.bottom+S(13))};
        if(PtIn(sline,x,y))return Z_SEEKBAR;
        if(PtIn(R_vol,x,y))return Z_VOLBAR;
        return -1;
    }
    if(RxOn()){   // estilo REMIX: lateral, cartoes da tela inicial e "ver tudo"
        for(int i=0;i<3;i++) if(R_rxNav[i].right>R_rxNav[i].left&&PtIn(R_rxNav[i],x,y)) return Z_RX_NAV_BASE+i;
        if(R_rxNovaPl.right>R_rxNovaPl.left&&PtIn(R_rxNovaPl,x,y)) return Z_RX_NOVAPL;
        if(R_rxSideDrag.right>R_rxSideDrag.left&&PtIn(R_rxSideDrag,x,y)) return Z_RX_SIDEDRAG;
        if(R_rxLetra.right>R_rxLetra.left&&PtIn(R_rxLetra,x,y)) return Z_RX_LETRA;
        if(R_rxPainel.right>R_rxPainel.left&&PtIn(R_rxPainel,x,y)) return Z_RX_PAINEL;
        if(R_rxSaida.right>R_rxSaida.left&&PtIn(R_rxSaida,x,y)) return Z_RX_SAIDA;
        for(size_t i=0;i<R_rxLetraLinhas.size()&&i<200;i++) if(PtIn(R_rxLetraLinhas[i],x,y)) return Z_RX_LETRA_LINHA_BASE+(int)i;
        if(g_rxPag==RXP_INICIO&&R_rxHero.right>R_rxHero.left&&PtIn(R_rxHero,x,y)) return Z_RX_HERO;
        if(g_rxPag==RXP_INICIO&&PtIn(R_rxMain,x,y))
            for(size_t i=0;i<g_rxAtalhos.size()&&i<16;i++) if(g_rxAtalhos[i].r.right>g_rxAtalhos[i].r.left&&PtIn(g_rxAtalhos[i].r,x,y)) return Z_RX_ATALHO_BASE+(int)i;
        if(R_rxTocar.right>R_rxTocar.left&&PtIn(R_rxTocar,x,y)) return Z_RX_TOCAR;
        if(R_rxVoltar.right>R_rxVoltar.left&&PtIn(R_rxVoltar,x,y)) return Z_RX_VOLTAR;
        if(R_rxBuscarOn.right>R_rxBuscarOn.left&&PtIn(R_rxBuscarOn,x,y)) return Z_RX_BUSCARON;
        if(R_rxBuscaPl.right>R_rxBuscaPl.left&&PtIn(R_rxBuscaPl,x,y)) return Z_RX_BUSCAPL;
        if(R_rxBuscaAl.right>R_rxBuscaAl.left&&PtIn(R_rxBuscaAl,x,y)) return Z_RX_BUSCAAL;
        if(R_rxAleat.right>R_rxAleat.left&&PtIn(R_rxAleat,x,y)) return Z_RX_ALEATORIO;
        for(size_t i=0;i<R_rxSidePl.size();i++) if(PtIn(R_rxSidePl[i],x,y)) return Z_RX_SIDE_BASE+(int)i;
        if(g_rxPag!=RXP_LISTA&&PtIn(R_rxMain,x,y)){
            for(size_t i=0;i<g_rxFilas.size();i++){ const RECT& v=g_rxFilas[i].verTudo; if(v.right>v.left&&PtIn(v,x,y)) return Z_RX_VERTUDO_BASE+(int)i; }
            for(size_t i=0;i<g_rxCards.size();i++){
                if(!PtIn(g_rxCards[i].r,x,y)) continue;
                return (PtIn(g_rxCards[i].play,x,y)?Z_RX_CARDPLAY_BASE:Z_RX_CARD_BASE)+(int)i;
            }
        }
    }
    if(g_current>=0&&PtIn(R_pencilPanel,x,y))return Z_ARTIST_EDIT;
    if(PtIn(R_runnerSlider,x,y))return Z_PLAYER_RESIZE;
    if(PtIn(R_shapeTgl,x,y))return Z_SHAPE_TOGGLE;
    if(R_sortBtn.right>R_sortBtn.left&&PtIn(R_sortBtn,x,y))return Z_SORT;
    if(R_folderBtn.right>R_folderBtn.left&&PtIn(R_folderBtn,x,y))return Z_FOLDER_BTN;
    if(R_hostBtn.right>R_hostBtn.left&&PtIn(R_hostBtn,x,y))return Z_HOST_BTN;
    if(R_fxBtn.right>R_fxBtn.left&&PtIn(R_fxBtn,x,y))return Z_FX_BTN;
    if(R_spadBtn.right>R_spadBtn.left&&PtIn(R_spadBtn,x,y))return Z_SPAD_BTN;
    if(R_dcBtn.right>R_dcBtn.left&&PtIn(R_dcBtn,x,y))return Z_DC_BTN;
    if(PtIn(R_listBtn,x,y))return Z_LISTMODE;
    if(PtIn(R_play,x,y))return Z_PLAYPAUSE;
    if(PtIn(R_prev,x,y))return Z_PREV;
    if(PtIn(R_next,x,y))return Z_NEXT;
    if(PtIn(R_shuffle,x,y))return Z_SHUFFLE;
    if(PtIn(R_repeat,x,y))return Z_REPEAT;
    if(PtIn(R_wavePanel,x,y))return Z_WAVESEEK;
    if(PtIn(R_seek,x,y))return Z_SEEKBAR;
    if(PtIn(R_vol,x,y))return Z_VOLBAR;
    if(R_tabTracks.right>R_tabTracks.left&&PtIn(R_tabTracks,x,y)) return Z_TAB_TRACKS;
    if(R_tabOnline.right>R_tabOnline.left&&PtIn(R_tabOnline,x,y)) return Z_TAB_ONLINE;
    if(R_plAdd.right>R_plAdd.left&&PtIn(R_plAdd,x,y)) return Z_PL_ADD;
    if(R_plMode.right>R_plMode.left&&PtIn(R_plMode,x,y)) return Z_PL_MODE;
    if(R_pickDone.right>R_pickDone.left&&PtIn(R_pickDone,x,y)) return Z_PICK_DONE;
    if(R_pickCancel.right>R_pickCancel.left&&PtIn(R_pickCancel,x,y)) return Z_PICK_CANCEL;
    if(R_onlineInfo.right>R_onlineInfo.left&&PtIn(R_onlineInfo,x,y)) return Z_EMPTY_ACTION;
    if(R_tabPlaylists.right>R_tabPlaylists.left&&PtIn(R_tabPlaylists,x,y)) return Z_TAB_PLAYLISTS;
    if(R_plBack.right>R_plBack.left&&PtIn(R_plBack,x,y)) return Z_PL_BACK;
    if(R_plNew.right>R_plNew.left&&PtIn(R_plNew,x,y)) return Z_PL_NEW;
    if(R_searchClear.right>R_searchClear.left&&!g_searchBuf.empty()&&PtIn(R_searchClear,x,y)) return Z_SEARCH_CLEAR;
    if(R_searchBox.right>R_searchBox.left&&PtIn(R_searchBox,x,y)) return Z_SEARCH_BOX;
    if(DcCardsOn()) for(size_t k=0;k<R_plDcBtns.size()&&k<R_plCards.size();++k) if(R_plDcBtns[k].right>R_plDcBtns[k].left&&PtIn(R_plDcBtns[k],x,y)&&UiHot(R_plCards[k])) return Z_DC_PLCARD_BASE+(int)k;
    for(size_t k=0;k<R_plCards.size();++k){ if(R_plCards[k].right<=R_plCards[k].left) continue; if(PtIn(R_plPlay[k],x,y)) return Z_PL_PLAY_BASE+(int)k; if(PtIn(R_plShuf[k],x,y)) return Z_PL_SHUF_BASE+(int)k; if(PtIn(R_plCards[k],x,y)) return Z_PL_CARD_BASE+(int)k; }
    if(g_pickMode){ for(size_t i=0;i<R_cardRects.size();++i) if(R_cardRects[i].right>R_cardRects[i].left&&PtIn(R_cardRects[i],x,y)) return Z_TRACK_BASE+(int)i; return -1; }   // marcando: o card inteiro marca
    for(size_t i=0;i<R_cardCoverButtons.size();++i)if(R_cardCoverButtons[i].right>R_cardCoverButtons[i].left&&PtIn(R_cardCoverButtons[i],x,y))return Z_COVER_BASE+(int)i;
    for(size_t i=0;i<R_rowUp.size();++i){
        if(R_rowUp[i].right>R_rowUp[i].left&&PtIn(R_rowUp[i],x,y))return Z_ROW_UP_BASE+(int)i;
        if(R_rowDown[i].right>R_rowDown[i].left&&PtIn(R_rowDown[i],x,y))return Z_ROW_DOWN_BASE+(int)i;
    }
    // Estilos novos: ▶ DISCORD em cima da capa (bot conectado) e botao de play sobre a capa (faixa atual: pausa/continua; outra: toca).
    if(!UiClassic()&&g_cfg.listMode==0&&DcCardsOn()) for(size_t i=0;i<R_cardDcBtns.size();++i)
        if(R_cardDcBtns[i].right>R_cardDcBtns[i].left&&PtIn(R_cardDcBtns[i],x,y)) return Z_DC_CARD_BASE+(int)i;
    if(!UiClassic()&&g_cfg.listMode==0) for(size_t i=0;i<R_cardPlayBtns.size();++i)
        if(R_cardPlayBtns[i].right>R_cardPlayBtns[i].left&&PtIn(R_cardPlayBtns[i],x,y)) return (int)i==g_current?Z_PLAYPAUSE:(Z_TRACK_BASE+(int)i);
    // Classico: botoes de transporte dos cards ANTES da zona de seek.
    if(UiClassic()&&g_cfg.listMode==0) for(size_t i=0;i<R_cardRects.size();++i){
        RECT rr=R_cardRects[i];if(rr.right-rr.left<=0)continue;
        int ccx=(rr.left+rr.right)/2, cy=rr.bottom-SI(40);
        bool cur=(int)i==g_current;
        if(PtIn({ccx-SI(120)-SI(15),cy-SI(15),ccx-SI(120)+SI(15),cy+SI(15)},x,y))return Z_SHUFFLE;
        if(PtIn({ccx-SI(66)-SI(19),cy-SI(19),ccx-SI(66)+SI(19),cy+SI(19)},x,y))return Z_CARD_PREV_BASE+(int)i;
        if(PtIn({ccx-SI(27),cy-SI(27),ccx+SI(27),cy+SI(27)},x,y))return cur?Z_PLAYPAUSE:(Z_TRACK_BASE+(int)i);
        if(PtIn({ccx+SI(66)-SI(19),cy-SI(19),ccx+SI(66)+SI(19),cy+SI(19)},x,y))return Z_CARD_NEXT_BASE+(int)i;
        if(PtIn({ccx+SI(120)-SI(15),cy-SI(15),ccx+SI(120)+SI(15),cy+SI(15)},x,y))return Z_REPEAT;
    }
    for(size_t i=0;i<R_cardSeekRects.size();++i)if(R_cardSeekRects[i].right>R_cardSeekRects[i].left&&PtIn(R_cardSeekRects[i],x,y))return Z_CARD_SEEK_BASE+(int)i;
    for(size_t i=0;i<R_cardRects.size();++i)if(R_cardRects[i].right>R_cardRects[i].left&&PtIn(R_cardRects[i],x,y))return Z_TRACK_BASE+(int)i;
    return -1;
}
// Faixa sob o cursor (para o menu de contexto): card/linha ou a arte do painel.
static int TrackAt(int x,int y){
    if(g_showSettings||AnyOverlay()) return -1;
    for(size_t i=0;i<R_cardRects.size();++i)if(R_cardRects[i].right>R_cardRects[i].left&&PtIn(R_cardRects[i],x,y))return (int)i;
    if(g_current>=0&&(PtIn(R_art,x,y)||(g_cfg.displayMode!=L"vertical"&&PtIn(R_playerPanel,x,y)))) return g_current;
    return -1;
}

static void OnMouseDragSlider(int x){
    for(auto&s:g_setSliders){
        if(s.id!=g_dragSeek)continue;
        float frac=(float)(x-s.r.left)/(float)std::max<int>(1,s.r.right-s.r.left);frac=std::max(0.f,std::min(1.f,frac));
        int val=(int)std::lround(s.minv+frac*(s.maxv-s.minv));
        if(IsEqId(s.id)){ g_cfg.eq[s.id-Z_EQ_BASE]=val; ApplyEqNow(); break; }
        switch(s.id){case Z_UI_SCALE:g_cfg.uiScale=val;break;case Z_TITLE_SCALE:g_cfg.titleScale=val;break;case Z_ARTIST_SCALE:g_cfg.artistScale=val;break;case Z_VERTICAL_SCALE:g_cfg.verticalScale=val;break;case Z_PLAYER_SIZE_SLIDER:g_cfg.playerScale=val;break;case Z_LED_BRIGHT:g_cfg.ledBrightness=val;break;case Z_LED_SPEED:g_cfg.ledSpeed=val;break;case Z_RUNNER_SPEED:g_cfg.runnerSpeed=val;break;case Z_PART_SPEED:g_cfg.particlesSpeed=val;break;case Z_CD_SPEED:g_cfg.cdSpeed=val;break;default:g_cfg.ledSpeed=val;break;}
        break;
    }
    g_cfg.Clamp();BuildLayout();
}
static int g_rsArt0=0, g_rsScale0=100;

static void OnLButtonDown(int x,int y){
    if(g_showSplash){g_showSplash=false;return;}
    if(g_searchFocus&&!PtIn(R_searchBox,x,y)) g_searchFocus=false;   // clicar fora tira o foco da busca (o filtro continua)
    if(g_hkCapture>=0&&!(g_showSettings&&PtIn(R_hkKey[g_hkCapture],x,y))) g_hkCapture=-1;
    if(g_confirmOpen){
        if(PtIn(R_confirmYes,x,y)){ ConfirmYes(); }
        else if(PtIn(R_confirmNo,x,y)||!PtIn(R_confirmBox,x,y)){ if(g_confirmKind==2) HostConfirm(false); g_confirmOpen=false; g_confirmKind=0; }
        return;
    }
    if(g_ctxOpen){
        int hit=-1;
        for(size_t i=0;i<R_ctxItems.size();++i) if(PtIn(R_ctxItems[i],x,y)) hit=(int)i;
        g_ctxOpen=false;
        if(hit<0||hit>=(int)g_ctxActs.size()) return;
        int act=g_ctxActs[(size_t)hit], arg=g_ctxArg; RECT box=R_ctxBox;
        if(RunMenuAction(g_ctxKind,act,arg)) return;   // menus novos (adicionar, pasta da playlist, online...)
        if(g_ctxKind==CTX_TRACK){
            int t=arg; if(t<0||t>=(int)g_tracks.size()) return;
            switch(act){
            case CA_PLAY: PlayIndex(t,true); break;
            case CA_COVER: OpenImgMenu(t,box); break;
            case CA_ARTIST: StartArtistEdit(t); break;
            case CA_RENAME: StartFileRename(t); break;
            case CA_FOLDER: PlatformOpenFolder(std::filesystem::path(g_tracks[(size_t)t].path).parent_path().wstring()); break;
            case CA_ADDPL: OpenPickPlaylistMenu(t,box.left,box.top); break;
            case CA_REMOVEPL: RemoveTrackFromOpenPlaylist(t); break;
            case CA_DELETE: AskDeleteTrack(t); OpenConfirm(); break;
            case CA_DC_PLAY: DiscordPlayTrack(t); break;
            default: break;
            }
        } else if(g_ctxKind==CTX_PLAYLIST){
            switch(act){
            case CA_PL_PLAY: PlayPlaylist(arg,false); break;
            case CA_PL_SHUF: PlayPlaylist(arg,true); break;
            case CA_PL_RENAME: StartPlaylistNameEdit(3,arg); break;
            case CA_PL_DELETE: AskDeletePlaylist(arg); OpenConfirm(); break;
            case CA_PL_HOST: HostTogglePlaylist(arg); break;
            case CA_DC_PLPLAY: DiscordPlayPlaylist(arg,false); break;
            case CA_DC_PLTOGGLE: DcToggleCfgPlaylist(arg); break;
            default: break;
            }
        } else if(g_ctxKind==CTX_PICKPL){
            if(act==CA_PICK_NEW) AddTrackToPlaylist(-1,arg);
            else if(act>=CA_PICK_BASE) AddTrackToPlaylist(act-CA_PICK_BASE,arg);
        }
        return;
    }
    if(g_folderMenuOpen){
        int hit=-1;
        for(size_t i=0;i<R_folderItems.size();++i) if(PtIn(R_folderItems[i],x,y)) hit=(int)i;
        g_folderMenuOpen=false;
        if(hit<0) return;
        const std::wstring& p=g_folderItemPaths[(size_t)hit];
        if(p.empty()) PlatformPickFolderAsync();
        else if(p==L"*") SwitchFolder(L"");
        else SwitchFolder(p);
        BuildLayout();
        return;
    }
    {
        WebPick& wb=WP();
        if(wb.open){
            if(PtIn(wb.btnClose,x,y)){wb.open=false;wb.editing=false;return;}
            if(PtIn(wb.qbox,x,y)){wb.editing=true;return;}
            if(PtIn(wb.btnSearch,x,y)){wb.editing=false;std::wstring q=wb.query;WebSearchAsync(q);return;}
            for(size_t i=0;i<wb.cells.size();++i) if(PtIn(wb.cells[i],x,y)){
                std::lock_guard<std::mutex> lk(wb.m);
                wb.sel=(i<wb.res.size())?(int)i:-1;
                return;
            }
            if(PtIn(wb.btnUse,x,y)){
                bool can=false;
                {std::lock_guard<std::mutex> lk(wb.m);can=(wb.sel>=0&&wb.sel<(int)wb.res.size()&&!wb.downloading);}
                if(can){WebDownloadSelectedAsync();}
                return;
            }
            if(PtIn(wb.btnCancel,x,y)||!PtIn(wb.box,x,y)){wb.open=false;wb.editing=false;return;}
            return;
        }
        if(g_imgMenuOpen){
            if(PtIn(R_imgLocal,x,y)){ g_imgMenuOpen=false; PlatformPickImageAsync(EV_PICK_IMAGE,g_imgMenuTrack); return; }
            if(PtIn(R_imgWeb,x,y)){
                wb.open=true; wb.track=g_imgMenuTrack; wb.sel=-1; wb.scroll=0;
                wb.editing=true;
                {std::lock_guard<std::mutex> lk(wb.m); ClearWebResultsPlatform(); wb.status=L"Buscando imagens..."; }
                std::wstring q=(g_imgMenuTrack>=0&&g_imgMenuTrack<(int)g_tracks.size())?(g_tracks[g_imgMenuTrack].title+L" "+g_tracks[g_imgMenuTrack].artist):L"capa album musica";
                wb.query=q;
                g_imgMenuOpen=false;
                WebSearchAsync(q);
                return;
            }
            g_imgMenuOpen=false;
            return;
        }
    }
    if(g_editArtist){
        LayoutEditor(g_winW,g_winH);
        if(PtIn(R_editSave,x,y)) CommitArtistEdit();
        else if(PtIn(R_editCancel,x,y)||!PtIn(R_editBox,x,y)) CancelArtistEdit();
        return;
    }
    if(g_spadP.open){ int z=HitTest(x,y); if(z<=0&&!PtIn(g_spadP.box,x,y)){ g_spadP.open=false; return; } if(z>0) SpadClick(z); return; }
    if(g_dcP.open){ int z=HitTest(x,y); if(z<=0&&!PtIn(g_dcP.box,x,y)){ g_dcP.open=false; return; } if(z>0) DcClick(z); return; }
    if(g_fxp.open){   // painel EFEITOS
        int fid=HitTest(x,y);
        if(fid==Z_FX_CLOSE||!PtIn(g_fxp.box,x,y)){ g_fxp.open=false; return; }
        if(fid==Z_FX_CLEAR){ ClearFx(); return; }
        if(fid==Z_FX_CANCEL){ stems::CancelQueued(true); SetStatus(L"Separação cancelada.",2000); return; }
        if(fid>=Z_FX_BASE&&fid<Z_FX_BASE+5){ CycleFx(fid-Z_FX_BASE); return; }
        if(fid>=Z_STEM_BASE&&fid<Z_STEM_BASE+6){ SetStemMode(fid-Z_STEM_BASE); return; }
        return;
    }
    if(host::PU().open){   // painel HOST
        host::PanelUI& p=host::PU(); int hid=HitTest(x,y);
        if(hid==Z_HOST_CLOSE||!PtIn(p.box,x,y)){ p.open=false; return; }
        if(hid==Z_HOST_TOGGLE){ HostToggle(); return; }
        if(hid==Z_HOST_TUNNEL){ if(host::St().tunRunning.load()){ host::TunnelStop(); g_cfg.hostTunnel=false; SetStatus(L"Túnel desligado.",2500); } else { g_cfg.hostTunnel=true; if(host::Running()) host::TunnelStart(g_cfg.hostPort); else SetStatus(L"Ligue o Host primeiro.",2500); } g_cfg.Save(); return; }
        if(hid==Z_HOST_NEWLINK){ if(!host::Running()){ SetStatus(L"Ligue o Host primeiro.",2500); return; } g_cfg.hostTunnel=true; g_cfg.Save(); host::TunnelRestart(g_cfg.hostPort); SetStatus(L"Gerando um link novo do túnel (ele é testado antes de aparecer)...",3500); return; }
        if(hid==Z_HOST_COPYTUN){ HostCopy(true); return; } if(hid==Z_HOST_COPYLAN){ HostCopy(false); return; }
        if(hid==Z_HOST_QRMODE){ p.qrTunnel=!p.qrTunnel; SetStatus(p.qrTunnel?L"QR pela internet (túnel), quando o link estiver pronto.":L"QR pela rede local (mesmo roteador).",2600); return; }
        if(hid==Z_HOST_QRNEW){ p.qrDev.clear(); host::RotateQr(); SetStatus(L"QR novo: o anterior não vale mais.",2400); return; }
        if(hid==Z_HOST_ONLINE){ HostSetOnline(!g_cfg.hostOnline); return; }
        if(hid==Z_HOST_QRCONF){ HostSetQrConfirm(!g_cfg.hostQrConfirm); return; }
        if(hid==Z_HOST_IPV6){ HostSetIpv6(!g_cfg.hostIPv6); return; }
        if(hid>=Z_HOST_DEVLINK_BASE&&hid<Z_HOST_DEVLINK_BASE+100){ HostCopyDeviceLink((size_t)(hid-Z_HOST_DEVLINK_BASE)); return; }
        if(hid>=Z_HOST_DEVLIB_BASE&&hid<Z_HOST_DEVLIB_BASE+100){ size_t i=(size_t)(hid-Z_HOST_DEVLIB_BASE); if(i<p.v.devs.size()){ bool on=!p.v.devs[i].lib; host::SetDeviceLib(p.v.devs[i].id,on); SetStatus(on?L"\""+Utf8ToWide(p.v.devs[i].name)+L"\" agora vê a biblioteca inteira do PC.":L"\""+Utf8ToWide(p.v.devs[i].name)+L"\" não vê mais a biblioteca (só playlists hosteadas).",3200); } return; }
        if(hid>=Z_HOST_DPLOK_BASE&&hid<Z_HOST_DPLOK_BASE+500){ size_t i=(size_t)(hid-Z_HOST_DPLOK_BASE); if(i<p.v.dpls.size()){ bool ok=!p.v.dpls[i].pcOk; host::SetDevPlaylistOk(p.v.dpls[i].dev,p.v.dpls[i].slug,ok); SetStatus(ok?L"Playlist liberada para os outros aparelhos.":L"Playlist voltou a ser só do aparelho dono.",3000); } return; }
        if(hid==Z_HOST_HTML){ HostMakeHtml(); return; }
        if(hid==Z_HOST_PASTA){ PlatformOpenFolder(Config::BaseDir()); return; }
        if(hid==Z_HOST_PORT){ StartHostEdit(6); return; } if(hid==Z_HOST_PIN){ StartHostEdit(7); return; } if(hid==Z_HOST_NAME){ StartHostEdit(8); return; }
        if(hid==Z_HOST_LAN){ g_cfg.hostLan=!g_cfg.hostLan; g_cfg.Save(); if(host::Running()){ HostStopNow(); HostStartFromCfg(); } SetStatus(g_cfg.hostLan?L"Rede local liberada (mesmo roteador).":L"Só pelo túnel (127.0.0.1).",2600); return; }
        if(hid>=Z_HOST_ACCEPT_BASE&&hid<Z_HOST_ACCEPT_BASE+100){ size_t i=(size_t)(hid-Z_HOST_ACCEPT_BASE); if(i<p.v.pending.size()){ bool ok=host::Approve(p.v.pending[i].id,true); if(g_hostReq==Utf8ToWide(p.v.pending[i].id)){ g_hostReq.clear(); g_confirmOpen=false; g_confirmKind=0; } SetStatus(ok?L"Dispositivo aceito.":L"Esse pedido expirou: peça para o celular tentar de novo.",2800); } return; }
        if(hid>=Z_HOST_DENY_BASE&&hid<Z_HOST_DENY_BASE+100){ size_t i=(size_t)(hid-Z_HOST_DENY_BASE); if(i<p.v.pending.size()){ bool ok=host::Approve(p.v.pending[i].id,false); if(g_hostReq==Utf8ToWide(p.v.pending[i].id)){ g_hostReq.clear(); g_confirmOpen=false; g_confirmKind=0; } SetStatus(ok?L"Pedido recusado.":L"Esse pedido já tinha expirado.",2500); } return; }
        if(hid>=Z_HOST_REVOKE_BASE&&hid<Z_HOST_REVOKE_BASE+100){ size_t i=(size_t)(hid-Z_HOST_REVOKE_BASE); if(i<p.v.devs.size()){ if(p.qrDev==p.v.devs[i].id) p.qrDev.clear(); host::Revoke(p.v.devs[i].id); SetStatus(L"Aparelho removido: ele precisa vincular de novo.",3000); } return; }
        if(hid>=Z_HOST_PL_BASE&&hid<Z_HOST_PL_BASE+200){ HostTogglePlaylist(hid-Z_HOST_PL_BASE); return; }
        if(hid>=Z_HOST_PLDEV_BASE&&hid<Z_HOST_PLDEV_BASE+4000){ int k=hid-Z_HOST_PLDEV_BASE; int pl=k/20, dv=k%20; if(pl<(int)g_playlists.size()&&dv<(int)p.v.devs.size()) host::ToggleTargetDevice(g_playlists[(size_t)pl].slug,p.v.devs[(size_t)dv].id); return; }
        return;
    }
    if(OU().open){
        OnlineUI& u=OU();
        int oid=HitTest(x,y);
        if(oid==Z_ON_CLOSE||!PtIn(u.box,x,y)){ u.open=false; u.editing=false; return; }
        if(oid==Z_ON_QBOX){ u.editing=true; return; }
        u.editing=false;
        if(oid==Z_ON_SEARCH){ OnlineSearchAsync(); return; }
        if(oid>=Z_ON_SRC_BASE&&oid<Z_ON_SRC_BASE+3){ u.source=oid-Z_ON_SRC_BASE; g_cfg.onlineSource=u.source; g_cfg.Save(); std::wstring q=Config::Trim(u.query); if(!q.empty()&&!IsUrlText(q)) OnlineSearchAsync(); return; }
        if(oid>=Z_ON_TAB_BASE&&oid<Z_ON_TAB_BASE+3){
            int t=oid-Z_ON_TAB_BASE;
            if(u.tipo!=t){ u.tipo=t; u.scroll=0; { std::lock_guard<std::mutex> lk(u.m); u.res.clear(); u.listas.clear(); u.status=t==0?L"Digite o nome da música (ou cole um link) e aperte ENTER.":(t==1?L"Procure playlists prontas pelo nome.":L"Procure álbuns pelo nome."); } std::wstring q=Config::Trim(u.query); if(!q.empty()&&!IsUrlText(q)) OnlineSearchAsync(); }
            return;
        }
        if(u.tipo!=0&&oid>=Z_ON_PLAY_BASE&&oid<Z_ON_PLAY_BASE+500){   // playlist/album: abre o link (vira lista de musicas)
            size_t i=(size_t)(oid-Z_ON_PLAY_BASE); std::wstring link;
            { std::lock_guard<std::mutex> lk(u.m); if(i<u.listas.size()) link=u.listas[i].link; }
            if(!link.empty()){ u.tipo=0; u.query=link; OnlineSearchAsync(); }
            return;
        }
        if(oid>=Z_ON_PLAY_BASE&&oid<Z_ON_PLAY_BASE+500){ OnlinePlayResult(oid-Z_ON_PLAY_BASE); return; }
        if(oid>=Z_ON_DL_BASE&&oid<Z_ON_DL_BASE+500){ OnlineDownloadResult(oid-Z_ON_DL_BASE); return; }
        if(oid>=Z_ON_ADD_BASE&&oid<Z_ON_ADD_BASE+500){ OnlineAddResult(oid-Z_ON_ADD_BASE,x,y); return; }
        if(oid==Z_ON_ADDALL){ OnlineAddAll(u.btnAddAll.left,u.btnAddAll.top-(int)S(220)); return; }
        if(oid==Z_ON_CFG){ u.open=false; u.editing=false; g_showSettings=true; EnsureToolsAsync(); SetStatus(L"Configurações > FONTES EXTERNAS: escolha o programa de linha de comando.",5000); BuildLayout(); return; }
        return;
    }
    int id=HitTest(x,y);
    if(id==Z_CLOSE){RequestClose();return;} if(id==Z_MIN){PlatformMinimize();return;} if(id==Z_GEAR){g_showSettings=!g_showSettings;g_hkCapture=-1;if(g_showSettings)EnsureToolsAsync();return;}
    if(id>=Z_RX_NAV_BASE&&id<Z_RX_NAV_BASE+3){
        int n=id-Z_RX_NAV_BASE;
        if(n==0){ g_rxPag=RXP_INICIO; g_searchFocus=false; g_rxScroll=0; BuildLayout(); }
        else if(n==1){ g_rxPag=RXP_DESCOBRIR; g_searchFocus=false; g_rxScroll=0; EnsureToolsAsync(); BuildLayout(); }
        else { g_rxPag=RXP_LISTA; EnterLibraryView(); }
        return;
    }
    if(id==Z_RX_NOVAPL){ OpenNewPlaylistMenu(R_rxNovaPl.left,R_rxNovaPl.bottom+4); return; }
    if(id>=Z_RX_ATALHO_BASE&&id<Z_RX_ATALHO_BASE+16){
        size_t i=(size_t)(id-Z_RX_ATALHO_BASE);
        if(i>=g_rxAtalhos.size()) return;
        const RxAtalho& k=g_rxAtalhos[i];
        if(k.tipo==1){ g_rxPag=RXP_LISTA; if(k.idx>=0&&k.idx<(int)g_playlists.size()) OpenPlaylistView(k.idx); return; }
        const std::vector<Track>& fonte=(g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks;
        if(k.idx>=0&&k.idx<(int)fonte.size()){ std::wstring chave=fonte[(size_t)k.idx].path; RxTocarLocal({chave},0); }
        return;
    }
    if(id==Z_RX_HERO){ if(g_rxHeroOk) RxAbrirItem(g_rxHeroItem,true); return; }
    if(id==Z_RX_LETRA){ g_rxLetraOn=!g_rxLetraOn; g_rxLetraScroll=0; BuildLayout(); return; }
    if(id==Z_RX_PAINEL){ g_rxPainelOn=!g_rxPainelOn; BuildLayout(); return; }
    if(id==Z_RX_SAIDA){ OpenSaidaMenu((int)R_rxSaida.left-(int)S(240),(int)R_rxSaida.top-(int)S(220)); return; }
    if(id>=Z_RX_LETRA_LINHA_BASE&&id<Z_RX_LETRA_LINHA_BASE+200){
        size_t i=(size_t)(id-Z_RX_LETRA_LINHA_BASE);
        const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
        if(ct&&g_player.loaded){
            letra::Letra L=letra::Para(ct->path,ct->title,ct->artist,DurSegundos(ct->path,ct->durSec),RxAvisarNovidades);
            if(i<L.linhas.size()) g_player.SeekMs((DWORD)std::max(0,L.linhas[i].ms));
        }
        return;
    }
    if(id==Z_RX_VOLTAR){ g_rxGenero.clear(); g_rxGeneroNome.clear(); g_rxScroll=0; BuildLayout(); return; }
    if(id==Z_RX_BUSCARON){ OU().tipo=0; OpenOnlineSearch(-1,g_searchBuf); return; }
    if(id==Z_RX_BUSCAPL||id==Z_RX_BUSCAAL){
        OU().tipo=(id==Z_RX_BUSCAPL)?1:2;
        { std::lock_guard<std::mutex> lk(OU().m); OU().res.clear(); OU().listas.clear(); }
        OpenOnlineSearch(-1,g_searchBuf);
        return;
    }
    if(id==Z_RX_TOCAR||id==Z_RX_ALEATORIO){
        bool al=(id==Z_RX_ALEATORIO);
        if(g_tracks.empty()){ SetStatus(g_view==2?L"Playlist vazia.":L"Biblioteca vazia.",2500); return; }
        if(g_cfg.shuffle!=al){ g_cfg.shuffle=al; g_cfg.Save(); }
        int start=0; if(al){ RebuildShuffleQueue(-1); start=g_shufQueue.empty()?0:g_shufQueue[0]; }
        PlayIndex(start,true); return;
    }
    if(id>=Z_RX_SIDE_BASE&&id<Z_RX_SIDE_BASE+500){
        size_t i=(size_t)(id-Z_RX_SIDE_BASE);
        if(i>=g_rxSideOrdem.size()) return;
        g_rxPag=RXP_LISTA;
        int pl=g_rxSideOrdem[i];
        if(pl<0) EnterLibraryView(); else if(pl<(int)g_playlists.size()) OpenPlaylistView(pl);
        return;
    }
    if(id>=Z_RX_VERTUDO_BASE&&id<Z_RX_VERTUDO_BASE+100){ RxVerTudo((size_t)(id-Z_RX_VERTUDO_BASE)); return; }
    if(id>=Z_RX_CARDPLAY_BASE&&id<Z_RX_CARDPLAY_BASE+4000){ RxClicarCard((size_t)(id-Z_RX_CARDPLAY_BASE),true); return; }
    if(id>=Z_RX_CARD_BASE&&id<Z_RX_CARD_BASE+4000){ RxClicarCard((size_t)(id-Z_RX_CARD_BASE),false); return; }
    if(id==Z_TAB_TRACKS){ EnterLibraryView(); return; }
    if(id==Z_TAB_PLAYLISTS){ ShowPlaylistCards(); return; }
    if(id==Z_PL_BACK){ ShowPlaylistCards(); return; }
    if(id==Z_PL_NEW){ OpenNewPlaylistMenu(R_plNew.left,R_plNew.bottom+4); return; }
    if(id==Z_TAB_ONLINE){ OpenOnlineSearch(-1,L""); return; }
    if(id==Z_PL_ADD){ OpenAddMenu(g_openPl,R_plAdd.left,R_plAdd.bottom+4); return; }
    if(id==Z_PL_MODE){ CyclePlaylistMode(g_openPl); return; }
    if(id==Z_PICK_DONE){ FinishPickMode(true); return; }
    if(id==Z_PICK_CANCEL){ FinishPickMode(false); return; }
    if(id==Z_ACTIVITY){ OpenActivityMenu(R_activity.left,R_activity.top-(int)S(90)); return; }
    if(id==Z_EMPTY_ACTION){ if(g_view==2) OpenAddMenu(g_openPl,R_onlineInfo.left,R_onlineInfo.bottom+4); else OpenFolderMenu(); return; }
    if(id==Z_SEARCH_BOX){ g_searchFocus=true; return; }
    if(id==Z_SEARCH_CLEAR){ ClearSearch(); return; }
    if(id>=Z_HK_KEY_BASE&&id<Z_HK_KEY_BASE+HK_COUNT){ g_hkCapture=id-Z_HK_KEY_BASE; SetStatus(L"Pressione a tecla (ou combinação) para: "+std::wstring(HkLabel(g_hkCapture))+L". Backspace limpa, Esc cancela.",4000); return; }
    if(id>=Z_HK_SCOPE_BASE&&id<Z_HK_SCOPE_BASE+HK_COUNT){ int a=id-Z_HK_SCOPE_BASE; g_cfg.hk[a].global=!g_cfg.hk[a].global; g_cfg.Save(); PlatformUpdateGlobalHotkeys(); SetStatus(g_cfg.hk[a].global?L"GLOBAL: funciona com o Remix em segundo plano ou com outro programa na frente.":L"FOCO: só funciona com a janela do Remix ativa (não atrapalha jogos).",3200); return; }
    if(id==Z_HK_RESET){ g_cfg.ResetHotkeys(); g_cfg.Save(); PlatformUpdateGlobalHotkeys(); SetStatus(L"Atalhos restaurados para o padrão.",2000); return; }
    if(id>=Z_PL_CARD_BASE&&id<Z_PL_CARD_BASE+500){ int k=id-Z_PL_CARD_BASE; int n=(int)g_playlists.size(); if(k==0) EnterLibraryView(); else if(k<=n) OpenPlaylistView(k-1); else OpenNewPlaylistMenu(x,y); return; }
    if(id>=Z_PL_PLAY_BASE&&id<Z_PL_PLAY_BASE+500){ PlayPlaylist(id-Z_PL_PLAY_BASE-1,false); return; }
    if(id>=Z_PL_SHUF_BASE&&id<Z_PL_SHUF_BASE+500){ PlayPlaylist(id-Z_PL_SHUF_BASE-1,true); return; }
    if(id==Z_SETTINGS_CLOSE){g_showSettings=false;return;}
    if(id==Z_SETTINGS_DEFAULT){SwitchFolder(L"");BuildLayout();return;}
    if(id==Z_SETTINGS_CUSTOM){PlatformPickFolderAsync();return;}
    if(id==Z_SETTINGS_MODE_SQUARE||id==Z_SETTINGS_MODE_CD||id==Z_SETTINGS_MODE_VERTICAL){
        if(id==Z_SETTINGS_MODE_SQUARE){g_cfg.displayMode=L"normal";g_cfg.artShape=L"square";}
        else if(id==Z_SETTINGS_MODE_CD){g_cfg.displayMode=L"normal";g_cfg.artShape=L"cd";}
        else {g_cfg.displayMode=L"vertical";}
        g_cfg.Save();g_showSettings=false;g_listScroll=0;g_setScroll=0;PlatformResizeForMode();return;}
    if(id>=Z_SETTINGS_STYLE_BASE&&id<Z_SETTINGS_STYLE_BASE+UI_STYLE_COUNT){
        g_cfg.uiStyle=id-Z_SETTINGS_STYLE_BASE; g_cfg.Save(); g_listScroll=0; BuildLayout();
        SetStatus(std::wstring(L"Estilo: ")+UiStyleName(g_cfg.uiStyle),2200); return; }
    if(id==Z_FX_BTN){ g_fxp.open=true; return; }
    if(id==Z_SPAD_BTN||id==Z_SET_SPAD){ g_spadP.open=true; g_spadP.scroll=0; return; }
    if(id==Z_DC_BTN||id==Z_SET_DC){ g_dcP.open=true; g_dcP.scroll=0; dc::ProbeTools(true); return; }
    if(id>=Z_DC_CARD_BASE&&id<Z_DC_CARD_BASE+1000000){ DiscordPlayTrack(id-Z_DC_CARD_BASE); return; }
    if(id>=Z_DC_PLCARD_BASE&&id<Z_DC_PLCARD_BASE+500){ DiscordPlayPlaylist(id-Z_DC_PLCARD_BASE-1,false); return; }
    if(id==Z_HOST_BTN||id==Z_SET_HOST_PANEL){ host::PU().open=true; host::PU().scroll=0; host::PU().v=host::GetView(); return; }
    if(id==Z_SET_HOST_ON){ HostToggle(); return; }
    if(id==Z_SET_HOST_PORT){ StartHostEdit(6); return; } if(id==Z_SET_HOST_PIN){ StartHostEdit(7); return; } if(id==Z_SET_HOST_NAME){ StartHostEdit(8); return; }
    if(id==Z_SET_HOST_TUNNEL){ g_cfg.hostTunnel=!g_cfg.hostTunnel; g_cfg.Save(); if(host::Running()){ if(g_cfg.hostTunnel) host::TunnelStart(g_cfg.hostPort); else host::TunnelStop(); } return; }
    if(id==Z_SET_HOST_LAN){ g_cfg.hostLan=!g_cfg.hostLan; g_cfg.Save(); if(host::Running()){ HostStopNow(); HostStartFromCfg(); } return; }
    if(id==Z_SET_CLI){ g_editArtist=true; g_editMode=12; g_editTrack=-1; g_editBuf=g_cfg.mediaCli; return; }
    if(id==Z_SET_CLI_BUSCAR){ PlatformPickProgramAsync(EV_PICK_CLI); return; }   // a pessoa escolhe o arquivo
    if(id==Z_SET_NOVIDADES){
        g_cfg.novidadesOnline=!g_cfg.novidadesOnline; g_cfg.Save();
        SetStatus(g_cfg.novidadesOnline?L"Novidades ligadas: o Início mostra o que está em alta (metadados de catálogo público).":L"Novidades desligadas: o Início mostra só a sua biblioteca, e o app não busca nada sozinho.",4000);
        g_rxScroll=0; BuildLayout(); return;
    }
    if(id==Z_SET_SEP){ g_editArtist=true; g_editMode=13; g_editTrack=-1; g_editBuf=g_cfg.sepCmd; return; }
    if(id==Z_SET_SEP_BUSCAR){ PlatformPickProgramAsync(EV_PICK_SEP); return; }
    if(id==Z_SET_STEMSCPU){
        g_cfg.stemsCpu=g_cfg.stemsCpu>=3?1:g_cfg.stemsCpu+1; g_cfg.Save();
        SetStatus(std::wstring(L"Separação: ")+stems::PerfilNome(g_cfg.stemsCpu)+L" ("+std::to_wstring(stems::NucleosDoPerfil())+L" de "+std::to_wstring((int)std::thread::hardware_concurrency())+L" núcleos)",3500);
        return;
    }
    if(id==Z_SET_HOST_COPYTUN){ HostCopy(true); return; } if(id==Z_SET_HOST_COPYLAN){ HostCopy(false); return; }
    if(id==Z_SET_HOST_ONLINE){ HostSetOnline(!g_cfg.hostOnline); return; }
    if(id==Z_SET_HOST_QRCONF){ HostSetQrConfirm(!g_cfg.hostQrConfirm); return; }
    if(id==Z_SET_HOST_IPV6){ HostSetIpv6(!g_cfg.hostIPv6); return; }
    if(id==Z_LED_EFFECT){g_cfg.ledEffect=g_cfg.ledEffect==L"respiracao"?L"pulso":g_cfg.ledEffect==L"pulso"?L"estatico":L"respiracao";g_cfg.Save();return;}
    if(id==Z_PARTICLES_TOGGLE){g_cfg.particlesOn=!g_cfg.particlesOn;g_cfg.Save();return;}
    if(id==Z_GLITCH_TOGGLE){g_cfg.glitchOn=!g_cfg.glitchOn;g_cfg.Save();return;}
    if(id==Z_PERF_TOGGLE){g_cfg.perfMode=!g_cfg.perfMode;g_cfg.Save();SetStatus(g_cfg.perfMode?L"Modo leve ligado: efeitos pesados desligados.":L"Modo leve desligado.",2500);return;}
    if(id==Z_BG_TOGGLE){g_cfg.bgOnClose=!g_cfg.bgOnClose;g_cfg.Save();SetStatus(g_cfg.bgOnClose?L"Fechar a janela com musica tocando: continua em segundo plano.":L"Fechar a janela: encerra o Remix.",2800);return;}
    if(id==Z_SYSMEDIA_TOGGLE){g_cfg.sysMedia=!g_cfg.sysMedia;g_cfg.Save();SetStatus(g_cfg.sysMedia?L"Controles do sistema ligados (teclas de midia, bandeja/applet de midia).":L"Controles do sistema desligados.",2800);return;}
    if(id==Z_QUIT_BTN){QuitApp();return;}
    if(id==Z_SET_ON_MODE){ g_cfg.onlineMode=g_cfg.onlineMode==L"download"?L"stream":L"download"; g_cfg.Save(); SetStatus(g_cfg.onlineMode==L"download"?L"Músicas online: baixa ao tocar (e já toca por streaming enquanto baixa).":L"Músicas online: streaming só na memória (nada fica no disco).",3500); return; }
    if(id==Z_SET_ON_FMT){ g_cfg.onlineFormat=g_cfg.onlineFormat==L"mp3"?L"m4a":(g_cfg.onlineFormat==L"m4a"?L"original":L"mp3"); g_cfg.Save(); return; }
    if(id==Z_SET_ON_SRC){ g_cfg.onlineSource=(g_cfg.onlineSource+1)%3; OU().source=g_cfg.onlineSource; g_cfg.Save(); return; }
    if(id==Z_SET_ON_FOLDER){ PlatformPickFolderFor(EV_PICK_DLFOLDER,0); return; }
    if(id==Z_SET_ON_RECHECK){ fonte::Reconferir(); EnsureToolsAsync(); SetStatus(L"Conferindo a fonte externa, o ffmpeg e o runtime JavaScript...",2500); return; }
    if(id==Z_RUNNER_TOGGLE){g_cfg.runnerOn=!g_cfg.runnerOn;g_cfg.Save();return;}
    if(id==Z_AUTOPLAY||id==Z_SET_AUTOPLAY){g_cfg.autoplay=!g_cfg.autoplay;g_cfg.Save();SetStatus(g_cfg.autoplay?L"Autoplay ligado":L"Autoplay desligado: toca so a faixa escolhida",1800);return;}
    if(id==Z_SORT||id==Z_SET_SORT){CycleSortMode();BuildLayout();return;}
    if(id==Z_SET_SORTDIR){g_cfg.sortDesc=!g_cfg.sortDesc;ApplySort();g_cfg.Save();BuildLayout();return;}
    if(id==Z_EQ_ON){g_cfg.eqOn=!g_cfg.eqOn;ApplyEqNow();g_cfg.Save();return;}
    if(id==Z_EQ_RESET){for(int&g:g_cfg.eq)g=0;ApplyEqNow();g_cfg.Save();return;}
    if(id==Z_VOL_ICON){ToggleMute();return;}
    if(id==Z_FOLDER_BTN){ if(g_view==2) OpenPlaylistFolderMenu(g_openPl); else OpenFolderMenu(); return; }   // playlist aberta: pasta DA PLAYLIST
    if(id>=Z_ROW_UP_BASE&&id<Z_ROW_UP_BASE+Z_TRACK_RANGE){MoveTrack(id-Z_ROW_UP_BASE,-1);BuildLayout();return;}
    if(id>=Z_ROW_DOWN_BASE&&id<Z_ROW_DOWN_BASE+Z_TRACK_RANGE){MoveTrack(id-Z_ROW_DOWN_BASE,+1);BuildLayout();return;}
    auto setThemeIdColor=[&](std::wstring&dst,int i){
        if(i<=0){dst=L"";return;}
        size_t nt=g_themes.size();
        if((size_t)i<=nt){dst=g_themes[i-1].id;return;}
        int bi=(int)((size_t)i-nt-1);
        dst=(bi>=0&&bi<15)?std::wstring(g_brightIds[bi]):L"";
    };
    if(id>=Z_RUNNER_COLOR_BASE&&id<Z_RUNNER_COLOR_BASE+20){setThemeIdColor(g_cfg.runnerColor,id-Z_RUNNER_COLOR_BASE);g_cfg.Save();return;}
    if(id>=Z_BTN_PLAY_BASE&&id<Z_BTN_PLAY_BASE+20){setThemeIdColor(g_cfg.btnPlayColor,id-Z_BTN_PLAY_BASE);g_cfg.Save();return;}
    if(id>=Z_BTN_NAV_BASE&&id<Z_BTN_NAV_BASE+20){setThemeIdColor(g_cfg.btnNavColor,id-Z_BTN_NAV_BASE);g_cfg.Save();return;}
    if(id>=Z_PART_COLOR_BASE&&id<Z_PART_COLOR_BASE+20){setThemeIdColor(g_cfg.particlesColor,id-Z_PART_COLOR_BASE);g_cfg.autoColor=false;g_cfg.Save();return;}
    if(id>=Z_LED_COLOR_BASE&&id<Z_LED_COLOR_BASE+20){setThemeIdColor(g_cfg.ledColor,id-Z_LED_COLOR_BASE);g_cfg.autoColor=false;g_cfg.Save();return;}
    if(id==Z_AUTOCOLOR_TOGGLE){g_cfg.autoColor=!g_cfg.autoColor;g_cfg.Save();return;}
    if(id==Z_COVERBLUR_TOGGLE){g_cfg.coverBlurBg=!g_cfg.coverBlurBg;g_cfg.Save();return;}
    if(id==Z_SET_WALL_CHOOSE){PlatformPickImageAsync(EV_PICK_WALL,0);return;}
    if(id==Z_CLR_WALLPAPER){g_cfg.bgWallpaper=L"";g_cfg.Save();return;}
    if(id>=Z_THEME_BASE&&id<Z_THEME_BASE+100){int i=id-Z_THEME_BASE;if(i<(int)g_themes.size()){g_cfg.theme=g_themes[i].id;ApplyTheme();g_cfg.Save();}return;}
    if(id==Z_ARTIST_EDIT){StartArtistEdit(g_current);return;}
    if(id==Z_PLAYER_RESIZE){g_dragSeek=id;g_rsArt0=std::max(1,g_panelArt);g_rsScale0=g_cfg.playerScale;return;}
    if(id==Z_RX_SIDEDRAG){g_dragSeek=id;return;}   // arrastar a divisória da lateral
    if(id==Z_SHAPE_TOGGLE){g_cfg.artShape=(g_cfg.artShape==L"cd")?L"square":L"cd";g_cfg.Save();return;}
    if(id==Z_LISTMODE){g_cfg.listMode=g_cfg.listMode?0:1;g_listScroll=0;g_cfg.Save();BuildLayout();return;}
    if(id==Z_WAVESEEK){
        R_waveDragRect=PtIn(R_wavePanel,x,y)?R_wavePanel:R_seek;
        g_dragSeek=id;
        if(g_player.loaded&&g_player.GetLengthMs()>0){float frac=(float)(x-R_waveDragRect.left)/(float)std::max<int>(1,R_waveDragRect.right-R_waveDragRect.left);frac=std::max(0.f,std::min(1.f,frac));g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}
        return;
    }
    if(id==Z_UI_SCALE||id==Z_TITLE_SCALE||id==Z_ARTIST_SCALE||id==Z_VERTICAL_SCALE||id==Z_PLAYER_SIZE_SLIDER||id==Z_LED_BRIGHT||id==Z_LED_SPEED||id==Z_RUNNER_SPEED||id==Z_PART_SPEED||id==Z_CD_SPEED||IsEqId(id)){g_dragSeek=id;OnMouseDragSlider(x);return;}
    if(g_showSettings) return;
    if(id>=Z_COVER_BASE&&id<Z_COVER_BASE+Z_TRACK_RANGE){int i=id-Z_COVER_BASE;if(i>=0&&i<(int)g_tracks.size()){
        RECT anchor=(i<(int)R_cardCoverButtons.size()&&R_cardCoverButtons[i].right>R_cardCoverButtons[i].left)?R_cardCoverButtons[i]:R_verticalCoverButton;
        OpenImgMenu(i,anchor);
    }return;}
    if(id>=Z_CARD_SEEK_BASE&&id<Z_CARD_SEEK_BASE+Z_TRACK_RANGE){int i=id-Z_CARD_SEEK_BASE;if(i>=0&&i<(int)g_tracks.size()){if(g_current!=i)PlayIndex(i,false);g_dragSeek=id;int x0=R_cardSeekRects[i].left,x1=R_cardSeekRects[i].right;float frac=(float)(x-x0)/std::max<int>(1,x1-x0);frac=std::max(0.f,std::min(1.f,frac));if(g_player.loaded)g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}return;}
    if(id==Z_PLAYPAUSE){TogglePlayPause();return;}
    if(id==Z_NEXT){NextTrack();return;} if(id==Z_PREV){PrevOrRestart();return;}
    if(id==Z_SHUFFLE){g_cfg.shuffle=!g_cfg.shuffle;g_cfg.Save();return;} if(id==Z_REPEAT){g_cfg.repeat=!g_cfg.repeat;g_cfg.Save();return;}
    if(id>=Z_CARD_PREV_BASE&&id<Z_CARD_PREV_BASE+Z_TRACK_RANGE){PrevOrRestart();return;}
    if(id>=Z_CARD_NEXT_BASE&&id<Z_CARD_NEXT_BASE+Z_TRACK_RANGE){NextTrack();return;}
    if(id==Z_SEEKBAR||id==Z_VOLBAR){g_dragSeek=id;if(id==Z_SEEKBAR&&g_player.loaded&&g_player.GetLengthMs()>0){float frac=(float)(x-R_seek.left)/(float)std::max<int>(1,R_seek.right-R_seek.left);frac=std::max(0.f,std::min(1.f,frac));g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}
        if(id==Z_VOLBAR){float frac=(float)(x-R_vol.left)/(float)std::max<int>(1,R_vol.right-R_vol.left);SetVolumePercent((int)(std::max(0.f,std::min(1.f,frac))*100));}
        return;}
    if(id>=Z_TRACK_BASE&&id<Z_TRACK_BASE+Z_TRACK_RANGE){int i=id-Z_TRACK_BASE;if(i<(int)g_tracks.size()){ if(g_pickMode) TogglePick(i); else PlayIndex(i,true); }return;}
}
static void OnRButtonDown(int x,int y){
    if(g_showSplash) return;
    if(AnyOverlay()){ g_ctxOpen=false; g_folderMenuOpen=false; g_imgMenuOpen=false; return; }
    if(g_pickMode) return;
    int t=TrackAt(x,y);
    if(t>=0) OpenCtxMenu(t,x,y);
    else if(g_view==1){ for(size_t k=1;k<R_plCards.size()&&k<=g_playlists.size();++k) if(R_plCards[k].right>R_plCards[k].left&&PtIn(R_plCards[k],x,y)){ OpenPlaylistCtxMenu((int)k-1,x,y); break; } }
}
static void OnMouseDrag(int x){
    if(g_dragSeek==-1) return;
    if(g_dragSeek==Z_SEEKBAR&&g_player.loaded){float frac=(float)(x-R_seek.left)/(float)std::max<int>(1,R_seek.right-R_seek.left);frac=std::max(0.f,std::min(1.f,frac));g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}
    else if(g_dragSeek==Z_WAVESEEK&&g_player.loaded){float frac=(float)(x-R_waveDragRect.left)/(float)std::max<int>(1,R_waveDragRect.right-R_waveDragRect.left);frac=std::max(0.f,std::min(1.f,frac));g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}
    else if(g_dragSeek>=Z_CARD_SEEK_BASE&&g_dragSeek<Z_CARD_SEEK_BASE+Z_TRACK_RANGE){int i=g_dragSeek-Z_CARD_SEEK_BASE;if(i>=0&&i<(int)R_cardSeekRects.size()&&i<(int)g_tracks.size()&&g_player.loaded){float frac=(float)(x-R_cardSeekRects[i].left)/(float)std::max<int>(1,R_cardSeekRects[i].right-R_cardSeekRects[i].left);frac=std::max(0.f,std::min(1.f,frac));if(g_current==i)g_player.SeekMs((DWORD)(frac*g_player.GetLengthMs()));}}
    else if(g_dragSeek==Z_VOLBAR){float frac=(float)(x-R_vol.left)/(float)std::max<int>(1,R_vol.right-R_vol.left);SetVolumePercent((int)(std::max(0.f,std::min(1.f,frac))*100));}
    else if(g_dragSeek==Z_RX_SIDEDRAG){ float esc=g_cfg.uiScale/100.0f*g_dpiMul; int v=(int)std::lround(x/std::max(0.1f,esc)); g_cfg.rxSideW=std::max(170,std::min(460,v)); BuildLayout(); }
    else if(g_dragSeek==Z_PLAYER_RESIZE){int refX=R_playerPanel.left+SI(22);double dist=(double)x-refX;if(dist<(double)S(60))dist=(double)S(60);int sc=(int)std::lround(g_rsScale0*dist/std::max(1,g_rsArt0));g_cfg.playerScale=std::max(60,std::min(170,sc));BuildLayout();}
    else OnMouseDragSlider(x);
}
static void OnLButtonUp(){ if(g_dragSeek!=-1){g_dragSeek=-1;g_cfg.Save();} }
static void OnWheel(int d){ // d = notches (>0 = pra cima)
    if(WP().open){WebPick& wb=WP();int rows=(int)((wb.cells.size()+3)/4);int content=rows*(int)S(150)+(int)S(40);wb.scroll-=d*SI(120);wb.scroll=std::max(0,std::min(wb.scroll,std::max(0,content-(int)(S(360)))));return;}
    if(g_spadP.open){ g_spadP.scroll-=d*SI(60); g_spadP.ClampScroll(); return; }
    if(g_dcP.open){ g_dcP.scroll-=d*SI(60); g_dcP.ClampScroll(); return; }
    if(host::PU().open){ host::PU().scroll-=d*SI(60); if(host::PU().scroll<0) host::PU().scroll=0; return; }
    if(OU().open){ OU().scroll-=d*SI(116); if(OU().scroll<0) OU().scroll=0; return; }   // LayoutOnline limita o maximo
    if(g_showSettings){g_setScroll-=d*(int)S(56);int maxSc=std::max(0,(int)(g_setContentH-(R_settingsPanel.bottom-R_settingsPanel.top-76)));g_setScroll=std::max(0,std::min(g_setScroll,maxSc));return;}
    if(g_showSplash||g_cfg.displayMode==L"vertical")return;
    if(RxOn()&&g_rxLetraOn){ g_rxLetraScroll=std::max(0,g_rxLetraScroll-d*SI(90)); g_rxLetraMexeu=GetTickCount64(); BuildLayout(); return; }
    if(RxOn()&&g_rxPag!=RXP_LISTA){   // tela inicial/descobrir: rola as fileiras
        g_rxScroll-=d*SI(110);
        if(g_rxScroll<0) g_rxScroll=0;
        BuildLayout();
        return;
    }
    g_listScroll-=d*SI(g_cfg.listMode?64:120);
    g_listScroll=std::max(0,std::min(g_listScroll,MaxScroll()));
    BuildLayout();
}
static void OnChar(int c){ // so caracteres imprimiveis
    WebPick& wb=WP();
    if(wb.open&&wb.editing){ if(c>=32&&c!=127&&(int)wb.query.size()<120)wb.query.push_back((wchar_t)c); return; }
    if(OU().open){ if(OU().editing&&c>=32&&c!=127&&(int)OU().query.size()<300) OU().query.push_back((wchar_t)c); return; }
    if(g_editArtist){ if(g_editMode==7&&(c<'0'||c>'9')) return; if(g_editMode==6&&(c<'0'||c>'9')) return; if(c>=32&&c!=127&&(int)g_editBuf.size()<(g_editMode==1?120:(g_editMode>=4?800:64))) g_editBuf.push_back((wchar_t)c); return; }
    if(g_searchFocus){ if(c>=32&&c!=127&&(int)g_searchBuf.size()<60){ g_searchBuf.push_back((wchar_t)c); if(RxOn()&&g_view!=0) EnterLibraryView(); BuildLayout(); } return; }   // a busca do topo vale para a biblioteca inteira
}
// Tecla pressionada com a janela em foco. kc = KeyCode (app_keys.h), mods = KM_*.
// Enter/Esc/Backspace dos editores e da busca sao fixos; o resto passa pelos atalhos configuraveis.
static void OnKeyEvent(int kc,int mods){
    if(g_fxp.open&&kc==KC_ESC){ g_fxp.open=false; return; }
    if(g_spadP.open&&!g_editArtist){ if(kc==KC_ESC){ g_spadP.open=false; return; } if(kc>='1'&&kc<='9'&&mods==0){ SpadToggleSound((size_t)(kc-'1')); return; } }
    if(g_dcP.open&&!g_editArtist&&kc==KC_ESC){ g_dcP.open=false; return; }
    if(host::PU().open&&kc==KC_ESC&&!g_editArtist&&!g_confirmOpen){ host::PU().open=false; return; }
    WebPick& wb=WP();
    if(g_hkCapture>=0){   // capturando um atalho nas configuracoes
        int a=g_hkCapture;
        if(kc==KC_ESC){ g_hkCapture=-1; SetStatus(L"Captura cancelada.",1500); return; }
        if(kc==KC_BACKSPACE){ g_cfg.hk[a].key=0; g_cfg.hk[a].mods=0; SetStatus(std::wstring(HkLabel(a))+L": sem atalho.",2000); }
        else {
            for(int i=0;i<HK_COUNT;i++) if(i!=a&&g_cfg.hk[i].key==kc&&g_cfg.hk[i].mods==mods){ g_cfg.hk[i].key=0; g_cfg.hk[i].mods=0; SetStatus(L"Esse atalho estava em \""+std::wstring(HkLabel(i))+L"\" e foi movido.",3000); }
            g_cfg.hk[a].key=kc; g_cfg.hk[a].mods=mods;
        }
        g_hkCapture=-1; g_cfg.Save(); PlatformUpdateGlobalHotkeys(); return;
    }
    if(g_confirmOpen){ if(kc==KC_ENTER) ConfirmYes(); else if(kc==KC_ESC){ if(g_confirmKind==2) HostConfirm(false); g_confirmOpen=false; g_confirmKind=0; } return; }
    if(g_ctxOpen||g_folderMenuOpen){ if(kc==KC_ESC){g_ctxOpen=false;g_folderMenuOpen=false;} return; }
    if(wb.open){
        if(kc==KC_ESC){wb.open=false;wb.editing=false;}
        else if(kc==KC_ENTER){wb.editing=false;WebSearchAsync(wb.query);}
        else if(kc==KC_BACKSPACE&&wb.editing){if(!wb.query.empty())wb.query.pop_back();}
        return;
    }
    auto clip=[](size_t lim){ std::wstring c=Config::Trim(PlatformClipboardText()); for(auto& ch:c) if(ch<32) ch=L' '; if(c.size()>lim) c.resize(lim); return c; };
    if(OU().open){
        OnlineUI& u=OU();
        if(kc==KC_ESC){ u.open=false; u.editing=false; }
        else if(kc==KC_ENTER||kc==KC_KPENTER) OnlineSearchAsync();
        else if(kc=='V'&&(mods&KM_CTRL)){ u.query+=clip(300); if(u.query.size()>300) u.query.resize(300); u.editing=true; }
        else if(kc==KC_BACKSPACE&&u.editing){ if(!u.query.empty()) u.query.pop_back(); }
        return;
    }
    if(g_editArtist){
        size_t lim=g_editMode==1?120:(g_editMode>=4?800:64);
        if(kc==KC_ENTER||kc==KC_KPENTER) CommitArtistEdit();
        else if(kc==KC_ESC) CancelArtistEdit();
        else if(kc==KC_BACKSPACE){ if(!g_editBuf.empty()) g_editBuf.pop_back(); }
        else if(kc=='V'&&(mods&KM_CTRL)){ g_editBuf+=clip(lim); if(g_editBuf.size()>lim) g_editBuf.resize(lim); }
        return;
    }
    if(g_searchFocus){
        if(kc==KC_ESC){ ClearSearch(); return; }
        if(kc==KC_ENTER){
            // sem nada na biblioteca (ou um link colado): a busca continua online
            if(RxOn()&&!g_searchBuf.empty()&&(g_visible.empty()||IsUrlText(Config::Trim(g_searchBuf)))){
                g_searchFocus=false; OU().tipo=0; OpenOnlineSearch(-1,g_searchBuf); return;
            }
            g_searchFocus=false; if(!g_visible.empty()){ if(g_pickMode) TogglePick(g_visible[0]); else PlayIndex(g_visible[0],true); } return;
        }
        if(kc=='V'&&(mods&KM_CTRL)){ std::wstring c=clip(600); if(IsUrlText(c)){ ClearSearch(); OpenOnlineSearch(g_view==2?g_openPl:-1,c); } else { g_searchBuf+=c; if(g_searchBuf.size()>60) g_searchBuf.resize(60); BuildLayout(); } return; }
        if(kc==KC_BACKSPACE){ if(!g_searchBuf.empty()){ g_searchBuf.pop_back(); BuildLayout(); } return; }
        if(!(mods&(KM_CTRL|KM_ALT))) return;   // digitando: letras vao para a busca (OnChar), nao para os atalhos
    }
    if(g_showSplash){g_showSplash=false;return;}
    if(g_pickMode&&(kc==KC_ENTER||kc==KC_KPENTER)&&mods==0&&!g_showSettings){ FinishPickMode(true); return; }
    if(kc=='V'&&mods==KM_CTRL&&!g_showSettings){ std::wstring c=clip(600); if(IsUrlText(c)){ OpenOnlineSearch(g_view==2?g_openPl:-1,c); return; } }   // Ctrl+V com link: abre na busca online
    int a=FindHotkey(kc,mods);
    if(a>=0){ RunHotkeyAction(a); return; }
    if(kc==KC_ESC&&mods==0) RunHotkeyAction(HK_CLOSE);   // Esc sempre fecha paineis, mesmo sem atalho definido
}

// Acoes agendadas (testes sem clique): --after 1500:vertical --after 3000:shot:/tmp/a.png
struct TimedAction { int ms; std::string action; bool done=false; };
static std::vector<TimedAction> g_timed;
static void RunAction(const std::string& a){
    fprintf(stderr,"[remix] acao: %s\n",a.c_str());
    if(a=="vertical"){g_cfg.displayMode=L"vertical";PlatformResizeForMode();}
    else if(a=="square"){g_cfg.displayMode=L"normal";g_cfg.artShape=L"square";PlatformResizeForMode();}
    else if(a=="cd"){g_cfg.displayMode=L"normal";g_cfg.artShape=L"cd";PlatformResizeForMode();}
    else if(a=="list"){g_cfg.listMode=1;BuildLayout();}
    else if(a=="grid"){g_cfg.listMode=0;BuildLayout();}
    else if(a=="settings"){g_showSettings=!g_showSettings;}
    else if(a.rfind("style:",0)==0){g_cfg.uiStyle=std::max(0,std::min(2,atoi(a.c_str()+6)));BuildLayout();}
    else if(a.rfind("hostpin:",0)==0){g_cfg.hostPin=Utf8ToWide(a.substr(8));}
    else if(a.rfind("hostport:",0)==0){g_cfg.hostPort=atoi(a.c_str()+9);}
    else if(a=="host:on"){g_cfg.hostTunnel=false;HostStartFromCfg();}
    else if(a=="host:off"){HostStopNow();}
    else if(a=="hostok"){if(g_confirmOpen&&g_confirmKind==2)ConfirmYes();}
    else if(a=="hostno"){if(g_confirmOpen&&g_confirmKind==2){HostConfirm(false);g_confirmOpen=false;g_confirmKind=0;}}
    else if(a=="hostpanel"){host::PU().open=true;host::PU().v=host::GetView();}
    else if(a=="fxpanel"){g_fxp.open=true;}
    else if(a=="spad"){g_spadP.open=true;}
    else if(a=="spad:on"||a=="spad:off"){ spad::SetOnAsync(a=="spad:on",[]{ AppPost(EV_REDRAW); }); }
    else if(a.rfind("spadadd:",0)==0){ std::wstring v=Utf8ToWide(a.substr(8)); for(auto& ch:v) if(ch==L'|') ch=L'\n'; OnPickedSoundpad(v); }
    else if(a.rfind("spadplay:",0)==0){ BuildSpadPanel(g_winW,g_winH); SpadToggleSound((size_t)atoi(a.c_str()+9)); }
    else if(a=="spaddump"){ spad::View v=spad::GetView(); fprintf(stderr,"[remix] soundpad: ligado=%d rodando=%d sons=%d tocando=%d saida=%s voz=%s estado=%s\n",(int)v.on,(int)v.running,(int)v.sounds.size(),(int)v.playing.size(),WideToUtf8(v.device).c_str(),WideToUtf8(v.inDevice).c_str(),WideToUtf8(v.status).c_str()); }
    else if(a=="dc"){g_dcP.open=true; dc::ProbeTools(true);}
    else if(a.rfind("dctoken:",0)==0){ std::wstring e; if(!dc::SetToken(Utf8ToWide(a.substr(8)),e)) fprintf(stderr,"[remix] token recusado: %s\n",WideToUtf8(e).c_str()); }
    else if(a=="dc:on"||a=="dc:off"){ dc::SetOn(a=="dc:on"); }
    else if(a.rfind("dcpl:",0)==0){ DcToggleCfgPlaylist(atoi(a.c_str()+5)); }
    else if(a.rfind("dcplay:",0)==0){ DiscordPlayTrack(atoi(a.c_str()+7)); }
    else if(a.rfind("dcplpl:",0)==0){ DiscordPlayPlaylist(atoi(a.c_str()+7),false); }
    else if(a.rfind("dccfg:",0)==0){ std::string r=a.substr(6); dc::UpdateCfg([&](dc::Cfg& c){ if(r=="fila0") c.publicQueue=false; else if(r=="fila1") c.publicQueue=true; else if(r=="pls0") c.publicPlaylists=false; else if(r=="pls1") c.publicPlaylists=true; else if(r.rfind("vot",0)==0) c.votePct=atoi(r.c_str()+3); else if(r.rfind("lim",0)==0) c.maxPerUser=atoi(r.c_str()+3); else if(r=="on0") c.online=false; else if(r=="on1") c.online=true; }); }
    else if(a=="dcdump"){ dc::View v=dc::GetView(); fprintf(stderr,"[remix] discord: fase=%d ligado=%d token=%d node=%d bot=%d estado=%s servidores=%d tocando=%d\n",v.phase,(int)v.on,(int)v.hasToken,(int)v.nodeOk,(int)v.depsOk,WideToUtf8(v.status).c_str(),v.guildCount,(int)v.guilds.size()); for(auto& g:v.guilds) fprintf(stderr,"[remix]   %s: %s pos=%d/%d fila=%d ouvindo=%d pausado=%d\n",WideToUtf8(g.name).c_str(),WideToUtf8(g.title).c_str(),g.pos,g.dur,g.queue,g.humans,(int)g.paused); for(auto& l:v.logs) fprintf(stderr,"[remix]   log: %s\n",WideToUtf8(l).c_str()); }
    else if(a=="wavedump"){ float pu; int fr,hop; { std::lock_guard<std::mutex> lk(WS().fm); pu=WS().pulse; fr=(int)(WS().spec.size()/48); hop=WS().specHopMs; } fprintf(stderr,"[remix] onda: pos=%lu pulso=%.2f quadros=%d hop=%d latencia=%u\n",(unsigned long)(g_player.loaded?g_player.GetPositionMs():0),pu,fr,hop,Player::OutputLatencyMs()); }
    else if(a.rfind("fx:",0)==0){ CycleFx(atoi(a.c_str()+3)); }
    else if(a=="fxclear"){ ClearFx(); }
    else if(a.rfind("stem:",0)==0){ SetStemMode(atoi(a.c_str()+5)); }
    else if(a=="fxdump"){ auto j=stems::Find(StemKeyCurrent()); fprintf(stderr,"[remix] fx: slow=%d speed=%d reverb=%d grave=%d 8d=%d stem=%s instalado=%d chave=%s estado=%d pct=%d fonte=%s\n",g_cfg.fxSlow,g_cfg.fxSpeed,g_cfg.fxReverb,g_cfg.fxBass,g_cfg.fx8d,WideToUtf8(g_cfg.stemMode).c_str(),(int)stems::Installed(),WideToUtf8(StemKeyCurrent()).c_str(),j?j->state.load():-1,j?j->pct.load():-1,WideToUtf8(g_currentSource).c_str()); }
    else if(a=="hosthtml"){host::WriteConnectHtml();}
    else if(a=="tunnel:on"){host::TunnelStart(g_cfg.hostPort);}
    else if(a.rfind("hostlib:",0)==0){ int i=atoi(a.c_str()+8); host::View v=host::GetView(); if(i>=0&&i<(int)v.devs.size()) host::SetDeviceLib(v.devs[(size_t)i].id,a.back()!='0'); }   // hostlib:<aparelho>:<0|1>
    else if(a.rfind("ontab:",0)==0){ OU().tipo=std::max(0,std::min(2,atoi(a.c_str()+6))); { std::lock_guard<std::mutex> lk(OU().m); OU().res.clear(); OU().listas.clear(); } }
    else if(a=="onaddall"){ OnlineAddAll(400,300); }
    else if(a=="ondump"){
        std::lock_guard<std::mutex> lk(OU().m);
        fprintf(stderr,"[remix] listas: %d\n",(int)OU().listas.size());
        int dz=0,yt=0;
        for(auto& it:OU().listas){ if(it.link.find(L"youtube")!=std::wstring::npos) yt++; else dz++; }
        fprintf(stderr,"[remix]   deezer=%d youtube=%d\n",dz,yt);
        for(size_t i=0;i<OU().listas.size()&&i<3;i++) fprintf(stderr,"[remix]   %s | %s\n",WideToUtf8(OU().listas[i].titulo).c_str(),WideToUtf8(OU().listas[i].link).c_str());
        for(size_t i=OU().listas.size()>3?OU().listas.size()-2:3;i<OU().listas.size();i++) fprintf(stderr,"[remix]   %s | %s\n",WideToUtf8(OU().listas[i].titulo).c_str(),WideToUtf8(OU().listas[i].link).c_str());
    }
    else if(a.rfind("onabrir:",0)==0){   // abre o N-esimo resultado de playlist/album
        size_t i=(size_t)atoi(a.c_str()+8); std::wstring link;
        { std::lock_guard<std::mutex> lk(OU().m); if(i<OU().listas.size()) link=OU().listas[i].link; }
        if(!link.empty()){ OU().tipo=0; OU().query=link; OnlineSearchAsync(); }
    }
    else if(a.rfind("rxside:",0)==0){ int i=atoi(a.c_str()+7); OnLButtonDown((int)((R_rxSidePl[(size_t)std::min<size_t>((size_t)std::max(0,i),R_rxSidePl.size()-1)].left+R_rxSidePl[(size_t)std::min<size_t>((size_t)std::max(0,i),R_rxSidePl.size()-1)].right)/2),(int)((R_rxSidePl[(size_t)std::min<size_t>((size_t)std::max(0,i),R_rxSidePl.size()-1)].top+R_rxSidePl[(size_t)std::min<size_t>((size_t)std::max(0,i),R_rxSidePl.size()-1)].bottom)/2)); }
    else if(a=="plaberta"){ const Playlist* p=OpenPlaylistPtr(); fprintf(stderr,"[remix] aberta: %s (%d itens)\n",p?WideToUtf8(p->name).c_str():"(nenhuma)",p?(int)p->entries.size():0); }
    else if(a=="pldump"){
        fprintf(stderr,"[remix] playlists (%d):\n",(int)g_playlists.size());
        for(auto& p:g_playlists) fprintf(stderr,"[remix]   \"%s\" slug=%s itens=%d link=%s\n",WideToUtf8(p.name).c_str(),WideToUtf8(p.slug).c_str(),(int)p.entries.size(),WideToUtf8(p.link).c_str());
    }
    else if(a=="onabrir"){ OU().open=true; OU().editing=true; BuildLayout(); }   // so abre a busca online, sem procurar nada
    else if(a.rfind("onbusca:",0)==0){ OU().open=true; OU().query=Utf8ToWide(a.substr(8)); OnlineSearchAsync(); }
    else if(a=="letra"){ g_rxLetraOn=!g_rxLetraOn; g_rxLetraScroll=0; BuildLayout(); }
    else if(a=="rxpainel"){ g_rxPainelOn=!g_rxPainelOn; BuildLayout(); }
    else if(a=="fonte"){   // teste da fronteira: estado + a trava do Rodar/Abrir
        fonte::Garantir();
        fonte::Estado e=fonte::Snapshot();
        fprintf(stderr,"[remix] fonte: configurada=%d caminho=%s versao=%s ffmpeg=%s js=%s\n",
                (int)fonte::Configurada(),WideToUtf8(e.cli).c_str(),WideToUtf8(e.vCli).c_str(),
                WideToUtf8(e.ffmpeg).c_str(),WideToUtf8(e.jsName).c_str());
        CapResult r1=fonte::Rodar({L"/bin/echo",L"nao deveria rodar"},5000);
        CapResult r2=fonte::Rodar({},5000);
        auto cmd=fonte::Cmd(); cmd.push_back(L"--version");
        CapResult r3=fonte::Rodar(cmd,10000);
        fprintf(stderr,"[remix] trava: outro programa started=%d | vazio started=%d | caminho certo started=%d saida=%s\n",
                (int)r1.started,(int)r2.started,(int)r3.started,r3.out.substr(0,40).c_str());
    }
    else if(a=="saidas"){ auto v=Player::OutDevices(); fprintf(stderr,"[remix] saidas (%d): ",(int)v.size()); for(auto& s:v) fprintf(stderr,"%s | ",WideToUtf8(s).c_str()); fprintf(stderr,"| atual=%s\n",WideToUtf8(Player::OutDeviceAtual()).c_str()); }
    else if(a.rfind("rxpag:",0)==0){ int n=atoi(a.c_str()+6); if(n==0){ g_rxPag=RXP_INICIO; BuildLayout(); } else if(n==2){ g_rxPag=RXP_DESCOBRIR; g_rxScroll=0; BuildLayout(); } else { g_rxPag=RXP_LISTA; EnterLibraryView(); } }
    else if(a.rfind("rxgen:",0)==0){ int n=atoi(a.c_str()+6); auto gs=desc::ListaGeneros(); if(n>=0&&n<(int)gs.size()){ g_rxGenero=gs[(size_t)n].id; g_rxGeneroNome=gs[(size_t)n].titulo; g_rxScroll=0; desc::AtualizarGeneros(g_rxGenero,g_rxGeneroNome,RxAvisarNovidades); BuildLayout(); } }
    else if(a.rfind("rxpl:",0)==0){ int n=atoi(a.c_str()+5); g_rxPag=RXP_LISTA; if(n>=0&&n<(int)g_playlists.size()) OpenPlaylistView(n); }
    else if(a=="descobrir"||a=="descobrir:forcar"){   // teste: monta as fileiras da tela inicial e imprime
        desc::Atualizar(a!="descobrir",RxAvisarNovidades);
        std::thread([]{
            for(int i=0;i<120&&desc::Carregando();i++) Sleep(500);
            desc::Home h=desc::Copia();
            fprintf(stderr,"[remix] descobrir: %d fileiras (at=%lld) %s\n",(int)h.fileiras.size(),(long long)h.at,WideToUtf8(h.err).c_str());
            for(auto& s:h.fileiras){
                fprintf(stderr,"[remix]   %-34s tipo=%d itens=%d\n",WideToUtf8(s.titulo).c_str(),(int)s.kind,(int)s.itens.size());
                for(size_t i=0;i<s.itens.size()&&i<2;i++) fprintf(stderr,"[remix]       %s - %s | %s\n",WideToUtf8(s.itens[i].titulo).c_str(),WideToUtf8(s.itens[i].sub).c_str(),WideToUtf8(s.itens[i].link).c_str());
            }
        }).detach();
    }
    else if(a.rfind("gosto:",0)==0){ desc::Registrar(Utf8ToWide(a.substr(6)),L"teste"); desc::SalvarPerfil(); }
    else if(a=="gostos"){ auto v=desc::TopArtistas(10); fprintf(stderr,"[remix] top artistas: "); for(auto& x:v) fprintf(stderr,"%s; ",WideToUtf8(x).c_str()); fprintf(stderr,"\n"); }
    else if(a.rfind("hostdevlink:",0)==0){ host::PU().v=host::GetView(); HostCopyDeviceLink((size_t)atoi(a.c_str()+12)); bool t=false; host::View v=host::GetView(); int i=atoi(a.c_str()+12); fprintf(stderr,"[remix] link do aparelho: %s\n",(i>=0&&i<(int)v.devs.size())?host::DeviceLinkUrl(v.devs[(size_t)i].id,host::PU().qrTunnel,t).c_str():"(sem aparelho)"); }
    else if(a=="hostqr"){ bool t=false; std::string u=host::QrUrl(false,t); fprintf(stderr,"[remix] qr: %s\n",u.c_str()); }
    else if(a.rfind("hostdplok:",0)==0){ int i=atoi(a.c_str()+10); host::View v=host::GetView(); if(i>=0&&i<(int)v.dpls.size()) host::SetDevPlaylistOk(v.dpls[(size_t)i].dev,v.dpls[(size_t)i].slug,true); }
    else if(a.rfind("hostplall:",0)==0){ int i=atoi(a.c_str()+10); if(i>=0&&i<(int)g_playlists.size()){ host::SetTargets(g_playlists[(size_t)i].slug,"ALL"); HostPublishNow(); } }
    else if(a=="hostonline:0"||a=="hostonline:1"){ HostSetOnline(a.back()=='1'); }
    else if(a=="tunnel:off"){host::TunnelStop();}
    else if(a=="next")NextTrack();
    else if(a=="play")TogglePlayPause();
    else if(a=="autoplay"){g_cfg.autoplay=!g_cfg.autoplay;}
    else if(a=="sort"){CycleSortMode();BuildLayout();}
    else if(a=="movedown"){MoveTrack(g_current,+1);BuildLayout();}
    else if(a=="eq"){g_cfg.eqOn=true;g_cfg.eq[0]=8;g_cfg.eq[1]=5;g_cfg.eq[6]=-6;g_cfg.eq[7]=4;ApplyEqNow();}
    else if(a=="mute")ToggleMute();
    else if(a=="volup")SetVolumePercent(g_cfg.volume+5);
    else if(a=="ctx"){if(g_current>=0)OpenCtxMenu(g_current,R_art.left+40,R_art.top+40);}
    else if(a=="foldermenu")OpenFolderMenu();
    else if(a=="confirm"){AskDeleteTrack(g_current);OpenConfirm();}
    else if(a=="rename"){StartFileRename(g_current);}
    else if(a=="perf"){g_cfg.perfMode=!g_cfg.perfMode;}
    else if(a=="close")RequestClose();
    else if(a=="show")ShowFromBackground();
    else if(a=="quit")QuitApp();
    else if(a=="minimize")PlatformMinimize();
    else if(a.rfind("size:",0)==0){ int w=0,h=0; if(sscanf(a.c_str()+5,"%dx%d",&w,&h)==2&&w>0&&h>0) PlatformSetWindowSize(w,h); }
    else if(a=="scrollend"){g_setScroll=100000;g_listScroll=100000;BuildLayout();LayoutSettings(g_winW,g_winH);}
    else if(a.rfind("scroll:",0)==0){g_setScroll=atoi(a.c_str()+7);g_listScroll=g_setScroll;BuildLayout();LayoutSettings(g_winW,g_winH);}   // testes: rola para uma posicao
    else if(a.rfind("shot:",0)==0){PlatformScreenshot(Utf8ToWide(a.substr(5)));}
    else if(a.rfind("track:",0)==0){int i=atoi(a.c_str()+6);PlayIndex(i,true);}
    else if(a.rfind("key:",0)==0){ Hotkey h; ParseHotkey(Utf8ToWide(a.substr(4)),h); if(h.key) OnKeyEvent(h.key,h.mods); }
    else if(a.rfind("search:",0)==0){ FocusSearch(); if(RxOn()&&g_view!=0) EnterLibraryView(); g_searchBuf=Utf8ToWide(a.substr(7)); BuildLayout(); }
    else if(a=="tab:playlists"){ ShowPlaylistCards(); }
    else if(a=="tab:tracks"){ EnterLibraryView(); }
    else if(a=="back"){ RunHotkeyAction(HK_CLOSE); }
    else if(a.rfind("plnew:",0)==0){ int pi=CreatePlaylist(Utf8ToWide(a.substr(6))); fprintf(stderr,"[remix] playlist criada: %d\n",pi); BuildLayout(); }
    else if(a.rfind("pladd:",0)==0){ AddTrackToPlaylist(atoi(a.c_str()+6),g_current); }
    else if(a.rfind("plopen:",0)==0){ OpenPlaylistView(atoi(a.c_str()+7)); }
    else if(a.rfind("plplay:",0)==0){ PlayPlaylist(atoi(a.c_str()+7),false); }
    else if(a.rfind("plshuf:",0)==0){ PlayPlaylist(atoi(a.c_str()+7),true); }
    else if(a.rfind("hkset:",0)==0){ std::string r=a.substr(6); size_t eq=r.find('='); if(eq!=std::string::npos){ int act=HkIndexById(Utf8ToWide(r.substr(0,eq))); if(act>=0){ ParseHotkey(Utf8ToWide(r.substr(eq+1)),g_cfg.hk[act]); g_cfg.Save(); PlatformUpdateGlobalHotkeys(); } } }
    else if(a.rfind("hkcap:",0)==0){ g_hkCapture=HkIndexById(Utf8ToWide(a.substr(6))); }
    else if(a.rfind("click:",0)==0){ int x=0,y=0; if(sscanf(a.c_str()+6,"%d,%d",&x,&y)==2){ OnLButtonDown(x,y); OnLButtonUp(); } }
    else if(a=="plctx"){ if(!g_playlists.empty()&&R_plCards.size()>1) OpenPlaylistCtxMenu(0,R_plCards[1].left+20,R_plCards[1].top+20); }
    else if(a.rfind("type:",0)==0){for(wchar_t c:Utf8ToWide(a.substr(5)))OnChar((int)c);}
    else if(a.rfind("pickstart:",0)==0){ StartPickMode(atoi(a.c_str()+10)); }
    else if(a.rfind("picktoggle:",0)==0){ TogglePick(atoi(a.c_str()+11)); }
    else if(a=="pickdone"){ FinishPickMode(true); }
    else if(a.rfind("pllink:",0)==0){ std::string r=a.substr(7); size_t c=r.find(':'); if(c!=std::string::npos) LinkPlaylistFolder(atoi(r.substr(0,c).c_str()),Utf8ToWide(r.substr(c+1))); }
    else if(a.rfind("pladdfiles:",0)==0){ std::string r=a.substr(11); size_t c=r.find(':'); if(c!=std::string::npos){ SetPendingPl(atoi(r.substr(0,c).c_str())); std::wstring v=Utf8ToWide(r.substr(c+1)); for(auto& ch:v) if(ch==L'|') ch=L'\n'; OnPickedFiles(v,false); } }
    else if(a.rfind("online:",0)==0){ OpenOnlineSearch(g_view==2?g_openPl:-1,Utf8ToWide(a.substr(7))); }
    else if(a.rfind("onsrc:",0)==0){ OU().source=atoi(a.c_str()+6); g_cfg.onlineSource=OU().source; }
    else if(a.rfind("onplay:",0)==0){ OnlinePlayResult(atoi(a.c_str()+7)); }
    else if(a.rfind("ondl:",0)==0){ OnlineDownloadResult(atoi(a.c_str()+5)); }
    else if(a.rfind("onadd:",0)==0){ OnlineAddResult(atoi(a.c_str()+6),300,200); }
    else if(a=="onaddall"){ OnlineAddAll(300,200); }
    else if(a=="onclose"){ OU().open=false; }
    else if(a.rfind("onmode:",0)==0){ g_cfg.onlineMode=Utf8ToWide(a.substr(7)); }
    else if(a=="addmenu"){ OpenAddMenu(g_openPl,R_plAdd.left,R_plAdd.bottom+4); }
    else if(a=="newmenu"){ OpenNewPlaylistMenu(300,200); }
    else if(a=="plfolder"){ OpenPlaylistFolderMenu(g_openPl); }
    else if(a.rfind("linkadd:",0)==0){ ResolveLinkAsync(Utf8ToWide(a.substr(8)),(g_view==2&&g_openPl>=0)?g_playlists[(size_t)g_openPl].slug:L""); }
    else if(a.rfind("seek:",0)==0){ if(g_player.loaded) g_player.SeekMs((DWORD)atoi(a.c_str()+5)); }
    else if(a.rfind("crash:",0)==0){   // testes do registro de erros: --after N:crash:av|throw|thread|param
        std::string k=a.substr(6);
        if(k=="av"){ volatile int* p=(int*)(uintptr_t)16; *p=1; }
        else if(k=="throw") throw std::runtime_error("teste de excecao na janela");
        else if(k=="thread") std::thread([]{ throw std::runtime_error("teste de excecao numa thread"); }).detach();
#ifdef _WIN32
        else if(k=="param"){ FILE* f=_wfopen(nullptr,L"rb"); if(f) fclose(f); PlatformLog("teste: o parametro invalido foi ignorado e o app continuou"); }
#endif
    }
    else if(a=="dump"){
        fprintf(stderr,"[remix] view=%d openPl=%d pick=%d tracks=%d current=%d playing=%d stream=%d pos=%lu len=%lu playlists=%d online=%d\n",g_view,g_openPl,(int)g_pickMode,(int)g_tracks.size(),g_current,(int)g_player.playing,(int)g_player.IsStream(),
            (unsigned long)(g_player.loaded?g_player.GetPositionMs():0),(unsigned long)(g_player.loaded?g_player.GetLengthMs():0),(int)g_playlists.size(),(int)OU().res.size());
        for(size_t i=0;i<g_tracks.size()&&i<12;++i) fprintf(stderr,"[remix]   %zu %s | %s | capa=%s\n",i,WideToUtf8(g_tracks[i].title).c_str(),WideToUtf8(g_tracks[i].path).c_str(),WideToUtf8(g_tracks[i].coverPath).c_str());
        { std::lock_guard<std::mutex> lk(OU().m); for(size_t i=0;i<OU().res.size()&&i<5;++i) fprintf(stderr,"[remix]   online %zu %s - %s (%d s) %s\n",i,WideToUtf8(OU().res[i].artist).c_str(),WideToUtf8(OU().res[i].title).c_str(),OU().res[i].dur,WideToUtf8(OU().res[i].url).c_str()); if(!OU().status.empty()) fprintf(stderr,"[remix]   status online: %s\n",WideToUtf8(OU().status).c_str()); }
        for(auto& c:StreamSnapshot()) fprintf(stderr,"[remix]   canal %d %s fase=%d recebido=%llus/%llus %s\n",c.id,c.id==g_curStreamId?"TOCANDO":"fila",c.phase,(unsigned long long)(c.end/c.rate),(unsigned long long)(c.len/c.rate),WideToUtf8(c.title.empty()?c.url:c.title).c_str());
        if(StatusVisible()) fprintf(stderr,"[remix]   aviso: %s\n",WideToUtf8(g_status).c_str());
        if(host::Running()){ host::View hv=host::GetView(); fprintf(stderr,"[remix]   host: porta=%d aparelhos=%d pedidos=%d tunel=[%s] pendente=[%s] estado=[%s] streams=%d\n",hv.port,(int)hv.devs.size(),(int)hv.pending.size(),hv.tunUrl.c_str(),hv.tunPending.c_str(),hv.tunStatus.c_str(),hv.streams); }
    }
}
static void RunTimedActions(unsigned long long elapsedMs){
    for(auto& ta:g_timed) if(!ta.done&&elapsedMs>=(unsigned long long)ta.ms){ ta.done=true; RunAction(ta.action); }
}
