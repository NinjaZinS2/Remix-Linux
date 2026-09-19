#pragma once
// Paineis SOUNDPAD (sons no microfone) e DISCORD (bot de musica do dono), montados com app_panel.h:
// o mesmo codigo desenha e responde ao clique no Linux e no Windows. So no PC (o celular nao tem isso).
#include "app_panel.h"

enum : int {
    Z_SP_CLOSE = 24010, Z_SP_ONOFF, Z_SP_VOICE, Z_SP_MON, Z_SP_VOL, Z_SP_STOPALL, Z_SP_ADD, Z_SP_OUT, Z_SP_IN, Z_SP_FOLDER, Z_SP_CABLE,
    Z_SP_TILE_BASE = 24100, Z_SP_TVOL_BASE = 24600, Z_SP_DEL_BASE = 25100,   // +500 sons (ate 25600)
    Z_DC_CLOSE = 26010, Z_DC_ONOFF, Z_DC_TOKEN, Z_DC_INVITE, Z_DC_INSTALL, Z_DC_PORTAL, Z_DC_QUEUE, Z_DC_PLS, Z_DC_ONLINE, Z_DC_VOTE, Z_DC_DJ, Z_DC_LIMIT, Z_DC_CLEARTOKEN, Z_DC_RPC,
    Z_DC_G_PAUSE_BASE = 26100, Z_DC_G_SKIP_BASE = 26150, Z_DC_G_STOP_BASE = 26200, Z_DC_G_LOOP_BASE = 26250,   // +50 servidores
    Z_DC_PL_BASE = 26300                                                                                    // +500 playlists (ate 26800)
};
static_assert(Z_SP_CLOSE > Z_SET_DC && Z_SP_DEL_BASE + 500 <= Z_DC_CLOSE && Z_DC_CLEARTOKEN < Z_DC_G_PAUSE_BASE && Z_DC_G_LOOP_BASE + 50 <= Z_DC_PL_BASE && Z_DC_PL_BASE + 500 < Z_COVER_BASE, "zonas do soundpad/discord sobrepostas");

static Panel g_spadP, g_dcP;
static std::vector<std::wstring> g_spadIds;        // som de cada ladrilho (na ordem da tela)
static std::vector<std::string> g_dcGuildIds;      // servidor de cada linha do painel DISCORD
static std::vector<std::wstring> g_dcPlSlugs;      // playlist de cada linha

static void RedrawSoon(){ AppPost(EV_REDRAW); }
static bool SpadPanelIsOpen(){ return g_spadP.open; }

// ---- SOUNDPAD -----------------------------------------------------------------------------------
static void BuildSpadPanel(int w,int h){
    Panel& p=g_spadP; spad::View v=spad::GetView();
    RECT a=p.Frame(w,h,SI(900),SI(700),L"SOUNDPAD  ·  SONS NO MICROFONE",Z_SP_CLOSE);
    int x=a.left, cw=a.right-a.left, y=a.top, g=SI(8), bh=SI(34);
#ifdef _WIN32
    const wchar_t* help=L"Usa um cabo de áudio virtual (VB-CABLE): o Remix toca em \"CABLE Input\" e no Discord/jogo você escolhe \"CABLE Output\" como microfone.";
#else
    const wchar_t* help=L"Ligado, aparece o microfone \"Remix Microfone\": escolha ele no Discord/jogo. Desligando, ele some.";
#endif
    p.Text({x,y,x+cw,y+SI(20)},help,S(11),PCL_GRAY);
    y+=SI(28);
    auto cell=[&](int k,int n){ int bw=(cw-g*(n-1))/n; int xx=x+k*(bw+g); return RECT{xx,y,k==n-1?x+cw:xx+bw,y+bh}; };
    p.Pill(cell(0,5),v.busy?L"AGUARDE...":(v.running?L"DESLIGAR MICROFONE":L"LIGAR MICROFONE"),v.running,Z_SP_ONOFF,S(10));
    p.Pill(cell(1,5),v.voice?L"MINHA VOZ: SIM":L"MINHA VOZ: NÃO",v.voice,Z_SP_VOICE);
    p.Pill(cell(2,5),v.monitor?L"OUVIR NO FONE: SIM":L"OUVIR NO FONE: NÃO",v.monitor,Z_SP_MON);
    p.Pill(cell(3,5),L"VOLUME: "+std::to_wstring(v.master)+L"%",false,Z_SP_VOL);
    p.Pill(cell(4,5),L"PARAR TODOS",!v.playing.empty(),Z_SP_STOPALL);
    y+=bh+g;
#ifdef _WIN32
    const int nrow=5;   // Windows: atalho para baixar o cabo virtual (driver gratuito da VB-Audio)
    p.Pill(cell(4,5),L"BAIXAR VB-CABLE",false,Z_SP_CABLE);
#else
    const int nrow=4;
#endif
    p.Pill(cell(0,nrow),L"+ ADICIONAR SONS",true,Z_SP_ADD);
    p.Pill(cell(1,nrow),L"SAÍDA: "+(v.outName.empty()?std::wstring(L"AUTOMÁTICA"):v.outName),!v.outName.empty(),Z_SP_OUT);
    p.Pill(cell(2,nrow),L"MICROFONE: "+(v.inName.empty()?std::wstring(L"PADRÃO"):v.inName),!v.inName.empty(),Z_SP_IN);
    p.Pill(cell(3,nrow),L"ABRIR PASTA",false,Z_SP_FOLDER);
    y+=bh+SI(10);
    std::wstring st;
    if(v.busy) st=L"Ligando/desligando o microfone virtual...";
    else if(v.running) st=L"Microfone ligado em \""+v.device+L"\""+(v.voice?(v.voiceOk?L"  ·  voz: "+v.inDevice:std::wstring()):std::wstring(L"  ·  sem a sua voz"))+L"  ·  "+std::to_wstring(v.playing.size())+L" tocando";
    else st=L"Microfone desligado: os sons tocam só no seu fone (bom para testar).";
    p.Text({x,y,x+cw,y+SI(18)},st,S(11),v.running?PCL_WHITE:PCL_GRAY,v.running);
    y+=SI(20);
    if(!v.status.empty()){ p.Text({x,y,x+cw,y+SI(18)},v.status,S(11),PCL_DANGER); y+=SI(20); }
    p.Text({x,y,x+cw,y+SI(16)},L"Clique num som para tocar/parar  ·  teclas 1 a 9 com o painel aberto  ·  % muda o volume do som  ·  o X do canto remove",S(10),PCL_GRAY);
    y+=SI(24);
    p.list={a.left-SI(6),y,a.right+SI(6),a.bottom};
    g_spadIds.clear();
    int tileW0=SI(170), cols=std::max(1,(cw+g)/(tileW0+g)), tw=(cw-(cols-1)*g)/cols, th=SI(88);
    int n=(int)v.sounds.size();
    p.contentH=n?((n+cols-1)/cols)*(th+g):SI(40);
    p.ClampScroll();
    p.Clip(p.list);
    if(!n) p.Text({x,y+SI(8),x+cw,y+SI(30)},L"Nenhum som ainda. Toque em + ADICIONAR SONS (mp3, wav, ogg, flac; outros formatos são convertidos).",S(11),PCL_GRAY);
    for(int i=0;i<n&&i<500;i++){
        const spad::Sound& s=v.sounds[(size_t)i];
        int tx=x+(i%cols)*(tw+g), ty=y+(i/cols)*(th+g)-p.scroll;
        g_spadIds.push_back(s.id);
        if(ty+th<p.list.top||ty>p.list.bottom) continue;
        bool playing=std::find(v.playing.begin(),v.playing.end(),s.id)!=v.playing.end();
        PItem& t=p.Add(PK_TILE,RECT{tx,ty,tx+tw,ty+th});
        t.text=s.name; t.sub=i<9?L"tecla "+std::to_wstring(i+1):L""; t.on=playing; t.v=playing?spad::Progress(s.id):-1.f; t.zone=Z_SP_TILE_BASE+i;
        p.Pill({tx+SI(8),ty+th-SI(30),tx+SI(8)+SI(58),ty+th-SI(8)},std::to_wstring(s.vol)+L"%",s.vol<100,Z_SP_TVOL_BASE+i,S(9));
        PItem& d=p.Add(PK_TEXT,RECT{tx+tw-SI(30),ty+SI(4),tx+tw-SI(4),ty+SI(28)}); d.text=L"✕"; d.px=S(11); d.center=true; d.color=PCL_GRAY; d.zone=Z_SP_DEL_BASE+i;
    }
    p.Unclip();
}
static void SpadToggleSound(size_t i){
    if(i>=g_spadIds.size()) return;
    std::wstring err; if(!spad::Toggle(g_spadIds[i],err)&&!err.empty()) SetStatus(err,3000);
}
static void SpadClick(int z){
    Panel& p=g_spadP;
    if(z==Z_SP_CLOSE){ p.open=false; return; }
    if(z==Z_SP_ONOFF){ bool on=!spad::Running(); spad::SetOnAsync(on,[]{ RedrawSoon(); }); SetStatus(on?L"Ligando o microfone virtual...":L"Microfone virtual desligado.",2200); return; }
    if(z==Z_SP_VOICE){ spad::SetVoice(!spad::GetView().voice); return; }
    if(z==Z_SP_MON){ spad::SetMonitor(!spad::GetView().monitor); return; }
    if(z==Z_SP_VOL){ int m=spad::GetView().master; spad::SetMaster(m>75?75:m>50?50:m>25?25:100); return; }
    if(z==Z_SP_STOPALL){ spad::StopAll(); return; }
    if(z==Z_SP_ADD){ PlatformPickAudioFilesAsync(EV_PICK_SOUNDPAD,0); return; }
    if(z==Z_SP_OUT||z==Z_SP_IN){ spad::CycleDevice(z==Z_SP_IN,[]{ RedrawSoon(); }); return; }
    if(z==Z_SP_CABLE){ PlatformOpenFolder(L"https://vb-audio.com/Cable/"); SetStatus(L"Instale o VB-CABLE, reinicie o Remix e ligue o microfone: no Discord/jogo escolha \"CABLE Output\".",6000); return; }
    if(z==Z_SP_FOLDER){ std::error_code ec; std::filesystem::create_directories(std::filesystem::path(spad::Dir()),ec); PlatformOpenFolder(spad::Dir()); return; }
    if(z>=Z_SP_TILE_BASE&&z<Z_SP_TILE_BASE+500){ SpadToggleSound((size_t)(z-Z_SP_TILE_BASE)); return; }
    if(z>=Z_SP_TVOL_BASE&&z<Z_SP_TVOL_BASE+500){ size_t i=(size_t)(z-Z_SP_TVOL_BASE); if(i<g_spadIds.size()) spad::CycleSoundVolume(g_spadIds[i]); return; }
    if(z>=Z_SP_DEL_BASE&&z<Z_SP_DEL_BASE+500){ size_t i=(size_t)(z-Z_SP_DEL_BASE); if(i<g_spadIds.size()){ spad::Remove(g_spadIds[i]); SetStatus(L"Som removido do Soundpad.",2000); } return; }
}
// Arquivos escolhidos (s = caminhos separados por \n): copia/converte em segundo plano.
static void OnPickedSoundpad(const std::wstring& s){
    std::vector<std::wstring> files; size_t st=0;
    while(st<s.size()){ size_t e=s.find(L'\n',st); std::wstring f=s.substr(st,e==std::wstring::npos?std::wstring::npos:e-st); if(!f.empty()) files.push_back(f); if(e==std::wstring::npos) break; st=e+1; }
    if(files.empty()) return;
    SetStatus(L"Adicionando "+std::to_wstring(files.size())+(files.size()==1?L" som...":L" sons..."),2500);
    std::thread([files]{ std::wstring err; int n=spad::AddFiles(files,err); AppPost(EV_SPAD_ADDED,err,n); }).detach();
}
static void OnSpadAdded(const std::wstring& err,int n){
    if(n>0) SetStatus(std::to_wstring(n)+(n==1?L" som adicionado ao Soundpad.":L" sons adicionados ao Soundpad.")+(err.empty()?L"":L"  ("+err+L")"),3500);
    else SetStatus(err.empty()?L"Nenhum som adicionado.":err,4000);
}

// ---- DISCORD ------------------------------------------------------------------------------------
static bool DcCardsOn(){ return dc::Ready(); }
static void StartDcEdit(int mode){   // 9 = token (mascarado), 10 = cargo DJ, 11 = Application ID do Rich Presence
    g_editArtist=true; g_editMode=mode; g_editTrack=-1;
    g_editBuf=mode==10?dc::GetView().cfg.djRole:(mode==11?g_cfg.rpcAppId:L"");
}
static void BuildDcPanel(int w,int h){
    Panel& p=g_dcP; dc::ProbeTools(false); dc::View v=dc::GetView();
    RECT a=p.Frame(w,h,SI(940),SI(740),L"DISCORD  ·  BOT DE MÚSICA DO SEU SERVIDOR",Z_DC_CLOSE);
    int x=a.left, cw=a.right-a.left, y=a.top, g=SI(8), bh=SI(34);
    p.Text({x,y,x+cw,y+SI(20)},L"O bot é seu: crie em discord.com/developers, cole o token e convide. No servidor usam /tocar, /fila, /pular... (o Host do celular é outra coisa).",S(11),PCL_GRAY);
    y+=SI(28);
    auto cell=[&](int k,int n){ int bw=(cw-g*(n-1))/n; int xx=x+k*(bw+g); return RECT{xx,y,k==n-1?x+cw:xx+bw,y+bh}; };
    bool starting=v.on&&v.phase==1;
    p.Pill(cell(0,5),starting?L"CONECTANDO...":(v.on?L"DESLIGAR BOT":L"LIGAR BOT"),v.on&&v.phase==2,Z_DC_ONOFF,S(10));
    p.Pill(cell(1,5),v.hasToken?L"TOKEN: DEFINIDO ✓":L"DEFINIR TOKEN...",v.hasToken,Z_DC_TOKEN);
    p.Pill(cell(2,5),L"COPIAR CONVITE",!v.appId.empty(),Z_DC_INVITE);
    p.Pill(cell(3,5),v.installing?L"INSTALANDO...":(v.depsOk?L"ATUALIZAR BOT":L"INSTALAR BOT"),!v.depsOk&&v.nodeOk&&!v.installing,Z_DC_INSTALL);
    p.Pill(cell(4,5),L"ABRIR O PORTAL",false,Z_DC_PORTAL);
    y+=bh+g;
    p.Pill(cell(0,3),v.cfg.publicQueue?L"FILA PÚBLICA: SIM":L"FILA PÚBLICA: NÃO (SÓ DJ)",v.cfg.publicQueue,Z_DC_QUEUE);
    p.Pill(cell(1,3),v.cfg.publicPlaylists?L"PLAYLISTS PARA TODOS: SIM":L"PLAYLISTS PARA TODOS: NÃO",v.cfg.publicPlaylists,Z_DC_PLS);
    p.Pill(cell(2,3),v.cfg.online?L"MÚSICAS ONLINE: SIM":L"MÚSICAS ONLINE: NÃO",v.cfg.online,Z_DC_ONLINE);
    y+=bh+g;
    p.Pill(cell(0,3),v.cfg.votePct>0?L"VOTAÇÃO: "+std::to_wstring(v.cfg.votePct)+L"% DA CHAMADA":std::wstring(L"VOTAÇÃO: DESLIGADA (SÓ DJ)"),v.cfg.votePct>0,Z_DC_VOTE);
    p.Pill(cell(1,3),L"CARGO DJ: "+(v.cfg.djRole.empty()?std::wstring(L"(NENHUM)"):v.cfg.djRole),false,Z_DC_DJ);
    p.Pill(cell(2,3),L"LIMITE: "+std::to_wstring(v.cfg.maxPerUser)+L" MÚSICAS POR PESSOA",false,Z_DC_LIMIT);
    y+=bh+g;
    {   // Rich Presence: mostra no SEU perfil do Discord o que esta tocando (nao precisa do bot)
        bool temId=!g_cfg.rpcAppId.empty();
        p.Pill(cell(0,2),temId?(drpc::Ligado()?L"RICH PRESENCE: LIGADO ●":L"RICH PRESENCE: ESPERANDO O DISCORD"):L"RICH PRESENCE: DEFINIR APPLICATION ID...",temId&&drpc::Ligado(),Z_DC_RPC);
        std::wstring nota=drpc::Erro();
        if(nota.empty()) nota=temId?L"O seu perfil mostra \"Ouvindo <música>\" enquanto o Remix toca."
                                   :L"Cole o Application ID do seu app (o mesmo do bot serve) para aparecer no seu perfil.";
        p.Text({x+(cw-g)/2+g,y+SI(6),x+cw,y+SI(26)},nota,S(10),PCL_GRAY);
    }
    y+=bh+SI(10);
    // estado + ferramentas
    std::wstring l1=v.status.empty()?(v.hasToken?L"Desligado.":L"Sem token: toque em DEFINIR TOKEN (Developer Portal > seu app > Bot > Reset Token)."):v.status;
    int c1=v.phase==2?PCL_OK:v.phase==3?PCL_DANGER:PCL_WHITE;
    p.Text({x,y,x+cw,y+SI(18)},l1,S(11),c1,true);
    y+=SI(20);
    std::wstring l2=v.probing&&!v.nodeOk?L"Procurando o Node.js...":(v.nodeOk?L"Node.js "+v.nodeVer+(v.depsOk?L"  ·  bot instalado":L"  ·  falta instalar o bot (INSTALAR BOT)"):(v.nodeVer.empty()?L"Node.js 22.12+ não encontrado: rode o instalador de dependências e escolha DISCORD.":v.nodeVer));
    if(!v.installMsg.empty()) l2+=L"  ·  "+v.installMsg;
    p.Text({x,y,x+cw,y+SI(18)},l2,S(10.5f),PCL_GRAY);
    y+=SI(26);
    // lista rolavel: servidores tocando, playlists liberadas, registro
    p.list={a.left-SI(6),y,a.right+SI(6),a.bottom};
    p.Clip(p.list);
    int ly=y-p.scroll, rowH=SI(34);
    auto title=[&](const std::wstring& t){ p.Text({x,ly+SI(4),x+cw,ly+SI(22)},t,S(11),PCL_TITLE,true); ly+=SI(26); };
    g_dcGuildIds.clear(); g_dcPlSlugs.clear();
    title(L"TOCANDO AGORA ("+std::to_wstring(v.guilds.size())+L")");
    if(v.guilds.empty()){ p.Text({x+SI(10),ly+SI(6),x+cw,ly+SI(24)},v.phase==2?L"Parado. No servidor: /tocar. No PC: botão ▶ DISCORD em cima de um card toca onde você está numa chamada.":L"O bot não está conectado.",S(11),PCL_GRAY); ly+=rowH; }
    for(size_t i=0;i<v.guilds.size()&&i<50;i++){
        const dc::GuildView& gv=v.guilds[i]; g_dcGuildIds.push_back(gv.id);
        RECT r={x,ly,x+cw,ly+rowH*2-SI(6)}; p.Row(r,gv.playing);
        std::wstring top=gv.name+(gv.joining?L"  ·  entrando na chamada...":L"  ·  "+std::to_wstring(gv.humans)+(gv.humans==1?L" ouvindo":L" ouvindo"))+L"  ·  fila: "+std::to_wstring(gv.queue)+(gv.loop?(gv.loop==1?L"  ·  repetindo a música":L"  ·  repetindo a fila"):L"");
        p.Text({x+SI(10),ly+SI(3),x+cw-SI(250),ly+SI(22)},top,S(11),PCL_WHITE,true);
        std::wstring now=gv.title.empty()?L"(nada tocando)":(gv.paused?L"❚❚ ":L"▶ ")+gv.title+(gv.artist.empty()?L"":L" — "+gv.artist)+L"   "+dc::Pos(gv.pos)+L" / "+dc::Dur(gv.dur);
        p.Text({x+SI(10),ly+SI(26),x+cw-SI(250),ly+SI(46)},now,S(10.5f),PCL_GRAY);
        int bx=x+cw-SI(240), bw=SI(54);
        p.Pill({bx,ly+SI(10),bx+bw,ly+SI(46)},gv.paused?L"▶":L"❚❚",false,Z_DC_G_PAUSE_BASE+(int)i,S(11));
        p.Pill({bx+SI(60),ly+SI(10),bx+SI(60)+bw,ly+SI(46)},L"PULAR",false,Z_DC_G_SKIP_BASE+(int)i,S(9));
        p.Pill({bx+SI(120),ly+SI(10),bx+SI(120)+bw,ly+SI(46)},gv.loop==1?L"REP. 1":gv.loop==2?L"REP. FILA":L"REPETIR",gv.loop>0,Z_DC_G_LOOP_BASE+(int)i,S(9));
        p.Pill({bx+SI(180),ly+SI(10),bx+SI(180)+bw,ly+SI(46)},L"PARAR",false,Z_DC_G_STOP_BASE+(int)i,S(9));
        ly+=rowH*2+SI(2);
    }
    ly+=SI(6);
    title(L"PLAYLISTS NO BOT (só as marcadas aparecem no Discord)");
    if(g_playlists.empty()){ p.Text({x+SI(10),ly+SI(6),x+cw,ly+SI(24)},L"Você não tem playlists. Crie na aba PLAYLISTS e libere aqui.",S(11),PCL_GRAY); ly+=rowH; }
    for(size_t i=0;i<g_playlists.size()&&i<500;i++){
        const Playlist& pl=g_playlists[i]; g_dcPlSlugs.push_back(pl.slug);
        bool onb=v.cfg.pls.count(pl.slug)>0;
        RECT r={x,ly,x+cw,ly+rowH-SI(6)}; p.Row(r,onb);
        p.Text({x+SI(10),ly,x+cw-SI(170),ly+rowH-SI(6)},pl.name+L"   ·   "+PlaylistCardSubtitle((int)i),S(11),onb?PCL_WHITE:PCL_GRAY,onb);
        p.Pill({x+cw-SI(160),ly+SI(2),x+cw-SI(4),ly+rowH-SI(8)},onb?L"NO BOT: SIM":L"NO BOT: NÃO",onb,Z_DC_PL_BASE+(int)i);
        ly+=rowH;
    }
    ly+=SI(6);
    title(L"REGISTRO");
    if(v.hasToken) p.Pill({x+cw-SI(170),ly-SI(28),x+cw-SI(4),ly-SI(4)},L"APAGAR TOKEN",false,Z_DC_CLEARTOKEN,S(9));
    if(v.logs.empty()){ p.Text({x+SI(10),ly,x+cw,ly+SI(18)},L"(vazio)",S(10),PCL_GRAY); ly+=SI(20); }
    for(size_t i=v.logs.size()>14?v.logs.size()-14:0;i<v.logs.size();i++){ p.Text({x+SI(10),ly,x+cw,ly+SI(18)},v.logs[i],S(10),PCL_GRAY); ly+=SI(20); }
    p.contentH=ly+p.scroll-y+SI(10);
    p.Unclip();
    p.ClampScroll();
}
static void DcClick(int z){
    Panel& p=g_dcP; dc::View v=dc::GetView();
    if(z==Z_DC_CLOSE){ p.open=false; return; }
    if(z==Z_DC_ONOFF){
        if(v.on){ dc::SetOn(false); SetStatus(L"Bot do Discord desligado.",2200); return; }
        if(!v.hasToken){ StartDcEdit(9); return; }
        dc::SetOn(true); SetStatus(L"Ligando o bot do Discord...",2200); return;
    }
    if(z==Z_DC_TOKEN){ StartDcEdit(9); return; }
    if(z==Z_DC_CLEARTOKEN){ dc::ClearToken(); SetStatus(L"Token apagado deste PC.",2500); return; }
    if(z==Z_DC_RPC){ StartDcEdit(11); return; }
    if(z==Z_DC_INVITE){
        std::string u=dc::InviteUrl();
        if(u.empty()){ SetStatus(L"Ligue o bot uma vez: o convite usa o ID do seu app.",3200); return; }
        if(PlatformSetClipboardText(Utf8ToWide(u))) SetStatus(L"Convite copiado: abra no navegador e escolha o servidor.",3500); else SetStatus(L"Convite: "+Utf8ToWide(u),8000);
        return;
    }
    if(z==Z_DC_INSTALL){ if(v.installing) return; if(!v.nodeOk){ SetStatus(L"Precisa do Node.js 22.12+: rode o instalador de dependências e escolha DISCORD.",4500); return; } dc::InstallAsync(); return; }
    if(z==Z_DC_PORTAL){ PlatformOpenFolder(L"https://discord.com/developers/applications"); return; }
    if(z==Z_DC_QUEUE){ dc::UpdateCfg([](dc::Cfg& c){ c.publicQueue=!c.publicQueue; }); return; }
    if(z==Z_DC_PLS){ dc::UpdateCfg([](dc::Cfg& c){ c.publicPlaylists=!c.publicPlaylists; }); return; }
    if(z==Z_DC_ONLINE){ dc::UpdateCfg([](dc::Cfg& c){ c.online=!c.online; }); return; }
    if(z==Z_DC_VOTE){ dc::UpdateCfg([](dc::Cfg& c){ c.votePct=c.votePct==50?66:c.votePct==66?75:c.votePct==75?100:c.votePct==100?0:50; }); return; }
    if(z==Z_DC_LIMIT){ dc::UpdateCfg([](dc::Cfg& c){ static const int opt[]={10,20,30,50,100,200}; int nx=10; for(int k=0;k<6;k++) if(opt[k]>c.maxPerUser){ nx=opt[k]; break; } c.maxPerUser=nx; }); return; }
    if(z==Z_DC_DJ){ StartDcEdit(10); return; }
    auto gid=[&](int base){ size_t i=(size_t)(z-base); return i<g_dcGuildIds.size()?g_dcGuildIds[i]:std::string(); };
    if(z>=Z_DC_G_PAUSE_BASE&&z<Z_DC_G_PAUSE_BASE+50){ dc::OwnerAction(gid(Z_DC_G_PAUSE_BASE),"pausetoggle"); return; }
    if(z>=Z_DC_G_SKIP_BASE&&z<Z_DC_G_SKIP_BASE+50){ dc::OwnerAction(gid(Z_DC_G_SKIP_BASE),"skip"); return; }
    if(z>=Z_DC_G_LOOP_BASE&&z<Z_DC_G_LOOP_BASE+50){ dc::OwnerAction(gid(Z_DC_G_LOOP_BASE),"loop"); return; }
    if(z>=Z_DC_G_STOP_BASE&&z<Z_DC_G_STOP_BASE+50){ dc::OwnerAction(gid(Z_DC_G_STOP_BASE),"stop"); return; }
    if(z>=Z_DC_PL_BASE&&z<Z_DC_PL_BASE+500){
        size_t i=(size_t)(z-Z_DC_PL_BASE); if(i>=g_dcPlSlugs.size()) return;
        std::wstring slug=g_dcPlSlugs[i]; bool on=false;
        dc::UpdateCfg([&](dc::Cfg& c){ if(c.pls.count(slug)) c.pls.erase(slug); else { c.pls.insert(slug); on=true; } });
        SetStatus(on?L"Playlist liberada no bot do Discord.":L"Playlist tirada do bot do Discord.",2400);
        return;
    }
}
static bool DcToggleCfgPlaylist(int pl){
    if(pl<0||pl>=(int)g_playlists.size()) return false;
    std::wstring slug=g_playlists[(size_t)pl].slug; bool on=false;
    dc::UpdateCfg([&](dc::Cfg& c){ if(c.pls.count(slug)) c.pls.erase(slug); else { c.pls.insert(slug); on=true; } });
    SetStatus(on?L"Playlist liberada no bot do Discord.":L"Playlist tirada do bot do Discord.",2400);
    return on;
}
// "Tocar no Discord" (o dono pelo PC): toca agora onde ele esta numa chamada.
static dc::QItem DcItemFromTrack(const Track& t){
    dc::QItem q; q.title=t.title.empty()?std::filesystem::path(t.path).stem().wstring():t.title; q.artist=t.artist; q.dur=t.durSec;
    if(IsOnlineTrack(t)) q.url=t.url; else q.path=t.path;
    return q;
}
static void DiscordPlayTrack(int i){
    if(i<0||i>=(int)g_tracks.size()) return;
    std::wstring msg; dc::OwnerPlay({DcItemFromTrack(g_tracks[(size_t)i])},L"",false,msg);
    SetStatus(msg,3500);
}
static std::vector<Playlist> ExpandedPlaylists();
static void DiscordPlayPlaylist(int pl,bool shuffle){
    if(pl<0||pl>=(int)g_playlists.size()) return;
    std::vector<Playlist> all=ExpandedPlaylists(); if(pl>=(int)all.size()) return;
    std::vector<dc::QItem> items; for(auto& e:all[(size_t)pl].entries){ if(e.path.empty()&&e.url.empty()) continue; items.push_back(dc::FromEntry(e)); }
    if(shuffle){ std::random_device rd; std::mt19937 rng(rd()); std::shuffle(items.begin(),items.end(),rng); }
    std::wstring msg; dc::OwnerPlay(items,g_playlists[(size_t)pl].name,true,msg);
    SetStatus(msg,3500);
}
