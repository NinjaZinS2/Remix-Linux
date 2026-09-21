#pragma once
// Desenho do estilo REMIX (1.6): barra lateral com a biblioteca, tela inicial
// com fileiras de novidades e o player numa barra embaixo, da largura toda.
// A geometria vem de BuildLayoutRemix (comum/app_layout.h); aqui so pinta.
#include "app_state.h"
#include "app_layout.h"

// ---------------------------------------------------------------- icones ---
// Desenhados a mao: a fonte embutida nao tem os simbolos de casa/lupa/lista.
static void RxIconCasa(const RectF& r,Color c){
    float cx=r.X+r.Width/2, t=r.Y+r.Height*.12f, b=r.Y+r.Height*.86f, hw=r.Width*.42f;
    gfx::Line(cx,t,cx-hw,r.Y+r.Height*.46f,1.8f,c);
    gfx::Line(cx,t,cx+hw,r.Y+r.Height*.46f,1.8f,c);
    gfx::Line(cx-hw*.78f,r.Y+r.Height*.50f,cx-hw*.78f,b,1.8f,c);
    gfx::Line(cx+hw*.78f,r.Y+r.Height*.50f,cx+hw*.78f,b,1.8f,c);
    gfx::Line(cx-hw*.78f,b,cx+hw*.78f,b,1.8f,c);
}
static void RxIconLupa(const RectF& r,Color c){
    float cx=r.X+r.Width*.44f, cy=r.Y+r.Height*.42f, rad=r.Width*.28f;
    gfx::StrokeEllipse(cx-rad,cy-rad,rad*2,rad*2,1.8f,c);
    gfx::Line(cx+rad*.72f,cy+rad*.72f,r.X+r.Width*.86f,r.Y+r.Height*.86f,2.f,c);
}
static void RxIconSom(const RectF& r,Color c,bool mudo){
    float x=r.X+r.Width*.22f, cy=r.Y+r.Height/2, hh=r.Height*.16f;
    gfx::FillRect(x,cy-hh,r.Width*.14f,hh*2,c);
    Vector2 a{x+r.Width*.14f,cy-hh}, b{x+r.Width*.14f,cy+hh}, cpt{x+r.Width*.40f,cy};
    gfx::Tri(Vector2{a.x,cy-r.Height*.30f},Vector2{b.x,cy+r.Height*.30f},cpt,c);
    gfx::Tri(Vector2{a.x,cy-r.Height*.30f},cpt,Vector2{x+r.Width*.40f,cy-r.Height*.30f},c);
    if(mudo){ gfx::Line(r.X+r.Width*.52f,cy-r.Height*.18f,r.X+r.Width*.86f,cy+r.Height*.18f,1.8f,c); gfx::Line(r.X+r.Width*.86f,cy-r.Height*.18f,r.X+r.Width*.52f,cy+r.Height*.18f,1.8f,c); }
    else { gfx::Arc(r.X+r.Width*.34f,cy-r.Height*.30f,r.Width*.42f,r.Height*.60f,-55,110,1.6f,c); }
}
static void RxIconLista(const RectF& r,Color c){
    for(int i=0;i<3;i++){
        float y=r.Y+r.Height*(.26f+i*.24f);
        gfx::Line(r.X+r.Width*.16f,y,r.X+r.Width*.22f,y,1.8f,c);
        gfx::Line(r.X+r.Width*.32f,y,r.X+r.Width*.86f,y,1.8f,c);
    }
}
// Capa de um item do descobrir (a miniatura chega pela internet em segundo plano).
static std::vector<std::pair<std::wstring,std::wstring>> g_rxThumbFila;   // url original -> pedido
static void RxCapa(const RectF& art,const std::wstring& capaUrl,const std::wstring& capaLocal,bool redondo,Color plate){
    if(redondo) gfx::FillEllipse(art,plate); else DrawRoundRect(art,S(UI_R_CARD),&plate,nullptr);
    std::wstring arq=capaLocal;
    if(arq.empty()&&!capaUrl.empty()){
        arq=OnlineThumbFile(capaUrl);
        if(!g_thumbReady.count(arq)){
            std::error_code ec;
            if(std::filesystem::exists(std::filesystem::path(arq),ec)) g_thumbReady.insert(arq);
            else { g_rxThumbFila.push_back({capaUrl,capaUrl}); arq.clear(); }
        }
    }
    if(arq.empty()) return;
    Img* im=GetThumb(arq); if(!im) return;
    if(redondo) gfx::DrawImgCircle(im,art,0); else gfx::DrawImgCover(im,art);
}
// Pede as miniaturas que faltaram neste quadro (uma leva por vez, sem repetir).
static void RxBaixarCapasPendentes(){
    if(g_rxThumbFila.empty()) return;
    static std::set<std::wstring> pedidas;
    std::vector<std::pair<std::wstring,std::wstring>> v;
    for(auto& p:g_rxThumbFila){ if(pedidas.count(p.first)) continue; pedidas.insert(p.first); v.push_back({p.first,p.second}); }
    g_rxThumbFila.clear();
    if(!v.empty()) FetchThumbsAsync(v);
}

// ------------------------------------------------------------- lateral -----
static void RxDrawSide(Color ab,Color white,Color gray){
    if(R_rxSide.right<=R_rxSide.left) return;
    const wchar_t* nomes[3]={L"Início",L"Descobrir",L"Sua biblioteca"};
    for(int i=0;i<3;i++){
        RECT r=R_rxNav[i]; if(r.right<=r.left) continue;
        bool sel=(i==0&&g_rxPag==RXP_INICIO)||(i==1&&g_rxPag==RXP_DESCOBRIR)||(i==2&&g_rxPag==RXP_LISTA);
        bool hot=UiHot(r);
        RectF b=RF(r);
        if(sel||hot){ Color bg=ToGdi(sel?UI().surfaceHi:UI().surface); DrawRoundRect(b,S(UI_R_CARD),&bg,nullptr); }
        RectF ic(b.X+S(10),b.Y+S(9),S(20),S(20));
        Color c=sel?white:gray;
        if(i==0) RxIconCasa(ic,c); else if(i==1) RxIconLupa(ic,c); else RxIconLista(ic,c);
        gfx::TextRect(nomes[i],RectF(b.X+S(40),b.Y,b.Width-S(48),b.Height),S(13),c,sel,gfx::Near,true,gfx::EllipsisChar);
    }
    if(R_rxNovaPl.right>R_rxNovaPl.left){
        RECT r=R_rxNovaPl; RectF b=RF(r);
        Color bg=ToGdi(UiHot(r)?UI().surfaceHi:UI().surface);
        DrawRoundRect(b,S(UI_R_PILL),&bg,nullptr);
        gfx::TextRect(L"+  NOVA PLAYLIST",b,S(11),UiHot(r)?white:gray,true,gfx::Center,true,gfx::EllipsisChar);
    }
    // lista: "Todas as músicas" e depois cada playlist
    for(size_t i=0;i<R_rxSidePl.size();i++){
        RECT r=R_rxSidePl[i]; RectF b=RF(r);
        // a lista vem ordenada pelo uso: g_rxSideOrdem diz qual playlist e cada linha
        int pl = i<g_rxSideOrdem.size()?g_rxSideOrdem[i]:-1;
        if(pl>=(int)g_playlists.size()) continue;
        bool sel = pl<0 ? (g_rxPag==RXP_LISTA&&g_view==0)
                        : (g_rxPag==RXP_LISTA&&g_view==2&&g_openPl==pl);
        if(sel||UiHot(r)){ Color bg=ToGdi(sel?UI().surfaceHi:UI().surface); DrawRoundRect(b,S(UI_R_CARD),&bg,nullptr); }
        float cv=b.Height-S(10);
        RectF art(b.X+S(6),b.Y+S(5),cv,cv);
        Color plate=ToGdi(UI().bg);
        if(pl<0){
            DrawRoundRect(art,S(4),&plate,nullptr);
            RxIconLista(RectF(art.X+S(3),art.Y+S(3),art.Width-S(6),art.Height-S(6)),ab);
        } else RxCapa(art,L"",g_playlists[(size_t)pl].coverPath,false,plate);
        float tx=art.X+cv+S(9), tw=b.Width-(tx-b.X)-S(8);
        std::wstring nome = pl<0?L"Todas as músicas":g_playlists[(size_t)pl].name;
        std::wstring sub;
        if(pl<0) sub=std::to_wstring(g_libCached?g_libTracks.size():g_tracks.size())+L" músicas";
        else sub=L"Playlist  ·  "+std::to_wstring(g_playlists[(size_t)pl].entries.size())+L" músicas";
        gfx::TextRect(nome,RectF(tx,b.Y+S(6),tw,S(17)),S(12),sel?ab:white,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(sub,RectF(tx,b.Y+S(23),tw,S(14)),S(10),gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
}

// ------------------------------------------------------- iconezinhos -------
static void RxIconLetra(const RectF& r,Color c){
    for(int i=0;i<4;i++){
        float y=r.Y+r.Height*(0.18f+i*0.22f);
        float w=r.Width*((i%2)?0.62f:0.86f);
        gfx::Line(r.X+r.Width*0.07f,y,r.X+r.Width*0.07f+w,y,1.7f,c);
    }
}
static void RxIconFila(const RectF& r,Color c){
    for(int i=0;i<3;i++){
        float y=r.Y+r.Height*(0.22f+i*0.26f);
        gfx::Line(r.X+r.Width*0.08f,y,r.X+r.Width*0.62f,y,1.7f,c);
    }
    gfx::Line(r.X+r.Width*0.74f,r.Y+r.Height*0.22f,r.X+r.Width*0.74f,r.Y+r.Height*0.78f,1.7f,c);
    gfx::Line(r.X+r.Width*0.74f,r.Y+r.Height*0.78f,r.X+r.Width*0.92f,r.Y+r.Height*0.50f,1.7f,c);
}
static void RxIconSaida(const RectF& r,Color c){   // caixinha de som / dispositivo
    gfx::StrokeRect(r.X+r.Width*0.20f,r.Y+r.Height*0.10f,r.Width*0.60f,r.Height*0.80f,1.7f,c);
    gfx::StrokeEllipse(r.X+r.Width*0.34f,r.Y+r.Height*0.44f,r.Width*0.32f,r.Height*0.32f,1.6f,c);
    gfx::FillEllipse(r.X+r.Width*0.46f,r.Y+r.Height*0.20f,r.Width*0.08f,r.Height*0.08f,c);
}
static void RxBotaoBarra(const RECT& r,void(*ico)(const RectF&,Color),bool on,Color ab,Color white,Color gray){
    if(r.right<=r.left) return;
    RectF b=RF(r);
    bool hot=UiHot(r);
    if(on||hot){ Color bg=ToGdi(on?UI().surfaceHi:UI().surface); DrawRoundRect(b,S(6),&bg,nullptr); }
    ico(RectF(b.X+S(6),b.Y+S(6),b.Width-S(12),b.Height-S(12)),on?ab:(hot?white:gray));
}

// --------------------------------------------------------- barra de baixo ---
static void RxDrawBar(int w,int h,Color ab,Color white,Color gray,Color navB,Color playP,Color playB){
    RectF bar=RF(R_rxBar);
    gfx::FillRect(bar,ToGdi(UI().bar));
    gfx::Line(0,bar.Y+.5f,(float)w,bar.Y+.5f,1,ToGdi(UI().border));
    const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
    // capa + titulo + artista
    {
        RectF art=RF(R_art);
        Color plate=ToGdi(UI().surface);
        if(g_cfg.artShape==L"cd") DrawCoverCircle(g_coverImg,art,ToGdi(UI().borderHi),1.2f,g_player.playing?g_rotation:0);
        else { DrawRoundRect(art,S(UI_R_CARD),&plate,nullptr); if(g_coverImg&&g_coverImg->ok) gfx::DrawImgCover(g_coverImg,art); }
        float tx=art.X+art.Width+S(12);
        float tw=std::min((float)S(240),(float)R_shuffle.left-tx-S(20));
        if(tw>S(60)){
            std::wstring title=ct?ct->title:L"Nada tocando";
            std::wstring artist=ct?ct->artist:L"—";
            RectF tr(tx,art.Y+S(4),tw,S(20));
            if(g_player.playing) DrawMarqueeText(title,S(13),true,tr,white,false,S(26),NowMs());
            else gfx::TextRect(title,tr,S(13),white,true,gfx::Near,false,gfx::EllipsisChar);
            gfx::TextRect(artist,RectF(tx,art.Y+S(26),tw,S(16)),S(11),gray,false,gfx::Near,false,gfx::EllipsisChar);
        }
    }
    // transporte
    gfx::TextRect(L"⇄",RF(R_shuffle),S(15),g_cfg.shuffle?ab:gray,false,gfx::Center,true);
    {RECT z=R_prev; RectF pv(z.left+S(4),z.top+S(7),z.right-z.left-S(8),z.bottom-z.top-S(14)); IconSkip(pv,navB,false);}
    gfx::FillEllipse(RF(R_play),playP);
    if(g_converting){ RectF pr=RF(R_play); gfx::Arc(pr.X-S(4),pr.Y-S(4),pr.Width+S(8),pr.Height+S(8),(float)(NowMs()%1000)*0.36f,100.f,3.f,ab); }
    {RECT z=R_play; RectF pl(z.left+S(11),z.top+S(11),z.right-z.left-S(22),z.bottom-z.top-S(22)); if(g_player.playing) IconPause(pl,playB); else IconPlay(pl,playB);}
    {RECT z=R_next; RectF nx(z.left+S(4),z.top+S(7),z.right-z.left-S(8),z.bottom-z.top-S(14)); IconSkip(nx,navB,true);}
    gfx::TextRect(L"⟳",RF(R_repeat),S(15),g_cfg.repeat?ab:gray,false,gfx::Center,true);
    // barra de tempo com os dois relogios
    {
        DWORD pos=(g_current>=0&&g_player.loaded)?g_player.GetPositionMs():0;
        DWORD len=(g_current>=0&&g_player.loaded)?g_player.GetLengthMs():0;
        float frac=len?std::min(1.f,(float)pos/len):0;
        RectF sk=RF(R_seek);
        float by=sk.Y+sk.Height/2.f-S(2);
        Color tb=ToGdi(UI().border);
        DrawRoundRect(RectF(sk.X,by,sk.Width,S(4)),2,&tb,nullptr);
        Color fill=UiHot(R_seek)?ab:ToGdi(UI().text);
        if(frac>0) DrawRoundRect(RectF(sk.X,by,sk.Width*frac,S(4)),2,&fill,nullptr);
        if(UiHot(R_seek)) gfx::FillCircle(sk.X+sk.Width*frac,by+S(2),S(6),fill);
        gfx::TextRect(FormatTime(pos),RectF(sk.X-S(50),sk.Y-S(1),S(44),S(16)),S(10),gray,false,gfx::Far,true);
        gfx::TextRect(FormatTime(len),RectF(sk.X+sk.Width+S(6),sk.Y-S(1),S(44),S(16)),S(10),gray,false,gfx::Near,true);
    }
    RxBotaoBarra(R_rxLetra,RxIconLetra,g_rxLetraOn,ab,white,gray);
    RxBotaoBarra(R_rxPainel,RxIconFila,g_rxPainelOn,ab,white,gray);
    RxBotaoBarra(R_rxSaida,RxIconSaida,!Player::OutDeviceName().empty(),ab,white,gray);
    // volume
    {
        RectF vt=RF(R_vol);
        Color vb=ToGdi(UI().border);
        DrawRoundRect(RectF(vt.X,vt.Y,vt.Width,vt.Height),2,&vb,nullptr);
        float vf=g_muted?0.f:(float)g_cfg.volume/100.f;
        Color vf2=UiHot(R_vol)?ab:ToGdi(UI().text);
        if(vf>0) DrawRoundRect(RectF(vt.X,vt.Y,vt.Width*vf,vt.Height),2,&vf2,nullptr);
        RectF ic=RF(R_volIcon);
        RxIconSom(RectF(ic.X+S(2),ic.Y+S(3),ic.Width-S(4),ic.Height-S(6)),UiHot(R_volIcon)?white:gray,g_muted);
    }
}

// ------------------------------------------------------- lista detalhada ----
// Linha da lista no estilo REMIX: número (ou equalizador quando está tocando,
// e play ao passar o mouse), capa, título/artista, de onde vem e a duração.
static void RxEqBarras(const RectF& r,Color c,bool animando){
    float bw=r.Width/5.f;
    for(int i=0;i<3;i++){
        float f=animando?(0.35f+0.65f*std::fabs(sinf((float)NowMs()/(220.f+i*70.f)+i))):0.5f;
        float h=r.Height*f;
        gfx::FillRect(r.X+i*bw*1.6f,r.Y+r.Height-h,bw,h,c);
    }
}
static void RxDrawLista(Color ab,Color white,Color gray){
    if(R_library.right<=R_library.left) return;
    float fT=S(12.5f), fS=S(10.5f);
    for(size_t vi=0;vi<g_visible.size();++vi){
        size_t i=(size_t)g_visible[vi];
        if(i>=R_cardRects.size()||i>=g_tracks.size()) continue;
        RECT rr=R_cardRects[i]; if(rr.right<=rr.left) continue;
        const Track& t=g_tracks[i];
        bool cur=((int)i==g_current), hot=UiHot(rr);
        RectF row=RF(rr);
        if(cur||hot){ Color rb=ToGdi(cur?UI().surfaceHi:UI().surface); DrawRoundRect(row,S(UI_R_CARD),&rb,nullptr); }
        if(cur){   // fiozinho da cor do tema na borda esquerda
            Color eb=ab; DrawRoundRect(RectF(row.X,row.Y+S(6),S(3),row.Height-S(12)),S(2),&eb,nullptr);
        }
        float x=row.X+S(14);
        {   // numero / equalizador / play
            RectF n(x,row.Y,S(26),row.Height);
            if(hot){ IconPlay(RectF(n.X+S(6),n.Y+row.Height/2-S(7),S(13),S(14)),white); }
            else if(cur) RxEqBarras(RectF(n.X+S(6),n.Y+row.Height/2-S(7),S(14),S(14)),ab,g_player.playing);
            else gfx::TextRect(std::to_wstring(vi+1),n,fS,gray,false,gfx::Center,true);
            x=n.X+n.Width+S(8);
        }
        float cv=row.Height-S(14);
        RectF art(x,row.Y+S(7),cv,cv);
        Color plate=ToGdi(UI().bg);
        if(g_cfg.artShape==L"cd") DrawCoverCircle(GetThumb(t.coverPath),art,ToGdi(UI().borderHi),1.f,cur&&g_player.playing?g_rotation:0);
        else { DrawRoundRect(art,S(4),&plate,nullptr); Img* im=GetThumb(t.coverPath); if(im) gfx::DrawImgCover(im,art); }
        x=art.X+cv+S(12);
        // colunas da direita: duração e, se couber, de onde vem
        float dirW=S(56), fonteW=row.Width>S(560)?S(120):0;
        float txW=row.Width-(x-row.X)-dirW-fonteW-S(20);
        if(txW<S(80)) txW=S(80);
        gfx::TextRect(t.title,RectF(x,row.Y+S(9),txW,S(19)),fT,cur?ab:white,true,gfx::Near,false,gfx::EllipsisChar);
        std::wstring sub=t.artist.empty()?L"Artista desconhecido":t.artist;
        gfx::TextRect(sub,RectF(x,row.Y+S(27),txW,S(16)),fS,gray,false,gfx::Near,false,gfx::EllipsisChar);
        if(fonteW>0){
            bool online=IsOnlineTrack(t);
            std::wstring de;
            if(online){ const StreamInfo* c=FindSnap(t.path); de=c?StreamTagLabel(*c,true):std::wstring(L"Online"); }
            else de=std::filesystem::path(t.path).parent_path().filename().wstring();
            RectF fr(row.X+row.Width-dirW-fonteW-S(10),row.Y,fonteW,row.Height);
            if(online){ Color pill=ToGdi(UI().surfaceHi); DrawRoundRect(RectF(fr.X+S(6),row.Y+row.Height/2-S(9),fonteW-S(12),S(18)),S(9),&pill,nullptr); }
            gfx::TextRect(de,fr,S(9.5f),online?ab:gray,false,gfx::Center,true,gfx::EllipsisChar);
        }
        int dur=DurSegundos(t.path,t.durSec);
        std::wstring tempo=dur>0?FormatTime((DWORD)dur*1000):std::wstring(L"--:--");
        gfx::TextRect(tempo,RectF(row.X+row.Width-dirW-S(10),row.Y,dirW,row.Height),fS,gray,false,gfx::Far,true);
        DrawOrderArrows(i,ab,gray);
        DrawPickMark(i,rr);
    }
}

// ------------------------------------------------------------- letra -------
// Letra no meio da tela, linha por linha: a atual em destaque, as outras mais
// apagadas. Clicar numa linha pula a música para aquele ponto.
static void RxDrawLetra(Color ab,Color white,Color gray){
    RectF main=RF(R_rxMain);
    gfx::PushClip(main);
    const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
    gfx::Text(ct?ct->title:L"Nada tocando",main.X,main.Y+S(2),S(20),white,true);
    if(ct) gfx::Text(ct->artist,main.X,main.Y+S(28),S(11),gray);
    if(!ct){ gfx::PopClip(); return; }
    letra::Letra L=letra::Para(ct->path,ct->title,ct->artist,DurSegundos(ct->path,ct->durSec),RxAvisarNovidades);
    if(L.estado==0){ gfx::TextRect(L"Procurando a letra...",RectF(main.X,main.Y+S(90),main.Width,S(24)),S(13),gray,false,gfx::Near,true); gfx::PopClip(); return; }
    if(L.estado==2){
        gfx::TextRect(L"Não achei a letra desta música.",RectF(main.X,main.Y+S(90),main.Width,S(24)),S(14),white,true,gfx::Near,true);
        gfx::TextRect(L"O Remix procura no LRCLIB pelo nome, artista e duração. Corrigir o nome ou o artista da faixa costuma resolver.",
                      RectF(main.X,main.Y+S(118),main.Width-S(40),S(22)),S(11),gray,false,gfx::Near,true,gfx::EllipsisWord);
        gfx::PopClip(); return;
    }
    DWORD pos=g_player.loaded?g_player.GetPositionMs():0;
    int atual=letra::LinhaAtual(L,(int)pos);
    gfx::PushClip(RectF(main.X,main.Y+S(46),main.Width,main.Height-S(46)));   // as linhas nao passam por cima do titulo
    if(L.sync){
        for(size_t i=0;i<R_rxLetraLinhas.size()&&i<L.linhas.size();i++){
            RECT r=R_rxLetraLinhas[i]; RectF b=RF(r);
            if(b.Y+b.Height<main.Y||b.Y>main.Y+main.Height) continue;
            bool ehAtual=((int)i==atual);
            Color c=ehAtual?white:(UiHot(r)?white:ToGdi(UI().textFaint));
            if(ehAtual){ Color bg=ToGdi(UI().surface); DrawRoundRect(RectF(b.X-S(6),b.Y-S(2),b.Width+S(12),b.Height+S(4)),S(6),&bg,nullptr); }
            gfx::TextRect(L.linhas[i].txt.empty()?L"♪":L.linhas[i].txt,RectF(b.X,b.Y,b.Width,b.Height),ehAtual?S(17):S(14),c,ehAtual,gfx::Near,true,gfx::EllipsisWord);
        }
    } else {
        float y=main.Y+S(52)-(float)g_rxLetraScroll;
        std::wstring t=L.texto; size_t i=0;
        while(i<=t.size()&&y<main.Y+main.Height){
            size_t e=t.find(L'\n',i); if(e==std::wstring::npos) e=t.size();
            if(y>main.Y-S(20)) gfx::TextRect(t.substr(i,e-i),RectF(main.X,y,main.Width-S(20),S(22)),S(13),ToGdi(UI().textDim),false,gfx::Near,true,gfx::EllipsisWord);
            y+=S(24);
            if(e==t.size()) break;
            i=e+1;
        }
    }
    gfx::PopClip();
    if(!L.fonte.empty())
        gfx::TextRect(L.fonte+(L.sync?L"  ·  toque numa linha para pular":L"  ·  sem marcação de tempo"),
                      RectF(main.X,main.Y+main.Height-S(22),main.Width-S(16),S(18)),S(9.5f),ToGdi(UI().textFaint),false,gfx::Far,true);
    gfx::PopClip();
}

// --------------------------------------------------- painel da direita -----
// "Tocando agora": capa grande, o que vem depois na fila e um resumo do artista.
static void RxDrawPainel(Color ab,Color white,Color gray){
    if(R_rxNp.right<=R_rxNp.left) return;
    RectF p=RF(R_rxNp);
    Color bg=Argb(215,(BYTE)GetRValue(UI().bg),(BYTE)GetGValue(UI().bg),(BYTE)GetBValue(UI().bg));
    DrawRoundRect(p,S(10),&bg,nullptr);
    gfx::PushClip(p);
    const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
    float x=p.X+S(14), w=p.Width-S(28), y=p.Y+S(14);
    gfx::Text(L"TOCANDO AGORA",x,y,S(9.5f),ToGdi(UI().textFaint),true); y+=S(20);
    if(!ct){ gfx::TextRect(L"Nada tocando.",RectF(x,y+S(10),w,S(20)),S(12),gray,false,gfx::Near,true); gfx::PopClip(); return; }
    float aw=std::min(w,(float)S(190));   // capa menor: sobra espaço para o artista e a fila
    RectF art(x+(w-aw)/2,y,aw,aw);
    Color plate=ToGdi(UI().surface);
    if(g_cfg.artShape==L"cd") DrawCoverCircle(g_coverImg,art,ToGdi(UI().borderHi),1.2f,g_player.playing?g_rotation:0);
    else { DrawRoundRect(art,S(UI_R_CARD),&plate,nullptr); if(g_coverImg&&g_coverImg->ok) gfx::DrawImgCover(g_coverImg,art); }
    y=art.Y+art.Height+S(12);
    gfx::TextRect(ct->title,RectF(x,y,w,S(24)),S(16),white,true,gfx::Near,false,gfx::EllipsisChar); y+=S(24);
    gfx::TextRect(ct->artist.empty()?L"Artista desconhecido":ct->artist,RectF(x,y,w,S(18)),S(11),gray,false,gfx::Near,false,gfx::EllipsisChar); y+=S(24);
    {   // de onde vem + duração
        int dur=DurSegundos(ct->path,ct->durSec);
        std::wstring de=IsOnlineTrack(*ct)?std::wstring(L"Online"):std::filesystem::path(ct->path).parent_path().filename().wstring();
        gfx::TextRect(de+(dur>0?(L"  ·  "+FormatTime((DWORD)dur*1000)):std::wstring()),RectF(x,y,w,S(16)),S(10),ToGdi(UI().textFaint),false,gfx::Near,false,gfx::EllipsisChar);
        y+=S(24);
    }
    // a seguir
    {   // sobre o artista (foto, fãs e quem é parecido) — só com o online ligado
        desc::Artista ar=desc::SobreArtista(ct->artist,RxAvisarNovidades);
        if(ar.estado==1&&y+S(92)<p.Y+p.Height){
            gfx::Text(L"SOBRE O ARTISTA",x,y,S(9.5f),ToGdi(UI().textFaint),true); y+=S(18);
            float fh=S(64);
            RectF foto(x,y,fh,fh);
            RxCapa(foto,ar.capa,L"",true,ToGdi(UI().surface));
            float tx2=x+fh+S(10);
            gfx::TextRect(ar.nome,RectF(tx2,y+S(8),w-fh-S(10),S(19)),S(12),white,true,gfx::Near,false,gfx::EllipsisChar);
            gfx::TextRect(desc::FormataFas(ar.fas),RectF(tx2,y+S(27),w-fh-S(10),S(16)),S(10),gray,false,gfx::Near,false,gfx::EllipsisChar);
            if(!ar.parecidos.empty()){
                std::wstring par;
                for(size_t i=0;i<ar.parecidos.size()&&i<3;i++){ if(!par.empty()) par+=L", "; par+=ar.parecidos[i].titulo; }
                gfx::TextRect(L"Parecidos: "+par,RectF(tx2,y+S(43),w-fh-S(10),S(16)),S(9.5f),ToGdi(UI().textFaint),false,gfx::Near,false,gfx::EllipsisChar);
            }
            y+=fh+S(14);
        }
    }
    gfx::Text(L"A SEGUIR",x,y,S(9.5f),ToGdi(UI().textFaint),true); y+=S(18);
    int mostrados=0;
    for(int k=1;k<=4&&mostrados<4;k++){
        int idx=-1;
        if(g_cfg.shuffle&&!g_shufQueue.empty()&&g_shufPos>=0){ int q=g_shufPos+k; if(q<(int)g_shufQueue.size()) idx=g_shufQueue[(size_t)q]; }
        else if(g_current>=0&&g_current+k<(int)g_tracks.size()) idx=g_current+k;
        if(idx<0||idx>=(int)g_tracks.size()) break;
        const Track& t=g_tracks[(size_t)idx];
        float rh=S(38);
        if(y+rh>p.Y+p.Height-S(8)) break;
        RectF cvr(x,y+S(3),rh-S(8),rh-S(8));
        Color pl2=ToGdi(UI().surface); DrawRoundRect(cvr,S(4),&pl2,nullptr);
        Img* im=GetThumb(t.coverPath); if(im) gfx::DrawImgCover(im,cvr);
        gfx::TextRect(t.title,RectF(cvr.X+cvr.Width+S(8),y+S(2),w-cvr.Width-S(12),S(17)),S(11),white,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(t.artist,RectF(cvr.X+cvr.Width+S(8),y+S(18),w-cvr.Width-S(12),S(15)),S(9.5f),gray,false,gfx::Near,false,gfx::EllipsisChar);
        y+=rh; mostrados++;
    }
    gfx::PopClip();
}

// ------------------------------------------------- cabecalho da biblioteca --
static std::wstring RxNomeDaPagina(){
    if(!g_searchBuf.empty()) return L"Buscando";
    if(g_view==2){ const Playlist* p=OpenPlaylistPtr(); if(p) return p->name; return L"Playlist"; }
    if(g_view==1) return L"Playlists";
    return L"Todas as músicas";
}
static void RxDrawCabecalho(Color ab,Color white,Color gray){
    if(R_rxCab.right<=R_rxCab.left) return;
    RectF b=RF(R_rxCab);
    gfx::TextRect(RxNomeDaPagina(),RectF(b.X,b.Y,b.Width-S(250),S(32)),S(24),white,true,gfx::Near,true,gfx::EllipsisChar);
    size_t n=g_visible.size();
    std::wstring sub=std::to_wstring(n)+(n==1?L" música":L" músicas");
    if(!g_searchBuf.empty()) sub=n?(std::to_wstring(n)+(n==1?L" música aqui para \"":L" músicas aqui para \"")+g_searchBuf+L"\""):
                                  (L"Nada na sua biblioteca para \""+g_searchBuf+L"\"  ·  ENTER procura online");
    gfx::TextRect(sub,RectF(b.X,b.Y+S(38),b.Width-S(250),S(18)),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
    if(R_rxBuscarOn.right>R_rxBuscarOn.left&&!g_searchBuf.empty()){
        // digitando: dá para levar a mesma busca para fora da biblioteca
        struct B{ const RECT* r; const wchar_t* t; };
        B bs[3]={{&R_rxBuscarOn,L"MÚSICAS ONLINE"},{&R_rxBuscaPl,L"PLAYLISTS"},{&R_rxBuscaAl,L"ÁLBUNS"}};
        for(auto& b:bs){
            if(b.r->right<=b.r->left) continue;
            RectF rr=RF(*b.r);
            Color bg=ToGdi(UiHot(*b.r)?UI().surfaceHi:UI().surface);
            DrawRoundRect(rr,rr.Height/2.f,&bg,nullptr);
            gfx::TextRect(b.t,rr,S(10),UiHot(*b.r)?white:gray,true,gfx::Center,true,gfx::EllipsisChar);
        }
    }
    if(R_rxTocar.right>R_rxTocar.left){
        RectF t=RF(R_rxTocar);
        Color bg=ab; DrawRoundRect(t,t.Height/2.f,&bg,nullptr);
        gfx::TextRect(L"TOCAR",t,S(11),ToGdi(UI().bg),true,gfx::Center,true);
        RECT ra=R_rxAleat; RectF a=RF(ra);
        Color abg=ToGdi(UiHot(ra)?UI().surfaceHi:UI().surface);
        DrawRoundRect(a,a.Height/2.f,&abg,nullptr);
        gfx::TextRect(L"ALEATÓRIO",a,S(11),g_cfg.shuffle?ab:white,true,gfx::Center,true);
    }
}

// ---------------------------------------------------------- tela inicial ---
static std::wstring RxSaudacao(){
    time_t tt=time(nullptr); struct tm lt{};
#ifdef _WIN32
    localtime_s(&lt,&tt);
#else
    localtime_r(&tt,&lt);
#endif
    if(lt.tm_hour<6) return L"Boa madrugada";
    if(lt.tm_hour<12) return L"Bom dia";
    if(lt.tm_hour<18) return L"Boa tarde";
    return L"Boa noite";
}
static const wchar_t* RxNomeTipo(int kind){
    return kind==desc::K_ALBUM?L"Álbum":kind==desc::K_PLAYLIST?L"Playlist":kind==desc::K_ARTISTA?L"Artista":L"Música";
}
// Fundo da area principal: chapa escura por cima do papel de parede/capa borrada.
static void RxDrawMainBg(){
    if(R_rxMain.right<=R_rxMain.left) return;
    RectF m=RF(R_rxMain);
    Color bg=Argb(215,(BYTE)GetRValue(UI().bg),(BYTE)GetGValue(UI().bg),(BYTE)GetBValue(UI().bg));
    DrawRoundRect(RectF(m.X,m.Y-S(8),m.Width,m.Height+S(12)),S(10),&bg,nullptr);
}
// Campo de busca do topo (no REMIX a busca da biblioteca mora na barra de cima).
static void RxDrawBusca(Color ab,Color white,Color gray){
    if(R_searchBox.right<=R_searchBox.left) return;
    RectF b=RF(R_searchBox);
    Color bg=ToGdi(UI().surface), pn=ToGdi(g_searchFocus?UI().borderHi:UI().border);
    DrawRoundRect(b,b.Height/2.f,&bg,&pn,1.2f);
    RxIconLupa(RectF(b.X+S(10),b.Y+S(9),S(18),S(18)),g_searchFocus?white:gray);
    std::wstring txt=g_searchBuf.empty()?L"Buscar música, artista, playlist ou colar um link":g_searchBuf;
    gfx::TextRect(txt,RectF(b.X+S(36),b.Y,b.Width-S(70),b.Height),S(12),g_searchBuf.empty()?gray:white,false,gfx::Near,true,gfx::EllipsisChar);
    if(g_searchFocus&&(NowMs()/500)%2==0){
        float tw=gfx::TextWidth(g_searchBuf,S(12),false);
        gfx::Line(b.X+S(37)+tw,b.Y+S(10),b.X+S(37)+tw,b.Y+b.Height-S(10),1.4f,white);
    }
    if(!g_searchBuf.empty()) gfx::TextRect(L"✕",RF(R_searchClear),S(12),gray,false,gfx::Center,true);
    (void)ab;
}
static void RxDrawInicio(int w,int h,Color ab,Color white,Color gray){
    (void)w; (void)h;
    if(g_rxPag==RXP_INICIO) desc::Atualizar(false,RxAvisarNovidades);   // cache na hora; busca nova so quando envelhece
    RectF main=RF(R_rxMain);
    gfx::PushClip(main);
    std::wstring tituloPag = g_rxPag==RXP_INICIO?RxSaudacao():(g_rxGenero.empty()?std::wstring(L"Descobrir"):g_rxGeneroNome);
    gfx::Text(tituloPag,main.X,main.Y-(float)g_rxScroll+S(2),S(22),white,true);
    if(R_rxBuscarOn.right>R_rxBuscarOn.left){
        RECT rb=R_rxBuscarOn; RectF b=RF(rb);
        Color bg=ToGdi(UiHot(rb)?UI().surfaceHi:UI().surface);
        DrawRoundRect(b,b.Height/2.f,&bg,nullptr);
        gfx::TextRect(L"BUSCAR ONLINE",b,S(10),white,true,gfx::Center,true);
    }
    if(R_rxVoltar.right>R_rxVoltar.left){
        RECT rv=R_rxVoltar; RectF b=RF(rv);
        Color bg=ToGdi(UiHot(rv)?UI().surfaceHi:UI().surface);
        DrawRoundRect(b,b.Height/2.f,&bg,nullptr);
        gfx::TextRect(L"← GÊNEROS",b,S(10),white,true,gfx::Center,true);
    }
    desc::Home home; std::vector<desc::Item> generos;
    if(g_rxPag==RXP_INICIO) home=desc::Copia();
    else {
        desc::AtualizarGeneros(g_rxGenero,g_rxGeneroNome,RxAvisarNovidades);
        if(!g_rxGenero.empty()) desc::HomeDoGenero(g_rxGenero,home);
        else generos=desc::ListaGeneros();
    }
    if(g_rxHeroOk&&R_rxHero.right>R_rxHero.left){   // destaque do topo
        RectF b=RF(R_rxHero);
        if(b.Y<main.Y+main.Height&&b.Y+b.Height>main.Y){
            bool hot=UiHot(R_rxHero);
            Color bg=ToGdi(hot?UI().surfaceHi:UI().surface);
            DrawRoundRect(b,S(10),&bg,nullptr);
            float cv=b.Height;
            RxCapa(RectF(b.X,b.Y,cv,cv),g_rxHeroItem.capa,L"",g_rxHeroItem.kind==desc::K_ARTISTA,ToGdi(UI().bg));
            float tx=b.X+cv+S(22), tw=b.Width-(tx-b.X)-S(24);
            gfx::Text(g_rxHeroFileira.empty()?L"EM DESTAQUE":g_rxHeroFileira,tx,b.Y+S(18),S(9.5f),ab,true);
            gfx::TextRect(g_rxHeroItem.titulo,RectF(tx,b.Y+S(34),tw,S(34)),S(24),white,true,gfx::Near,false,gfx::EllipsisChar);
            gfx::TextRect(g_rxHeroItem.sub.empty()?RxNomeTipo(g_rxHeroItem.kind):g_rxHeroItem.sub,RectF(tx,b.Y+S(68),tw,S(20)),S(11),gray,false,gfx::Near,false,gfx::EllipsisChar);
            RectF bt=RF(R_rxHeroBtn);
            Color bb=ab; DrawRoundRect(bt,bt.Height/2.f,&bb,nullptr);
            gfx::TextRect(g_rxHeroItem.kind==desc::K_FAIXA?L"OUVIR AGORA":L"ABRIR",bt,S(11),ToGdi(UI().bg),true,gfx::Center,true);
        }
    }
    for(const RxAtalho& k:g_rxAtalhos){   // atalhos: capa pequena + nome, em duas colunas
        if(k.r.right<=k.r.left) continue;
        RectF b=RF(k.r);
        if(b.Y>main.Y+main.Height||b.Y+b.Height<main.Y) continue;
        bool hot=UiHot(k.r);
        Color bg=ToGdi(hot?UI().surfaceHi:UI().surface);
        DrawRoundRect(b,S(UI_R_CARD),&bg,nullptr);
        float cv=b.Height;
        RectF art(b.X,b.Y,cv,cv);
        RxCapa(art,L"",k.capa,false,ToGdi(UI().bg));
        float tx=art.X+cv+S(12), tw=b.Width-(tx-b.X)-S(52);
        gfx::TextRect(k.nome,RectF(tx,b.Y+S(10),tw,S(19)),S(12),white,true,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(k.sub,RectF(tx,b.Y+S(29),tw,S(16)),S(10),gray,false,gfx::Near,false,gfx::EllipsisChar);
        if(hot){   // play redondo no canto, como nos players novos
            float d=S(30), px=b.X+b.Width-d-S(10), py=b.Y+(b.Height-d)/2;
            gfx::FillEllipse(RectF(px,py,d,d),ab);
            IconPlay(RectF(px+d*.34f,py+d*.28f,d*.40f,d*.44f),ToGdi(UI().bg));
        }
    }
    for(size_t f=0;f<g_rxFilas.size();f++){
        const RxFila& fl=g_rxFilas[f];
        std::wstring titulo,nota;
        if(fl.fonte==-1){ titulo=L"Tocados recentemente"; nota=L"volte de onde parou"; }
        else if(fl.fonte==-2){ titulo=L"Explorar por gênero"; nota=L"as paradas de cada estilo"; }
        else if(fl.fonte==-3){ titulo=L"Sua mistura"; nota=L"do seu jeito, muda todo dia"; }
        else if(fl.fonte==-4){ titulo=L"Da sua biblioteca"; nota=L"o que já está no seu computador"; }
        else if((size_t)fl.fonte<home.fileiras.size()){ titulo=home.fileiras[(size_t)fl.fonte].titulo; nota=home.fileiras[(size_t)fl.fonte].nota; }
        RectF hr=RF(fl.head);
        if(hr.Y<main.Y+main.Height&&hr.Y+S(40)>main.Y){
            gfx::Text(titulo,hr.X,hr.Y,S(15),white,true);
            if(!nota.empty()){
                float tw=gfx::TextWidth(titulo,S(15),true);
                gfx::Text(nota,hr.X+tw+S(10),hr.Y+S(4),S(10),gray);
            }
            if(fl.verTudo.right>fl.verTudo.left)
                gfx::TextRect(RxFileiraAberta(fl.fonte)?L"VER MENOS":L"VER TUDO",RF(fl.verTudo),S(10),UiHot(fl.verTudo)?white:gray,true,gfx::Far,true);
        }
    }
    for(const RxCard& k:g_rxCards){
        RectF b=RF(k.r);
        if(b.Y>main.Y+main.Height||b.Y+b.Height<main.Y) continue;
        bool hot=UiHot(k.r);
        if(hot){ Color bg=ToGdi(UI().surface); DrawRoundRect(RectF(b.X-S(6),b.Y-S(6),b.Width+S(12),b.Height+S(12)),S(UI_R_CARD),&bg,nullptr); }
        float cv=b.Width;
        RectF art(b.X,b.Y,cv,cv);
        std::wstring nome,sub,capaUrl,capaLocal; bool redondo=false;
        const RxFila& fl=g_rxFilas[(size_t)k.fila<g_rxFilas.size()?(size_t)k.fila:0];
        if(fl.fonte==-2){
            if((size_t)k.item>=generos.size()) continue;
            const desc::Item& gI=generos[(size_t)k.item];
            RxCapa(art,gI.capa,L"",false,ToGdi(UI().surfaceHi));
            Color veu=Argb(120,0,0,0); DrawRoundRect(art,S(UI_R_CARD),&veu,nullptr);
            gfx::TextRect(gI.titulo,RectF(art.X+S(8),art.Y+art.Height-S(34),art.Width-S(16),S(26)),S(13),white,true,gfx::Near,true,gfx::EllipsisChar);
            continue;
        }
        if(fl.fonte==-1||fl.fonte==-3||fl.fonte==-4){
            const std::vector<int>& lista=(fl.fonte==-3)?g_rxMistura:(fl.fonte==-4?g_rxBiblio:g_rxRecentes);
            if((size_t)k.item>=lista.size()) continue;
            const std::vector<Track>& fonteT=(g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks;
            int ti=lista[(size_t)k.item]; if(ti<0||ti>=(int)fonteT.size()) continue;
            const Track* t=&fonteT[(size_t)ti];
            nome=t->title; sub=t->artist.empty()?L"Música":t->artist; capaLocal=t->coverPath;
            redondo=(g_cfg.artShape==L"cd");
        } else {
            if((size_t)fl.fonte>=home.fileiras.size()) continue;
            const desc::Shelf& s=home.fileiras[(size_t)fl.fonte];
            if((size_t)k.item>=s.itens.size()) continue;
            const desc::Item& it=s.itens[(size_t)k.item];
            nome=it.titulo; sub=it.sub.empty()?RxNomeTipo(it.kind):it.sub; capaUrl=it.capa;
            redondo=(it.kind==desc::K_ARTISTA);
        }
        RxCapa(art,capaUrl,capaLocal,redondo,ToGdi(UI().surfaceHi));
        if(hot){   // botao de play verde no canto da capa, como nos players novos
            RectF p=RF(k.play);
            gfx::FillEllipse(p,ab);
            IconPlay(RectF(p.X+p.Width*.32f,p.Y+p.Height*.26f,p.Width*.42f,p.Height*.48f),ToGdi(UI().bg));
        }
        gfx::TextRect(nome,RectF(b.X,b.Y+cv+S(8),b.Width,S(18)),S(12),white,true,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(sub,RectF(b.X,b.Y+cv+S(26),b.Width,S(16)),S(10),gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
    if(g_rxFilas.empty()){
        bool bus=(g_rxPag==RXP_INICIO?desc::Carregando():desc::CarregandoGeneros());
        std::wstring msg=bus?L"Buscando...":L"Sem novidades agora. Toque alguma música e volte aqui.";
        gfx::TextRect(msg,RectF(main.X,main.Y+S(90),main.Width,S(30)),S(13),gray,false,gfx::Near,true);
    }
    {   // barrinha de rolagem discreta (so quando tem mais coisa para baixo)
        int vis=(int)(main.Height);
        if(g_rxContentH>vis+4){
            float fr=(float)vis/(float)g_rxContentH, alt=std::max(S(30),main.Height*fr);
            float pos=main.Y+(main.Height-alt)*((float)g_rxScroll/(float)std::max(1,g_rxContentH-vis));
            Color sb=Argb(90,255,255,255);
            DrawRoundRect(RectF(main.X+main.Width-S(5),pos,S(3),alt),S(2),&sb,nullptr);
        }
    }
    gfx::PopClip();
    RxBaixarCapasPendentes();
}
