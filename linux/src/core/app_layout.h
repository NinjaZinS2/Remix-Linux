#pragma once
// Geometria compartilhada (Windows e Linux): BuildLayout / LayoutSettings.
// Usa g_winW/g_winH (area cliente) e so produz RECTs; quem desenha e cada
// plataforma. Os menus flutuantes (pasta, contexto, imagem, confirmacao) sao
// posicionados ao abrir, em app_core.h.
#include "app_core.h"

static void LayoutSettings(int w,int h){
    g_setSliders.clear(); g_setLabels.clear(); g_setSections.clear();
    R_themeCirclesSettings.clear(); R_runColors.clear(); R_playColors.clear(); R_navColors.clear();
    R_partColors.clear(); R_ledColors.clear();
    R_shortcutsBox={0,0,0,0};
    // TELA INTEIRA e escala FIXA: as configuracoes NAO usam S()/SI() (uiScale afeta so o player).
    int m=10;
    int pw=w-m*2, ph=h-m*2, px=m, py=m;
    R_settingsPanel={px,py,px+pw,py+ph};
    R_settingsClose={px+pw-58,py+12,px+pw-20,py+48};
    int top=py+66;
    int L=px+30, R=px+pw-30;
    bool wide=(R-L)>=880;
    auto slider=[&](int id,int x,int y,int wid,int minv,int maxv){
        g_setSliders.push_back({{x,y,x+wid,y+8},{x-10,y-16,x+wid+52,y+24},id,minv,maxv});
    };
    auto label=[&](int x,int y,const wchar_t*s){ g_setLabels.push_back({{x,y,x+400,y+18},s}); };
    auto colorRow=[&](std::vector<RECT>&v,int x,int y,int maxW)->int{
        v.push_back({x,y,x+22,y+22});
        size_t nt=g_themes.size();
        int total=1+(int)nt+15, gp=30, d=22;
        int cap=std::max(4,maxW/gp);
        int rows=(total+cap-1)/cap, idx=1;
        for(size_t i=0;i<nt;i++){int r=idx/cap,c=idx%cap;int xx=x+c*gp,yy=y+r*36;v.push_back({xx,yy,xx+d,yy+d});idx++;}
        for(int i=0;i<15;i++){int r=idx/cap,c=idx%cap;int xx=x+c*gp,yy=y+r*36;v.push_back({xx,yy,xx+d,yy+d});idx++;}
        return rows;
    };
    auto sect=[&](int x,int y,int cw,int hh,const wchar_t*t){ g_setSections.push_back({{x,y,x+cw,y+hh},t}); };
    // HOST (acesso pelo celular): ligar / painel, porta / PIN / nome, tunel / rede local, linha de estado
    auto sectHost=[&](int x,int& y,int cw){
        // ligar/painel; porta/PIN/nome; tunel/rede local; copiar links; online/QR com aceite/IPv6; linha de estado
        sect(x,y,cw,318,L"HOST (ACESSO PELO CELULAR)");
        int hw=(cw-50)/2, tw=(cw-60)/3;
        R_setHostOn={x+20,y+52,x+20+hw,y+86}; R_setHostPanel={x+30+hw,y+52,x+cw-20,y+86};
        R_setHostPort={x+20,y+96,x+20+tw,y+130}; R_setHostPin={x+30+tw,y+96,x+30+tw*2,y+130}; R_setHostName={x+40+tw*2,y+96,x+cw-20,y+130};
        R_setHostTunnel={x+20,y+140,x+20+hw,y+174}; R_setHostLan={x+30+hw,y+140,x+cw-20,y+174};
        R_setHostCopyTun={x+20,y+184,x+20+hw,y+218}; R_setHostCopyLan={x+30+hw,y+184,x+cw-20,y+218};
        R_setHostOnline={x+20,y+228,x+20+tw,y+262}; R_setHostQrConf={x+30+tw,y+228,x+30+tw*2,y+262}; R_setHostIpv6={x+40+tw*2,y+228,x+cw-20,y+262};
        y+=338;
    };
    // SOUNDPAD e DISCORD: abrem os paineis (o cabecalho esconde as pilulas quando nao cabem)
    auto sectExtras=[&](int x,int& y,int cw){
        sect(x,y,cw,120,L"SOUNDPAD E DISCORD (SÓ NO PC)");
        int hw=(cw-50)/2;
        R_setSpad={x+20,y+52,x+20+hw,y+86}; R_setDc={x+30+hw,y+52,x+cw-20,y+86};
        y+=140;
    };
    // ESTILO: classico / limpo / spotify+LED (3 botoes iguais + uma linha de explicacao)
    auto sectStyle=[&](int x,int& y,int cw){
        sect(x,y,cw,124,L"ESTILO DA INTERFACE");
        int bw=(cw-50)/UI_STYLE_COUNT;
        for(int k=0;k<UI_STYLE_COUNT;k++){ int bx=x+20+k*(bw+10); R_settingsStyle[k]={bx,y+52,bx+bw,y+88}; }   // dois botoes: CLÁSSICO e REMIX
        y+=144;
    };
    // secoes comuns as duas larguras
    auto sectLibrary=[&](int x,int& y,int cw){
        sect(x,y,cw,140,L"BIBLIOTECA");   // rotulo (y+50), botoes, caminho da pasta (y+110)
        int bw=(cw-60)/2;
        R_settingsDefault={x+20,y+68,x+20+bw,y+104}; R_settingsCustom={x+30+bw,y+68,x+cw-20,y+104};
        y+=160;
    };
    auto sectMode=[&](int x,int& y,int cw){
        sect(x,y,cw,160,L"MODO DE EXIBIÇÃO");   // botoes + velocidade do CD
        int mw3=(cw-60)/3;
        R_settingsModeSquare={x+20,y+58,x+20+mw3,y+94};
        R_settingsModeCd={x+30+mw3,y+58,x+30+mw3*2,y+94};
        R_settingsModeVertical={x+40+mw3*2,y+58,x+cw-20,y+94};
        slider(Z_CD_SPEED,x+20,y+122,cw-110,0,200);
        y+=180;
    };
    auto sectThemes=[&](int x,int& y,int cw){
        sect(x,y,cw,120,L"TEMAS");
        size_t nt=g_themes.size(); int cap=std::max(3,(cw-48)/60);
        for(size_t i=0;i<nt;i++){int r=(int)(i/cap),c=(int)(i%cap);int xx=x+24+c*60,yy=y+54+r*62;R_themeCirclesSettings.push_back({xx,yy,xx+46,yy+46});}
        int tr_=((int)nt+cap-1)/cap; y+=66+tr_*62;
    };
    auto sectEffects=[&](int x,int& y,int cw){
        sect(x,y,cw,96,L"EFEITOS");
        int ew=(cw-60)/3;   // tres chaves iguais
        R_setParticles={x+20,y+52,x+20+ew,y+86}; R_setGlitch={x+30+ew,y+52,x+30+ew*2,y+86};
        R_setPerf={x+40+ew*2,y+52,x+cw-20,y+86};
        y+=116;
    };
    auto sectColors=[&](int x,int& y,int cw){
        sect(x,y,cw,380,L"CORES E PARTÍCULAS");
        R_setAutoColor={x+20,y+50,x+320,y+86};
        slider(Z_PART_SPEED,x+20,y+124,cw-110,10,300);
        int c1=y+172;
        label(x+20,c1,L"Cor das partículas");
        int rp=colorRow(R_partColors,x+20,c1+26,cw-40);
        int c2=c1+26+rp*36+18;
        label(x+20,c2,L"Cor dos LEDs");
        int rl=colorRow(R_ledColors,x+20,c2+26,cw-40);
        int usedH=(c2+26+rl*36+18)-y;
        g_setSections.back().first.bottom=g_setSections.back().first.top+usedH;
        y+=usedH+20;
    };
    auto sectBackground=[&](int x,int& y,int cw){
        sect(x,y,cw,156,L"FUNDO");
        R_setWallChoose={x+20,y+52,x+270,y+88};
        R_setWallClear={x+280,y+52,x+430,y+88};
        R_setCoverBlur={x+20,y+102,x+cw-20,y+136};
        y+=176;
    };
    auto sectPlayback=[&](int x,int& y,int cw){
        sect(x,y,cw,226,L"REPRODUÇÃO");
        int bw=(cw-60)/3;
        R_setAutoplay={x+20,y+50,x+20+bw,y+84};
        R_setSort={x+30+bw,y+50,x+30+bw*2,y+84};
        R_setSortDir={x+40+bw*2,y+50,x+cw-20,y+84};
        // 2a linha (abaixo das duas linhas de ajuda): ao fechar / controles do sistema
        int hw=(cw-50)/2;
        R_setBgClose={x+20,y+134,x+20+hw,y+168};
        R_setSysMedia={x+30+hw,y+134,x+cw-20,y+168};
        // 3a linha: sair de vez (mesmo tocando em 2o plano)
        R_setQuit={x+20,y+178,x+20+hw,y+212};
        y+=246;
    };
    auto sectScales=[&](int x,int& y,int cw){
        sect(x,y,cw,330,L"TAMANHOS E ESCALAS");
        const int ids[5]={Z_UI_SCALE,Z_TITLE_SCALE,Z_ARTIST_SCALE,Z_VERTICAL_SCALE,Z_PLAYER_SIZE_SLIDER};
        const int mn[5]={70,80,80,70,60}, mx[5]={150,180,200,120,170};
        for(int k=0;k<5;k++) slider(ids[k],x+20,y+68+k*56,cw-110,mn[k],mx[k]);
        y+=352;
    };
    auto sectLed=[&](int x,int& y,int cw){
        sect(x,y,cw,204,L"CONTROLE DO LED");
        slider(Z_LED_BRIGHT,x+20,y+62,cw-110,0,100);
        slider(Z_LED_SPEED,x+20,y+118,cw-110,0,100);
        R_setEffect={x+20,y+154,x+260,y+186};
        y+=226;
    };
    auto sectButtons=[&](int x,int& y,int cw){
        sect(x,y,cw,260,L"BOTÕES");
        label(x+20,y+48,L"Botão play");
        int bp=colorRow(R_playColors,x+20,y+72,cw-40);
        int ny=y+72+bp*36+20;
        label(x+20,ny,L"Anterior / Próximo");
        int bn=colorRow(R_navColors,x+20,ny+26,cw-40);
        int usedH=(ny+26+bn*36+18)-y;
        g_setSections.back().first.bottom=g_setSections.back().first.top+usedH;
        y+=usedH+20;
    };
    auto sectRunner=[&](int x,int& y,int cw){
        sect(x,y,cw,220,L"LED CORREDOR (LINHA)");
        R_setRunnerToggle={x+20,y+54,x+150,y+86};
        slider(Z_RUNNER_SPEED,x+170,y+62,cw-250,0,200);
        label(x+20,y+108,L"Cor da linha");
        int rr=colorRow(R_runColors,x+20,y+132,cw-40);
        g_setSections.back().first.bottom=g_setSections.back().first.top+(132+rr*36+18);
        y+=132+rr*36+18+20;
    };
    auto sectEq=[&](int x,int& y,int cw){
        sect(x,y,cw,478,L"EQUALIZADOR");
        R_setEqOn={x+20,y+50,x+260,y+84};
        R_setEqReset={x+270,y+50,x+370,y+84};
        for(int k=0;k<8;k++) slider(Z_EQ_BASE+k,x+20,y+122+k*44,cw-110,-12,12);
        y+=498;
    };
    auto sectOnline=[&](int x,int& y,int cw){
        // linha de status (y+50), 2 linhas de botoes, caminho onde salvar, "conferir de novo"
        sect(x,y,cw,276,L"FONTES EXTERNAS");
        int bw=(cw-50)/2;
        R_setOnMode={x+20,y+74,x+20+bw,y+108}; R_setOnFmt={x+30+bw,y+74,x+cw-20,y+108};
        R_setOnSrc={x+20,y+116,x+20+bw,y+150}; R_setOnFolder={x+30+bw,y+116,x+cw-20,y+150};
        R_setOnRecheck={x+20,y+186,x+20+std::min(230,bw),y+218};
        R_setNovidades={x+20,y+226,x+20+bw,y+258};
        y+=296;
        // caminho da CLI que a pessoa configurou + procurar no sistema
        sect(x,y,cw,186,L"PROGRAMA DE LINHA DE COMANDO (OPCIONAL)");
        R_setCli={x+20,y+112,x+cw-20,y+146};
        R_setCliBuscar={x+20,y+154,x+20+std::min(260,bw),y+186};
        y+=206;
        // separador de partes (stems): programa configurado e quanta CPU ele pode usar
        sect(x,y,cw,224,L"SEPARAR EM PARTES / STEMS (OPCIONAL)");
        R_setSep={x+20,y+112,x+cw-20,y+146};
        R_setSepBuscar={x+20,y+154,x+20+std::min(260,bw),y+186};
        R_setStemsCpu={x+30+std::min(260,bw),y+154,x+cw-20,y+186};
        y+=244;
    };
    auto sectShortcuts=[&](int x,int& y,int cw){
        // uma linha por acao: rotulo | tecla (clique = capturar) | FOCO/GLOBAL; embaixo, restaurar + dicas
        int rows=HK_COUNT; int hh=54+rows*34+100;
        sect(x,y,cw,hh,L"ATALHOS");
        R_shortcutsBox={x,y,x+cw,y+hh};
        int ky=y+52; int keyW=std::min(190,(cw-40)/3), scW=84;
        for(int a=0;a<HK_COUNT;a++){ int ry=ky+a*34; R_hkScope[a]={x+cw-20-scW,ry,x+cw-20,ry+28}; R_hkKey[a]={R_hkScope[a].left-10-keyW,ry,R_hkScope[a].left-10,ry+28}; }
        int by=ky+rows*34+12; R_hkReset={x+20,by,x+220,by+34};
        y+=hh+20;
    };
    if(wide){
        int colW=(R-L-40)/2, xr=L+colW+40;
        int y=top;
        sectStyle(L,y,colW); sectHost(L,y,colW); sectExtras(L,y,colW); sectLibrary(L,y,colW); sectMode(L,y,colW); sectThemes(L,y,colW); sectEffects(L,y,colW);
        sectColors(L,y,colW); sectBackground(L,y,colW); sectPlayback(L,y,colW); sectShortcuts(L,y,colW);
        int ry=top;
        sectOnline(xr,ry,colW); sectScales(xr,ry,colW); sectLed(xr,ry,colW); sectButtons(xr,ry,colW); sectRunner(xr,ry,colW); sectEq(xr,ry,colW);
        g_setContentH=(y>ry?y:ry)-py;
    } else {
        int cw=R-L;
        int cy=top;
        sectStyle(L,cy,cw); sectHost(L,cy,cw); sectExtras(L,cy,cw); sectLibrary(L,cy,cw); sectOnline(L,cy,cw); sectMode(L,cy,cw); sectThemes(L,cy,cw); sectEffects(L,cy,cw);
        sectColors(L,cy,cw); sectScales(L,cy,cw); sectLed(L,cy,cw); sectButtons(L,cy,cw); sectRunner(L,cy,cw);
        sectBackground(L,cy,cw); sectPlayback(L,cy,cw); sectEq(L,cy,cw); sectShortcuts(L,cy,cw);
        g_setContentH=cy-py;
    }
    if(g_setScroll>g_setContentH-(ph-76)) g_setScroll=std::max(0,g_setContentH-(ph-76));
    if(g_setScroll<0) g_setScroll=0;
}

// Area da biblioteca (barra de abas + grade/lista/cards). Usada pelos dois estilos: o
// CLASSICO passa a area a direita do painel do player; o REMIX passa a area principal.
// abas = false (estilo REMIX): quem navega e a barra lateral e a busca fica no topo,
// entao a barra so aparece quando tem algo proprio da tela (marcar musicas, playlist aberta).
static void BuildLibraryArea(int gx,int gtop,int gright,int gbottom,bool abas=true){
    int gw=gright-gx, h=gbottom, w=g_winW;
    (void)w;
    bool manual=(g_cfg.sortMode==L"manual");
    if(gw<SI(280)){ R_library={0,0,0,0}; return; }

    // barra da biblioteca: abas MUSICAS/PLAYLISTS (ou voltar + nome da playlist) e a busca
    bool temBarra=abas||(g_pickMode&&g_view==0)||g_view==2||g_view==1;
    int top=gtop, barH=temBarra?SI(40):0;
    R_libBar=temBarra?RECT{gx,top,gright,top+barH}:RECT{0,0,0,0};
    int by0=top+SI(4), by1=top+barH-SI(4);
    int sx=gx;
    if(g_pickMode&&g_view==0){   // marcando musicas da biblioteca para uma playlist
        int dw=std::min((int)S(190),gw/3), cw2=std::min((int)S(120),gw/4);
        R_pickDone={gx,by0,gx+dw,by1}; R_pickCancel={R_pickDone.right+SI(8),by0,R_pickDone.right+SI(8)+cw2,by1};
        sx=(int)R_pickCancel.right+SI(12);
    } else if(g_view==2){        // playlist aberta: voltar, + ADICIONAR, busca e (se tiver online) o modo
        int bw=std::min((int)S(140),gw/4), aw=std::min((int)S(130),gw/4);
        R_plBack={gx,by0,gx+bw,by1}; R_plAdd={R_plBack.right+SI(8),by0,R_plBack.right+SI(8)+aw,by1};
        sx=(int)R_plAdd.right+SI(12);
        bool hasOnline=false;
        if(const Playlist* op=OpenPlaylistPtr()){ hasOnline=!op->link.empty(); for(auto& e:op->entries) if(!e.url.empty()){ hasOnline=true; break; } }
        if(hasOnline&&gright-sx>=SI(320)){ int mw=(int)S(124); R_plMode={gright-mw,by0,gright,by1}; }
    } else if(abas){             // abas MUSICAS / PLAYLISTS / ONLINE (encolhem se faltar espaco)
        int tabW=(int)S(104), onW=(int)S(92), gap=SI(8);
        int need=tabW*2+onW+gap*2;
        if(need>gw-SI(60)){ float k=(float)std::max(SI(150),gw-SI(60))/(float)need; tabW=(int)(tabW*k); onW=(int)(onW*k); }
        R_tabTracks={gx,by0,gx+tabW,by1}; R_tabPlaylists={R_tabTracks.right+gap,by0,R_tabTracks.right+gap+tabW,by1};
        R_tabOnline={R_tabPlaylists.right+gap,by0,R_tabPlaylists.right+gap+onW,by1};
        sx=(int)R_tabOnline.right+SI(12);
    }
    int sEnd=(R_plMode.right>R_plMode.left)?(int)R_plMode.left-SI(8):gright;
    if(g_view==1){ R_plNew={std::max(sx,gright-(int)S(180)),by0,gright,by1}; }
    else if(abas&&sEnd-sx>=SI(140)){ R_searchBox={sx,by0,sEnd,by1}; R_searchClear={R_searchBox.right-SI(34),by0,R_searchBox.right,by1}; }
    int gridTop=top+barH+(temBarra?SI(8):0);
    R_library={gx,gridTop,gright,gbottom};
    int libH=R_library.bottom-R_library.top;
    if(g_view==1){
        // cards de playlists: "Todas as musicas" + cada playlist + "nova"
        int cols=gw>=SI(760)?3:(gw>=SI(500)?2:1); g_gridCols=cols;
        int gapX=SI(20),gapY=SI(18),cardH=SI(190); int cardW=(gw-(cols-1)*gapX)/cols;
        int n=(int)g_playlists.size()+2;
        g_contentH=((n+cols-1)/cols)*(cardH+gapY);
        g_listScroll=std::max(0,std::min(g_listScroll,std::max(0,g_contentH-libH)));
        for(int k=0;k<n;k++){
            int row=k/cols,col=k%cols; int x=gx+col*(cardW+gapX),y=gridTop+row*(cardH+gapY)-g_listScroll;
            if(y>gbottom||y+cardH<gridTop-SI(40)){ R_plCards.push_back({0,0,0,0}); R_plPlay.push_back({0,0,0,0}); R_plShuf.push_back({0,0,0,0}); R_plDcBtns.push_back({0,0,0,0}); continue; }
            R_plCards.push_back({x,y,x+cardW,y+cardH});
            // ▶ DC em cima do quadrado da capa (so playlists de verdade; "Todas as musicas" e "nova" nao)
            if(k>0&&k<n-1){ int cv=SI(96), ax=x+SI(16), ay=y+SI(16); R_plDcBtns.push_back({ax+SI(6),ay+cv-SI(30),ax+cv-SI(6),ay+cv-SI(6)}); } else R_plDcBtns.push_back({0,0,0,0});
            if(k==n-1){ R_plPlay.push_back({0,0,0,0}); R_plShuf.push_back({0,0,0,0}); }
            else { int bw2=(cardW-SI(40))/2; R_plPlay.push_back({x+SI(16),y+cardH-SI(50),x+SI(16)+bw2,y+cardH-SI(16)}); R_plShuf.push_back({x+SI(24)+bw2,y+cardH-SI(50),x+cardW-SI(16),y+cardH-SI(16)}); }
        }
    } else {
        R_cardRects.assign(g_tracks.size(),RECT{0,0,0,0}); R_cardCoverButtons.assign(g_tracks.size(),RECT{0,0,0,0}); R_cardSeekRects.assign(g_tracks.size(),RECT{0,0,0,0}); R_cardPlayBtns.assign(g_tracks.size(),RECT{0,0,0,0}); R_cardDcBtns.assign(g_tracks.size(),RECT{0,0,0,0});
        R_rowUp.assign(g_tracks.size(),RECT{0,0,0,0}); R_rowDown.assign(g_tracks.size(),RECT{0,0,0,0});
        if(g_cfg.listMode!=0){
            int rowH=RxOn()?SI(58):SI(64);
            g_gridCols=1; g_contentH=(int)g_visible.size()*rowH;
            g_listScroll=std::max(0,std::min(g_listScroll,std::max(0,g_contentH-libH)));
            for(size_t vi=0;vi<g_visible.size();++vi){
                size_t i=(size_t)g_visible[vi];
                int y=gridTop+(int)vi*rowH-g_listScroll;
                if(y>gbottom||y+rowH<gridTop) continue;
                RECT rr={gx,y,R_library.right,y+rowH-SI(8)};
                R_cardRects[i]=rr;
                if(manual){ R_rowUp[i]={rr.right-SI(74),rr.top+SI(6),rr.right-SI(44),rr.top+SI(28)}; R_rowDown[i]={rr.right-SI(74),rr.top+SI(30),rr.right-SI(44),rr.top+SI(52)}; }
            }
        } else if(UiClassic()){
            int cols=gw>=SI(760)?3:(gw>=SI(500)?2:1);
            g_gridCols=cols;
            int gapX=SI(20),gapY=SI(18),cardH=SI(225);
            int cardW=(gw-(cols-1)*gapX)/cols;
            g_contentH=(int)(((int)g_visible.size()+cols-1)/cols)*(cardH+gapY);
            g_listScroll=std::max(0,std::min(g_listScroll,std::max(0,g_contentH-libH)));
            for(size_t vi=0;vi<g_visible.size();++vi){
                size_t i=(size_t)g_visible[vi];
                int row=(int)vi/cols,col=(int)vi%cols;
                int x=gx+col*(cardW+gapX),y=gridTop+row*(cardH+gapY)-g_listScroll;
                if(y>gbottom||y+cardH<gridTop-SI(40)) continue;
                R_cardRects[i]={x,y,x+cardW,y+cardH};
                R_cardCoverButtons[i]={x+cardW-SI(42),y+SI(16),x+cardW-SI(14),y+SI(44)};
                // so a linha da barra de seek (nao invade os botoes de transporte)
                R_cardSeekRects[i]={x+SI(180),y+SI(140),x+cardW-SI(22),y+SI(160)};
                if(manual){ R_rowUp[i]={x+cardW-SI(42),y+SI(50),x+cardW-SI(14),y+SI(72)}; R_rowDown[i]={x+cardW-SI(42),y+SI(76),x+cardW-SI(14),y+SI(98)}; }
            }
        } else {
            // Estilos novos: card em pe (capa grande, nome e artista embaixo). O transporte
            // fica so no player; sobre a capa aparece um botao de play quando o mouse passa.
            int gapX=SI(18),gapY=SI(20);
            int cols=std::max(1,(gw+gapX)/(SI(190)+gapX)); if(cols>6) cols=6;
            g_gridCols=cols;
            int cardW=(gw-(cols-1)*gapX)/cols;
            int cover=cardW-SI(20);
            int cardH=cover+SI(74);
            g_contentH=(int)(((int)g_visible.size()+cols-1)/cols)*(cardH+gapY);
            g_listScroll=std::max(0,std::min(g_listScroll,std::max(0,g_contentH-libH)));
            for(size_t vi=0;vi<g_visible.size();++vi){
                size_t i=(size_t)g_visible[vi];
                int row=(int)vi/cols,col=(int)vi%cols;
                int x=gx+col*(cardW+gapX),y=gridTop+row*(cardH+gapY)-g_listScroll;
                if(y>gbottom||y+cardH<gridTop-SI(40)) continue;
                R_cardRects[i]={x,y,x+cardW,y+cardH};
                R_cardCoverButtons[i]={x+cardW-SI(44),y+SI(14),x+cardW-SI(18),y+SI(40)};
                R_cardSeekRects[i]={0,0,0,0};   // sem seek no card: quem arrasta e a barra do player
                R_cardPlayBtns[i]={x+SI(10)+cover-SI(52),y+SI(10)+cover-SI(52),x+SI(10)+cover-SI(8),y+SI(10)+cover-SI(8)};
                R_cardDcBtns[i]={x+SI(18),y+SI(18),x+SI(18)+std::min(SI(104),cover-SI(56)),y+SI(46)};   // ▶ DISCORD em cima do quadrado da capa
                if(manual){ R_rowUp[i]={x+SI(10),y+cover-SI(40),x+SI(38),y+cover-SI(16)}; R_rowDown[i]={x+SI(42),y+cover-SI(40),x+SI(70),y+cover-SI(16)}; }
            }
        }
    }
    // lista vazia: texto + botao no meio da area da lista (antes o texto ficava por cima da barra)
    if(g_view!=1&&g_tracks.empty()){ int cx=(gx+gright)/2, bw2=std::min((int)S(300),gw-SI(20)); int ey=gridTop+std::min(SI(150),libH/3); R_onlineInfo={cx-bw2/2,ey,cx+bw2/2,ey+SI(40)}; }
}

// ------------------------------------------------------------ estilo REMIX --
// Moldura: barra de cima (marca + busca + atalhos), barra lateral (navegacao e
// biblioteca), area principal (inicio com fileiras OU a grade da biblioteca) e
// o player numa barra embaixo, da largura toda. Os RECTs do transporte sao os
// mesmos do estilo classico (R_play, R_seek...), so que colocados na barra de
// baixo: assim o clique e os atalhos continuam funcionando sem nenhuma mudanca.
static int RxFonteFileira(size_t i){ return i<g_rxFilas.size()?g_rxFilas[i].fonte:0; }
static bool RxFileiraAberta(int fonte){ for(int f:g_rxAbertas) if(f==fonte) return true; return false; }
// "Tocados recentemente": so o que ainda esta na biblioteca (musica apagada nao vira quadrado vazio).
// "Sua mistura": escolhe musicas da biblioteca pesando quanto voce ouve o artista,
// dando uma chance para o que voce nunca tocou e sorteando o desempate uma vez por
// dia (a mesma mistura durante o dia, outra amanha).
static void RxMontarMistura(const std::vector<Track>& fonte){
    g_rxMistura.clear(); g_rxMisturaChave.clear();
    if(fonte.size()<6) return;
    std::vector<std::pair<double,int>> v; v.reserve(fonte.size());
    for(size_t i=0;i<fonte.size();i++){
        const Track& t=fonte[i];
        double nota=desc::PesoArtista(t.artist);
        double vezes=desc::PesoFaixa(t.path);
        if(vezes<=0) nota+=0.8;                       // ainda nao ouviu: merece aparecer
        else nota+=std::min(2.0,vezes*0.3);
        nota+=desc::SorteioDoDia(t.path)*1.6;
        v.push_back({nota,(int)i});
    }
    std::sort(v.begin(),v.end(),[](const std::pair<double,int>& a,const std::pair<double,int>& b){ return a.first>b.first; });
    // no maximo 3 do mesmo artista (mistura com um artista so nao e mistura) e
    // sem repetir o que ja esta em "Tocados recentemente"
    std::map<std::wstring,int> porArtista;
    std::map<std::wstring,bool> jaEmRecentes;
    for(auto& ch:g_rxRecentesChave) jaEmRecentes[ch]=true;
    for(auto& x:v){
        if(g_rxMistura.size()>=18) break;
        const Track& t=fonte[(size_t)x.second];
        if(jaEmRecentes.count(t.path)) continue;
        std::wstring a=t.artist; size_t corte=a.find_first_of(L",&");   // "A, B feat. C" conta como A
        if(corte!=std::wstring::npos) a=a.substr(0,corte);
        while(!a.empty()&&a.back()==L' ') a.pop_back();
        for(auto& c:a) c=(wchar_t)towlower(c);
        if(++porArtista[a]>3) continue;
        g_rxMistura.push_back(x.second); g_rxMisturaChave.push_back(t.path);
    }
}
// Atalhos do topo do Início: o que você mais ouviu por último — as playlists que
// você mais abre e as músicas que mais repetiu, intercaladas.
static void RxMontarAtalhos(const std::vector<Track>& fonte){
    g_rxAtalhos.clear();
    std::vector<std::pair<double,int>> pls;
    for(size_t i=0;i<g_playlists.size();i++){
        double pe=desc::PesoPlaylist(g_playlists[i].slug);
        if(pe>0) pls.push_back({pe,(int)i});
    }
    std::sort(pls.begin(),pls.end(),[](const std::pair<double,int>& a,const std::pair<double,int>& b){ return a.first>b.first; });
    std::map<std::wstring,int> pos;
    for(size_t i=0;i<fonte.size();i++) pos[fonte[i].path]=(int)i;
    std::vector<std::pair<double,int>> fx;
    for(auto& ch:desc::MaisTocadas(24)){
        auto it=pos.find(ch); if(it==pos.end()) continue;
        fx.push_back({desc::PesoFaixa(ch),it->second});
    }
    size_t a=0,b=0;
    while(g_rxAtalhos.size()<8&&(a<pls.size()||b<fx.size())){
        if(a<pls.size()){
            const Playlist& p=g_playlists[(size_t)pls[a].second];
            RxAtalho k; k.tipo=1; k.idx=pls[a].second; k.nome=p.name;
            k.sub=std::to_wstring(p.entries.size())+(p.entries.size()==1?L" música":L" músicas");
            k.capa=p.coverPath;
            g_rxAtalhos.push_back(k); a++;
        }
        if(g_rxAtalhos.size()>=8) break;
        if(b<fx.size()){
            const Track& t=fonte[(size_t)fx[b].second];
            RxAtalho k; k.tipo=0; k.idx=fx[b].second; k.nome=t.title; k.sub=t.artist; k.capa=t.coverPath;
            g_rxAtalhos.push_back(k); b++;
        }
    }
}
static void RxMontarRecentes(){
    g_rxRecentes.clear(); g_rxRecentesChave.clear();
    const std::vector<Track>& fonte=(g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks;
    std::map<std::wstring,int> idx;
    for(size_t i=0;i<fonte.size();i++) idx[fonte[i].path]=(int)i;
    for(auto& ch:desc::Recentes(60)){
        auto it=idx.find(ch); if(it==idx.end()) continue;
        g_rxRecentes.push_back(it->second); g_rxRecentesChave.push_back(ch);
        if(g_rxRecentes.size()>=40) break;
    }
}

// "Da sua biblioteca": completa o Inicio com o que a pessoa ja tem (vale muito quando as
// novidades online estao desligadas ou nao ha internet). Agrupado por artista.
static void RxMontarBiblioteca(const std::vector<Track>& fonte){
    g_rxBiblio.clear(); g_rxBiblioChave.clear();
    if(fonte.empty()) return;
    std::map<std::wstring,bool> ja;
    for(auto& ch:g_rxRecentesChave) ja[ch]=true;
    for(auto& ch:g_rxMisturaChave) ja[ch]=true;
    std::vector<int> ord;
    for(size_t i=0;i<fonte.size();i++) if(!ja.count(fonte[i].path)) ord.push_back((int)i);
    std::sort(ord.begin(),ord.end(),[&](int a,int b){
        const Track& x=fonte[(size_t)a]; const Track& y=fonte[(size_t)b];
        int c=_wcsicmp(x.artist.c_str(),y.artist.c_str()); if(c) return c<0;
        return _wcsicmp(x.title.c_str(),y.title.c_str())<0;
    });
    for(int i:ord){ if(g_rxBiblio.size()>=18) break; g_rxBiblio.push_back(i); g_rxBiblioChave.push_back(fonte[(size_t)i].path); }
}

static void BuildLayoutRemix(int w,int h,int chrome){
    const int topH=SI(56), barH=SI(96), gap=SI(14);
    g_headerH=topH;
    R_rxTop={0,0,w,topH};
    // Largura da lateral: a pessoa arrasta a divisória (fica em SideW no config.ini).
    int sideW=0;
    if(w>=SI(840)){
        int pedido=SI(g_cfg.rxSideW);
        sideW=std::max(SI(170),std::min(pedido,std::min(SI(460),w/3)));
    }
    g_sideW=sideW;
    R_rxSideDrag = sideW? RECT{sideW-SI(4),topH,sideW+SI(5),h-barH} : RECT{0,0,0,0};
    // Janela estreita: sem lateral nao teria como navegar, entao volta a barra de
    // abas MUSICAS/PLAYLISTS/ONLINE da biblioteca.
    if(!sideW&&g_rxPag!=RXP_LISTA) g_rxPag=RXP_LISTA;
    // Digitou na busca: a tela vai para a lista, que e onde os resultados aparecem.
    if(!g_searchBuf.empty()&&g_rxPag!=RXP_LISTA) g_rxPag=RXP_LISTA;
    R_rxSide = sideW? RECT{0,topH,sideW,h-barH} : RECT{0,0,0,0};
    R_rxBar={0,h-barH,w,h};
    int mx = sideW? sideW+gap : gap;
    R_rxMain={mx,topH+SI(8),w-gap,h-barH-SI(6)};
    R_rxNp={0,0,0,0};
    if(g_rxPainelOn&&w>=SI(1100)){          // painel da direita: tocando agora + artista + fila
        int npw=std::min(SI(340),w/4);
        R_rxNp={w-gap-npw,topH+SI(8),w-gap,h-barH-SI(6)};
        R_rxMain.right=(int)R_rxNp.left-gap;
    }

    // ---- barra de cima: marca a esquerda, busca no meio, atalhos a direita
    int rx=w-SI(12)-chrome;
    R_gear={rx-SI(40),SI(9),rx,SI(47)}; rx=(int)R_gear.left-SI(8);
    R_listBtn={rx-SI(46),SI(9),rx,SI(47)}; rx=(int)R_listBtn.left-SI(10);
    int bx=(sideW?sideW:SI(112))+SI(12);
    int minBusca=bx+SI(150);                       // a busca tem prioridade: pilula so entra se sobrar espaco
    auto pilula=[&](RECT& r,float wid){ int x1=rx, x0=rx-(int)S(wid); if(x0<minBusca){ r={0,0,0,0}; return; } r={x0,SI(10),x1,SI(46)}; rx=x0-SI(8); };
    pilula(R_dcBtn,100); pilula(R_spadBtn,110); pilula(R_fxBtn,96); pilula(R_hostBtn,78);
    if(g_rxPag==RXP_LISTA){ pilula(R_folderBtn,g_view==2?160.f:100.f); pilula(R_sortBtn,160); }
    else { R_folderBtn={0,0,0,0}; R_sortBtn={0,0,0,0}; }
    R_shapeTgl={0,0,0,0}; R_autoTgl={0,0,0,0};
    {   // busca do topo: e a busca da biblioteca (o mesmo texto e o mesmo X).
        // Sem lateral a busca volta para a barra de abas, para nao aparecer duas vezes.
        int bw2=std::min((int)S(420),rx-SI(12)-bx);
        if(sideW&&bw2>=SI(150)){ R_searchBox={bx,SI(10),bx+bw2,SI(46)}; R_searchClear={R_searchBox.right-SI(34),SI(10),R_searchBox.right,SI(46)}; }
        else { R_searchBox={0,0,0,0}; R_searchClear={0,0,0,0}; }
    }
    // ---- barra lateral: Início / Descobrir / Sua biblioteca + playlists
    R_rxSidePl.clear(); g_rxSideOrdem.clear();
    R_rxNav[0]=R_rxNav[1]=R_rxNav[2]=RECT{0,0,0,0};
    R_rxNovaPl={0,0,0,0}; R_rxVerTodas={0,0,0,0};
    if(sideW){
        int x0=SI(10), x1=sideW-SI(10), y=topH+SI(12), ih=SI(38);
        for(int i=0;i<3;i++){ R_rxNav[i]={x0,y,x1,y+ih}; y+=ih+SI(2); }
        y+=SI(14);
        R_rxNovaPl={x0,y,x1,y+SI(34)}; y+=SI(34)+SI(10);
        // ordem da lista: "Todas as músicas" e depois as playlists que você mais usa.
        // Só recalcula quando a lista muda: senão ela se reordenaria a cada clique.
        std::wstring ass;
        for(auto& p:g_playlists){ ass+=p.slug; ass+=L"\x01"; }
        if(ass!=g_rxOrdemAss){
            g_rxOrdemAss=ass;
            std::vector<std::pair<double,int>> ord;
            for(size_t i=0;i<g_playlists.size();i++) ord.push_back({desc::PesoPlaylist(g_playlists[i].slug),(int)i});
            std::stable_sort(ord.begin(),ord.end(),[](const std::pair<double,int>& a,const std::pair<double,int>& b){ return a.first>b.first; });
            g_rxOrdemFixa.clear();
            for(auto& o:ord) g_rxOrdemFixa.push_back(o.second);
        }
        int fim=(int)R_rxSide.bottom-SI(8), ph=SI(46);
        g_rxSideOrdem.push_back(-1);
        for(int idx:g_rxOrdemFixa) if(idx>=0&&idx<(int)g_playlists.size()) g_rxSideOrdem.push_back(idx);
        for(size_t i=0;i<g_rxSideOrdem.size();i++){ if(y+ph>fim) break; R_rxSidePl.push_back({x0,y,x1,y+ph-SI(4)}); y+=ph; }
        g_rxSideOrdem.resize(R_rxSidePl.size());
    }
    // ---- barra de baixo: capa + titulo | transporte + seek | volume
    {
        int bt=(int)R_rxBar.top, cv=barH-SI(30);
        R_art={SI(14),bt+SI(15),SI(14)+cv,bt+SI(15)+cv};
        int cx=w/2, sw=std::min((int)S(520),std::max(SI(220),w-SI(560)));
        int ty=bt+SI(24);
        R_play={cx-SI(19),ty-SI(19),cx+SI(19),ty+SI(19)};
        R_prev={cx-SI(58)-SI(15),ty-SI(15),cx-SI(58)+SI(15),ty+SI(15)};
        R_next={cx+SI(58)-SI(15),ty-SI(15),cx+SI(58)+SI(15),ty+SI(15)};
        R_shuffle={cx-SI(104)-SI(14),ty-SI(14),cx-SI(104)+SI(14),ty+SI(14)};
        R_repeat={cx+SI(104)-SI(14),ty-SI(14),cx+SI(104)+SI(14),ty+SI(14)};
        int sy=bt+SI(62);
        R_seek={cx-sw/2,sy-SI(9),cx+sw/2,sy+SI(9)};
        R_wavePanel={0,0,0,0};
        int vx=w-SI(20)-chrome;
        R_vol={vx-SI(96),bt+barH/2-SI(3),vx,bt+barH/2+SI(3)};
        R_volIcon={(int)R_vol.left-SI(28),bt+barH/2-SI(12),(int)R_vol.left-SI(6),bt+barH/2+SI(12)};
        {   // botoes: letra, fila/painel e onde tocar. Se faltar espaco, a barra de
            // tempo encolhe um pouco antes de algum botao sumir (no Windows os botoes
            // de fechar/minimizar comem 72 px da direita).
            int d=SI(28), precisa=3*d+2*SI(6)+SI(16);
            int bx2=(int)R_volIcon.left-SI(10), by2=bt+barH/2-SI(14);
            if(bx2-precisa<(int)R_seek.right+SI(8)){
                int novo=std::max((int)R_seek.left+SI(120),bx2-precisa-SI(8));
                if(novo<(int)R_seek.right) R_seek.right=novo;
            }
            auto bt2=[&](RECT& r){ if(bx2-d<(int)R_seek.right+SI(8)){ r={0,0,0,0}; return; } r={bx2-d,by2,bx2,by2+d}; bx2=bx2-d-SI(6); };
            bt2(R_rxSaida); bt2(R_rxPainel); bt2(R_rxLetra);
        }
        R_playerPanel={0,0,0,0};
        R_runnerSlider={0,0,0,0};
        R_pencilPanel={0,0,0,0};
    }
    // ---- area principal
    g_rxCards.clear(); g_rxFilas.clear(); R_rxLetraLinhas.clear();
    R_rxCab={0,0,0,0}; R_rxTocar={0,0,0,0}; R_rxAleat={0,0,0,0}; R_rxVoltar={0,0,0,0}; R_rxBuscarOn={0,0,0,0};
    if(g_rxLetraOn){   // letra: uma linha por vez, clicável (pula para aquele ponto)
        R_library={0,0,0,0};
        const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
        if(ct){
            letra::Letra L=letra::Para(ct->path,ct->title,ct->artist,DurSegundos(ct->path,ct->durSec),RxAvisarNovidades);
            int lh=SI(34);
            if(L.sync&&GetTickCount64()-g_rxLetraMexeu>6000){   // segue a música sozinha (a menos que você acabou de rolar)
                int atual=letra::LinhaAtual(L,(int)(g_player.loaded?g_player.GetPositionMs():0));
                int visH=(int)(R_rxMain.bottom-R_rxMain.top)-SI(60);
                if(atual>=0) g_rxLetraScroll=std::max(0,atual*lh-visH/2+lh);
                else g_rxLetraScroll=0;
            }
            int y0=(int)R_rxMain.top+SI(52)-g_rxLetraScroll;
            for(size_t i=0;i<L.linhas.size()&&i<200;i++){
                int yy=y0+(int)i*lh;
                R_rxLetraLinhas.push_back({(int)R_rxMain.left+SI(10),yy,(int)R_rxMain.right-SI(10),yy+lh-SI(4)});
            }
        }
        return;
    }
    if(g_rxPag==RXP_LISTA){
        // cabecalho da pagina: nome da lista, quantas musicas e os botoes de tocar
        int cabH=SI(74);
        if(!sideW){ BuildLibraryArea((int)R_rxMain.left,(int)R_rxMain.top,(int)R_rxMain.right,(int)R_rxMain.bottom,true); return; }
        if(!g_pickMode&&R_rxMain.bottom-R_rxMain.top>cabH+SI(120)){
            R_rxCab={(int)R_rxMain.left,(int)R_rxMain.top+SI(6),(int)R_rxMain.right,(int)R_rxMain.top+SI(6)+cabH};
            int by=(int)R_rxCab.top+SI(26), bw2=(int)S(112);
            if(!g_searchBuf.empty()){
                // digitando: os botoes viram "procurar isso em outro lugar"
                int w3=(int)S(124);
                R_rxBuscaAl={(int)R_rxMain.right-w3,by,(int)R_rxMain.right,by+SI(34)};
                R_rxBuscaPl={(int)R_rxBuscaAl.left-w3-SI(8),by,(int)R_rxBuscaAl.left-SI(8),by+SI(34)};
                R_rxBuscarOn={(int)R_rxBuscaPl.left-w3-SI(8),by,(int)R_rxBuscaPl.left-SI(8),by+SI(34)};
                R_rxTocar={0,0,0,0}; R_rxAleat={0,0,0,0};
                BuildLibraryArea((int)R_rxMain.left,(int)R_rxCab.bottom+SI(4),(int)R_rxMain.right,(int)R_rxMain.bottom,false);
                return;
            }
            R_rxTocar={(int)R_rxMain.right-bw2*2-SI(8),by,(int)R_rxMain.right-bw2-SI(8),by+SI(34)};
            R_rxAleat={(int)R_rxMain.right-bw2,by,(int)R_rxMain.right,by+SI(34)};
            BuildLibraryArea((int)R_rxMain.left,(int)R_rxCab.bottom+SI(4),(int)R_rxMain.right,(int)R_rxMain.bottom,false);
        } else BuildLibraryArea((int)R_rxMain.left,(int)R_rxMain.top,(int)R_rxMain.right,(int)R_rxMain.bottom,false);
        return;
    }
    R_library={0,0,0,0};
    // Tela inicial: fileiras de cartoes. Cada fileira mostra o que couber numa
    // linha; "ver tudo" abre as outras linhas ali mesmo (sem rolagem horizontal,
    // que no mouse e no touch do celular so atrapalha).
    int mxL=(int)R_rxMain.left, mxR=(int)R_rxMain.right, mw=mxR-mxL;
    int gapX=SI(16), cardW=std::max(SI(120),std::min((int)S(168),(mw-gapX*4)/5));
    int cols=std::max(1,(mw+gapX)/(cardW+gapX));
    int cardH=cardW+SI(56);
    desc::Home home; bool comRecentes=false, comGeneros=false;
    bool comMistura=false, comBiblio=false;
    if(g_rxPag==RXP_INICIO){
        if(g_cfg.novidadesOnline){ desc::Atualizar(false,RxAvisarNovidades); home=desc::Copia(); }
        RxMontarRecentes(); comRecentes=!g_rxRecentes.empty();
        RxMontarMistura((g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks); comMistura=g_rxMistura.size()>=6;
        RxMontarAtalhos((g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks);
        RxMontarBiblioteca((g_libCached&&!g_libTracks.empty())?g_libTracks:g_tracks);
        comBiblio=!g_rxBiblio.empty();
    }
    else {
        // Descobrir: grade de generos e, dentro de um, as paradas dele
        if(g_cfg.novidadesOnline){
            desc::AtualizarGeneros(g_rxGenero,g_rxGeneroNome,RxAvisarNovidades);
            if(!g_rxGenero.empty()) desc::HomeDoGenero(g_rxGenero,home);
            else comGeneros=true;
        }
        int by=(int)R_rxMain.top+SI(8);
        R_rxBuscarOn={mxR-(int)S(150),by,mxR,by+SI(32)};
        if(!g_rxGenero.empty()) R_rxVoltar={(int)R_rxBuscarOn.left-(int)S(118),by,(int)R_rxBuscarOn.left-SI(8),by+SI(32)};
    }
    std::vector<desc::Item> generos = comGeneros?desc::ListaGeneros():std::vector<desc::Item>();
    // fileiras locais (negativas) vem antes das do descobrir
    std::vector<int> locais;
    if(comMistura) locais.push_back(-3);
    if(comRecentes) locais.push_back(-1);
    if(comBiblio) locais.push_back(-4);
    if(comGeneros) locais.push_back(-2);
    int nFil=(int)home.fileiras.size()+(int)locais.size();
    int y=(int)R_rxMain.top+SI(46)-g_rxScroll;        // espaco do titulo da pagina
    R_rxHero={0,0,0,0}; R_rxHeroBtn={0,0,0,0}; g_rxHeroOk=false;
    if(g_rxPag==RXP_INICIO&&!home.fileiras.empty()&&mw>=SI(520)){
        // destaque: o primeiro item de "dos artistas que você ouve" ou, na falta, o que está bombando
        const desc::Shelf* esc=nullptr;
        for(auto& s:home.fileiras) if(s.chave==L"lancamentos"){ esc=&s; break; }
        if(!esc) for(auto& s:home.fileiras) if(s.chave==L"chart-faixas"){ esc=&s; break; }
        if(!esc) esc=&home.fileiras[0];
        if(!esc->itens.empty()){
            g_rxHeroItem=esc->itens[0]; g_rxHeroOk=true; g_rxHeroFileira=esc->titulo;
            int hh=SI(150);
            R_rxHero={mxL,y,mxR,y+hh};
            R_rxHeroBtn={mxL+hh+SI(22),y+hh-SI(52),mxL+hh+SI(22)+(int)S(132),y+hh-SI(18)};
            y+=hh+SI(18);
        }
    }
    if(g_rxPag==RXP_INICIO&&!g_rxAtalhos.empty()){
        // grade de atalhos larga (capa pequena + nome), como os players novos fazem
        int acols=mw>=SI(900)?3:(mw>=SI(620)?2:1);
        int ah=SI(56), agx=SI(10), agy=SI(10);
        int aw=(mw-(acols-1)*agx)/acols;
        size_t n=std::min<size_t>(g_rxAtalhos.size(),(size_t)(acols*2));
        for(size_t i=0;i<g_rxAtalhos.size();i++){
            if(i>=n){ g_rxAtalhos[i].r={0,0,0,0}; continue; }
            int r=(int)i/acols, c=(int)i%acols;
            int ax=mxL+c*(aw+agx), ay=y+r*(ah+agy);
            g_rxAtalhos[i].r={ax,ay,ax+aw,ay+ah};
        }
        int linhas=((int)n+acols-1)/acols;
        y+=linhas*(ah+agy)+SI(14);
    } else for(auto& k:g_rxAtalhos) k.r={0,0,0,0};
    for(int f=0; f<nFil; f++){
        int fonte = f<(int)locais.size() ? locais[(size_t)f] : f-(int)locais.size();
        int total = fonte==-1 ? (int)g_rxRecentes.size()
                  : fonte==-2 ? (int)generos.size()
                  : fonte==-3 ? (int)g_rxMistura.size()
                  : fonte==-4 ? (int)g_rxBiblio.size()
                              : (int)home.fileiras[(size_t)fonte].itens.size();
        if(total<=0) continue;
        bool todos=(fonte==-2);                       // generos: sempre a grade inteira
        int altura = todos?cardW:cardH;               // o quadrado do genero nao tem texto embaixo
        int linhas = (todos||RxFileiraAberta(fonte)) ? (total+cols-1)/cols : 1;
        int mostra = std::min(total,linhas*cols);
        RxFila fl; fl.fonte=fonte;
        fl.head={mxL,y,mxR,y+SI(30)};
        if(total>cols&&!todos) fl.verTudo={mxR-(int)S(110)-SI(10),y,mxR-SI(10),y+SI(28)};   // deixa a barrinha de rolagem livre
        g_rxFilas.push_back(fl);
        int fi=(int)g_rxFilas.size()-1;   // fileira vazia nao entra: o cartao guarda a posicao real
        int cy=y+SI(38);
        for(int i=0;i<mostra;i++){
            int r=i/cols, c=i%cols;
            int cxp=mxL+c*(cardW+gapX), cyp=cy+r*(altura+SI(14));
            if(cyp>(int)R_rxMain.bottom||cyp+altura<(int)R_rxMain.top){ continue; }
            RxCard k; k.fila=fi; k.item=i;
            k.r={cxp,cyp,cxp+cardW,cyp+altura};
            k.play={cxp+cardW-SI(46),cyp+cardW-SI(46),cxp+cardW-SI(10),cyp+cardW-SI(10)};
            g_rxCards.push_back(k);
        }
        y=cy+linhas*(altura+SI(14))+SI(10);
    }
    g_rxContentH=y+g_rxScroll-(int)R_rxMain.top+SI(20);
    int maxSc=std::max(0,g_rxContentH-((int)R_rxMain.bottom-(int)R_rxMain.top));
    if(g_rxScroll>maxSc) g_rxScroll=maxSc;
    if(g_rxScroll<0) g_rxScroll=0;
}

static void BuildLayout(){
    int w=g_winW,h=g_winH;
    R_titlebar={0,0,w,44}; R_close={0,0,0,0}; R_min={0,0,0,0};
    R_wavePanel={0,0,0,0}; R_shapeTgl={0,0,0,0}; R_listBtn={0,0,0,0}; R_autoTgl={0,0,0,0}; R_sortBtn={0,0,0,0}; R_folderBtn={0,0,0,0}; R_hostBtn={0,0,0,0}; R_fxBtn={0,0,0,0}; R_spadBtn={0,0,0,0}; R_dcBtn={0,0,0,0}; R_volIcon={0,0,0,0};
    R_cardRects.clear(); R_cardCoverButtons.clear(); R_cardSeekRects.clear(); R_cardPlayBtns.clear(); R_cardDcBtns.clear(); R_plDcBtns.clear(); R_themeCircles.clear();
    g_headerH=0; g_sideW=0;
    R_rowUp.clear(); R_rowDown.clear();
    R_libBar=R_tabTracks=R_tabPlaylists=R_searchBox=R_searchClear=R_plBack=R_plNew={0,0,0,0};
    R_tabOnline=R_plAdd=R_plMode=R_pickDone=R_pickCancel=R_onlineInfo={0,0,0,0};
    R_plCards.clear(); R_plPlay.clear(); R_plShuf.clear();
    g_visible.clear(); for(size_t i=0;i<g_tracks.size();++i) if(TrackMatchesSearch(g_tracks[i])) g_visible.push_back((int)i);
    // Buscando: o que melhor casa vem primeiro e, no empate, o que voce mais ouve
    // (a ordem normal da lista nao muda: isso vale so enquanto tem texto na busca).
    if(!g_searchBuf.empty()&&g_visible.size()>1){
        std::wstring q=FoldText(g_searchBuf);
        auto grau=[&](const Track& t){
            std::wstring ti=FoldText(t.title), ar=FoldText(t.artist);
            if(ti.rfind(q,0)==0) return 0;
            if(ar.rfind(q,0)==0) return 1;
            if(ti.find(q)!=std::wstring::npos) return 2;
            if(ar.find(q)!=std::wstring::npos) return 3;
            return 4;                                  // so casou pelo nome do arquivo
        };
        std::vector<std::pair<int,double>> nota(g_tracks.size());
        for(int i:g_visible) nota[(size_t)i]={grau(g_tracks[(size_t)i]),desc::PesoArtista(g_tracks[(size_t)i].artist)+desc::PesoFaixa(g_tracks[(size_t)i].path)*0.5};
        std::stable_sort(g_visible.begin(),g_visible.end(),[&](int a,int b){
            if(nota[(size_t)a].first!=nota[(size_t)b].first) return nota[(size_t)a].first<nota[(size_t)b].first;
            return nota[(size_t)a].second>nota[(size_t)b].second;
        });
    }
    g_gridCols=0; g_contentH=0;
    bool manual=(g_cfg.sortMode==L"manual");
    int chrome=g_customChrome?SI(72):0;   // espaco dos botoes fechar/minimizar (Windows)
    if(g_customChrome){ R_close={w-SI(40),SI(10),w-SI(10),SI(40)}; R_min={w-SI(74),SI(10),w-SI(44),SI(40)}; }
    if(g_cfg.displayMode==L"vertical"){
        int cw=std::min(430,std::max(320,w-24)); int left=(w-cw)/2; int top=56;
        int art=std::min(205,std::max(150,(int)(h*.30f*g_cfg.verticalScale/100.0f)));
        R_art={left+(cw-art)/2,top,left+(cw+art)/2,top+art};
        R_verticalCoverButton={R_art.right-34,R_art.top+8,R_art.right-8,R_art.top+34};
        R_seek={left+26,R_art.bottom+88,left+cw-26,R_art.bottom+112};
        int B=R_art.bottom+112;
        R_prev={left+84,B+36,left+124,B+76}; R_play={left+cw/2-32,B+30,left+cw/2+32,B+94};
        R_next={left+cw-124,B+36,left+cw-84,B+76}; R_shuffle={left+34,B+44,left+66,B+70}; R_repeat={left+cw-66,B+44,left+cw-34,B+70};
        R_volIcon={left+34,B+101,left+58,B+125};
        R_vol={left+70,B+110,left+cw-70,B+116};
        R_heart={left+12,(LONG)S(58),left+44,(LONG)S(90)};
        R_gear={w-56-chrome,(LONG)S(56),w-16-chrome,(LONG)S(94)};
        if(g_customChrome){ R_gear={w-56-chrome+SI(10),SI(10),w-16-chrome+SI(10),SI(46)}; }
        // Cabecalho do vertical: [AUTO] [QUAD|CD] ... [engrenagem/fechar]. Nada se
        // sobrepoe: o toggle e centralizado quando cabe, senao encosta no AUTO e
        // encolhe; sem espaco nenhum, o AUTO some (fica nas configuracoes).
        {
            int rightLimit=g_customChrome?(int)R_gear.left-SI(6):w-SI(10);
            int autoW=(int)S(62), tglW=(int)S(190);
            R_autoTgl={SI(10),(LONG)S(8),SI(10)+autoW,(LONG)S(46)};
            int tx=(w-tglW)/2, minTx=(int)R_autoTgl.right+SI(8);
            if(tx<minTx) tx=minTx;
            if(tx+tglW>rightLimit) tglW=rightLimit-tx;
            if(tglW<(int)S(120)){
                R_autoTgl={0,0,0,0};
                tglW=std::min((int)S(190),rightLimit-SI(10)); tx=std::max(SI(10),(w-tglW)/2);
                if(tx+tglW>rightLimit) tx=std::max(SI(10),rightLimit-tglW);
            }
            R_shapeTgl={tx,(LONG)S(6),tx+tglW,(LONG)S(48)};
        }
        R_library={0,0,0,0}; R_modeSquare=R_modeCd=R_modeVertical={0,0,0,0};
        g_listScroll=0;
    } else if(RxOn()){
        R_modeSquare=R_modeCd=R_modeVertical={0,0,0,0}; R_themeCircles.clear();
        BuildLayoutRemix(w,h,chrome);
    } else {
        int headerH=SI(56), margin=SI(24);
        g_headerH=headerH;
        R_heart={w-SI(96)-chrome,SI(12),w-SI(60)-chrome,SI(50)}; R_gear={w-SI(58)-chrome,SI(10),w-SI(14)-chrome,SI(54)};
        R_listBtn={w-SI(152)-chrome,SI(10),w-SI(100)-chrome,SI(54)};
        R_shapeTgl={SI(92),SI(8),SI(92)+(int)S(170),SI(52)};
        R_autoTgl={R_shapeTgl.right+SI(10),SI(8),R_shapeTgl.right+SI(10)+(int)S(74),SI(52)};
        R_sortBtn={R_autoTgl.right+SI(10),SI(8),R_autoTgl.right+SI(10)+(int)S(170),SI(52)};
        R_folderBtn={R_sortBtn.right+SI(10),SI(8),R_sortBtn.right+SI(10)+(int)S(g_view==2?160.f:100.f),SI(52)};   // playlist aberta: "PASTA DA PLAYLIST"
        int limit=R_listBtn.left-SI(10);
        if(R_folderBtn.right>limit){ R_folderBtn.right=std::max((int)R_folderBtn.left,limit); }
        if(R_folderBtn.right-R_folderBtn.left<SI(40)){ R_folderBtn={0,0,0,0}; if(R_sortBtn.right>limit) R_sortBtn.right=std::max((int)R_sortBtn.left,limit); }
        {   // HOST: pilula depois da PASTA (some se nao couber; fica nas configuracoes)
            int hx=(R_folderBtn.right>R_folderBtn.left?R_folderBtn.right:(R_sortBtn.right>R_sortBtn.left?R_sortBtn.right:R_autoTgl.right))+SI(10);
            R_hostBtn={hx,SI(8),hx+(int)S(78),SI(52)}; if(R_hostBtn.right>limit) R_hostBtn={0,0,0,0};
        }
        {   // EFEITOS: pilula depois do HOST (some se nao couber)
            int fx=(R_hostBtn.right>R_hostBtn.left?R_hostBtn.right:(R_folderBtn.right>R_folderBtn.left?R_folderBtn.right:(R_sortBtn.right>R_sortBtn.left?R_sortBtn.right:R_autoTgl.right)))+SI(10);
            R_fxBtn={fx,SI(8),fx+(int)S(96),SI(52)}; if(R_fxBtn.right>limit) R_fxBtn={0,0,0,0};
        }
        if(R_fxBtn.right>R_fxBtn.left){   // SOUNDPAD e DISCORD depois dos EFEITOS (somem se nao couberem; ficam nas configuracoes)
            int sx=R_fxBtn.right+SI(10);
            R_spadBtn={sx,SI(8),sx+(int)S(110),SI(52)}; if(R_spadBtn.right>limit) R_spadBtn={0,0,0,0};
            if(R_spadBtn.right>R_spadBtn.left){ int dx=R_spadBtn.right+SI(10); R_dcBtn={dx,SI(8),dx+(int)S(100),SI(52)}; if(R_dcBtn.right>limit) R_dcBtn={0,0,0,0}; }
        }
        R_modeSquare=R_modeCd=R_modeVertical={0,0,0,0}; R_themeCircles.clear();
        float ps=g_cfg.playerScale/100.0f;
        int availH=h-headerH-SI(28);
        int overhead=SI(44)+SI(30)+SI(26)+SI(40)+SI(36)+SI(66)+SI(26)+SI(20);
        int art=(int)(SI(300)*ps);
        art=std::max(SI(150),std::min(art,std::min(availH-overhead,(int)(w*.55f))));
        g_panelArt=art;
        int pad=SI(22), panelW=art+pad*2, panelY=headerH+SI(6);
        R_playerPanel={margin,panelY,margin+panelW,panelY+art+overhead};
        R_art={margin+pad,panelY+pad,margin+pad+art,panelY+pad+art};
        int x0=R_art.left;
        int ay=R_art.bottom+SI(30);
        int wy=UiClassic()?ay+SI(34):ay+SI(38), wb=UiClassic()?wy+SI(38):wy+SI(26);   // estilos novos: onda discreta
        R_wavePanel={x0,wy,x0+art,wb};
        int sy2=wb+SI(18);
        R_seek={x0,sy2-9,x0+art,sy2+9};
        int tcy=sy2+SI(54);
        int ccx=x0+art/2;
        R_play={ccx-SI(32),tcy-SI(32),ccx+SI(32),tcy+SI(32)};
        R_prev={ccx-SI(80)-SI(21),tcy-SI(20),ccx-SI(80)+SI(21),tcy+SI(20)};
        R_next={ccx+SI(80)-SI(21),tcy-SI(20),ccx+SI(80)+SI(21),tcy+SI(20)};
        R_shuffle={ccx-SI(148)-SI(17),tcy-SI(15),ccx-SI(148)+SI(17),tcy+SI(15)};
        R_repeat={ccx+SI(148)-SI(17),tcy-SI(15),ccx+SI(148)+SI(17),tcy+SI(15)};
        R_volIcon={x0,tcy+SI(38),x0+SI(22),tcy+SI(60)};
        R_vol={x0+SI(30),tcy+SI(46),x0+art-SI(44),tcy+SI(52)};
        R_runnerSlider={R_playerPanel.right-SI(22),R_playerPanel.bottom-SI(22),R_playerPanel.right,R_playerPanel.bottom};
        int gx=R_playerPanel.right+SI(18);
        if(!UiClassic()){ g_sideW=R_playerPanel.right+margin; gx=g_sideW+SI(20); }   // coluna solida + divisoria
        int gw=w-margin-gx;
        BuildLibraryArea(gx,headerH+SI(6),w-margin,h-margin);
    }
    LayoutSettings(w,h);
}
static RECT TrackRowRect(int i){ int vi=-1; for(size_t k=0;k<g_visible.size();++k) if(g_visible[k]==i){ vi=(int)k; break; } if(vi<0) return {0,0,0,0}; return {R_library.left,R_library.top+vi*SI(ROW_H)-g_listScroll,R_library.right,R_library.top+vi*SI(ROW_H)-g_listScroll+SI(ROW_H-4)}; }
static int MaxScroll(){
    if(R_library.right-R_library.left<=0) return 0;
    return std::max(0,(int)(g_contentH-(R_library.bottom-R_library.top)));
}
// Geometria do seletor de imagem da web (cabe em qualquer tamanho de janela).
static void LayoutWebPick(int w,int h){
    WebPick& wb=WP();
    int bw=std::min(760,w-40),bh=std::min(620,h-40),bx=(w-bw)/2,by=(h-bh)/2;
    wb.box={bx,by,bx+bw,by+bh};
    wb.btnClose={(LONG)(bx+bw-S(44)),(LONG)(by+S(10)),(LONG)(bx+bw-S(12)),(LONG)(by+S(42))};
    wb.qbox={(LONG)(bx+S(16)),(LONG)(by+S(12)),(LONG)(bx+bw-S(206)),(LONG)(by+S(48))};
    wb.btnSearch={(LONG)(bx+bw-S(198)),(LONG)(by+S(12)),(LONG)(bx+bw-S(52)),(LONG)(by+S(48))};
    size_t n;
    {std::lock_guard<std::mutex> lk(wb.m);n=wb.res.size();}
    int gridW=bw-(int)S(32);
    int cols=gridW/(int)S(170); if(cols<1)cols=1; if(cols>6)cols=6;
    int cellW=(gridW-(cols-1)*(int)S(10))/cols,cellH=(int)S(140);
    int gridBottom=by+bh-(int)S(64);
    wb.cells.clear();
    for(size_t i=0;i<n;++i){
        int row=(int)(i/cols),col=(int)(i%cols);
        int cx=bx+(int)S(16)+col*(cellW+(int)S(10));
        int cy=by+(int)S(60)+row*(cellH+(int)S(10))-wb.scroll;
        if(cy>gridBottom||cy+cellH<by+(int)S(56)){wb.cells.push_back({0,0,0,0});continue;}
        wb.cells.push_back({cx,cy,cx+cellW,cy+cellH});
    }
    wb.btnUse={(LONG)(bx+bw-S(252)),(LONG)(by+bh-S(50)),(LONG)(bx+bw-S(132)),(LONG)(by+bh-S(14))};
    wb.btnCancel={(LONG)(bx+bw-S(124)),(LONG)(by+bh-S(50)),(LONG)(bx+bw-S(16)),(LONG)(by+bh-S(14))};
}
// Pilula de downloads (canto inferior direito, modo normal): calculada a cada quadro.
static bool LayoutActivity(int w,int h,int& waiting,float& pct,std::wstring& title){
    R_activity={0,0,0,0};
    if(g_cfg.displayMode==L"vertical"||g_showSettings||!DownloadActivity(waiting,pct,title)) return false;
    int aw=std::min((int)S(360),w-SI(40));
    R_activity={w-SI(16)-aw,h-SI(54),w-SI(16),h-SI(18)};
    return true;
}
// Geometria da tela de busca online: busca, fontes e resultados em linhas com ▶ ⬇ +.
static RECT R_onList;
// Painel EFEITOS (sobreposto): 5 efeitos com nivel 0..3, modos de stem e o estado da separacao.
static bool StemJobActive(){ auto j=stems::Find(StemKeyCurrent()); int st=j?j->state.load():-1; return st==stems::S_QUEUED||st==stems::S_DOWNLOADING||st==stems::S_SEPARATING; }
static void LayoutFxPanel(int w,int h){
    FxPanelUI& p=g_fxp;
    int bw=std::min((int)S(740),w-24), bh=std::min((int)S(400),h-24), bx=(w-bw)/2, by=(h-bh)/2;
    p.box={bx,by,bx+bw,by+bh};
    p.btnClose={bx+bw-SI(46),by+SI(8),bx+bw-SI(10),by+SI(44)};
    int pad=SI(18), gap=SI(10), y=by+SI(76);
    int cw=(bw-2*pad-4*gap)/5;
    for(int i=0;i<5;i++){ int x=bx+pad+i*(cw+gap); p.fx[i]={x,y,x+cw,y+SI(52)}; }
    y+=SI(52)+SI(12);
    p.btnClear={bx+pad,y,bx+pad+(int)S(190),y+SI(34)};
    y+=SI(34)+SI(44);
    int sw=(bw-2*pad-5*gap)/6;
    for(int i=0;i<6;i++){ int x=bx+pad+i*(sw+gap); p.stem[i]={x,y,x+sw,y+SI(44)}; }
    y+=SI(44)+SI(14);
    p.btnCancel={bx+bw-pad-(int)S(120),y,bx+bw-pad,y+SI(34)};
    p.btnCpu={p.btnCancel.left-SI(10)-(int)S(210),y,p.btnCancel.left-SI(10),y+SI(34)};
    p.info={bx+pad,y,p.btnCpu.left-SI(12),by+bh-SI(12)};
}
// Texto do estado dos stems da faixa atual (duas linhas).
static void FxStemStatus(std::wstring& l1,std::wstring& l2){
    l1.clear(); l2.clear();
    if(!stems::InstalledCached()){ l1=L"Nenhum separador configurado: Configurações > SEPARAR EM PARTES (STEMS)"; l2=L"Você escolhe o programa; os efeitos funcionam sem ele."; return; }
    if(g_current<0||g_current>=(int)g_tracks.size()){ l1=L"Toque uma música para separar."; return; }
    std::wstring key=StemKeyCurrent();
    if(stems::Complete(key)){ l1=L"Stems desta música prontos (ficam guardados): trocar de modo é na hora."; return; }
    auto j=stems::Find(key); int st=j?j->state.load():-1;
    if(st==stems::S_QUEUED) l1=L"Na fila para separar...";
    else if(st==stems::S_DOWNLOADING) l1=L"Baixando o áudio para separar...";
    else if(st==stems::S_SEPARATING){ l1=L"Separando: "+std::to_wstring(j->pct.load())+L"%"; l2=L"Na primeira vez leva cerca de metade da duração da música; enquanto isso toca a completa."; }
    else if(st==stems::S_FAILED){ std::lock_guard<std::mutex> lk(j->m); l1=L"Falhou: "+j->err; }
    else if(st==stems::S_CANCELED) l1=L"Separação cancelada.";
    else l1=StemModeNow()==stems::M_FULL?L"Escolha um modo para separar esta música.":L"Vai separar quando a música tocar.";
}
// Painel HOST (sobreposto): estado, botoes, pedidos pendentes, dispositivos, playlists hosteadas.
static void LayoutHostPanel(int w,int h){
    host::PanelUI& p=host::PU();
    if(p.v.version!=host::St().version.load()) p.v=host::GetView();
    int bw=std::min((int)S(860),w-24), bh=std::min((int)S(700),h-24), bx=(w-bw)/2, by=(h-bh)/2;
    p.box={bx,by,bx+bw,by+bh};
    p.btnClose={bx+bw-SI(46),by+SI(12),bx+bw-SI(12),by+SI(46)};
    int x=bx+SI(18), cw=bw-SI(36), y=by+SI(52), g=SI(8), bhgt=SI(34);
    int b5=(cw-g*4)/5;
    auto five=[&](RECT* r[5]){ for(int k=0;k<5;k++){ int xx=x+k*(b5+g); *r[k]={xx,y,k==4?x+cw:xx+b5,y+bhgt}; } y+=bhgt+g; };
    { RECT* a[5]={&p.btnToggle,&p.btnTunnel,&p.btnNewLink,&p.btnHtml,&p.btnPasta}; five(a); }
    { RECT* a[5]={&p.btnPort,&p.btnPin,&p.btnName,&p.btnLan,&p.btnIpv6}; five(a); }
    // faixa do QR: quadrado a esquerda; a direita, estado + copiar + opcoes do QR
    int band=SI(206), qs=std::min(band,SI(206));
    p.qrBox={x,y,x+qs,y+qs};
    int rx=x+qs+SI(16), rw=x+cw-rx, h2=(rw-g)/2;
    p.info={rx,y,x+cw,y+SI(62)};
    int ry=y+SI(66);
    p.btnCopyTun={rx,ry,rx+h2,ry+bhgt}; p.btnCopyLan={rx+h2+g,ry,x+cw,ry+bhgt}; ry+=bhgt+g;
    p.btnQrMode={rx,ry,rx+h2,ry+bhgt}; p.btnQrNew={rx+h2+g,ry,x+cw,ry+bhgt}; ry+=bhgt+g;
    p.btnOnline={rx,ry,rx+h2,ry+bhgt}; p.btnQrConfirm={rx+h2+g,ry,x+cw,ry+bhgt};
    y+=band+SI(10);
    p.list={bx+SI(8),y,bx+bw-SI(8),by+bh-SI(12)};
    int listTop=p.list.top, ly=listTop-p.scroll, rowH=SI(34);
    auto row=[&](){ RECT r={x,ly,x+cw,ly+rowH-SI(6)}; ly+=rowH; return r; };
    p.accept.clear(); p.deny.clear(); p.revoke.clear(); p.devLib.clear(); p.devLink.clear(); p.plHost.clear(); p.plDev.clear(); p.dplOk.clear();
    ly+=SI(26);                                                        // titulo "pedidos"
    for(size_t i=0;i<p.v.pending.size();i++){ RECT r=row(); p.deny.push_back({r.right-SI(90),r.top,r.right,r.bottom}); p.accept.push_back({r.right-SI(190),r.top,r.right-SI(98),r.bottom}); }
    ly+=SI(26);                                                        // titulo "aparelhos"
    if(p.v.devs.empty()) ly+=rowH;
    for(size_t i=0;i<p.v.devs.size()&&i<100;i++){ RECT r=row(); p.revoke.push_back({r.right-SI(96),r.top,r.right,r.bottom}); p.devLib.push_back({r.right-SI(96)-SI(8)-SI(150),r.top,r.right-SI(104),r.bottom}); p.devLink.push_back({r.right-SI(262)-SI(96),r.top,r.right-SI(262),r.bottom}); }
    ly+=SI(26);                                                        // titulo "playlists do PC"
    if(g_playlists.empty()) ly+=rowH;
    for(size_t i=0;i<g_playlists.size()&&i<200;i++){
        RECT r=row(); p.plHost.push_back({r.right-SI(130),r.top,r.right,r.bottom});
        std::vector<RECT> chips; std::string t=host::Targets(g_playlists[i].slug);
        if(!t.empty()&&!p.v.devs.empty()){ int cx=x+SI(16); for(size_t j=0;j<p.v.devs.size()&&j<20;j++){ int cwid=SI(120); if(cx+cwid>x+cw){ cx=x+SI(16); ly+=rowH; } chips.push_back({cx,ly,cx+cwid,ly+rowH-SI(8)}); cx+=cwid+SI(6); } ly+=rowH; }
        p.plDev.push_back(chips);
    }
    ly+=SI(26);                                                        // titulo "playlists dos aparelhos"
    if(p.v.dpls.empty()) ly+=rowH;
    for(size_t i=0;i<p.v.dpls.size()&&i<500;i++){ RECT r=row(); p.dplOk.push_back(p.v.dpls[i].share?RECT{r.right-SI(120),r.top,r.right,r.bottom}:RECT{0,0,0,0}); }
    p.contentH=ly+p.scroll-listTop;
    int maxSc=std::max(0,p.contentH-(int)(p.list.bottom-p.list.top)); if(p.scroll>maxSc) p.scroll=maxSc; if(p.scroll<0) p.scroll=0;
    // QR: texto atual (tunel testado ou rede local) -> so recodifica quando muda
    bool isTun=false; std::string txt;
    if(!p.qrDev.empty()){ txt=p.v.running?host::DeviceLinkUrl(p.qrDev,p.qrTunnel,isTun):std::string(); if(txt.empty()) p.qrDev.clear(); }
    if(p.qrDev.empty()) txt=p.v.running?host::QrUrl(p.qrTunnel,isTun):std::string();
    if(txt!=p.qrText){ p.qrText=txt; p.qr=qr::Code(); if(!txt.empty()&&!qr::Encode(txt,p.qr,1,1,20)){ p.qr=qr::Code(); } }
}
static void LayoutOnline(int w,int h){
    OnlineUI& u=OU();
    int bw=std::min((int)S(860),w-24), bh=std::min((int)S(660),h-24), bx=(w-bw)/2, by=(h-bh)/2;
    u.box={bx,by,bx+bw,by+bh};
    u.btnClose={bx+bw-SI(46),by+SI(12),bx+bw-SI(12),by+SI(46)};
    int qy=by+SI(56);
    u.btnSearch={bx+bw-SI(16)-(int)S(120),qy,bx+bw-SI(16),qy+SI(40)};
    u.qbox={bx+SI(16),qy,u.btnSearch.left-SI(8),qy+SI(40)};
    int ty=qy+SI(50), tw2=std::min((int)S(130),(bw-SI(48))/3);
    for(int i=0;i<3;i++) u.tab[i]={bx+SI(16)+i*(tw2+SI(8)),ty,bx+SI(16)+i*(tw2+SI(8))+tw2,ty+SI(30)};
    int sy=ty+SI(38), sw=std::min((int)S(150),(bw-SI(48))/3);
    for(int i=0;i<3;i++) u.src[i]= u.tipo==0 ? RECT{bx+SI(16)+i*(sw+SI(8)),sy,bx+SI(16)+i*(sw+SI(8))+sw,sy+SI(32)} : RECT{0,0,0,0};
    int listTop=(u.tipo==0?sy+SI(44):sy+SI(6)), listBot=by+bh-SI(62), rowH=SI(58);
    R_onList={bx+SI(8),listTop,bx+bw-SI(8),listBot};
    size_t n; { std::lock_guard<std::mutex> lk(u.m); n=u.tipo==0?u.res.size():u.listas.size(); }
    int maxSc=std::max(0,(int)n*rowH-(listBot-listTop)); u.scroll=std::max(0,std::min(u.scroll,maxSc));
    u.rows.assign(n,RECT{0,0,0,0}); u.bPlay.assign(n,RECT{0,0,0,0}); u.bDl.assign(n,RECT{0,0,0,0}); u.bAdd.assign(n,RECT{0,0,0,0});
    for(size_t i=0;i<n;++i){
        int y=listTop+(int)i*rowH-u.scroll; if(y+rowH<=listTop||y>=listBot) continue;
        RECT r={bx+SI(16),y,bx+bw-SI(16),y+rowH-SI(6)}; u.rows[i]=r;
        int bs=SI(38), cy=(r.top+r.bottom)/2;
        // ordem na tela: ▶ tocar (principal)  ·  + playlist  ·  ↓ salvar copia (por ultimo)
        u.bDl[i]={r.right-SI(8)-bs,cy-bs/2,r.right-SI(8),cy+bs/2};
        u.bAdd[i]={u.bDl[i].left-SI(6)-bs,cy-bs/2,u.bDl[i].left-SI(6),cy+bs/2};
        u.bPlay[i]={u.bAdd[i].left-SI(6)-bs,cy-bs/2,u.bAdd[i].left-SI(6),cy+bs/2};
    }
    u.btnAddAll=(n>0&&u.tipo==0)?RECT{bx+bw-SI(16)-(int)S(260),by+bh-SI(50),bx+bw-SI(16),by+bh-SI(14)}:RECT{0,0,0,0};
    // Sem CLI configurada: um so botao no meio, levando as Configuracoes.
    if(n==0&&!u.busy.load()&&!fonte::Configurada()){ int cwid=(int)S(300),chh=SI(42),cxm=(R_onList.left+R_onList.right)/2,cym=(listTop+listBot)/2;
        u.btnCfg={cxm-cwid/2,cym+SI(10),cxm+cwid/2,cym+SI(10)+chh}; }
    else u.btnCfg=RECT{0,0,0,0};
}
// Geometria do editor de texto (artista / nome do arquivo).
static void LayoutEditor(int w,int h){
    int bw=std::min(g_editMode>=4?640:480,w-40), bh=180, bx=(w-bw)/2, by=(h-bh)/2;   // link: caixa mais larga
    R_editBox={bx,by,bx+bw,by+bh};
    R_editSave={bx+bw-238,by+bh-56,bx+bw-128,by+bh-18};
    R_editCancel={bx+bw-118,by+bh-56,bx+bw-20,by+bh-18};
}
#include "app_panels.h"
