#pragma once
// Configuracoes, splash, editor de artista, menu de imagem e seletor web.
// Porte 1:1 das funcoes correspondentes do main.cpp do Windows.
#include "app_draw.h"
#include "app_web.h"

static void DrawSettings(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(255,5,7,17),UI().bg));
    RECT rp=R_settingsPanel; int px=rp.left,py=rp.top,pw=rp.right-rp.left,ph=rp.bottom-rp.top;
    Color accent=ToGdi(g_theme.accent);
    Color white=C_WHITE, gray=C_GRAY2, ab=accent, dim=Cs(Argb(255,30,33,48),UI().border);
    // Escala FIXA nas configuracoes.
    const float h1=22, lab=13, sm=11, st=14, fBtn=12;
    gfx::Text(L"CONFIGURAÇÕES",(float)(px+28),(float)(py+20),h1,UiClassic()?ab:white,true);
    gfx::TextRect(std::wstring(L"Remix Player ")+REMIX_VERSAO,RectF((float)R_settingsClose.left-300,(float)(py+24),280,20),sm,gray,false,gfx::Far,true);
    gfx::Text(L"×",(float)(R_settingsClose.left+8),(float)(py+14),S(20),white);
    // conteudo com clip e scroll
    gfx::PushClip(RectF((float)(px+6),(float)(py+56),(float)(pw-12),(float)(ph-64)));
    rlPushMatrix();
    rlTranslatef(0,-(float)g_setScroll,0);
    // Botao moderno: pildula com texto centralizado bold; ativo ganha preenchimento
    // do acento, borda forte, glow externo e brilho superior sutil.
    auto btn=[&](RECT rr,const std::wstring& t,bool on){
        RectF b=RF(rr);
        if(b.Width<10||b.Height<10) return;
        if(!UiClassic()){   // estilos novos: chapa lisa; ligado = tom mais claro, texto branco e um risco do tema embaixo
            Color fb=ToGdi(on?UI().surfaceHi:UI().surface);
            DrawRoundRect(b,S(UI_R_PILL),&fb,nullptr);
            gfx::TextRect(t,b,fBtn,on?white:ToGdi(UI().textDim),true,gfx::Center,true);
            if(on) gfx::FillRect(b.X+b.Width*.28f,b.Y+b.Height-2.f,b.Width*.44f,2.f,ToGdi(g_theme.accent));
            return;
        }
        if(on){
            RectF glow(b.X-2,b.Y-2,b.Width+4,b.Height+4);
            gfx::StrokeRoundRect(glow,14,4.f,ToGdi(g_theme.accent,60));
        }
        Color fb=on?ToGdi(g_theme.accent,78):Cs(Argb(255,18,21,38),UI().surfaceHi);
        Color bp=on?ToGdi(g_theme.accent):Cs(Argb(255,64,69,97),UI().borderHi);
        DrawRoundRect(b,10,&fb,&bp,on?2.f:1.3f);
        RectF hl(b.X+4,b.Y+2,b.Width-8,b.Height*0.44f);
        Color hb=Argb(on?30:12,255,255,255);
        DrawRoundRect(hl,7,&hb,nullptr);
        gfx::TextRect(t,b,fBtn,on?white:gray,true,gfx::Center,true);
    };
    // Chave liga/desliga (estilos novos): rotulo a esquerda, chave a direita. No classico e o botao de sempre.
    auto tgl=[&](RECT rr,const std::wstring& classicText,const std::wstring& label,bool on){
        if(UiClassic()){ btn(rr,classicText,on); return; }
        RectF b=RF(rr); if(b.Width<10||b.Height<10) return;
        Color fb=ToGdi(UI().surface); DrawRoundRect(b,S(UI_R_PILL),&fb,nullptr);
        float sw=34.f,sh=18.f; RectF sr(b.X+b.Width-sw-10.f,b.Y+(b.Height-sh)/2.f,sw,sh);
        Color tr=on?ToGdi(g_theme.accent):ToGdi(UI().borderHi); DrawRoundRect(sr,sh/2.f,&tr,nullptr);
        gfx::FillEllipse(on?sr.X+sw-sh+2.f:sr.X+2.f,sr.Y+2.f,sh-4.f,sh-4.f,on?ToGdi(UI().bg):ToGdi(UI().text));
        gfx::TextRect(label,RectF(b.X+12.f,b.Y,b.Width-sw-30.f,b.Height),fBtn,on?white:ToGdi(UI().textDim),true,gfx::Near,true,gfx::EllipsisChar);
    };
    const wchar_t* seta=UiClassic()?L"":L" ▾";   // botoes que alternam entre opcoes
    // caixas de secao com titulo
    {
        Color sfb=Cs(Argb(255,12,15,29),UI().bar), sbp=Cs(Argb(255,38,42,64),UI().border), dv=Cs(Argb(255,32,36,56),UI().border);
        for(auto&s:g_setSections){
            RectF sr=RF(s.first);
            if(UiClassic()) DrawRoundRect(sr,12,&sfb,&sbp,1.f); else DrawRoundRect(sr,S(UI_R_CARD),&sfb,nullptr);
            gfx::Text(s.second,sr.X+16,sr.Y+12,st,UiClassic()?ab:white,true);
            gfx::Line(sr.X+14,sr.Y+40,sr.X+sr.Width-14.f,sr.Y+40,1.f,dv);
        }
    }
    for(auto&lp:g_setLabels) gfx::Text(lp.second,(float)lp.first.left,(float)lp.first.top,lab,UiClassic()?ab:gray,true);
    // estilo da interface
    for(int k=0;k<UI_STYLE_COUNT;k++) btn(R_settingsStyle[k],UiStyleName(k),g_cfg.uiStyle==k);
    gfx::Text(L"Clássico: o visual original, com LED e cards.   Remix: barra lateral com a biblioteca, início com novidades e o player embaixo.",(float)R_settingsStyle[0].left,(float)R_settingsStyle[0].bottom+10,sm,gray);
    {   // SOUNDPAD e DISCORD
        bool sp=spad::Running(), dr=dc::Ready();
        btn(R_setSpad,sp?L"ABRIR SOUNDPAD (MICROFONE LIGADO)":L"ABRIR SOUNDPAD",sp);
        btn(R_setDc,dr?L"ABRIR DISCORD (BOT CONECTADO)":L"ABRIR DISCORD",dr);
        gfx::TextRect(L"Soundpad: sons no seu microfone (Discord, jogos). Discord: bot de música com fila, votação e as suas playlists.",RectF((float)R_setSpad.left,(float)R_setSpad.bottom+10,(float)(R_setDc.right-R_setSpad.left),16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
    {   // HOST (acesso pelo celular)
        bool on=host::Running(); host::View hv=host::GetView();
        tgl(R_setHostOn,on?L"HOST: LIGADO":L"HOST: DESLIGADO",L"LIGAR O HOST",on);
        btn(R_setHostPanel,L"ABRIR PAINEL (QR CODE)",false);
        btn(R_setHostPort,L"PORTA: "+std::to_wstring(g_cfg.hostPort),false);
        btn(R_setHostPin,g_cfg.hostPin.empty()?L"PIN: DEFINIR...":L"PIN: "+std::wstring(g_cfg.hostPin.size(),L'•'),!g_cfg.hostPin.empty());
        btn(R_setHostName,L"NOME: "+(g_cfg.hostName.empty()?Utf8ToWide(hostnet::HostName()):g_cfg.hostName),false);
        tgl(R_setHostTunnel,g_cfg.hostTunnel?L"TÚNEL: LIGADO":L"TÚNEL: DESLIGADO",L"TÚNEL CLOUDFLARE (internet)",g_cfg.hostTunnel);
        tgl(R_setHostLan,g_cfg.hostLan?L"REDE LOCAL: SIM":L"REDE LOCAL: NÃO",L"REDE LOCAL (mesmo roteador)",g_cfg.hostLan);
        btn(R_setHostCopyTun,hv.tunUrl.empty()?(hv.tunPending.empty()?L"COPIAR LINK DO TÚNEL":L"TESTANDO O LINK..."):L"COPIAR LINK DO TÚNEL",!hv.tunUrl.empty());
        btn(R_setHostCopyLan,L"COPIAR LINK LOCAL",!hv.lanUrls.empty());
        tgl(R_setHostOnline,g_cfg.hostOnline?L"ONLINE NO CELULAR: SIM":L"ONLINE NO CELULAR: NÃO",L"ONLINE NO CELULAR",g_cfg.hostOnline);
        tgl(R_setHostQrConf,g_cfg.hostQrConfirm?L"QR PEDE ACEITE: SIM":L"QR PEDE ACEITE: NÃO",L"QR PEDE ACEITE",g_cfg.hostQrConfirm);
        tgl(R_setHostIpv6,g_cfg.hostIPv6?L"IPv6: SIM":L"IPv6: NÃO",L"IPv6 (LAN)",g_cfg.hostIPv6);
        std::wstring st=on?L"Ligado na porta "+std::to_wstring(hv.port)+(hv.lanUrls.empty()?L"":L"   ·   local: "+Utf8ToWide(hv.lanUrls[0]))+L"   ·   túnel: "+Utf8ToWide(hv.tunUrl.empty()?hv.tunStatus:hv.tunUrl)
                          :L"Desligado. No painel tem o QR code: o celular escaneia, digita um nome e já fica vinculado.";
        gfx::TextRect(st,RectF((float)R_setHostOnline.left,(float)R_setHostOnline.bottom+12,(float)(R_setHostIpv6.right-R_setHostOnline.left),16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(L"Aparelho vinculado não vê nada até você liberar: BIBLIOTECA por aparelho ou playlists hosteadas (painel).",RectF((float)R_setHostOnline.left,(float)R_setHostOnline.bottom+30,(float)(R_setHostIpv6.right-R_setHostOnline.left),16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
    // biblioteca
    std::wstring mode=g_cfg.musicFolder.empty()?L"PADRÃO — detectar músicas do PC":L"PASTA PERSONALIZADA";
    gfx::Text(mode,(float)R_settingsDefault.left,(float)(R_settingsDefault.top-18),sm,white);
    btn(R_settingsDefault,L"PADRÃO (PASTAS DO USUÁRIO)",g_cfg.musicFolder.empty());
    btn(R_settingsCustom,L"ESCOLHER PASTA",!g_cfg.musicFolder.empty());
    if(!g_cfg.musicFolder.empty()){
        gfx::TextRect(g_cfg.musicFolder,RectF((float)R_settingsDefault.left,(float)(R_settingsDefault.bottom+6),(float)(R_settingsCustom.right-R_settingsDefault.left),16),sm,gray,false,gfx::Near,false,gfx::EllipsisPath);
    }
    btn(R_settingsModeSquare,L"QUADRADO",g_cfg.displayMode==L"normal"&&g_cfg.artShape==L"square");
    btn(R_settingsModeCd,L"CD",g_cfg.displayMode==L"normal"&&g_cfg.artShape==L"cd");
    btn(R_settingsModeVertical,L"VERTICAL",g_cfg.displayMode==L"vertical");
    for(size_t i=0;i<R_themeCirclesSettings.size()&&i<g_themes.size();++i){
        auto&r=R_themeCirclesSettings[i]; bool sel=g_themes[i].id==g_cfg.theme;
        RectF rr=RF(r);
        if(UiClassic()){ gfx::FillEllipse(rr,ToGdi(g_themes[i].accent,90)); gfx::StrokeEllipse(rr,sel?2.4f:1.3f,sel?C_WHITE:ToGdi(g_themes[i].accent)); }
        else { gfx::FillEllipse(rr,ToGdi(g_themes[i].accent)); if(sel) gfx::StrokeEllipse(RectF(rr.X-3,rr.Y-3,rr.Width+6,rr.Height+6),2.f,C_WHITE); }
    }
    tgl(R_setParticles,L"PARTÍCULAS: "+std::wstring(g_cfg.particlesOn?L"LIGADO":L"DESLIGADO"),L"PARTÍCULAS",g_cfg.particlesOn);
    tgl(R_setGlitch,L"GLITCH",L"GLITCH",g_cfg.glitchOn);
    tgl(R_setPerf,g_cfg.perfMode?L"MODO LEVE: LIGADO (PC fraco)":L"MODO LEVE: DESLIGADO",L"MODO LEVE",g_cfg.perfMode);
    if(R_shortcutsBox.right>R_shortcutsBox.left){
        for(int a=0;a<HK_COUNT;a++){
            RECT kr=R_hkKey[a]; if(kr.right<=kr.left) continue;
            gfx::TextRect(HkLabel(a),RectF((float)R_shortcutsBox.left+20,(float)kr.top,(float)(kr.left-R_shortcutsBox.left-30),(float)(kr.bottom-kr.top)),sm,a%2?gray:white,false,gfx::Near,true,gfx::EllipsisChar);
            bool cap=(g_hkCapture==a);
            btn(kr,cap?L"pressione a tecla...":HotkeyLabel(g_cfg.hk[a]),cap||g_cfg.hk[a].key!=0);
            btn(R_hkScope[a],g_cfg.hk[a].global?L"GLOBAL":L"FOCO",g_cfg.hk[a].global);
        }
        btn(R_hkReset,L"RESTAURAR PADRÕES",false);
        float hw=(float)(R_shortcutsBox.right-20-(R_hkReset.right+16));
        gfx::TextRect(L"Clique na tecla para trocar (Backspace limpa). FOCO = só com a janela do Remix ativa (não atrapalha jogos).",RectF((float)R_hkReset.right+16,(float)R_hkReset.top+2,hw,16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(L"GLOBAL = funciona em 2º plano/com outro programa na frente (sessão X11; no Wayland use o atalho do sistema: remix --cmd next).",RectF((float)R_hkReset.right+16,(float)R_hkReset.top+20,hw,16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
    // secao FUNDO: wallpaper e capa embaçada
    btn(R_setWallChoose,L"WALLPAPER: ESCOLHER IMAGEM...",false);
    btn(R_setWallClear,L"REMOVER WALLPAPER",!g_cfg.bgWallpaper.empty());
    tgl(R_setCoverBlur,L"FUNDO EMBAÇADO (USA A CAPA): "+std::wstring(g_cfg.coverBlurBg?L"LIGADO":L"DESLIGADO"),L"FUNDO EMBAÇADO (capa)",g_cfg.coverBlurBg);
    // reproducao
    tgl(R_setAutoplay,g_cfg.autoplay?L"AUTOPLAY: LIGADO":L"AUTOPLAY: DESLIGADO",L"AUTOPLAY",g_cfg.autoplay);
    btn(R_setSort,L"ORDEM: "+SortModeName(g_cfg.sortMode)+L" ▾",g_cfg.sortMode==L"manual");
    btn(R_setSortDir,std::wstring(g_cfg.sortDesc?L"DECRESCENTE":L"CRESCENTE")+seta,false);
    tgl(R_setBgClose,g_cfg.bgOnClose?L"FECHAR: CONTINUA TOCANDO":L"FECHAR: ENCERRA O APP",L"TOCAR EM 2º PLANO",g_cfg.bgOnClose);
    tgl(R_setSysMedia,g_cfg.sysMedia?L"CONTROLES DO SISTEMA: SIM":L"CONTROLES DO SISTEMA: NÃO",L"CONTROLES DO SISTEMA",g_cfg.sysMedia);
    btn(R_setQuit,L"SAIR DO REMIX (Ctrl+Q)",false);
    if(R_setOnMode.right>R_setOnMode.left){   // ONLINE
        bool probed=fonte::JaConferiu(); bool okT=probed&&fonte::Configurada()&&fonte::FfmpegOk(); std::wstring tools;
        if(!probed) tools=L"Conferindo o que está instalado...";
        else { fonte::Estado e=fonte::Snapshot();
            tools=e.cli.empty()?L"Fonte externa: NÃO CONFIGURADA":L"Fonte externa: "+e.vCli;
            tools+=e.ffmpeg.empty()?L"   ·   ffmpeg: NÃO ENCONTRADO":L"   ·   ffmpeg "+e.vFf;
            tools+=e.jsName.empty()?(e.jsOld.empty()?std::wstring(L"   ·   runtime JavaScript: não encontrado"):L"   ·   "+e.jsOld+L" é antigo (Deno 2.3+ ou Node 22+)"):L"   ·   "+e.jsName+L" "+e.vJs; }
        gfx::TextRect(tools,RectF((float)R_setOnMode.left,(float)R_setOnMode.top-24,(float)(R_setOnFmt.right-R_setOnMode.left),18),sm,okT||!probed?white:Argb(255,235,150,110),false,gfx::Near,false,gfx::EllipsisChar);
        btn(R_setOnMode,std::wstring(g_cfg.onlineMode==L"download"?L"AO TOCAR: SALVAR CÓPIA":L"AO TOCAR: STREAMING")+seta,g_cfg.onlineMode==L"download");
        btn(R_setOnFmt,L"FORMATO: "+std::wstring(g_cfg.onlineFormat==L"original"?L"ORIGINAL":(g_cfg.onlineFormat==L"m4a"?L"M4A":L"MP3"))+seta,false);
        static const wchar_t* srcs[3]={L"BUSCA: YOUTUBE MUSIC",L"BUSCA: YOUTUBE",L"BUSCA: SOUNDCLOUD"};
        btn(R_setOnSrc,std::wstring(srcs[std::max(0,std::min(2,g_cfg.onlineSource))])+seta,false);
        btn(R_setOnFolder,L"PASTA ONDE SALVAR...",!g_cfg.downloadFolder.empty());
        gfx::TextRect(L"Cópias salvas em "+OnlineDownloadBaseCached()+L"   ·   streaming fica só na memória (fechar o app não deixa arquivo pela metade)",RectF((float)R_setOnSrc.left,(float)R_setOnSrc.bottom+8,(float)(R_setOnFolder.right-R_setOnSrc.left),16),sm,gray,false,gfx::Near,false,gfx::EllipsisPath);
        {   // programa de linha de comando que a pessoa configurou
            std::wstring cam=g_cfg.mediaCli.empty()?std::wstring(L"(nenhum configurado)"):g_cfg.mediaCli;
            bool ok=fonte::Configurada();
            gfx::TextRect(L"O Remix não instala nem distribui nada: informe o caminho de um programa de linha de comando compatível que você já tenha.",
                          RectF((float)R_setCli.left,(float)R_setCli.top-40,(float)(R_setCli.right-R_setCli.left),18),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
            gfx::TextRect(ok?(L"Encontrado: "+fonte::Versao()):(g_cfg.mediaCli.empty()?std::wstring(fonte::Contrato()):std::wstring(L"Não encontrei esse arquivo (ou ele não é executável).")),
                          RectF((float)R_setCli.left,(float)R_setCli.top-20,(float)(R_setCli.right-R_setCli.left),18),sm,ok?white:Argb(255,235,150,110),false,gfx::Near,false,gfx::EllipsisChar);
            btn(R_setCli,L"CAMINHO: "+cam,ok);
            btn(R_setCliBuscar,L"ESCOLHER O PROGRAMA...",false);
        }
        btn(R_setOnRecheck,L"PROCURAR DE NOVO",false);
        gfx::TextRect(okT?L"Spotify, Deezer e Apple Music: o Remix lê a lista e acha cada música no YouTube Music.":L"Instale pela sua distro o ffmpeg e, se quiser fontes externas, uma CLI compatível — depois aponte o caminho dela aqui embaixo.",
            RectF((float)R_setOnRecheck.right+14,(float)R_setOnRecheck.top,(float)(R_setOnFolder.right-R_setOnRecheck.right-14),(float)(R_setOnRecheck.bottom-R_setOnRecheck.top)),sm,gray,false,gfx::Near,true,gfx::EllipsisChar);
    }
    {   // textos de ajuda presos a largura da secao (nada vaza da caixa)
        float pw2=(float)(R_setSortDir.right-R_setAutoplay.left);
        gfx::TextRect(L"Controles do sistema: teclas de mídia e o applet de mídia do desktop (MPRIS).",RectF((float)(R_setQuit.right+14),(float)R_setQuit.top,(float)(R_setSysMedia.right-R_setQuit.right-14),(float)(R_setQuit.bottom-R_setQuit.top)),sm,gray,false,gfx::Near,true,gfx::EllipsisChar);
        gfx::TextRect(g_cfg.autoplay?L"Ao acabar uma musica, toca a proxima (ordem da lista ou aleatorio com ⇄).":L"Ao acabar uma musica, para. Toque a proxima manualmente.",RectF((float)R_setAutoplay.left,(float)(R_setAutoplay.bottom+8),pw2,16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(g_cfg.sortMode==L"manual"?L"Ordem manual: use as setas ▲▼ nas faixas (ou Ctrl+↑/↓ na faixa atual). Salva em order.ini.":L"A ordem escolhida fica salva e vale para o autoplay e para ◀ ▶.",RectF((float)R_setAutoplay.left,(float)(R_setAutoplay.bottom+26),pw2,16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
    }
    // equalizador
    tgl(R_setEqOn,g_cfg.eqOn?L"EQUALIZADOR: LIGADO":L"EQUALIZADOR: DESLIGADO",L"EQUALIZADOR",g_cfg.eqOn);
    btn(R_setEqReset,L"ZERAR",false);
    // sliders
    auto isEq=[&](int id){return id>=Z_EQ_BASE&&id<Z_EQ_BASE+8;};
    auto sval=[&](int id)->int{if(isEq(id))return g_cfg.eq[id-Z_EQ_BASE];switch(id){case Z_UI_SCALE:return g_cfg.uiScale;case Z_TITLE_SCALE:return g_cfg.titleScale;case Z_ARTIST_SCALE:return g_cfg.artistScale;case Z_VERTICAL_SCALE:return g_cfg.verticalScale;case Z_PLAYER_SIZE_SLIDER:return g_cfg.playerScale;case Z_LED_BRIGHT:return g_cfg.ledBrightness;case Z_RUNNER_SPEED:return g_cfg.runnerSpeed;case Z_PART_SPEED:return g_cfg.particlesSpeed;case Z_CD_SPEED:return g_cfg.cdSpeed;default:return g_cfg.ledSpeed;}};
    static const wchar_t* eqNames[8]={L"60 Hz (sub-grave)",L"150 Hz (grave)",L"400 Hz",L"1 kHz (voz)",L"2.5 kHz",L"6 kHz (presença)",L"10 kHz",L"15 kHz (brilho)"};
    auto sname=[&](int id)->const wchar_t*{if(isEq(id))return eqNames[id-Z_EQ_BASE];switch(id){case Z_UI_SCALE:return L"Escala geral";case Z_TITLE_SCALE:return L"Nome da musica";case Z_ARTIST_SCALE:return L"Nome do artista";case Z_VERTICAL_SCALE:return L"Escala do vertical";case Z_PLAYER_SIZE_SLIDER:return L"Tamanho do player";case Z_LED_BRIGHT:return L"Brilho do LED";case Z_RUNNER_SPEED:return L"Velocidade da linha (corredor)";case Z_PART_SPEED:return L"Velocidade das particulas";case Z_CD_SPEED:return L"Velocidade de giro do CD";default:return L"Velocidade do LED";}};
    for(auto&s:g_setSliders){
        int val=sval(s.id); bool eq=isEq(s.id);
        Color knob=(eq&&!g_cfg.eqOn)?Cs(Argb(255,90,94,118),UI().borderHi):(UiClassic()?ab:white);
        Color fillC=(eq&&!g_cfg.eqOn)?knob:(UiClassic()?ab:ToGdi(UI().textDim));   // novos: preenchimento cinza claro, botao branco
        gfx::Text(sname(s.id),(float)s.r.left,(float)s.r.top-16,sm,gray);
        RectF tr((float)s.r.left,(float)s.r.top,(float)(s.r.right-s.r.left),7);
        DrawRoundRect(tr,3,&dim,nullptr);
        float q=(float)(val-s.minv)/std::max(1,s.maxv-s.minv);
        int fill=(int)((s.r.right-s.r.left)*std::max(0.f,std::min(1.f,q)));
        if(eq){ // preenche a partir do centro (0 dB)
            int mid=(s.r.right-s.r.left)/2; int a=std::min(mid,fill), b=std::max(mid,fill);
            if(b-a>1){RectF fr((float)(s.r.left+a),(float)s.r.top,(float)(b-a),7);DrawRoundRect(fr,3,&fillC,nullptr);}
            gfx::Line((float)(s.r.left+mid),(float)s.r.top-3,(float)(s.r.left+mid),(float)s.r.top+10,1,Cs(Argb(255,70,74,95),UI().borderHi));
        } else if(fill>2){RectF fr((float)s.r.left,(float)s.r.top,(float)fill,7);DrawRoundRect(fr,3,&fillC,nullptr);}
        gfx::FillEllipse((float)(s.r.left+fill-7),(float)(s.r.top-3),14.f,14.f,knob);
        wchar_t bv[24]; if(eq) swprintf(bv,24,L"%+d dB",val); else swprintf(bv,24,L"%d%%",val);
        gfx::Text(bv,(float)(s.r.right+10),(float)(s.r.top-5),sm,white);
    }
    btn(R_setEffect,L"Efeito: "+g_cfg.ledEffect+L" ▾",false);
    if(!UiGlow()){   // nota presa a largura da secao do LED
        float sr=(float)(px+pw); for(auto&sc:g_setSections) if(sc.first.left<=R_setEffect.left&&sc.first.right>=R_setEffect.right&&sc.first.top<=R_setEffect.top&&sc.first.bottom>=R_setEffect.bottom) sr=(float)sc.first.right-20;
        gfx::TextRect(L"Neste estilo o LED e o corredor ficam desligados (Clássico ou Spotify + LED ligam).",RectF((float)R_setEffect.right+14,(float)R_setEffect.top,sr-(float)R_setEffect.right-14,(float)(R_setEffect.bottom-R_setEffect.top)),sm,gray,false,gfx::Near,true,gfx::EllipsisChar);
    }
    // linhas de cor (indice 0 = segue tema)
    auto rowIdAt=[&](size_t i)->std::wstring{
        if(i==0) return L"";
        size_t nt=g_themes.size();
        if(i<=nt) return g_themes[i-1].id;
        size_t bi=i-nt-1;
        return bi<15?std::wstring(g_brightIds[bi]):std::wstring();
    };
    auto colorRowDraw=[&](std::vector<RECT>&v,const std::wstring& cur){
        bool dimmed=g_cfg.autoColor;
        for(size_t i=0;i<v.size();++i){
            std::wstring id=rowIdAt(i);
            COLORREF c=(i==0||id.empty())?g_theme.accent:(id[0]==L'#'?ParseHexColor(id):FindTheme(g_themes,id).accent);
            RECT&r=v[i]; RectF rr=RF(r);
            if(i==0){
                gfx::FillEllipse(rr,ToGdi(c,dimmed?60:80));
                gfx::TextRect(L"T",rr,sm,white,false,gfx::Center,true);
            } else {
                gfx::FillEllipse(rr,ToGdi(c,UiClassic()?(dimmed?55:150):(dimmed?70:255)));   // novos: cor cheia quando ativa
            }
            bool sel=((i==0&&cur.empty())||(i>0&&!cur.empty()&&cur==id));
            BYTE ringA=dimmed?(BYTE)60:(BYTE)255;
            gfx::StrokeEllipse(rr,sel?2.2f:1.1f,sel?Cs(Argb(255,240,242,250),UI().text):Cs(Argb(ringA,90,94,115),UI().textFaint,ringA));
        }
    };
    tgl(R_setAutoColor,g_cfg.autoColor?L"CORES: AUTOMÁTICO (segue o tema)":L"CORES: MANUAIS",L"CORES AUTOMÁTICAS",g_cfg.autoColor);
    colorRowDraw(R_playColors,g_cfg.btnPlayColor);
    colorRowDraw(R_navColors,g_cfg.btnNavColor);
    tgl(R_setRunnerToggle,g_cfg.runnerOn?L"LIGADO":L"DESLIGADO",L"LIGAR",g_cfg.runnerOn);
    colorRowDraw(R_runColors,g_cfg.runnerColor);
    colorRowDraw(R_partColors,g_cfg.particlesColor);
    colorRowDraw(R_ledColors,g_cfg.ledColor);
    rlDrawRenderBatchActive();
    rlPopMatrix();
    gfx::PopClip();
    // aviso quando nao ha zenity/kdialog (dialogos de pasta/imagem)
    if(!sys::HasDialogTool())
        gfx::Text(L"Instale zenity ou kdialog para escolher pastas e imagens.",(float)(px+28),(float)(py+ph-22),sm,Argb(255,200,120,90));
}

static void DrawSplash(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(255,3,4,10),UI().bg));
    if(g_splashImg&&g_splashImg->ok){
        float iw=(float)g_splashImg->w,ih=(float)g_splashImg->h,sc=std::min((w-80)/iw,(h-100)/ih),dw=iw*sc,dh=ih*sc;
        gfx::DrawImg(g_splashImg,RectF((w-dw)/2,(h-dh)/2-8,dw,dh));
    }
}

static void DrawArtistEditor(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(170,2,4,10),UI().bg,170));
    LayoutEditor(w,h);
    int bx=R_editBox.left, by=R_editBox.top, bw=R_editBox.right-R_editBox.left, bh=R_editBox.bottom-R_editBox.top;
    RectF box((float)bx,(float)by,(float)bw,(float)bh);
    Color pb=Cs(Argb(255,12,15,30),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,14,&pb,&apn,2);
    const float lab=S(13), sm=S(10), txt=S(14);
    Color abr=ToGdi(g_theme.accent), white=C_WHITE, gray=C_GRAY2;
    const wchar_t* etitle=g_editMode==1?L"RENOMEAR ARQUIVO (no disco)":g_editMode==2?L"NOVA PLAYLIST":g_editMode==4?L"COLAR LINK NA PLAYLIST":g_editMode==5?L"NOVA PLAYLIST A PARTIR DE UM LINK":g_editMode==3?L"RENOMEAR PLAYLIST":g_editMode==6?L"PORTA DO HOST":g_editMode==7?L"PIN DO HOST":g_editMode==8?L"NOME DO PC NO CELULAR":g_editMode==9?L"TOKEN DO BOT DO DISCORD":g_editMode==10?L"CARGO DJ DO DISCORD":g_editMode==11?L"APPLICATION ID (RICH PRESENCE)":g_editMode==12?L"PROGRAMA DE LINHA DE COMANDO":L"EDITAR NOME DO ARTISTA";
    gfx::Text(etitle,(float)(bx+22),(float)(by+18),lab,abr,true);
    std::wstring t;
    if(g_editMode==6) t=L"Porta TCP de 1024 a 65535 (padrão 49875). Só números.";
    else if(g_editMode==7) t=L"De 4 a 12 números. O celular digita este PIN na primeira vez; depois você aceita o aparelho aqui.";
    else if(g_editMode==8) t=L"Como o seu PC aparece no celular.";
    else if(g_editMode==9) t=L"Developer Portal > seu app > Bot > Reset Token > Copy. Cole com Ctrl+V (fica só neste PC, nunca aparece).";
    else if(g_editMode==10) t=L"Nome do cargo (igual no servidor). Quem tem ele controla a música sem votação.";
    else if(g_editMode==11) t=L"Developer Portal > seu app > Application ID > Copy (uns 18 números). Vazio desliga. É o nome desse app que aparece no seu perfil.";
    else if(g_editMode==12) t=L"Caminho de um programa de linha de comando compatível que você já tenha instalado (ex.: /usr/bin/<programa>). Vazio desliga as fontes externas.";
    else if(g_editMode==2) t=L"Nome da playlist (as músicas ficam onde estão; só o caminho é guardado)";
    else if(g_editMode==4||g_editMode==5) t=L"Música, álbum ou playlist do Spotify, YouTube / YouTube Music, Deezer, Apple Music ou SoundCloud  (Ctrl+V cola)";
    else if(g_editMode==3) t=L"Playlist: "+(g_editTrack>=0&&g_editTrack<(int)g_playlists.size()?g_playlists[(size_t)g_editTrack].name:L"");
    else t=(g_editMode==1?L"Arquivo: ":L"Faixa: ")+(g_editTrack>=0&&g_editTrack<(int)g_tracks.size()?(g_editMode==1?std::filesystem::path(g_tracks[g_editTrack].path).filename().wstring():g_tracks[g_editTrack].title):L"");
    gfx::TextRect(t,RectF((float)(bx+22),(float)(by+46),(float)(bw-44),16),sm,gray,false,gfx::Near,false,gfx::EllipsisChar);
    RectF line((float)(bx+22),(float)(by+74),(float)(bw-44),36);
    Color lb=Cs(Argb(255,20,23,38),UI().surfaceHi); DrawRoundRect(line,8,&lb,nullptr);
    float tpx=(float)(bx+32), tpy=(float)(by+82);
    gfx::PushClip(line);
    std::wstring shown=g_editMode==9?std::wstring(std::min<size_t>(g_editBuf.size(),60),L'•'):g_editBuf;   // token: so bolinhas
    float emw=gfx::TextWidth(shown,txt), eoff=emw>(float)(bw-64)?emw-(float)(bw-64):0.f;   // texto longo (link): mostra o final
    gfx::Text(shown,tpx-eoff,tpy,txt,white);
    if((NowMs()/500)%2==0){
        float mw=emw-eoff;
        float cx=tpx+(mw>1.f?mw:0.f);
        if(cx>tpx+(float)(bw-64)) cx=tpx+(float)(bw-64);
        gfx::Line(cx+3,(float)(by+80),cx+3,(float)(by+102),2,C_WHITE);
    }
    gfx::PopClip();
    auto ebtnC=[&](RECT r,const wchar_t*s,bool primary){
        RectF b=RF(r);
        Color fb=primary?Cs(ToGdi(g_theme.accent,60),g_theme.accent):Cs(Argb(255,24,27,42),UI().surfaceHi);
        Color p=primary?Cs(ToGdi(g_theme.accent),g_theme.accent):Cs(Argb(255,70,74,95),UI().borderHi);
        DrawRoundRect(b,8,&fb,&p,1.5f);
        gfx::TextRect(s,b,sm,primary?abr:gray,false,gfx::Center,true);
    };
    ebtnC(R_editSave,L"SALVAR",true);
    ebtnC(R_editCancel,L"CANCELAR",false);
    gfx::Text(L"ENTER salva   ·   ESC cancela",(float)(bx+22),(float)(by+bh-40),sm,gray);
}

static void DrawImgMenu(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(150,2,4,10),UI().bg,150));
    RectF box=RF(R_imgBox);
    Color pb=Cs(Argb(255,12,15,30),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(12),&pb,&apn,1.6f);
    const float lab=S(11);
    {
        RectF rf=RF(R_imgLocal);
        Color fb=Cs(Argb(255,20,23,38),UI().surfaceHi), ln=Cs(Argb(255,58,62,86),UI().borderHi);
        DrawRoundRect(rf,S(8),&fb,&ln,1.f);
        gfx::TextRect(L"Imagem deste computador",rf,lab,C_WHITE,true,gfx::Center,true);
    }
    {
        RectF rf=RF(R_imgWeb);
        Color fb=Cs(Argb(255,20,23,38),g_theme.accent), ln=ToGdi(g_theme.accent);
        DrawRoundRect(rf,S(8),&fb,&ln,1.2f);
        gfx::TextRect(L"Buscar na internet",rf,lab,UiClassic()?ToGdi(g_theme.accent):ToGdi(UI().bg),true,gfx::Center,true);
    }
}

static void DrawWebPick(int w,int h){
    WebPick& wb=WP();
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(190,2,4,10),UI().bg,190));
    RECT&b=wb.box;int bw=b.right-b.left,bh=b.bottom-b.top;
    RectF box=RF(b);
    Color pb=Cs(Argb(255,10,13,26),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(14),&pb,&apn,1.8f);
    const float lab=S(11), sm=S(10);
    Color white=C_WHITE, gray=C_GRAY2, ab=ToGdi(g_theme.accent);
    // caixa de busca
    {
        RectF q=RF(wb.qbox);
        Color qb=Cs(Argb(255,18,21,36),UI().surfaceHi), qp=wb.editing?ToGdi(g_theme.accent):Cs(Argb(255,58,62,88),UI().borderHi);
        DrawRoundRect(q,S(9),&qb,&qp,wb.editing?1.8f:1.2f);
        std::wstring show=wb.query.empty()&&!wb.editing?L"Pesquisar (nome da musica)...":wb.query;
        gfx::PushClip(q);
        gfx::Text(show,q.X+S(12),q.Y+(q.Height-S(16))/2.f,sm,wb.query.empty()&&!wb.editing?gray:white);
        if(wb.editing&&(NowMs()/500)%2==0){
            float mw=gfx::TextWidth(wb.query,sm);
            float cx=q.X+S(12)+mw;
            gfx::Line(cx+S(3),q.Y+S(7),cx+S(3),q.Y+q.Height-S(7),2,C_WHITE);
        }
        gfx::PopClip();
    }
    { // botao buscar
        RectF s=RF(wb.btnSearch);
        Color sb=Cs(ToGdi(g_theme.accent,55),g_theme.accent), sp=ToGdi(g_theme.accent);
        DrawRoundRect(s,S(9),&sb,&sp,1.4f);
        gfx::TextRect(L"BUSCAR",s,lab,UiClassic()?ab:ToGdi(UI().bg),true,gfx::Center,true);
    }
    gfx::TextRect(L"✕",RF(wb.btnClose),S(13),white,false,gfx::Center,true);
    // grade com clip
    gfx::PushClip(RectF((float)b.left+S(8),(float)b.top+S(54),(float)bw-S(16),(float)bh-S(116)));
    {
        std::lock_guard<std::mutex> lk(wb.m);
        for(size_t i=0;i<wb.res.size()&&i<wb.cells.size();++i){
            RECT c=wb.cells[i];if(c.right-c.left<=0)continue;
            bool sel=wb.sel==(int)i;
            RectF cell=RF(c);
            Color cb=Cs(Argb(255,16,19,34),UI().surfaceHi), cpn=sel?ToGdi(g_theme.accent):Cs(Argb(255,45,49,72),UI().borderHi);
            DrawRoundRect(cell,S(8),&cb,&cpn,sel?2.4f:1.1f);
            Img* im=WebThumb(wb.res[i]); // sobe a miniatura (textura) na thread principal
            if(im&&im->ok){
                float iw=(float)im->w,ih=(float)im->h;
                float sc=std::min((cell.Width-S(8))/iw,(cell.Height-S(8))/ih);
                float dw=iw*sc,dh=ih*sc;
                gfx::DrawImg(im,RectF(cell.X+(cell.Width-dw)/2.f,cell.Y+(cell.Height-dh)/2.f,dw,dh));
            } else {
                gfx::TextRect(L"...",cell,sm,gray,false,gfx::Center,true);
            }
        }
    }
    gfx::PopClip();
    // status
    std::wstring status;
    {std::lock_guard<std::mutex> lk(wb.m);status=wb.status;}
    gfx::TextRect(status,RectF((float)b.left+S(16),(float)b.bottom-S(46),(float)bw-S(270),S(30)),sm,gray,false,gfx::Near,true,gfx::EllipsisChar);
    // USAR ESSA / CANCELAR
    {
        bool can=false;{std::lock_guard<std::mutex> lk(wb.m);can=(wb.sel>=0&&!wb.downloading);}
        RectF u=RF(wb.btnUse);
        Color ub=can?Cs(ToGdi(g_theme.accent,80),g_theme.accent):Cs(Argb(255,22,25,40),UI().surfaceHi);
        Color up=can?ToGdi(g_theme.accent):Cs(Argb(255,70,74,95),UI().borderHi);
        DrawRoundRect(u,S(9),&ub,&up,1.6f);
        gfx::TextRect(L"USAR ESSA",u,lab,can?(UiClassic()?ab:ToGdi(UI().bg)):gray,true,gfx::Center,true);
        RectF cn=RF(wb.btnCancel);
        Color nb=Cs(Argb(255,20,23,38),UI().surfaceHi), np=Cs(Argb(255,70,74,95),UI().borderHi);
        DrawRoundRect(cn,S(9),&nb,&np,1.4f);
        gfx::TextRect(L"CANCELAR",cn,lab,gray,true,gfx::Center,true);
    }
}

// ---- downloads (pilula no canto) e tela de busca online ------------------------------
static void DrawActivity(int w,int h){
    int waiting=0; float pct=0; std::wstring title;
    if(!LayoutActivity(w,h,waiting,pct,title)) return;
    RectF b=RF(R_activity); Color bg=Cs(Argb(235,12,15,30),UI().surface), pn=ToGdi(g_theme.accent);
    DrawRoundRect(b,S(10),&bg,&pn,1.2f);
    RectF bar(b.X+S(10),b.Y+b.Height-S(8),b.Width-S(20),S(3));
    gfx::FillRect(bar,Cs(Argb(255,40,44,65),UI().border)); gfx::FillRect(RectF(bar.X,bar.Y,bar.Width*std::max(0.f,std::min(1.f,pct/100.f)),bar.Height),ToGdi(g_theme.accent));
    wchar_t pc[16]; swprintf(pc,16,L"%.0f%%",pct);
    std::wstring t=L"↓ "+(title.empty()?std::wstring(L"na fila"):std::wstring(pc)+L"  "+title)+(waiting>0?L"   (+"+std::to_wstring(waiting)+L" na fila)":L"");
    gfx::TextRect(t,RectF(b.X+S(12),b.Y,b.Width-S(24),b.Height-S(4)),S(11),C_WHITE,false,gfx::Near,true,gfx::EllipsisChar);
}
static void DrawSpinner(float cx,float cy,float r,Color c,float thick){
    gfx::StrokeEllipse(RectF(cx-r,cy-r,2*r,2*r),thick,gfx::WithA(c,50));
    gfx::Arc(cx-r,cy-r,2*r,2*r,(float)(NowMs()%1000)*0.36f,110.f,thick,c);
}
// enquanto a busca nao volta: linhas-fantasma piscando + circulo girando + segundos
static void DrawOnlineLoading(bool link,int source,ULONGLONG since){
    RectF L=RF(R_onList); float rowH=S(58); int rows=std::max(1,(int)(L.Height/rowH));
    for(int k=0;k<rows;k++){
        RectF row(L.X+S(8),L.Y+k*rowH,L.Width-S(16),rowH-S(6));
        int a=(int)(10+18*(0.5f+0.5f*sinf(NowMs()/260.f-k*0.7f)));
        Color rb=Cs(Argb(255,16,19,34),UI().surfaceHi), rp=Cs(Argb(255,40,44,64),UI().border); DrawRoundRect(row,S(9),&rb,&rp,1.f);
        Color bar=UiClassic()?Argb(255,30+a,33+a,52+a):Argb(255,44+a,44+a,44+a);
        float th=row.Height-S(12), tx=row.X+S(12)+th;
        DrawRoundRect(RectF(row.X+S(6),row.Y+S(6),th,th),S(6),&bar,nullptr);
        DrawRoundRect(RectF(tx,row.Y+S(10),row.Width*(0.30f+0.05f*(k%3)),S(12)),S(5),&bar,nullptr);
        DrawRoundRect(RectF(tx,row.Y+S(30),row.Width*0.18f,S(9)),S(4),&bar,nullptr);
    }
    float cx=L.X+L.Width/2, cy=L.Y+L.Height*0.42f;
    Color panel=Cs(Argb(235,10,13,26),UI().surfaceHi), pn=Cs(ToGdi(g_theme.accent,120),UI().borderHi);
    DrawRoundRect(RectF(cx-S(200),cy-S(60),S(400),S(120)),S(14),&panel,&pn,1.2f);
    DrawSpinner(cx,cy-S(18),S(18),ToGdi(g_theme.accent),S(3.5f));
    static const wchar_t* srcN[3]={L"YouTube Music",L"YouTube",L"SoundCloud"};
    unsigned secs=since?(unsigned)((GetTickCount64()-since)/1000):0;
    std::wstring t=(link?std::wstring(L"Lendo o link"):L"Buscando no "+std::wstring(srcN[std::max(0,std::min(2,source))]))+L"...   "+std::to_wstring(secs)+L" s";
    gfx::TextRect(t,RectF(cx-S(190),cy+S(12),S(380),S(22)),S(12),C_WHITE,true,gfx::Center,true);
    gfx::TextRect(link?L"links do Spotify podem levar uns 20 s":L"costuma levar de 2 a 6 segundos",RectF(cx-S(190),cy+S(34),S(380),S(18)),S(10),C_GRAY2,false,gfx::Center,true);
}
static void DrawOnline(int w,int h){
    OnlineUI& u=OU();
    LayoutOnline(w,h);
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(200,2,4,10),UI().bg));
    RectF box=RF(u.box); Color pb=Cs(Argb(255,10,13,26),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(14),&pb,&apn,1.8f);
    Color white=C_WHITE, gray=C_GRAY2, ab=ToGdi(g_theme.accent);
    int tpl=OnlineTargetPl();
    gfx::TextRect(tpl>=0?L"BUSCAR ONLINE   →   "+g_playlists[(size_t)tpl].name:std::wstring(L"BUSCAR ONLINE  /  COLAR LINK"),RectF(box.X+S(18),box.Y+S(12),box.Width-S(80),S(34)),S(14),ab,true,gfx::Near,true,gfx::EllipsisChar);
    gfx::TextRect(L"✕",RF(u.btnClose),S(14),white,false,gfx::Center,true);
    {
        RectF q=RF(u.qbox); Color qb=Cs(Argb(255,18,21,36),UI().surfaceHi), qp=u.editing?ab:Cs(Argb(255,58,62,88),UI().borderHi);
        DrawRoundRect(q,S(9),&qb,&qp,u.editing?1.8f:1.2f);
        gfx::PushClip(q);
        float tw=gfx::TextWidth(u.query,S(12)), maxw=q.Width-S(24), off=tw>maxw?tw-maxw:0.f;
        if(u.query.empty()&&!u.editing) gfx::TextRect(L"Nome da música ou artista... ou cole um link (Spotify, YouTube, Deezer, Apple Music, SoundCloud)",RectF(q.X+S(12),q.Y,maxw,q.Height),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
        else gfx::Text(u.query,q.X+S(12)-off,q.Y+(q.Height-S(17))/2.f,S(12),white);
        if(u.editing&&(NowMs()/500)%2==0){ float cx=q.X+S(12)+tw-off; gfx::Line(cx+S(2),q.Y+S(9),cx+S(2),q.Y+q.Height-S(9),2,white); }
        gfx::PopClip();
    }
    { RectF sb=RF(u.btnSearch); Color f=Cs(ToGdi(g_theme.accent,55),g_theme.accent), fp=ToGdi(g_theme.accent); DrawRoundRect(sb,S(9),&f,&fp,1.4f); Color tc=UiClassic()?ab:ToGdi(UI().bg); if(u.busy.load()) DrawSpinner(sb.X+sb.Width/2,sb.Y+sb.Height/2,S(9),tc,S(2.5f)); else gfx::TextRect(L"BUSCAR",sb,S(12),tc,true,gfx::Center,true); }
    if(u.tipo==1&&!u.busy.load()){
        std::lock_guard<std::mutex> lk(u.m);
        if(u.listas.empty()) gfx::TextRect(L"Playlists do Deezer e do YouTube. Do Spotify, Apple Music ou SoundCloud: cole o link aqui em cima.",
            RectF((float)u.box.left+S(16),(float)u.tab[0].bottom+S(6),(float)(u.box.right-u.box.left)-S(32),S(18)),S(10),C_GRAY,false,gfx::Near,true,gfx::EllipsisChar);
    }
    static const wchar_t* tabN[3]={L"MÚSICAS",L"PLAYLISTS",L"ÁLBUNS"};
    for(int i=0;i<3;i++) DrawPill(u.tab[i],tabN[i],u.tipo==i,S(10));
    static const wchar_t* srcN[3]={L"YOUTUBE MUSIC",L"YOUTUBE",L"SOUNDCLOUD"};
    for(int i=0;i<3;i++) if(u.src[i].right>u.src[i].left) DrawPill(u.src[i],srcN[i],u.source==i,S(10));
    gfx::PushClip(RF(R_onList));
    if(u.tipo!=0){   // playlists ou albuns prontos: capa, nome e quantas musicas
        std::lock_guard<std::mutex> lk(u.m);
        for(size_t i=0;i<u.listas.size()&&i<u.rows.size();++i){
            RECT rr=u.rows[i]; if(rr.right<=rr.left) continue;
            const desc::Item& it=u.listas[i];
            RectF row=RF(rr);
            bool hot=UiHot(rr);
            Color rb=Cs(Argb(210,9,12,25),(hot?UI().surfaceHi:UI().surface));
            DrawRoundRect(row,S(UI_R_CARD),&rb,nullptr);
            float cv=row.Height-S(12);
            RectF art(row.X+S(8),row.Y+S(6),cv,cv);
            Color plate=Cs(Argb(255,18,21,34),UI().bg);
            DrawRoundRect(art,S(4),&plate,nullptr);
            if(!it.capa.empty()){
                std::wstring tf=OnlineThumbFile(it.capa);
                if(g_thumbReady.count(tf)){ Img* im=GetThumb(tf); if(im) gfx::DrawImgCover(im,art); }
                else { std::error_code ec; if(std::filesystem::exists(std::filesystem::path(tf),ec)) g_thumbReady.insert(tf); else g_rxThumbFila.push_back({it.capa,it.capa}); }
            }
            float tx=art.X+cv+S(12);
            gfx::TextRect(it.titulo,RectF(tx,row.Y+S(10),row.Width-(tx-row.X)-S(120),S(20)),S(13),C_WHITE,true,gfx::Near,false,gfx::EllipsisChar);
            gfx::TextRect(it.sub,RectF(tx,row.Y+S(30),row.Width-(tx-row.X)-S(120),S(17)),S(10),C_GRAY,false,gfx::Near,false,gfx::EllipsisChar);
            {   // de onde veio a lista, e o "abrir" quando o mouse passa
                const wchar_t* fonte = it.link.find(L"youtube")!=std::wstring::npos?L"YOUTUBE":
                                       (it.link.find(L"deezer")!=std::wstring::npos?L"DEEZER":L"ONLINE");
                RectF fr(row.X+row.Width-S(196),row.Y+row.Height/2-S(9),S(76),S(18));
                Color pill=Cs(Argb(255,24,27,44),UI().surfaceHi);
                DrawRoundRect(fr,S(9),&pill,nullptr);
                gfx::TextRect(fonte,fr,S(8.5f),C_GRAY,true,gfx::Center,true);
            }
            gfx::TextRect(hot?L"ABRIR ▸":L"",RectF(row.X+row.Width-S(110),row.Y,S(100),row.Height),S(10),ToGdi(g_theme.accent),true,gfx::Far,true);
        }
        RxBaixarCapasPendentes();
    } else {
        std::lock_guard<std::mutex> lk(u.m);
        for(size_t i=0;i<u.res.size()&&i<u.rows.size();++i){
            RECT rr=u.rows[i]; if(rr.right<=rr.left) continue;
            const OTrack& t=u.res[i];
            RectF row=RF(rr); Color rb=Cs(Argb(255,16,19,34),UI().surfaceHi), rp=Cs(Argb(255,45,49,72),UI().borderHi); DrawRoundRect(row,S(9),&rb,&rp,1.f);
            float th=row.Height-S(12); RectF art(row.X+S(6),row.Y+S(6),th,th);
            Color plate=Cs(Argb(255,24,27,44),UI().surfaceHi); DrawRoundRect(art,S(6),&plate,nullptr);
            if(!t.thumb.empty()){ std::wstring tf=OnlineThumbFile(t.thumb); if(g_thumbReady.count(tf)){ Img* im=GetThumb(tf); if(im) gfx::DrawImgCover(im,art); } }
            float tx=art.X+art.Width+S(12), tw=(float)u.bPlay[i].left-tx-S(10);
            gfx::TextRect(t.title.empty()?t.url:t.title,RectF(tx,row.Y+S(5),tw,S(24)),S(13),white,true,gfx::Near,true,gfx::EllipsisWord);
            std::wstring sub=t.artist;
            if(t.dur>0){ wchar_t d[16]; swprintf(d,16,L"%d:%02d",t.dur/60,t.dur%60); sub+=(sub.empty()?L"":L"  ·  ")+std::wstring(d); }
            sub+=(sub.empty()?L"":L"  ·  ")+std::wstring(SourceName(t.src));
            gfx::TextRect(sub,RectF(tx,row.Y+S(29),tw,S(18)),S(10),UiClassic()?ab:gray,false,gfx::Near,true,gfx::EllipsisChar);
            auto ib=[&](const RECT& r,const wchar_t* s,bool primary){ RectF b=RF(r); Color f=primary?Cs(ToGdi(g_theme.accent,60),g_theme.accent):Cs(Argb(255,24,27,42),UI().surfaceHi), p=primary?ToGdi(g_theme.accent):Cs(Argb(255,70,74,95),UI().borderHi); if(UiClassic()) DrawRoundRect(b,S(8),&f,&p,1.2f); else DrawRoundRect(b,S(UI_R_PILL),&f,nullptr); gfx::TextRect(s,b,S(14),primary?(UiClassic()?ab:ToGdi(UI().bg)):white,true,gfx::Center,true); };
            ib(u.bPlay[i],L"▶",true); ib(u.bAdd[i],L"+",false); ib(u.bDl[i],L"↓",false);   // tocar e a acao principal; salvar uma copia e secundaria
        }
        if(u.res.empty()&&u.busy.load()) DrawOnlineLoading(u.fromLink,u.source,u.busySince);
        else if(u.btnCfg.right>u.btnCfg.left){                  // sem CLI configurada: explica e leva as Configuracoes
            RectF la=RF(R_onList); float cy=la.Y+la.Height/2;
            gfx::TextRect(L"Fontes externas desligadas",RectF(la.X,cy-S(84),la.Width,S(28)),S(17),white,true,gfx::Center,true);
            gfx::TextRect(L"Para ouvir de fontes externas, o Remix executa um programa de linha de comando que você instala.",
                          RectF(la.X+S(30),cy-S(52),la.Width-S(60),S(22)),S(11.5f),gray,false,gfx::Center,true,gfx::EllipsisWord);
            gfx::TextRect(L"Nada é baixado nem instalado pelo app.",
                          RectF(la.X+S(30),cy-S(32),la.Width-S(60),S(22)),S(11.5f),gray,false,gfx::Center,true,gfx::EllipsisWord);
            RectF cb=RF(u.btnCfg); Color cf=Cs(ToGdi(g_theme.accent,55),g_theme.accent), cp=ToGdi(g_theme.accent);
            DrawRoundRect(cb,S(9),&cf,&cp,1.4f);
            gfx::TextRect(L"CONFIGURAR FONTE EXTERNA",cb,S(12),UiClassic()?ab:ToGdi(UI().bg),true,gfx::Center,true);
        }
    }
    gfx::PopClip();
    std::wstring st; bool fromLink=false; size_t n=0;
    { std::lock_guard<std::mutex> lk(u.m); st=u.status; fromLink=u.fromLink; n=u.res.size(); }
    bool hasAll=u.btnAddAll.right>u.btnAddAll.left;
    float footW=hasAll?(float)(u.btnAddAll.left-u.box.left)-S(30):box.Width-S(32);
    float fx=box.X+S(16); bool busy=u.busy.load();
    if(busy){ DrawSpinner(fx+S(8),box.Y+box.Height-S(32),S(8),ab,S(2.5f)); fx+=S(26); }
    gfx::TextRect(st+(n&&!busy?L"   ·   ▶ toca  +  playlist  ↓ salva uma cópia":L""),RectF(fx,box.Y+box.Height-S(52),footW-(fx-box.X-S(16)),S(40)),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
    if(hasAll) DrawPill(u.btnAddAll,tpl>=0?L"ADICIONAR TODAS NA PLAYLIST":(fromLink?L"SALVAR COMO PLAYLIST":L"ADICIONAR TODAS..."),true,S(11));
}
// ---- painel HOST (acesso pelo celular) --------------------------------------
static void DrawFxPanel(int w,int h){
    FxPanelUI& p=g_fxp; LayoutFxPanel(w,h);
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(200,2,4,10),UI().bg,200));
    RectF box=RF(p.box); Color pb=Cs(Argb(255,10,13,26),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(14),&pb,&apn,1.8f);
    Color white=C_WHITE, gray=C_GRAY2, ab=ToGdi(g_theme.accent);
    gfx::Text(L"EFEITOS  ·  STEMS",box.X+S(18),box.Y+S(14),S(14),UiClassic()?ab:white,true);
    gfx::TextRect(L"✕",RF(p.btnClose),S(14),white,false,gfx::Center,true);
    gfx::TextRect(L"Cada clique sobe o nível (● ○ ○ → ● ● ●) e o seguinte desliga. Slow e speed não somam.",RectF(box.X+S(18),box.Y+S(44),box.Width-S(36),S(20)),S(11),gray,false,gfx::Near,true,gfx::EllipsisChar);
    for(int i=0;i<5;i++){
        int lv=FxLevel(i); std::wstring dots; for(int k=1;k<=3;k++) dots+=(k<=lv?L"●":L"○");
        DrawPill(p.fx[i],std::wstring(FxName(i))+L"  "+dots,lv>0,S(11));
    }
    DrawPill(p.btnClear,L"DESLIGAR EFEITOS",false,S(10));
    gfx::Text(L"STEMS: separar a música",box.X+S(18),(float)p.stem[0].top-S(28),S(12),white,true);
    int cur=StemModeNow();
    for(int i=0;i<6;i++) DrawPill(p.stem[i],stems::ModeName(i),i==cur,S(10));
    std::wstring l1,l2; FxStemStatus(l1,l2);
    RectF inf=RF(p.info);
    gfx::TextRect(l1,RectF(inf.X,inf.Y,inf.Width,S(20)),S(11),white,false,gfx::Near,true,gfx::EllipsisChar);
    if(!l2.empty()) gfx::TextRect(l2,RectF(inf.X,inf.Y+S(22),inf.Width,S(20)),S(10.5f),gray,false,gfx::Near,true,gfx::EllipsisChar);
    if(StemJobActive()) DrawPill(p.btnCancel,L"CANCELAR",false,S(10));
}
// ---- paineis montados no codigo comum (SOUNDPAD, DISCORD: app_panels.h) ------
static Color PanelColor(int c){
    switch(c){ case PCL_GRAY: return C_GRAY2; case PCL_ACCENT: return ToGdi(g_theme.accent); case PCL_TITLE: return UiClassic()?ToGdi(g_theme.accent):C_WHITE;
               case PCL_DANGER: return Argb(255,255,120,130); case PCL_OK: return Argb(255,110,220,150); default: return C_WHITE; }
}
static void TextTwoLines(const std::wstring& s,const RectF& rc,float px,Color c,bool bold){   // ate 2 linhas, quebra na palavra
    float lh=gfx::LineHeight(px);
    if(gfx::TextWidth(s,px,bold)<=rc.Width){ gfx::Text(s,rc.X,rc.Y,px,c,bold); return; }
    size_t cut=0;
    for(size_t i=1;i<=s.size();++i) if(i==s.size()||s[i]==L' '){ if(gfx::TextWidth(s.substr(0,i),px,bold)<=rc.Width) cut=i; else break; }
    if(cut==0){ size_t n=1; while(n<s.size()&&gfx::TextWidth(s.substr(0,n+1),px,bold)<=rc.Width) n++; cut=n; }
    std::wstring l2=s.substr(cut); while(!l2.empty()&&l2[0]==L' ') l2.erase(0,1);
    gfx::Text(s.substr(0,cut),rc.X,rc.Y,px,c,bold);
    gfx::TextRect(l2,RectF(rc.X,rc.Y+lh,rc.Width,lh),px,c,bold,gfx::Near,false,gfx::EllipsisChar);
}
static void DrawPanel(const Panel& p){
    Color ab=ToGdi(g_theme.accent);
    for(auto& it:p.items){
        RectF r=RF(it.r);
        switch(it.kind){
        case PK_DIM: gfx::FillRect(r.X,r.Y,r.Width,r.Height,Cs(Argb(200,2,4,10),UI().bg,200)); break;
        case PK_BOX: { Color pb=Cs(Argb(255,10,13,26),UI().surface), apn=Cs(ab,UI().borderHi); DrawRoundRect(r,S(14),&pb,&apn,1.8f); } break;
        case PK_TEXT: gfx::TextRect(it.text,r,it.px,PanelColor(it.color),it.bold,it.center?gfx::Center:gfx::Near,true,gfx::EllipsisChar); break;
        case PK_PILL: DrawPill(it.r,it.text,it.on,it.px); break;
        case PK_ROW: { Color rb=it.on?Cs(ToGdi(g_theme.accent,38),UI().surfaceHi):Cs(Argb(255,16,19,34),UI().surfaceHi); DrawRoundRect(r,S(UI_R_PILL),&rb,nullptr); } break;
        case PK_CLIP: gfx::PushClip(r); break;
        case PK_UNCLIP: gfx::PopClip(); break;
        case PK_BAR: { gfx::FillRect(r,Cs(Argb(255,40,44,65),UI().border)); if(it.v>0) gfx::FillRect(RectF(r.X,r.Y,r.Width*std::min(1.f,it.v),r.Height),ab); } break;
        case PK_TILE: {
            Color fb=it.on?Cs(ToGdi(g_theme.accent,70),UI().surfaceHi):Cs(Argb(255,16,19,34),UI().surfaceHi), ln=it.on?ab:Cs(Argb(255,50,54,76),UI().border);
            if(UiClassic()) DrawRoundRect(r,S(10),&fb,&ln,1.4f); else DrawRoundRect(r,S(UI_R_CARD),&fb,it.on?&ln:nullptr,1.6f);
            TextTwoLines(it.text,RectF(r.X+S(10),r.Y+S(8),r.Width-S(40),S(40)),S(12),C_WHITE,true);
            if(!it.sub.empty()) gfx::TextRect(it.sub,RectF(r.X+S(72),r.Y+r.Height-S(28),r.Width-S(82),S(20)),S(9),C_GRAY2,false,gfx::Far,true,gfx::EllipsisChar);
            if(it.v>=0){ RectF pb(r.X+S(8),r.Y+r.Height-S(5),r.Width-S(16),S(3)); gfx::FillRect(pb,Cs(Argb(255,40,44,65),UI().border)); gfx::FillRect(RectF(pb.X,pb.Y,pb.Width*std::min(1.f,it.v),pb.Height),ab); }
        } break;
        default: break;
        }
    }
}
static void DrawSpadPanel(int w,int h){ BuildSpadPanel(w,h); DrawPanel(g_spadP); }
static void DrawDcPanel(int w,int h){ BuildDcPanel(w,h); DrawPanel(g_dcP); }
static void DrawHostPanel(int w,int h){
    host::PanelUI& p=host::PU(); LayoutHostPanel(w,h); const host::View& v=p.v;
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(200,2,4,10),UI().bg,200));
    RectF box=RF(p.box); Color pb=Cs(Argb(255,10,13,26),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(14),&pb,&apn,1.8f);
    Color white=C_WHITE, gray=C_GRAY2, ab=ToGdi(g_theme.accent);
    gfx::Text(L"HOST  ·  ACESSO PELO CELULAR",box.X+S(18),box.Y+S(14),S(14),UiClassic()?ab:white,true);
    gfx::TextRect(L"✕",RF(p.btnClose),S(14),white,false,gfx::Center,true);
    bool on=v.running;
    DrawPill(p.btnToggle,on?L"DESLIGAR":L"LIGAR",on,S(11));
    DrawPill(p.btnTunnel,v.tunRunning?L"TÚNEL: LIGADO":L"TÚNEL: DESLIGADO",v.tunRunning,S(10));
    DrawPill(p.btnNewLink,L"NOVO LINK",false,S(10));
    DrawPill(p.btnHtml,L"HTML P/ WHATSAPP",false,S(10));
    DrawPill(p.btnPasta,L"ABRIR PASTA",false,S(10));
    DrawPill(p.btnPort,L"PORTA: "+std::to_wstring(g_cfg.hostPort),false,S(10));
    DrawPill(p.btnPin,g_cfg.hostPin.empty()?L"PIN: DEFINIR...":L"PIN: "+std::wstring(g_cfg.hostPin.size(),L'•'),!g_cfg.hostPin.empty(),S(10));
    DrawPill(p.btnName,L"NOME: "+(g_cfg.hostName.empty()?Utf8ToWide(hostnet::HostName()):g_cfg.hostName),false,S(10));
    DrawPill(p.btnLan,g_cfg.hostLan?L"REDE LOCAL: SIM":L"REDE LOCAL: NÃO",g_cfg.hostLan,S(10));
    DrawPill(p.btnIpv6,g_cfg.hostIPv6?L"IPv6: SIM":L"IPv6: NÃO",g_cfg.hostIPv6,S(10));
    // QR code (fundo branco + zona de silencio de 4 modulos)
    {
        RectF q=RF(p.qrBox); Color wb=Argb(255,255,255,255);
        if(!p.qr.modules.empty()&&p.qr.size>0){
            int n=p.qr.size+8; float m=std::floor(q.Width/(float)n); if(m<1.f) m=1.f;
            float side=m*n, ox=q.X+(q.Width-side)/2.f, oy=q.Y+(q.Height-side)/2.f;
            gfx::FillRect(ox,oy,side,side,wb);
            Color blk=Argb(255,0,0,0);
            for(int yy=0;yy<p.qr.size;yy++) for(int xx=0;xx<p.qr.size;xx++) if(p.qr.get(xx,yy)) gfx::FillRect(ox+(xx+4)*m,oy+(yy+4)*m,m,m,blk);
        } else {
            Color ph=Cs(Argb(255,16,19,34),UI().surfaceHi); DrawRoundRect(q,S(UI_R_CARD),&ph,nullptr);
            gfx::TextRect(on?L"Gerando o QR...":L"Ligue o Host para ver o QR code",RectF(q.X+S(10),q.Y,q.Width-S(20),q.Height),S(11),gray,false,gfx::Center,true,gfx::EllipsisChar);
        }
    }
    {   // estado ao lado do QR
        RectF inf=RF(p.info); bool viaTun=p.qrText.rfind("https://",0)==0; long long left=host::QrSecondsLeft();
        std::wstring dnome; if(!p.qrDev.empty()) for(auto& dd:v.devs) if(dd.id==p.qrDev) dnome=Utf8ToWide(dd.name);
        std::wstring l1=!on?L"Desligado"+std::wstring(v.lastError.empty()?L"":L"  ·  "+Utf8ToWide(v.lastError))
                        :p.qrText.empty()?L"Sem link para o QR ainda (ligue REDE LOCAL ou espere o túnel)."
                        :!p.qrDev.empty()?L"QR para religar \""+dnome+L"\"  ·  "+(viaTun?L"pela internet":L"rede local")+L"  ·  vale sempre (NOVO QR volta ao normal)"
                        :std::wstring(L"Escaneie com a câmera do celular  ·  ")+(viaTun?L"pela internet":L"rede local")+L"  ·  uso único, vale "+std::to_wstring(left/60)+L" min";
        std::wstring l2=L"Local: "+(v.lanUrls.empty()?std::wstring(g_cfg.hostLan?L"nenhum IP de rede local":L"desligada"):Utf8ToWide(v.lanUrls[0]))+(v.lan6Urls.empty()?L"":L"  ·  IPv6 ligado");
        std::wstring l3=L"Túnel: "+(v.tunUrl.empty()?Utf8ToWide(v.tunStatus):Utf8ToWide(v.tunUrl));
        gfx::TextRect(l1,RectF(inf.X,inf.Y,inf.Width,S(18)),S(11),on?white:gray,true,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(l2,RectF(inf.X,inf.Y+S(21),inf.Width,S(18)),S(11),gray,false,gfx::Near,false,gfx::EllipsisChar);
        gfx::TextRect(l3,RectF(inf.X,inf.Y+S(40),inf.Width,S(18)),S(11),v.tunUrl.empty()?gray:ab,false,gfx::Near,false,gfx::EllipsisChar);
    }
    DrawPill(p.btnCopyTun,v.tunUrl.empty()?(v.tunPending.empty()?L"COPIAR LINK DO TÚNEL":L"TESTANDO O LINK..."):L"COPIAR LINK DO TÚNEL",!v.tunUrl.empty(),S(10));
    DrawPill(p.btnCopyLan,L"COPIAR LINK LOCAL",!v.lanUrls.empty(),S(10));
    DrawPill(p.btnQrMode,p.qrTunnel?L"QR: INTERNET":L"QR: REDE LOCAL",false,S(10));
    DrawPill(p.btnQrNew,L"NOVO QR",false,S(10));
    DrawPill(p.btnOnline,g_cfg.hostOnline?L"ONLINE NO CELULAR: SIM":L"ONLINE NO CELULAR: NÃO",g_cfg.hostOnline,S(10));
    DrawPill(p.btnQrConfirm,g_cfg.hostQrConfirm?L"QR PEDE ACEITE: SIM":L"QR PEDE ACEITE: NÃO",g_cfg.hostQrConfirm,S(10));
    // lista com rolagem
    gfx::PushClip(RF(p.list));
    float ix=box.X+S(18), iw=box.Width-S(36);
    float y=(float)p.list.top-p.scroll, rowH=S(34); float x=ix;
    auto title=[&](const std::wstring& t){ gfx::Text(t,x,y+S(4),S(11),UiClassic()?ab:white,true); y+=S(26); };
    auto rowBg=[&](float yy){ RectF r(x,yy,iw,rowH-S(6)); Color rb=Cs(Argb(255,16,19,34),UI().surfaceHi); DrawRoundRect(r,S(UI_R_PILL),&rb,nullptr); };
    title(L"PEDIDOS PARA CONECTAR ("+std::to_wstring(v.pending.size())+L")");
    for(size_t i=0;i<v.pending.size()&&i<p.accept.size();i++){
        rowBg(y); const host::PairReq& q=v.pending[i];
        gfx::TextRect(Utf8ToWide(q.name)+L"   ·   "+Utf8ToWide(q.ip)+(q.viaTunnel?L" (internet)":L" (rede local)")+(q.viaQr?L"  ·  QR":L"  ·  PIN"),RectF(x+S(10),y,iw-S(210),rowH-S(6)),S(11),white,false,gfx::Near,true,gfx::EllipsisChar);
        DrawPill(p.accept[i],L"ACEITAR",true,S(10)); DrawPill(p.deny[i],L"RECUSAR",false,S(10)); y+=rowH;
    }
    title(L"APARELHOS VINCULADOS ("+std::to_wstring(v.devs.size())+L")");
    if(v.devs.empty()){ gfx::Text(L"Nenhum ainda. Escaneie o QR code com o celular.",x+S(10),y+S(6),S(11),gray); y+=rowH; }
    for(size_t i=0;i<v.devs.size()&&i<p.revoke.size();i++){
        rowBg(y); const host::Device& d=v.devs[i]; long long ago=host::NowSec()-d.lastSeen; std::wstring seen=ago<120?L"agora":ago<3600?std::to_wstring(ago/60)+L" min atrás":ago<86400?std::to_wstring(ago/3600)+L" h atrás":std::to_wstring(ago/86400)+L" d atrás";
        gfx::TextRect(Utf8ToWide(d.name)+(d.persist?L"":L" (temporário)")+L"   ·   "+Utf8ToWide(d.ip)+L"   ·   visto "+seen,RectF(x+S(10),y,iw-S(390),rowH-S(6)),S(11),white,false,gfx::Near,true,gfx::EllipsisChar);
        if(i<p.devLink.size()) DrawPill(p.devLink[i],L"LINK",p.qrDev==d.id,S(10));
        if(i<p.devLib.size()) DrawPill(p.devLib[i],d.lib?L"BIBLIOTECA: SIM":L"BIBLIOTECA: NÃO",d.lib,S(10));
        DrawPill(p.revoke[i],L"REMOVER",false,S(10)); y+=rowH;
    }
    title(L"PLAYLISTS DO PC NO CELULAR");
    if(g_playlists.empty()){ gfx::Text(L"Você não tem playlists. Crie uma na aba PLAYLISTS e hosteie aqui.",x+S(10),y+S(6),S(11),gray); y+=rowH; }
    for(size_t i=0;i<g_playlists.size()&&i<p.plHost.size();i++){
        rowBg(y); std::string t=host::Targets(g_playlists[i].slug);
        gfx::TextRect(g_playlists[i].name,RectF(x+S(10),y,iw-S(150),rowH-S(6)),S(11),white,true,gfx::Near,true,gfx::EllipsisChar);
        std::wstring lab=t.empty()?L"HOST: NÃO":t=="ALL"?L"HOST: TODOS":L"HOST: ALGUNS";
        DrawPill(p.plHost[i],lab,!t.empty(),S(10)); y+=rowH;
        for(size_t j=0;j<p.plDev[i].size()&&j<v.devs.size();j++){
            bool sel=t=="ALL"||(","+t+",").find(","+v.devs[j].id+",")!=std::string::npos;
            DrawPill(p.plDev[i][j],Utf8ToWide(v.devs[j].name),sel,S(9));
        }
        if(!p.plDev[i].empty()) y=(float)p.plDev[i].back().bottom+S(8);
    }
    title(L"PLAYLISTS DOS APARELHOS (cada uma é só do dono, a não ser que você libere)");
    if(v.dpls.empty()){ gfx::Text(L"Nenhuma. O celular cria as dele; elas ficam guardadas aqui, separadas por aparelho.",x+S(10),y+S(6),S(11),gray); y+=rowH; }
    for(size_t i=0;i<v.dpls.size()&&i<p.dplOk.size();i++){
        const host::DevPlaylist& dp=v.dpls[i]; rowBg(y);
        std::wstring dn=L"?"; for(auto& d:v.devs) if(d.id==dp.dev) dn=Utf8ToWide(d.name);
        std::wstring st=!dp.share?L"privada":dp.pcOk?L"compartilhada com os outros aparelhos":L"pediu para compartilhar";
        gfx::TextRect(Utf8ToWide(dp.name)+L"   ·   "+dn+L"   ·   "+std::to_wstring(dp.ids.size())+L" faixas   ·   "+st,RectF(x+S(10),y,iw-S(140),rowH-S(6)),S(11),dp.share&&!dp.pcOk?white:gray,false,gfx::Near,true,gfx::EllipsisChar);
        if(p.dplOk[i].right>p.dplOk[i].left) DrawPill(p.dplOk[i],dp.pcOk?L"BLOQUEAR":L"LIBERAR",!dp.pcOk,S(10));
        y+=rowH;
    }
    gfx::PopClip();
}
// ---- menus flutuantes / confirmacao (Linux) --------------------------------
static void DrawMenuBox(const RectF& box){
    Color pb=Cs(Argb(255,12,15,30),UI().surface), apn=Cs(ToGdi(g_theme.accent),UI().borderHi);
    DrawRoundRect(box,S(10),&pb,&apn,1.4f);
}
static void DrawFolderMenu(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(90,2,4,10),UI().bg,90));
    DrawMenuBox(RF(R_folderBox));
    for(size_t i=0;i<R_folderItems.size()&&i<g_folderItemPaths.size();++i){
        RectF r=RF(R_folderItems[i]);
        const std::wstring& p=g_folderItemPaths[i];
        bool cur=(!p.empty()&&p!=L"*"&&_wcsicmp(p.c_str(),g_cfg.musicFolder.c_str())==0)||(p==L"*"&&g_cfg.musicFolder.empty());
        Color fb=cur?Cs(ToGdi(g_theme.accent,50),UI().surfaceHi):Cs(Argb(255,20,23,38),UI().surface), ln=cur?ToGdi(g_theme.accent):Cs(Argb(255,50,54,76),UI().border);
        DrawRoundRect(r,S(7),&fb,&ln,1.f);
        std::wstring label=p.empty()?L"Escolher outra pasta...":(p==L"*"?L"Padrão: Músicas, Downloads, Documentos, Área de trabalho":std::filesystem::path(p).filename().wstring()+L"   ("+p+L")");
        gfx::TextRect(label,RectF(r.X+S(10),r.Y,r.Width-S(16),r.Height),S(11),cur?(UiClassic()?ToGdi(g_theme.accent):C_WHITE):C_WHITE,p.empty()||p==L"*",gfx::Near,true,gfx::EllipsisPath);
    }
}
static void DrawCtxMenu(int w,int h){
    (void)w;(void)h;
    DrawMenuBox(RF(R_ctxBox));
    for(size_t i=0;i<R_ctxItems.size()&&i<g_ctxLabels.size();++i){
        RectF r=RF(R_ctxItems[i]);
        bool danger=i<g_ctxDanger.size()&&g_ctxDanger[i];
        Color fb=Cs(Argb(255,20,23,38),UI().surfaceHi), ln=danger?Argb(255,120,50,64):Cs(Argb(255,50,54,76),UI().borderHi);
        DrawRoundRect(r,S(6),&fb,&ln,1.f);
        gfx::TextRect(g_ctxLabels[i],RectF(r.X+S(10),r.Y,r.Width-S(12),r.Height),S(11),danger?Argb(255,255,120,130):C_WHITE,false,gfx::Near,true);
    }
}
static void DrawConfirm(int w,int h){
    gfx::FillRect(0,0,(float)w,(float)h,Cs(Argb(170,2,4,10),UI().bg,170));
    OpenConfirm();
    RectF box=RF(R_confirmBox);
    Color pb=Cs(Argb(255,12,15,30),UI().surface), apn=Argb(255,190,70,90);
    DrawRoundRect(box,14,&pb,&apn,2);
    gfx::Text(g_confirmKind==2?L"NOVO DISPOSITIVO QUER SE CONECTAR":g_confirmKind==1?L"EXCLUIR PLAYLIST":L"EXCLUIR MÚSICA",box.X+22,box.Y+18,S(13),g_confirmKind==2?ToGdi(g_theme.accent):Argb(255,255,120,130),true);
    gfx::TextRect(g_confirmText,RectF(box.X+22,box.Y+52,box.Width-44,60),S(11),C_WHITE,false,gfx::Near,false,gfx::EllipsisChar);
    gfx::Text(g_confirmKind==2?L"Aceite só se for você. Dá para remover o aparelho depois, no painel HOST.":L"O arquivo vai para a lixeira do sistema (da para recuperar).",box.X+22,box.Y+82,S(10),C_GRAY2);
    auto cbtn=[&](RECT r,const wchar_t* t,bool primary){
        RectF b=RF(r);
        Color fb=primary?Argb(255,90,30,40):Cs(Argb(255,24,27,42),UI().surfaceHi), p=primary?Argb(255,220,80,100):Cs(Argb(255,70,74,95),UI().borderHi);
        DrawRoundRect(b,8,&fb,&p,1.5f);
        gfx::TextRect(t,b,S(10),primary?C_WHITE:C_GRAY2,false,gfx::Center,true);
    };
    if(g_confirmKind==2){ DrawPill(R_confirmYes,L"ACEITAR",true,S(10)); DrawPill(R_confirmNo,L"RECUSAR",false,S(10)); }
    else { cbtn(R_confirmYes,L"EXCLUIR",true); cbtn(R_confirmNo,L"CANCELAR",false); }
}
