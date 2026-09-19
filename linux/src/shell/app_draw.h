#pragma once
// Desenho do player (modo normal e vertical). Porte 1:1 de DrawNormal /
// DrawVertical do main.cpp do Windows, trocando GDI+ pelas primitivas do gfx.h.
#include "app_state.h"
#include "app_layout.h"

// Relogio de animacao relativo ao inicio do app (float nao perde precisao).
static ULONGLONG g_t0 = GetTickCount64();
static DWORD NowMs(){ return (DWORD)(GetTickCount64()-g_t0); }

static void DrawSettings(int w,int h);

static void DrawCoverCircle(Img* img,const RectF& r,Color pen,float penW,float rotation){
    float cx=r.X+r.Width/2.f,cy=r.Y+r.Height/2.f;
    gfx::FillEllipse(r,Cs(Argb(255,18,20,30),UI().surfaceHi));
    if(img&&img->ok) gfx::DrawImgCircle(img,r,rotation);
    float hr=r.Width*.11f; gfx::FillEllipse(cx-hr,cy-hr,hr*2,hr*2,Cs(Argb(255,7,9,18),UI().bg));
    gfx::StrokeEllipse(r,penW,pen);
}

// alternador rapido de aparencia da capa (QUAD / CD)
static void DrawShapeToggle(Color ab,Color white,Color gray){
    RectF st=RF(R_shapeTgl);
    Color sbg=Cs(Argb(255,13,16,30),UI().surface), spn=Cs(Argb(255,60,64,88),UI().border);
    if(UiClassic()) DrawRoundRect(st,S(10),&sbg,&spn,1.2f); else DrawRoundRect(st,S(UI_R_PILL),&sbg,nullptr);
    bool cd=g_cfg.artShape==L"cd";
    RectF hl(cd?st.X+st.Width/2.f:st.X,st.Y,st.Width/2.f,st.Height);
    Color hbg=Cs(ToGdi(g_theme.accent,50),UI().surfaceHi);
    DrawRoundRect(hl,UiClassic()?S(9):S(UI_R_PILL),&hbg,nullptr);
    gfx::TextRect(L"QUAD",RectF(st.X,st.Y,st.Width/2.f,st.Height),S(14),cd?gray:white,true,gfx::Center,true);
    gfx::TextRect(L"CD",RectF(st.X+st.Width/2.f,st.Y,st.Width/2.f,st.Height),S(14),!cd?gray:(UiClassic()?ab:white),true,gfx::Center,true);
}

// Botao "pilula" do cabecalho (AUTO, ORDEM): aceso quando 'on'.
static void DrawPill(const RECT& r,const std::wstring& t,bool on,float px){
    if(r.right-r.left<=0) return;
    RectF b=RF(r);
    if(UiClassic()){
        Color bg=on?ToGdi(g_theme.accent,55):Argb(255,13,16,30), pn=on?ToGdi(g_theme.accent):Argb(255,60,64,88);
        DrawRoundRect(b,S(10),&bg,&pn,1.2f);
        gfx::TextRect(t,b,px,on?ToGdi(g_theme.accent):C_GRAY,true,gfx::Center,true,gfx::EllipsisChar);
    } else {   // estilos novos: chapa lisa, sem contorno; ligado = tom mais claro e texto branco
        Color bg=ToGdi(on?UI().surfaceHi:UI().surface);
        DrawRoundRect(b,S(UI_R_PILL),&bg,nullptr);
        gfx::TextRect(t,b,px,ToGdi(on?UI().text:UI().textDim),true,gfx::Center,true,gfx::EllipsisChar);
    }
}
static std::wstring SortLabel(){ return L"ORDEM: "+SortModeName(g_cfg.sortMode)+(g_cfg.sortMode==L"manual"?L"":(g_cfg.sortDesc?L" ▼":L" ▲")); }
// Setas de ordem manual (so quando a lista esta em ordem MANUAL).
static void DrawOrderArrows(size_t i,Color ab,Color gray){
    if(g_cfg.sortMode!=L"manual"||i>=R_rowUp.size()) return;
    if(R_rowUp[i].right>R_rowUp[i].left) gfx::TextRect(L"▲",RF(R_rowUp[i]),S(11),i>0?ab:gray,false,gfx::Center,true);
    if(R_rowDown[i].right>R_rowDown[i].left) gfx::TextRect(L"▼",RF(R_rowDown[i]),S(11),i+1<g_tracks.size()?ab:gray,false,gfx::Center,true);
}
// Fechar / minimizar (so quando a janela nao tem borda do sistema: Windows).
static void DrawChromeButtons(Color white,Color gray){
    if(R_close.right>R_close.left){
        RectF c=RF(R_close); Color bg=Argb(255,40,18,26), pn=Argb(255,120,50,64);
        DrawRoundRect(c,S(8),&bg,&pn,1.2f);
        gfx::Line(c.X+c.Width*.32f,c.Y+c.Height*.32f,c.X+c.Width*.68f,c.Y+c.Height*.68f,2.f,white);
        gfx::Line(c.X+c.Width*.68f,c.Y+c.Height*.32f,c.X+c.Width*.32f,c.Y+c.Height*.68f,2.f,white);
    }
    if(R_min.right>R_min.left){
        RectF m=RF(R_min); Color bg=Argb(255,16,19,34), pn=Argb(255,60,64,88);
        DrawRoundRect(m,S(8),&bg,&pn,1.2f);
        gfx::Line(m.X+m.Width*.30f,m.Y+m.Height*.62f,m.X+m.Width*.70f,m.Y+m.Height*.62f,2.f,gray);
    }
}
// Aviso curto na base da janela.
static void DrawStatusToast(int w,int h){
    if(!StatusVisible()) return;
    float px=S(12); float tw=gfx::TextWidth(g_status,px,true)+S(36);
    RectF b(((float)w-tw)/2.f,(float)h-S(46),tw,S(32));
    Color bg=Cs(Argb(230,12,15,30),UI().surfaceHi), pn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(b,UiClassic()?S(10):S(UI_R_CARD),&bg,&pn,1.2f);
    gfx::TextRect(g_status,b,px,C_WHITE,true,gfx::Center,true);
}
// Icone de volume + barra + porcentagem (a barra sozinha confundia iniciantes).
static void DrawVolumeBar(Color ab,Color gray,float barH){
    if(!UiClassic()) ab=ToGdi(UI().textDim);   // estilos novos: volume em cinza (a cor do tema fica para a musica)
    Color ic=g_muted?gray:ab;
    DrawVolumeIcon(R_volIcon,ic,g_muted,g_cfg.volume);
    RectF vt((float)R_vol.left,(float)R_vol.top,(float)(R_vol.right-R_vol.left),barH); Color vb=Cs(Argb(255,40,42,60),UI().border); DrawRoundRect(vt,2,&vb,nullptr);
    float fw=(float)((R_vol.right-R_vol.left)*(g_muted?0:g_cfg.volume)/100.f);
    if(fw>1){ RectF vf((float)R_vol.left,(float)R_vol.top,fw,barH); DrawRoundRect(vf,2,&ab,nullptr); }
    wchar_t pct[16]; swprintf(pct,16,L"%d%%",g_muted?0:g_cfg.volume);
    gfx::Text(pct,(float)R_vol.right+S(8),(float)R_vol.top-S(5),S(10),gray);
}
// ---- barra da biblioteca: abas / voltar, busca, nova playlist ----------------
static void DrawSearchIcon(float cx,float cy,float r,Color c){ gfx::StrokeEllipse(RectF(cx-r,cy-r,2*r,2*r),1.6f,c); gfx::Line(cx+r*0.7f,cy+r*0.7f,cx+r*1.7f,cy+r*1.7f,2,c); }
static void DrawLibBar(Color ab,Color white,Color gray){
    if(R_libBar.right<=R_libBar.left) return;
    if(g_view==2){
        DrawPill(R_plBack,L"◀ PLAYLISTS",false,S(11)); DrawPill(R_plAdd,L"+ ADICIONAR",true,S(11));
        if(const Playlist* op=OpenPlaylistPtr()){ std::wstring m=op->mode.empty()?(g_cfg.onlineMode==L"download"?L"ONLINE: SALVAR CÓPIA":L"ONLINE: STREAM"):(op->mode==L"download"?L"ONLINE: SALVAR CÓPIA ●":L"ONLINE: STREAM ●"); DrawPill(R_plMode,m,!op->mode.empty(),S(10)); }
    } else if(g_pickMode){
        DrawPill(R_pickDone,L"CONCLUIR ("+std::to_wstring(g_pickSel.size())+L")",true,S(11)); DrawPill(R_pickCancel,L"CANCELAR",false,S(11));
    } else {
        DrawPill(R_tabTracks,L"MÚSICAS",g_view==0,S(11));
        DrawPill(R_tabPlaylists,L"PLAYLISTS",g_view==1,S(11));
        DrawPill(R_tabOnline,L"ONLINE",OU().open,S(11));
    }
    if(R_plNew.right>R_plNew.left) DrawPill(R_plNew,L"+ NOVA PLAYLIST",false,S(11));
    if(R_searchBox.right>R_searchBox.left){
        RectF b=RF(R_searchBox);
        Color fb=Cs(Argb(235,10,13,26),UI().surface), ln=g_searchFocus?ToGdi(g_theme.accent):Cs(Argb(255,50,54,76),UI().border);
        if(UiClassic()) DrawRoundRect(b,S(9),&fb,&ln,g_searchFocus?1.5f:1.f); else DrawRoundRect(b,S(UI_R_CARD),&fb,g_searchFocus?&ln:nullptr,1.4f);
        DrawSearchIcon(b.X+S(16),b.Y+b.Height/2-S(1),S(5),g_searchFocus?ab:gray);
        float tx=b.X+S(32), tw=b.Width-S(32)-(g_searchBuf.empty()?S(10):S(120));
        if(g_searchBuf.empty()&&!g_searchFocus){
            std::wstring ph=(g_pickMode&&g_pickPl>=0&&g_pickPl<(int)g_playlists.size())?L"Marcando para \""+g_playlists[(size_t)g_pickPl].name+L"\"   ·   buscar...":(g_view==2&&g_openPl>=0&&g_openPl<(int)g_playlists.size())?L"Playlist: "+g_playlists[(size_t)g_openPl].name+L"   ·   clique aqui para buscar":L"Buscar faixa (nome, artista ou arquivo)...";
            gfx::TextRect(ph,RectF(tx,b.Y,tw,b.Height),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
        } else {
            std::wstring txt=g_searchBuf; if(g_searchFocus&&(NowMs()/500)%2==0) txt+=L"|";
            gfx::TextRect(txt,RectF(tx,b.Y,tw,b.Height),S(12),white,false,gfx::Near,true,gfx::EllipsisChar);
        }
        if(!g_searchBuf.empty()){
            std::wstring cnt=std::to_wstring(g_visible.size())+L" de "+std::to_wstring(g_tracks.size());
            gfx::TextRect(cnt,RectF(b.X+b.Width-S(120),b.Y,S(80),b.Height),S(10),gray,false,gfx::Far,true);
            gfx::TextRect(L"✕",RF(R_searchClear),S(12),white,false,gfx::Center,true);
        }
    }
}
// ---- cards de playlists ("Todas as musicas", cada playlist, "nova") ----------------
static void DrawPlaylistCards(Color ab,Color white,Color gray){
    if(g_view!=1) return;
    int n=(int)g_playlists.size()+2;
    const std::vector<Track>& lib=g_libCached?g_libTracks:g_tracks;
    for(int k=0;k<n&&k<(int)R_plCards.size();k++){
        RECT rr=R_plCards[(size_t)k]; if(rr.right<=rr.left) continue;
        RectF card=RF(rr); bool isAll=(k==0), isNew=(k==n-1); int pi=k-1;
        bool hot=isAll?(g_openPl<0):(!isNew&&pi==g_openPl);
        if(UiClassic()){ Color cb=Argb(255,9,12,25); Color cp=hot?ToGdi(g_theme.accent,220):Argb(255,40,44,65); DrawRoundRect(card,16,&cb,&cp,1.5f); }
        else { Color cb=ToGdi((hot||UiHot(rr))?UI().surfaceHi:UI().surface); DrawRoundRect(card,S(UI_R_CARD),&cb,nullptr); }
        if(isNew){
            gfx::TextRect(L"+",RectF(card.X,card.Y+S(26),card.Width,S(64)),S(46),ab,true,gfx::Center,true);
            gfx::TextRect(L"NOVA PLAYLIST",RectF(card.X,card.Y+S(98),card.Width,S(24)),S(12),white,true,gfx::Center,true);
            gfx::TextRect(L"vazia, de uma pasta, de um link ou da busca online",RectF(card.X+S(10),card.Y+S(124),card.Width-S(20),S(20)),S(9),gray,false,gfx::Center,true,gfx::EllipsisChar);
            continue;
        }
        int cov=SI(96); RectF art(card.X+S(16),card.Y+S(16),(float)cov,(float)cov);
        std::wstring coverPath=isAll?(lib.empty()?L"":lib[0].coverPath):g_playlists[(size_t)pi].coverPath;
        Img* im=coverPath.empty()?nullptr:GetThumb(coverPath);
        Color plate=Cs(Argb(255,18,21,34),UI().bg); DrawRoundRect(art,UiClassic()?10:S(4),&plate,nullptr);
        if(im) gfx::DrawImgCover(im,art);
        else { gfx::StrokeEllipse(RectF(art.X+cov*0.18f,art.Y+cov*0.18f,cov*0.64f,cov*0.64f),2.f,ToGdi(g_theme.accent,160)); gfx::FillEllipse(RectF(art.X+cov*0.42f,art.Y+cov*0.42f,cov*0.16f,cov*0.16f),ToGdi(g_theme.accent,160)); }
        float tx=art.X+cov+S(14), tw=card.Width-(tx-card.X)-S(12);
        std::wstring name=isAll?L"Todas as músicas":g_playlists[(size_t)pi].name;
        size_t count=isAll?lib.size():g_playlists[(size_t)pi].entries.size();
        gfx::TextRect(name,RectF(tx,card.Y+S(18),tw,S(44)),S(16),white,true,gfx::Near,false,gfx::EllipsisWord);
        gfx::TextRect(isAll?std::to_wstring(count)+(count==1?L" faixa na biblioteca":L" faixas na biblioteca"):PlaylistCardSubtitle(pi),RectF(tx,card.Y+S(66),tw,S(18)),S(11),UiClassic()?ab:gray,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(isAll?L"clique: abrir a biblioteca":L"clique: abrir  ·  botão direito: opções",RectF(tx,card.Y+S(88),tw,S(16)),S(9),gray,false,gfx::Near,false,gfx::EllipsisChar);
        DrawPill(R_plPlay[(size_t)k],L"▶ TOCAR",false,S(10));
        DrawPill(R_plShuf[(size_t)k],L"⇄ ALEATÓRIO",false,S(10));
        if((size_t)k<R_plDcBtns.size()&&R_plDcBtns[(size_t)k].right>R_plDcBtns[(size_t)k].left&&UiHot(rr)&&DcCardsOn()) DrawPill(R_plDcBtns[(size_t)k],L"▶ DISCORD",true,S(9));
    }
}
// marcando musicas para uma playlist: circulo de selecao por card/linha
static void DrawPickMark(size_t i,const RECT& rr){
    if(!g_pickMode||i>=g_tracks.size()) return;
    bool have=PickAlreadyIn(g_tracks[i]), sel=g_pickSel.count(g_tracks[i].path)>0;
    RectF card=RF(rr);
    if(sel){ Color ov=ToGdi(g_theme.accent,40), pn=ToGdi(g_theme.accent); DrawRoundRect(card,S(12),&ov,&pn,2.2f); }
    else if(have){ Color ov=Argb(130,5,7,18); DrawRoundRect(card,S(12),&ov,nullptr); }
    float d=S(26); RectF c(card.X+card.Width-d-S(10),card.Y+S(10),d,d);
    gfx::FillEllipse(c,sel?ToGdi(g_theme.accent):(have?Cs(Argb(255,60,64,88),UI().borderHi):Cs(Argb(215,12,15,30),UI().surfaceHi)));
    gfx::StrokeEllipse(c,1.6f,sel?C_WHITE:Argb(255,120,124,150));
    if(sel||have){ gfx::Line(c.X+d*0.27f,c.Y+d*0.52f,c.X+d*0.44f,c.Y+d*0.70f,2.4f,C_WHITE); gfx::Line(c.X+d*0.44f,c.Y+d*0.70f,c.X+d*0.75f,c.Y+d*0.32f,2.4f,C_WHITE); }
    if(have) gfx::TextRect(L"já está",RectF(c.X-S(72),c.Y,S(66),d),S(9),C_GRAY,false,gfx::Far,true);
}
static void DrawOnlineTag(const RectF& art,const std::wstring& url){   // faixa online: etiqueta na capa + estado do canal (tocando / na fila)
    const StreamInfo* c=FindSnap(url);
    float hh=std::max(S(11),art.Height*0.18f);
    RectF tag(art.X,art.Y+art.Height-hh,art.Width,hh);
    gfx::FillRect(tag,Argb(200,6,8,18));
    Color lc=(c&&c->phase==5)?Argb(255,235,120,110):ToGdi(g_theme.accent);
    gfx::TextRect(c?StreamTagLabel(*c,art.Width>=S(90)):std::wstring(L"ONLINE"),tag,std::max(S(7),hh*(c?0.5f:0.62f)),lc,true,gfx::Center,true,gfx::EllipsisChar);
    if(c&&c->phase!=5){ float fr=StreamBufFrac(*c); gfx::FillRect(RectF(tag.X,tag.Y+tag.Height-S(2),tag.Width,S(2)),Argb(255,40,44,65)); gfx::FillRect(RectF(tag.X,tag.Y+tag.Height-S(2),tag.Width*fr,S(2)),lc); }
}
#include "app_draw_remix.h"   // estilo REMIX: lateral, tela inicial e barra do player

static void DrawNormal(int w,int h){
    g_streamSnap=StreamSnapshot();
    DrawAppBackground(w,h,Cs(Argb(255,5,7,18),UI().bg));
    Color accent=ToGdi(g_theme.accent); Color ab=accent, white=C_WHITE, gray=C_GRAY, dimB=Cs(Argb(255,44,48,70),UI().borderHi);
    if(!UiClassic()){   // estilos novos: cabecalho e coluna do player sao faixas solidas encostadas na janela
        Color barC=ToGdi(UI().bar), divC=ToGdi(UI().border);
        if(g_sideW>0) gfx::FillRect(0,(float)g_headerH,(float)g_sideW,(float)(h-g_headerH),barC);
        gfx::FillRect(0,0,(float)w,(float)g_headerH,barC);
        gfx::Line(0,(float)g_headerH+.5f,(float)w,(float)g_headerH+.5f,1,divC);
        if(g_sideW>0) gfx::Line((float)g_sideW+.5f,(float)g_headerH,(float)g_sideW+.5f,(float)h,1,divC);
        if(RxOn()) RxDrawMainBg();   // chapa da area principal (o papel de parede continua aparecendo por baixo)
    }
    const float fBrand=S(17), fTitleBase=TextScale(20,g_cfg.titleScale), fArtistBase=TextScale(13,g_cfg.artistScale);
    const float fLabel=S(12), fSmall=S(10);
    gfx::Text(L"REMIX",S(18),S(14),fBrand,ab,true);
    DrawShapeToggle(ab,white,gray);
    DrawPill(R_autoTgl,L"AUTO",g_cfg.autoplay,S(12));
    DrawPill(R_sortBtn,SortLabel(),g_cfg.sortMode==L"manual",S(11));
    DrawPill(R_folderBtn,g_view==2?L"PASTA DA PLAYLIST ▾":L"PASTA ▾",g_folderMenuOpen,S(11));
    if(R_hostBtn.right>R_hostBtn.left) DrawPill(R_hostBtn,host::Running()?L"HOST ●":L"HOST",host::Running(),S(11));
    if(R_fxBtn.right>R_fxBtn.left) DrawPill(R_fxBtn,AnyFxOn()?L"EFEITOS ●":L"EFEITOS",AnyFxOn(),S(11));
    if(R_spadBtn.right>R_spadBtn.left){ bool on=spad::Running(); DrawPill(R_spadBtn,on?L"SOUNDPAD ●":L"SOUNDPAD",on,S(11)); }
    if(R_dcBtn.right>R_dcBtn.left){ bool on=dc::Ready(); DrawPill(R_dcBtn,on?L"DISCORD ●":L"DISCORD",on,S(11)); }
    DrawChromeButtons(white,gray);
    if(R_listBtn.right>R_listBtn.left){
        bool lm=g_cfg.listMode!=0;
        Color lpn=lm?(UiClassic()?ToGdi(g_theme.accent):white):Cs(Argb(255,90,94,118),UI().textFaint);
        float bx=(R_listBtn.left+R_listBtn.right)/2.f-S(20),by=(R_listBtn.top+R_listBtn.bottom)/2.f-S(12);
        for(int i=0;i<3;i++){
            gfx::StrokeRect(bx,by+i*S(10)-1.f,S(5),S(5),1.6f,lpn);
            gfx::Line(bx+S(11),by+i*S(10)+S(2),bx+S(36),by+i*S(10)+S(2),1.6f,lpn);
        }
    }
    gfx::Text(L"⚙",(float)R_gear.left+S(7),(float)R_gear.top+S(2),S(26),gray);
    DWORD pos=(g_current>=0&&g_player.loaded)?g_player.GetPositionMs():0;
    DWORD len=(g_current>=0&&g_player.loaded)?g_player.GetLengthMs():0;
    float frac=len?std::min(1.f,(float)pos/len):0;
    float tSec=NowMs()/1000.0f;
    COLORREF cNav=(!UiClassic()&&g_cfg.btnNavColor.empty())?UI().text:ResolveCustom(g_cfg.btnNavColor), cPlay=ResolveCustom(g_cfg.btnPlayColor);
    Color navB=ToGdi(cNav), playP=ToGdi(cPlay), playB=UiClassic()?ToGdi(cPlay):ToGdi(UI().bg);   // novos: play cheio, simbolo escuro
    // ---------------- painel grande do player (so no CLASSICO: no REMIX ele virou a barra de baixo) --
    if(!RxOn()){
        RectF pnl=RF(R_playerPanel);
        float pr_=UiClassic()?18.f:S(UI_R_CARD);
        if(UiClassic()){ Color pb=Argb(255,8,11,24); DrawRoundRect(pnl,18,&pb,nullptr); }
        if(UiLed()>0){   // LED: contorno neon do painel (brilho, velocidade e efeito das configuracoes) - classico e spotify
            BYTE la=LedAlpha(g_glowPhase,40,200); COLORREF cLed=ResolveCustom(g_cfg.ledColor);
            for(int i=0;i<3;i++){ Color lc=ToGdi(cLed,(BYTE)(la/(i+1))); RectF gr(pnl.X-(float)i,pnl.Y-(float)i,pnl.Width+2.f*i,pnl.Height+2.f*i); DrawRoundRect(gr,pr_+i,nullptr,&lc,1.f+i*.5f); }
        }
        BYTE ra=RunnerAlpha();
        if(UiRunner()&&ra>6) DrawRunnerRect(pnl,pr_,ResolveCustom(g_cfg.runnerColor),ra,g_runnerPhase);
        bool isCd=g_cfg.artShape==L"cd";
        int asize=R_art.right-R_art.left;
        int x0=R_art.left;
        RectF cvr((float)R_art.left,(float)R_art.top,(float)asize,(float)asize);
        if(isCd){
            DrawCoverCircle(g_coverImg,cvr,UiClassic()?accent:ToGdi(UI().borderHi),UiClassic()?2.f:1.4f,g_player.playing?g_rotation:0);
            if(g_cfg.particlesOn&&FxOn()){
                RectF clip(cvr.X+4,cvr.Y+4,cvr.Width-8,cvr.Height-8);
                DrawParticles(RectF(cvr.X+6,cvr.Y+6,cvr.Width-12,cvr.Height-12),ResolveCustom(g_cfg.particlesColor),tSec,1.7f,&clip);
            }
        } else {
            if(UiClassic()){ Color plate=Argb(255,16,19,32); DrawRoundRect(cvr,14,&plate,&accent,2); }
            else { Color plate=ToGdi(UI().surface); DrawRoundRect(cvr,S(UI_R_CARD),&plate,nullptr); }
            if(g_coverImg&&g_coverImg->ok){
                if(GlitchNow(NowMs())){
                    float sh=(float)asize,ih=(float)g_coverImg->h,iw=(float)g_coverImg->w;
                    struct SL{float y0,y1,dx;}; SL sl[3]={{0,sh*.34f,-6},{sh*.34f,sh*.63f,5},{sh*.63f,sh,-3}};
                    for(auto&s:sl){
                        RectF dst(cvr.X+s.dx,cvr.Y+s.y0,cvr.Width,s.y1-s.y0);
                        RectF src(0,(s.y0/sh)*ih,iw,((s.y1-s.y0)/sh)*ih);
                        gfx::DrawImgPart(g_coverImg,src,dst);
                    }
                    gfx::FillRect(cvr.X-4,cvr.Y+sh*.12f,cvr.Width+8,3.f,Argb(40,255,60,60));
                    gfx::FillRect(cvr.X+3,cvr.Y+sh*.55f,cvr.Width+6,3.f,Argb(40,60,180,255));
                } else gfx::DrawImgCover(g_coverImg,RectF(cvr.X+3,cvr.Y+3,cvr.Width-6,cvr.Height-6));
            }
            if(g_cfg.particlesOn&&FxOn()) DrawParticles(RectF(cvr.X+2,cvr.Y+2,cvr.Width-4,cvr.Height-4),ResolveCustom(g_cfg.particlesColor),tSec,1.f);
        }
        // titulo + artista + lapis (link para renomear)
        const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
        std::wstring title=ct?ct->title:L"Nenhuma musica";
        std::wstring artist=ct?ct->artist:L"—";
        {
            int ty0=R_art.bottom+SI(3);
            int availTxt=R_wavePanel.top-SI(2)-ty0; if(availTxt<S(40)) availTxt=(int)S(40);
            float fth=TextScale(20,g_cfg.titleScale), fah=TextScale(13,g_cfg.artistScale);
            int th=std::max((int)S(22),std::min((int)(fth*1.35f)+SI(4),availTxt*45/100));
            int ah=std::max((int)S(16),availTxt-th-SI(2));
            float fTitleFit=std::min(fth,(float)th*0.78f), fArtistFit=std::min(fah,(float)ah*0.80f);
            RectF tr((float)x0,(float)ty0,(float)asize,(float)th);
            DrawMarqueeText(title,fTitleFit,true,tr,white,false,S(30),NowMs());
            int ay2=ty0+th+SI(2);
            RectF ar((float)x0,(float)ay2,(float)asize,(float)ah);
            DrawMarqueeText(artist,fArtistFit,false,ar,UiClassic()?ab:gray,false,S(26),NowMs());
            float mw=gfx::TextWidth(artist,fArtistFit,false);
            float aw=mw>2.f?std::min(mw,(float)(asize-S(34))):S(60);
            R_pencilPanel={(LONG)(x0+(int)aw+S(6)),(LONG)(ay2),(LONG)(x0+(int)aw+S(28)),(LONG)(ay2+ah)};
            gfx::TextRect(L"✎",RF(R_pencilPanel),S(13),gray,false,gfx::Center,true);
        }
        // wave do painel (posicao fixa do layout — hit-test da seek depende dela)
        int wy=R_wavePanel.top, wb=R_wavePanel.bottom;
        int wl=x0, wr=R_art.right;
        int countP=UiClassic()?std::max(30,(wr-wl)/(FxOn()?6:12)):std::max(24,(wr-wl)/(FxOn()?9:14));   // novos: onda mais aberta
        float bw=UiClassic()?2.6f:3.f,gap=countP>1?((wr-wl-countP*bw)/(float)(countP-1)):0.f;
        for(int k=0;k<countP;k++){
            float v=(len?AudioWaveBar(k,countP,pos,len,tSec):WaveIdleAt(k,countP,tSec));
            float bh=std::max(4.f,(float)(wb-wy)*v);
            float x=(float)wl+k*(bw+gap), y=((float)wy+(float)wb-bh)/2.f;
            float pk=countP>1?(float)k/(countP-1):0.f;
            gfx::FillRect(x,y,bw,bh,pk<=frac?ab:dimB);
        }
        // barra de seek + tempos
        float syl=(float)(R_seek.top+R_seek.bottom)/2.f;
        gfx::Line((float)wl,syl,(float)wr,syl,3,Cs(Argb(75,85,87,110),UI().borderHi));
        gfx::Line((float)wl,syl,(float)wl+(wr-wl)*frac,syl,3,accent);
        float kx=(float)wl+(float)(wr-wl)*frac;
        gfx::FillEllipse(kx-S(6),syl-S(6),S(12),S(12),UiClassic()?ab:white);
        if(UiClassic()) gfx::StrokeEllipse(kx-S(6),syl-S(6),S(12),S(12),1.4f,Argb(220,255,255,255));
        gfx::Text(FormatTime(pos),(float)wl,syl+S(9),fSmall,gray);
        gfx::TextRect(FormatTime(len),RectF((float)wl,syl+S(9),(float)(wr-wl),18),fSmall,gray,false,gfx::Far);
        // transporte centralizado no painel
        gfx::TextRect(L"⇄",RF(R_shuffle),S(17),g_cfg.shuffle?navB:gray,false,gfx::Center,true);
        {RECT z=R_prev; RectF pv(z.left+S(5),z.top+S(9),z.right-z.left-S(10),z.bottom-z.top-S(18)); IconSkip(pv,navB,false);}
        if(UiClassic()) gfx::StrokeEllipse(RF(R_play),2.4f,playP); else gfx::FillEllipse(RF(R_play),playP);
        if(g_converting){ RectF pr=RF(R_play); gfx::Arc(pr.X-S(4),pr.Y-S(4),pr.Width+S(8),pr.Height+S(8),(float)(NowMs()%1000)*0.36f,100.f,3.f,ab); }   // conectando/convertendo
        {RECT z=R_play; RectF pl(z.left+S(17),z.top+S(17),z.right-z.left-S(34),z.bottom-z.top-S(34)); if(g_player.playing) IconPause(pl,playB); else IconPlay(pl,playB);}
        {RECT z=R_next; RectF nx(z.left+S(5),z.top+S(9),z.right-z.left-S(10),z.bottom-z.top-S(18)); IconSkip(nx,navB,true);}
        gfx::TextRect(L"⟳",RF(R_repeat),S(17),g_cfg.repeat?navB:gray,false,gfx::Center,true);
        // volume (icone + barra + %)
        DrawVolumeBar(ab,gray,5);
        Color grip=Cs(Argb(160,150,153,175),UI().borderHi);
        for(int i=1;i<=3;i++) gfx::Line(pnl.X+pnl.Width-i*7,pnl.Y+pnl.Height-5,pnl.X+pnl.Width-5,pnl.Y+pnl.Height-i*7,2,grip);
    }
    // ---------------- direita: lista simples ou grade de cards ----------------
    bool libClip=R_library.right>R_library.left; if(libClip) gfx::PushClip(RF(R_library));
    if(RxOn()&&g_cfg.listMode!=0){ RxDrawLista(ab,white,gray); }
    else if(g_cfg.listMode!=0){
        float fRowT=S(13);
        for(size_t i=0;i<g_tracks.size() && i<R_cardRects.size();++i){
            RECT rr=R_cardRects[i]; if(rr.right-rr.left<=0) continue;
            bool cur=(int)i==g_current;
            RectF row=RF(rr);
            BYTE ra=RunnerAlpha();
            Color rb=UiClassic()?(cur?ToGdi(g_theme.accent,40):Argb(210,9,12,25)):ToGdi((cur||UiHot(rr))?UI().surfaceHi:UI().surface);
            Color rp=UiClassic()?(cur?ToGdi(g_theme.accent,190):Argb(255,32,36,56)):ToGdi(UI().borderHi);
            if(UiClassic()) DrawRoundRect(row,S(10),&rb,&rp,1.2f); else DrawRoundRect(row,S(UI_R_CARD),&rb,nullptr);
            if(cur&&UiRunner()&&ra>6){
                RectF edge((float)rr.left,(float)rr.top,S(3),(float)(rr.bottom-rr.top));
                Color eb=ToGdi(ResolveCustom(g_cfg.runnerColor),(BYTE)(ra*.7f));
                DrawRoundRect(edge,1,&eb,nullptr);
            }
            int th=SI(44); RectF art((float)(rr.left+SI(10)),(float)(rr.top+(rr.bottom-rr.top-th)/2),(float)th,(float)th);
            if(g_cfg.artShape==L"cd") DrawCoverCircle(GetThumb(g_tracks[i].coverPath),art,rp,1.2f,cur&&g_player.playing?g_rotation:0);
            else {Color plate=Cs(Argb(255,18,21,34),UI().bg);DrawRoundRect(art,UiClassic()?6:S(4),&plate,nullptr);Img* im=GetThumb(g_tracks[i].coverPath);if(im)gfx::DrawImgCover(im,art);}
            if(IsOnlineTrack(g_tracks[i])) DrawOnlineTag(art,g_tracks[i].path);
            float tx=(float)rr.left+S(72);
            RectF rtT(tx,(float)rr.top+S(9),(float)(rr.right-tx-S(60)),S(21));
            if(cur) DrawMarqueeText(g_tracks[i].title,fRowT,true,rtT,white,false,S(26),NowMs());
            else gfx::TextRect(g_tracks[i].title,rtT,fRowT,white,true,gfx::Near,false,gfx::EllipsisWord);
            {   // linha: artista + estado do canal de streaming (tocando / na fila: pronta, carregando...)
                float aw=(float)(rr.right-tx-S(60));
                const StreamInfo* sc=IsOnlineTrack(g_tracks[i])?FindSnap(g_tracks[i].path):nullptr;
                if(sc&&aw>S(380)){ aw-=S(250); gfx::TextRect(StreamRowText(*sc),RectF((float)rr.right-S(320),(float)rr.top+S(31),S(236),S(16)),fSmall,sc->phase==5?Argb(255,235,120,110):gray,false,gfx::Far,false,gfx::EllipsisChar); }
                gfx::TextRect(g_tracks[i].artist,RectF(tx,(float)rr.top+S(31),aw,S(16)),fSmall,UiClassic()?ab:gray,false,gfx::Near,false,gfx::EllipsisWord);
            }
            if(cur) gfx::FillEllipse((float)(rr.right-S(24)),(float)(rr.top+(rr.bottom-rr.top)/2.f-S(4)),S(8),S(8),ab);
            DrawOrderArrows(i,ab,gray);
            DrawPickMark(i,rr);
        }
    } else if(UiClassic())
    for(size_t i=0;i<g_tracks.size() && i<R_cardRects.size();++i){
        RECT rr=R_cardRects[i];
        if(rr.right-rr.left<=0) continue;
        bool cur=(int)i==g_current;
        RectF card=RF(rr);
        Color cb=Argb(255,9,12,25);
        Color cp=cur?ToGdi(g_theme.accent,220):Argb(255,40,44,65);
        DrawRoundRect(card,16,&cb,&cp,1.5f);
        BYTE ra=RunnerAlpha();
        if(cur&&UiRunner()&&ra>6) DrawRunnerRect(card,16.f,ResolveCustom(g_cfg.runnerColor),(BYTE)(ra*.55f),g_runnerPhase+.5f);
        int cover=SI(142); RectF art((float)(rr.left+SI(18)),(float)(rr.top+SI(18)),(float)cover,(float)cover);
        if(g_cfg.artShape==L"cd") DrawCoverCircle(GetThumb(g_tracks[i].coverPath),art,cp,1.5f,cur&&g_player.playing?g_rotation:0);
        else {Color plate=Argb(255,18,21,34);DrawRoundRect(art,10,&plate,nullptr);Img* im=GetThumb(g_tracks[i].coverPath);if(im)gfx::DrawImgCover(im,art);}
        if(IsOnlineTrack(g_tracks[i])) DrawOnlineTag(art,g_tracks[i].path);
        if(cur&&g_cfg.particlesOn&&g_player.playing){
            bool ccd=g_cfg.artShape==L"cd";
            RectF clip(art.X+3,art.Y+3,art.Width-6,art.Height-6);
            if(ccd) DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),tSec,1.25f,&clip);
            else { gfx::PushClip(clip); DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),tSec,1.25f); gfx::PopClip(); }
        }
        if(!g_pickMode){ RECT cbn=R_cardCoverButtons[i]; DrawCameraIcon(cbn,ToGdi(g_theme.accent,220)); }
        DrawOrderArrows(i,ab,gray);
        int tx=rr.left+SI(180);
        int tw=rr.right-SI(50)-tx;
        RectF trc((float)tx,(float)(rr.top+SI(16)),(float)tw,(float)SI(34));
        RectF tar((float)tx,(float)(rr.top+SI(52)),(float)tw,(float)SI(24));
        if(cur){
            DrawMarqueeText(g_tracks[i].title,fTitleBase,true,trc,white,false,S(30),NowMs());
            DrawMarqueeText(g_tracks[i].artist,fArtistBase,false,tar,ab,false,S(24),NowMs());
        } else {
            gfx::TextRect(g_tracks[i].title,trc,fTitleBase,white,true,gfx::Near,false,gfx::EllipsisWord);
            gfx::TextRect(g_tracks[i].artist,tar,fArtistBase,ab,false,gfx::Near,false,gfx::EllipsisWord);
        }
        DWORD cpos=(cur&&g_player.loaded)?pos:0, clen=(cur&&g_player.loaded)?len:0;
        float cfrac=clen?std::min(1.f,(float)cpos/clen):0;
        int wl=tx, wr=rr.right-SI(22), wyy=rr.top+SI(108), wbb=rr.top+SI(144);
        int count=std::max(30,(wr-wl)/(FxOn()?5:10));
        float bw=2.2f,gap=(wr-wl-count*bw)/(float)std::max(1,count-1);
        for(int k=0;k<count;k++){
            float v=clen?AudioWaveBar(k,count,cpos,clen,tSec):WaveIdleAt(k,count,tSec);
            float bh=std::max(4.f,(float)(wbb-wyy)*v);
            float x=(float)wl+k*(bw+gap), y=((float)wyy+(float)wbb-bh)/2.f;
            float p=count>1?(float)k/(count-1):0.f;
            gfx::FillRect(x,y,bw,bh,p<=cfrac?accent:Argb(255,48,51,72));
        }
        float sy=(float)wbb+8;
        gfx::Line((float)wl,sy,(float)wr,sy,2,Argb(75,82,85,110));
        gfx::Line((float)wl,sy,(float)wl+(wr-wl)*cfrac,sy,2,accent);
        float kx=(float)wl+(float)(wr-wl)*cfrac;
        gfx::FillEllipse(kx-5,sy-5,10.f,10.f,ab); gfx::StrokeEllipse(kx-5,sy-5,10.f,10.f,1,Argb(220,255,255,255));
        gfx::Text(FormatTime(cpos),(float)wl,sy+7,fSmall,gray);
        gfx::TextRect(FormatTime(clen),RectF((float)wl,sy+7,(float)(wr-wl),18),fSmall,gray,false,gfx::Far);
        // transporte simetrico ao centro do card
        int ccx=(rr.left+rr.right)/2, cy=rr.bottom-SI(40);
        gfx::TextRect(L"⇄",RectF((float)(ccx-SI(120)-S(15)),(float)(cy-S(11)),S(30),S(26)),S(18),g_cfg.shuffle?navB:gray,false,gfx::Center,true);
        RectF pv((float)(ccx-SI(66)-S(14)),(float)(cy-S(11)),S(28),S(23)); IconSkip(pv,navB,false);
        gfx::StrokeEllipse((float)(ccx-SI(25)),(float)(cy-S(25)),S(50),S(50),2.4f,playP);
        {RectF pl((float)(ccx-S(10)),(float)(cy-S(12)),S(20),S(24)); if(cur&&g_player.playing) IconPause(pl,playB); else IconPlay(pl,playB);}
        RectF nx((float)(ccx+SI(66)-S(14)),(float)(cy-S(11)),S(28),S(23)); IconSkip(nx,navB,true);
        gfx::TextRect(L"⟳",RectF((float)(ccx+SI(120)-S(15)),(float)(cy-S(11)),S(30),S(26)),S(18),g_cfg.repeat?navB:gray,false,gfx::Center,true);
        if(cur){gfx::Text(L"▶",(float)(rr.right-SI(28)),(float)(rr.bottom-SI(24)),fSmall,ab);}
        DrawPickMark(i,rr);
    }
    else
    for(size_t i=0;i<g_tracks.size() && i<R_cardRects.size();++i){   // estilos novos: capa grande, nome e artista; play sobre a capa com o mouse
        RECT rr=R_cardRects[i];
        if(rr.right-rr.left<=0) continue;
        bool cur=(int)i==g_current, hot=UiHot(rr);
        RectF card=RF(rr);
        Color cb=ToGdi((cur||hot)?UI().surfaceHi:UI().surface);
        DrawRoundRect(card,S(UI_R_CARD),&cb,nullptr);
        BYTE ra=RunnerAlpha();
        if(cur&&UiRunner()&&ra>6) DrawRunnerRect(card,S(UI_R_CARD),ResolveCustom(g_cfg.runnerColor),(BYTE)(ra*.55f),g_runnerPhase+.5f);
        int cw=rr.right-rr.left, cover=cw-SI(20);
        RectF art((float)(rr.left+SI(10)),(float)(rr.top+SI(10)),(float)cover,(float)cover);
        if(g_cfg.artShape==L"cd") DrawCoverCircle(GetThumb(g_tracks[i].coverPath),art,ToGdi(UI().borderHi),1.2f,cur&&g_player.playing?g_rotation:0);
        else {Color plate=ToGdi(UI().bg);DrawRoundRect(art,S(4),&plate,nullptr);Img* im=GetThumb(g_tracks[i].coverPath);if(im)gfx::DrawImgCover(im,art);}
        if(IsOnlineTrack(g_tracks[i])) DrawOnlineTag(art,g_tracks[i].path);
        if(cur&&g_cfg.particlesOn&&g_player.playing){
            bool ccd=g_cfg.artShape==L"cd";
            RectF clip(art.X+3,art.Y+3,art.Width-6,art.Height-6);
            if(ccd) DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),tSec,1.25f,&clip);
            else { gfx::PushClip(clip); DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),tSec,1.25f); gfx::PopClip(); }
        }
        if(!g_pickMode&&hot){ RECT cbn=R_cardCoverButtons[i]; DrawCameraIcon(cbn,white); }
        if(!g_pickMode&&(hot||(cur&&g_player.playing))&&i<R_cardPlayBtns.size()&&R_cardPlayBtns[i].right>R_cardPlayBtns[i].left){   // botao de play sobre a capa
            RectF pb=RF(R_cardPlayBtns[i]); Color shadow=Argb(90,0,0,0);
            gfx::FillEllipse(RectF(pb.X+2,pb.Y+3,pb.Width,pb.Height),shadow);
            gfx::FillEllipse(pb,ToGdi(cPlay));
            RectF gl(pb.X+pb.Width*.32f,pb.Y+pb.Height*.28f,pb.Width*.40f,pb.Height*.44f);
            if(cur&&g_player.playing) IconPause(gl,ToGdi(UI().bg)); else IconPlay(gl,ToGdi(UI().bg));
        }
        if(!g_pickMode&&hot&&i<R_cardDcBtns.size()&&R_cardDcBtns[i].right>R_cardDcBtns[i].left&&DcCardsOn()) DrawPill(R_cardDcBtns[i],L"▶ DISCORD",true,S(10));   // tocar no bot, em cima da capa
        DrawOrderArrows(i,ab,gray);
        float tx=(float)(rr.left+SI(12)), tw=(float)(cw-SI(24));
        float ty=(float)(rr.top+SI(10))+cover+S(8);
        RectF trc(tx,ty,tw,S(20)), tar(tx,ty+S(21),tw,S(16));
        if(cur) DrawMarqueeText(g_tracks[i].title,S(13),true,trc,white,false,S(26),NowMs());
        else gfx::TextRect(g_tracks[i].title,trc,S(13),white,true,gfx::Near,true,gfx::EllipsisWord);
        {   // segunda linha: artista; na faixa atual um marcador; online, o estado do canal
            const StreamInfo* sc=IsOnlineTrack(g_tracks[i])?FindSnap(g_tracks[i].path):nullptr;
            if(cur&&g_player.loaded){
                gfx::Text(g_player.playing?L"▶":L"❚❚",tx,tar.Y,S(9),ab);
                gfx::TextRect(g_tracks[i].artist,RectF(tx+S(14),tar.Y,tw-S(14),tar.Height),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
            } else if(sc){
                gfx::TextRect(StreamRowText(*sc),tar,S(11),sc->phase==5?Argb(255,235,120,110):gray,false,gfx::Near,true,gfx::EllipsisChar);
            } else {
                gfx::TextRect(g_tracks[i].artist,tar,S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
            }
        }
        DrawPickMark(i,rr);
    }
    if(libClip){ DrawPlaylistCards(ab,white,gray); gfx::PopClip(); }
    DrawLibBar(ab,white,gray);
    if(g_tracks.empty()&&g_view!=1&&R_library.right>R_library.left){   // lista vazia: no meio da area da lista, abaixo da barra
        RectF lib=RF(R_library);
        float ty=R_onlineInfo.bottom>R_onlineInfo.top?(float)R_onlineInfo.top-S(66):lib.Y+S(80);
        std::wstring t1,t2;
        if(g_view==2){ t1=L"Esta playlist está vazia"; t2=L"Adicione da biblioteca, arquivos, uma pasta (vinculada ou não), um link ou a busca online."; }
        else if(SS().busy){ t1=L"Procurando músicas no PC..."; t2=L"Músicas, Downloads, Documentos e Área de trabalho"; }
        else { t1=L"Nenhuma música encontrada"; t2=g_cfg.musicFolder.empty()?L"Escolha a pasta das suas músicas (ou use a aba ONLINE).":L"A pasta escolhida não tem músicas: "+g_cfg.musicFolder; }
        gfx::TextRect(t1,RectF(lib.X,ty,lib.Width,S(26)),S(16),white,true,gfx::Center,true);
        gfx::TextRect(t2,RectF(lib.X+S(20),ty+S(30),lib.Width-S(40),S(20)),S(11),gray,false,gfx::Center,true,gfx::EllipsisChar);
        if(R_onlineInfo.right>R_onlineInfo.left&&(g_view==2||!SS().busy)) DrawPill(R_onlineInfo,g_view==2?L"+ ADICIONAR MÚSICAS":L"ESCOLHER PASTA",true,S(12));
        (void)fLabel;
    }
    if(RxOn()){
        RxDrawSide(ab,white,gray);
        RxDrawBusca(ab,white,gray);
        if(g_rxLetraOn) RxDrawLetra(ab,white,gray);
        else if(g_rxPag==RXP_LISTA) RxDrawCabecalho(ab,white,gray);
        else RxDrawInicio(w,h,ab,white,gray);
        RxDrawPainel(ab,white,gray);
        RxDrawBar(w,h,ab,white,gray,navB,playP,playB);
    }
    if(g_showSettings) DrawSettings(w,h);
}

static void DrawVertical(int w,int h){
    DrawAppBackground(w,h,Cs(Argb(255,4,6,14),UI().bg)); Color accent=ToGdi(g_theme.accent); Color ab=accent, white=C_WHITE, gray=C_GRAY;
    const float ft=TextScale(17,g_cfg.titleScale), fs=S(10);
    gfx::Text(L"⚙",(float)R_gear.left,(float)R_gear.top,S(26),gray);   // no vertical o pill AUTO ocupa o canto do "REMIX"
    int ar=R_art.right-R_art.left; RectF art((float)R_art.left,(float)R_art.top,(float)ar,(float)ar);
    if(g_cfg.artShape==L"square"){
        if(UiClassic()){ Color plate=Argb(255,16,19,32); DrawRoundRect(art,14,&plate,&accent,2); }
        else { Color plate=ToGdi(UI().surface); DrawRoundRect(art,S(UI_R_CARD),&plate,nullptr); }
        if(g_coverImg&&g_coverImg->ok){ gfx::PushClip(RectF(art.X+3,art.Y+3,art.Width-6,art.Height-6)); gfx::DrawImgCover(g_coverImg,RectF(art.X+3,art.Y+3,art.Width-6,art.Height-6)); gfx::PopClip(); }
    } else DrawCoverCircle(g_coverImg,art,UiClassic()?accent:ToGdi(UI().borderHi),UiClassic()?2.f:1.4f,g_player.playing?g_rotation:0);
    if(g_cfg.particlesOn&&FxOn()){
        bool vcd=g_cfg.artShape==L"cd";
        RectF vclip(art.X+3,art.Y+3,art.Width-6,art.Height-6);
        if(vcd) DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),NowMs()/1000.f,1.6f,&vclip);
        else { gfx::PushClip(vclip); DrawParticles(RectF(art.X+4,art.Y+4,art.Width-8,art.Height-8),ResolveCustom(g_cfg.particlesColor),NowMs()/1000.f,1.6f); gfx::PopClip(); }
    }
    DrawCameraIcon(R_verticalCoverButton,UiClassic()?ToGdi(g_theme.accent,220):gray);
    if(R_shapeTgl.right>R_shapeTgl.left) DrawShapeToggle(ab,white,gray);
    if(R_autoTgl.right>R_autoTgl.left) DrawPill(R_autoTgl,L"AUTO",g_cfg.autoplay,S(11));

    const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
    std::wstring title=ct?ct->title:L"Nenhuma musica";
    std::wstring artist=ct?ct->artist:L"—";
    // Caixas adaptativas entre a arte e os botoes: escalas grandes nao se sobrepõem.
    {
        int vy0=R_art.bottom+(int)S(8);
        int vB=R_shuffle.top-(int)S(8);
        int vAvail=vB-vy0; if(vAvail<(int)S(40)) vAvail=(int)S(40);
        float vfth=TextScale(17,g_cfg.titleScale), vfah=TextScale(12,g_cfg.artistScale);
        int vth=std::max((int)S(20),std::min((int)(vfth*1.35f)+(int)S(4),vAvail*55/100));
        int vah=std::max((int)S(16),vAvail-vth-(int)S(2));
        float ftFit=std::min(vfth,(float)vth*0.78f), faFit=std::min(vfah,(float)vah*0.80f);
        DrawMarqueeText(title,ftFit,true,RectF(20,(float)vy0,(float)w-40,(float)vth),white,true,S(30),NowMs());
        int vay=vy0+vth+(int)S(2);
        RectF arV(20,(float)vay,(float)w-40,(float)vah);
        DrawMarqueeText(artist,faFit,false,arV,UiClassic()?ab:gray,true,S(26),NowMs());
        float mwV=gfx::TextWidth(artist,faFit,false);
        float aw=mwV>2.f?std::min(mwV,(float)(w/2-S(20))):S(40);
        float px=(float)w/2+aw/2+S(4);
        int pencilH=std::min(vah,(int)(faFit*1.9f));   // altura da linha do artista, nao da caixa toda
        R_pencilVert={(LONG)(px-S(6)),(LONG)(vay),(LONG)(px+S(18)),(LONG)(vay+pencilH)};
        gfx::TextRect(L"✎",RF(R_pencilVert),S(13),gray,false,gfx::Center,true);
    }
    DWORD pos=g_player.loaded?g_player.GetPositionMs():0,len=g_player.loaded?g_player.GetLengthMs():0;float frac=len?std::min(1.f,(float)pos/len):0;
    COLORREF cNav=(!UiClassic()&&g_cfg.btnNavColor.empty())?UI().text:ResolveCustom(g_cfg.btnNavColor),cPlay=ResolveCustom(g_cfg.btnPlayColor);Color navB=ToGdi(cNav),playP=ToGdi(cPlay),playB=UiClassic()?ToGdi(cPlay):ToGdi(UI().bg);
    float tSec=NowMs()/1000.0f;
    int sl=R_seek.left,srx=R_seek.right,sw=srx-sl;int count=std::max(28,sw/5);float bw=2.4f,gap=(sw-count*bw)/(float)std::max(1,count-1),top=(float)R_seek.top,bottom=(float)R_seek.bottom;
    Color dimBar=Cs(Argb(255,50,52,70),UI().borderHi);
    for(int i=0;i<count;i++){float p=count==1?0.f:(float)i/(count-1),bh=std::max(4.f,(bottom-top)*(len?AudioWaveBar(i,count,pos,len,tSec):WaveIdleAt(i,count,tSec))),x=sl+i*(bw+gap),y=(top+bottom-bh)/2;gfx::FillRect(x,y,bw,bh,p<=frac?ab:dimBar);}
    float sy=bottom+8;
    gfx::Line((float)sl,sy,(float)srx,sy,3,Cs(Argb(75,85,87,110),UI().borderHi));gfx::Line((float)sl,sy,(float)(sl+sw*frac),sy,3,accent);
    float kx=sl+sw*frac;gfx::FillEllipse(kx-6,sy-6,12.f,12.f,UiClassic()?ab:white);if(UiClassic())gfx::StrokeEllipse(kx-6,sy-6,12.f,12.f,1,Argb(220,255,255,255));
    gfx::Text(FormatTime(pos),(float)sl,sy+10,fs,gray);
    gfx::TextRect(FormatTime(len),RectF((float)sl,sy+10,(float)sw,18),fs,gray,false,gfx::Far);
    gfx::TextRect(L"⇄",RF(R_shuffle),S(13),g_cfg.shuffle?navB:gray,false,gfx::Center,true);
    {RectF pv((float)(R_prev.left+4),(float)(R_prev.top+9),(float)(R_prev.right-R_prev.left-8),(float)(R_prev.bottom-R_prev.top-18)); IconSkip(pv,navB,false);}
    if(UiClassic()) gfx::StrokeEllipse(RF(R_play),2.2f,playP); else gfx::FillEllipse(RF(R_play),playP);
    if(g_converting){ RectF pr=RF(R_play); gfx::Arc(pr.X-S(4),pr.Y-S(4),pr.Width+S(8),pr.Height+S(8),(float)(NowMs()%1000)*0.36f,100.f,3.f,ab); }
    {RectF pl((float)(R_play.left+17),(float)(R_play.top+17),(float)(R_play.right-R_play.left-34),(float)(R_play.bottom-R_play.top-34)); if(g_player.playing) IconPause(pl,playB); else IconPlay(pl,playB);}
    {RectF nx((float)(R_next.left+4),(float)(R_next.top+9),(float)(R_next.right-R_next.left-8),(float)(R_next.bottom-R_next.top-18)); IconSkip(nx,navB,true);}
    gfx::TextRect(L"⟳",RF(R_repeat),S(13),g_cfg.repeat?navB:gray,false,gfx::Center,true);
    DrawVolumeBar(ab,gray,4);
    DrawChromeButtons(white,gray);
    if(UiLed()>0){ BYTE a=LedAlpha(g_glowPhase,35,180);COLORREF cLed=ResolveCustom(g_cfg.ledColor);
        for(int i=0;i<4;i++) gfx::StrokeRect((float)i,(float)i,(float)(w-1-i*2),(float)(h-1-i*2),(float)(1+i),ToGdi(cLed,(BYTE)(a/(i+1)))); }
    if(UiRunner()){
        BYTE ra=RunnerAlpha(); COLORREF rc=ResolveCustom(g_cfg.runnerColor);
        if(ra>6){
            DrawRunnerRect(RectF(2,2,(float)w-4,(float)h-4),10.f,rc,ra,g_runnerPhase);
            RectF artRing(art.X-6,art.Y-6,art.Width+12,art.Height+12);
            if(g_cfg.artShape==L"square") DrawRunnerRect(artRing,14.f,rc,ra,g_runnerPhase+.25f);
            else DrawRunnerCircle(artRing,rc,ra,g_runnerPhase+.25f);
            RectF pring((float)R_play.left-5,(float)R_play.top-5,(float)(R_play.right-R_play.left)+10,(float)(R_play.bottom-R_play.top)+10);
            DrawRunnerCircle(pring,rc,(BYTE)(ra*.8f),g_runnerPhase+.6f);
        }
    }
    if(g_showSettings) DrawSettings(w,h);
}
