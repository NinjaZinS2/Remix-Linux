#pragma once
// Nucleo compartilhado Windows/Linux do Remix: estado, reproducao, biblioteca,
// ordem da playlist, analise de audio (onda + espectro), volume/mudo, menus
// (pasta, contexto, confirmacao), renomear/excluir arquivo e eventos vindos de
// threads. NAO desenha nada e nao conhece Win32 nem raylib: tudo que depende do
// sistema entra pelos hooks Platform* declarados abaixo, implementados em
// main.cpp (Windows, GDI+) e linux/main_linux.cpp (raylib).
#include "platform.h"
#include "config.h"
#include "theme.h"
#include "playlist.h"
#include "app_playlists.h"
#include "player_ma.h"
#include <vector>
#include <string>
#include <map>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>
#include <atomic>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <set>

// ------------------------------------------------------------ hooks --------
void AppPost(int type, const std::wstring& s = L"", int n = 0); // evento de thread -> UI (drenado no loop)
void PlatformPickFolderAsync();                                 // responde com EV_PICK_FOLDER (s = pasta ou vazio)
void PlatformPickImageAsync(int evType, int ctx);               // responde com evType (s = imagem, n = ctx)
void PlatformPickProgramAsync(int evType);                      // escolher um executavel (fonte externa)
void PlatformResizeForMode();
void PlatformMinimize();
void PlatformClose();          // encerra o app de vez
void PlatformHide();           // esconde a janela (segue tocando em 2o plano)
void PlatformSetWindowSize(int w,int h);   // testes (--after N:size:WxH)
void PlatformUpdateGlobalHotkeys();       // registra/desregistra no sistema os atalhos marcados como GLOBAL
static void BuildLayout();                 // app_layout.h (mesma unidade de compilacao)
void PlatformRestoreAndFocus();
void PlatformRedraw();
void PlatformLoadCover(const std::wstring& path);
void PlatformEvictThumb(const std::wstring& path);
void PlatformClearThumbs();
bool PlatformTrash(const std::wstring& path);                   // manda para a lixeira
bool PlatformHaveFfmpeg();
std::wstring PlatformTranscodeToWav(const std::wstring& src);   // roda em thread; vazio = falhou
void PlatformOpenFolder(const std::wstring& path);              // abre no gerenciador de arquivos
void PlatformWatchStart(const std::wstring& dir);
void PlatformWatchStop();
bool PlatformWatchTake(unsigned long long quietMs);             // houve mudanca e ja passou a janela de acalmia
bool PlatformScreenshot(const std::wstring& pngPath);           // testes
void PlatformPickFolderFor(int evType, int ctx);                // pasta para outra finalidade (s = pasta, n = ctx)
void PlatformPickAudioFilesAsync(int evType, int ctx);          // varios arquivos de audio (s = caminhos separados por \n)
std::wstring PlatformClipboardText();                           // colar (Ctrl+V)
bool PlatformSetClipboardText(const std::wstring& text);         // copiar (link do Host)
bool PlatformHttpGet(const std::string& url, std::string& body);
// busca de capa na web (HTTP + decodificacao de imagem sao por plataforma)
static void WebSearchAsync(std::wstring q);
static void WebDownloadSelectedAsync();
static void ConsumeWebDownload();
static void ClearWebResultsPlatform();

// ------------------------------------------------------------ estado -------
static Config g_cfg;
std::wstring g_cfgMediaCli(){ return g_cfg.mediaCli; }   // online_resolve.h le o caminho configurado
#include "app_ui.h"
// Estilo ativo (configuracoes > ESTILO). O LED e o corredor so existem no classico e no spotify.
static inline const UiPal& UI(){ return UiPalFor(g_cfg.uiStyle); }
static inline bool UiClassic(){ return g_cfg.uiStyle==UI_CLASSICO; }
static inline bool UiGlow(){ return true; }   // LED/corredor: agora ligam e desligam nas EFEITOS, nao no estilo
static inline int  UiLed(){ return UiGlow()?g_cfg.ledBrightness:0; }
static inline bool UiRunner(){ return UiGlow()&&g_cfg.runnerOn; }
// Posicao do mouse (cada casca atualiza no seu laco): destaque do card/linha sob o
// cursor e botoes que so aparecem ali (estilos novos).
static int g_mouseX=-9999, g_mouseY=-9999;
static inline bool UiHot(const RECT& r){ return g_mouseX>=r.left&&g_mouseX<r.right&&g_mouseY>=r.top&&g_mouseY<r.bottom; }
static std::vector<Theme> g_themes;
static Theme g_theme;
static std::vector<Track> g_tracks;
static int g_current = -1;
static Player g_player;
static float g_rotation = 0.0f;
static float g_glowPhase = 0.0f;
static float g_runnerPhase = 0.0f;
static bool g_showSettings = false;
static bool g_showSplash = true;
// Segundo plano: janela escondida com a musica tocando (fechar com bgOnClose).
static bool g_hiddenToBg=false, g_bgAutoQuit=false, g_userPaused=false; static float g_bgIdleSec=0;
static bool g_safeMode=false;   // Windows: a abertura anterior nao terminou -> sem splash, efeitos, bandeja e atalhos globais
// ---- busca, vistas (biblioteca / playlists) e atalhos ----
static bool g_searchFocus=false; static std::wstring g_searchBuf; static std::vector<int> g_visible;   // faixas visiveis (filtro da busca)
static int g_view=0;                       // 0 = faixas da biblioteca, 1 = cards de playlists, 2 = faixas de uma playlist
static int g_openPl=-1;                    // playlist aberta (g_view==2)
static std::vector<Track> g_libTracks; static bool g_libCached=false;   // biblioteca guardada enquanto uma playlist esta aberta
static Track g_nowPlaying; static bool g_nowPlayingValid=false;        // faixa tocando (pode nao estar na lista visivel)
static int g_hkCapture=-1;                 // acao cujo atalho esta sendo capturado (configuracoes)
static std::wstring g_pendingAddPath;      // faixa a adicionar na playlist recem-criada
static int g_confirmKind=0;                // 0 = excluir faixa, 1 = excluir playlist, 2 = aceitar dispositivo (host)
// ---- marcar musicas da biblioteca para uma playlist / musicas online ----
static bool g_pickMode=false; static int g_pickPl=-1; static std::set<std::wstring> g_pickSel;
static std::wstring g_lastOnlineUrl, g_lastOnlineTitle;         // ultima musica em streaming (diario)
static ULONGLONG g_journalLast=0; static std::wstring g_journalSig;
static ULONGLONG g_splashStart = 0;
static const ULONGLONG SPLASH_MS = 2400;
static int g_dragSeek = -1;
static int g_listScroll = 0;
static std::wstring g_pendingPlay;
static std::thread g_waveThread;
static std::thread g_scanThread;
static int g_winW = 1500, g_winH = 812;      // area cliente atual
static bool g_customChrome = false;          // Windows: janela sem borda -> fechar/minimizar no cabecalho
static bool g_muted = false;
static int g_volBeforeMute = 80;
static std::wstring g_status;                // aviso curto na base da janela
static ULONGLONG g_statusUntil = 0;

static const int MIN_WIN_W = 560;
static const int MIN_WIN_H = 430;
static const int ROW_H = 56;

// Eventos vindos de threads.
enum : int {
    EV_NEXT_TRACK = 1, EV_REDRAW, EV_WEB_DOWNLOAD_DONE, EV_THUMBS_INVALIDATE, EV_COMMAND,
    EV_PICK_FOLDER, EV_PICK_IMAGE, EV_PICK_WALL, EV_TRANSCODED,
    EV_ART_READY, EV_ONLINE_META, EV_ONLINE_READY, EV_ONLINE_FAIL, EV_ONLINE_THUMB, EV_ONLINE_SEARCH, EV_ONLINE_RESOLVED, EV_ONLINE_JOB,
    EV_PICK_PL_FOLDER, EV_PICK_PL_FILES, EV_PICK_PL_ADDFOLDER, EV_PICK_NEWPL_FOLDER, EV_PICK_DLFOLDER, EV_TOOLS_READY,
    EV_PICK_DLONCE,  // pasta escolhida na hora de baixar (s = pasta ou vazio se cancelou)
    EV_HOST_PEDIDO,  // host: celular pediu para parear (s = id do pedido)
    EV_HOST_STATUS,  // host: aviso do servidor/tunel (s = texto, n = 1 quando e a URL do tunel)
    EV_STEMS,        // stems: mudou o estado de uma separacao (s = chave, n = estado)
    EV_PICK_SOUNDPAD, // soundpad: arquivos escolhidos (s = caminhos separados por \n)
    EV_PICK_CLI,      // fonte externa: executavel escolhido (s = caminho)
    EV_SPAD_ADDED    // soundpad: sons copiados/convertidos (s = erro, n = quantos)
};
// Analise incremental do streaming (online_play.h entrega o PCM; esta converte em
// onda/espectro e a UI publica em WS() para a musica online mostrar como a local).
// O dono passa o job do canal: a analise roda sem prender o SPool() (so st->m -> w.m).
struct StreamJob;
void StreamWavePump(StreamJob* jp, const int16_t* s16, size_t nSamples, uint64_t absFirstFrame);
#include "descobrir.h"   // novidades/recomendacoes da tela inicial (Deezer publico + o que voce ouve)
#include "letras.h"      // letra da musica (LRCLIB), guardada no disco para sempre
#include "online_play.h"
#include "stems.h"
#include "discord_rpc.h"  // Rich Presence: mostra no Discord o que esta tocando
static int g_curStreamId=0; static bool g_curStreamOpen=false; static ULONGLONG g_queueTick=0;   // canal de streaming tocando agora (a fila fica em online_play.h)
static std::map<std::wstring,OTrack> g_onlineInfo;              // url -> metadados/links achados (busca, streaming)
// Teclas (mapeadas por cada plataforma).
enum AppKey : int { K_NONE = 0, K_SPACE, K_LEFT, K_RIGHT, K_UP, K_DOWN, K_ENTER, K_ESC, K_BACKSPACE, K_R, K_M, K_DELETE, K_F2, K_PLUS, K_MINUS, K_Q };

// Estado compartilhado com threads em background. Alocado uma unica vez e
// nunca destruido: threads desanexadas podem terminar depois do main sem
// tocar em objetos ja destruidos.
struct WaveState {
    std::mutex m;
    std::vector<float> data;   // onda (320 buckets 0..1)
    std::wstring path;
    std::atomic<unsigned long> job{0};
    std::mutex fm;
    std::vector<float> spec;   // espectrograma: frames*48 (48 bandas por ~50 ms)
    int specHopMs = 50;
    float bands[48] = {0};
    bool hasSpec = false;
    float pulse = 0, onsetMax = 0.05f, energyMax = 0.02f;   // batida atual (0..1) e normalizacoes que se adaptam a musica
    double specHopF = 50.0;    // passo exato do espectro em ms (1102 amostras a 44,1 kHz = 24,99 ms; em inteiro a onda adiantava 4%)
};
static WaveState& WS(){ static WaveState* s = new WaveState(); return *s; }
struct ScanState {
    std::mutex m;
    std::vector<Track> result;
    std::atomic<bool> again{false};   // pedido de nova varredura enquanto uma roda
    bool computer=true; std::wstring folder;
    std::atomic<bool> busy{false};
    std::atomic<bool> ready{false};
    std::atomic<unsigned long> gen{0};
};
static ScanState& SS(){ static ScanState* s = new ScanState(); return *s; }

enum : int {
    Z_CLOSE=1, Z_MIN, Z_PLAYPAUSE, Z_NEXT, Z_PREV, Z_SHUFFLE, Z_REPEAT,
    Z_SEEKBAR, Z_VOLBAR, Z_HEART, Z_MODE_SQUARE, Z_MODE_CD, Z_MODE_VERTICAL, Z_GEAR,
    Z_ARTIST_EDIT=16, Z_PLAYER_RESIZE,
    Z_SHAPE_TOGGLE=18, Z_LISTMODE=19, Z_WAVESEEK=20,
    Z_IMG_LOCAL=21, Z_IMG_WEB=22,
    Z_WEB_QBOX=23, Z_WEB_SEARCH=24, Z_WEB_USE=25, Z_WEB_CANCEL=26, Z_WEB_CLOSE=27,
    Z_THEME_BASE=100, Z_TRACK_BASE=1000000,   // faixa i = BASE+i (ate 1 milhao): nao pode colidir com os IDs fixos
    Z_SETTINGS_FOLDER=900, Z_SETTINGS_CLOSE, Z_LED_BRIGHT, Z_LED_SPEED, Z_LED_EFFECT,
    Z_UI_SCALE, Z_TITLE_SCALE, Z_ARTIST_SCALE, Z_VERTICAL_SCALE,
    Z_SETTINGS_DEFAULT, Z_SETTINGS_CUSTOM, Z_SETTINGS_MODE_SQUARE, Z_SETTINGS_MODE_CD, Z_SETTINGS_MODE_VERTICAL,
    Z_RUNNER_TOGGLE=600, Z_RUNNER_SPEED=605, Z_RUNNER_COLOR_BASE=610, Z_BTN_PLAY_BASE=630, Z_BTN_NAV_BASE=650,
    Z_PARTICLES_TOGGLE=670, Z_GLITCH_TOGGLE=671, Z_PLAYER_SIZE_SLIDER=672, Z_AUTOCOLOR_TOGGLE=673,
    Z_SET_WALL_CHOOSE=674, Z_CLR_WALLPAPER=675, Z_COVERBLUR_TOGGLE=676, Z_CD_SPEED=677,
    Z_PART_SPEED=680, Z_LED_COLOR_BASE=684, Z_PART_COLOR_BASE=704,
    Z_EQ_BASE=740, Z_SET_AUTOPLAY=760, Z_SET_SORT=761, Z_SET_SORTDIR=762, Z_EQ_ON=763, Z_EQ_RESET=764,
    Z_AUTOPLAY=765, Z_SORT=766, Z_VOL_ICON=767, Z_FOLDER_BTN=768, Z_CONFIRM_YES=769, Z_CONFIRM_NO=770, Z_PERF_TOGGLE=771, Z_BG_TOGGLE=772, Z_QUIT_BTN=773, Z_SYSMEDIA_TOGGLE=774, Z_TAB_TRACKS=780, Z_TAB_PLAYLISTS=781, Z_SEARCH_BOX=782, Z_SEARCH_CLEAR=783, Z_PL_BACK=784, Z_PL_NEW=785, Z_HK_RESET=786,
    Z_TAB_ONLINE=787, Z_PL_ADD=788, Z_PICK_DONE=789, Z_PICK_CANCEL=790, Z_ACTIVITY=791, Z_SET_ON_MODE=792, Z_SET_ON_FMT=793, Z_SET_ON_SRC=794,
    Z_SET_ON_FOLDER=795, Z_SET_ON_RECHECK=796, Z_PL_MODE=797, Z_ON_CLOSE=798, Z_ON_QBOX=799, Z_ON_SEARCH=800, Z_ON_ADDALL=801, Z_ON_SRC_BASE=810, Z_ON_TAB_BASE=814, Z_SET_CLI=817, Z_SET_CLI_BUSCAR=818, Z_ON_CFG=819,
    Z_SETTINGS_STYLE_BASE=820,   // +0 classico, +1 limpo, +2 spotify
    // Host: faixa 22000+ (na 1.4.0 ficaram em 821..838 e colidiam com Z_SETTINGS_STYLE_BASE+1/+2:
    // o botao HOST virava "Limpo" e LIGAR O HOST virava "Spotify + LED"). Os static_assert abaixo travam isso.
    Z_HOST_BTN=22000, Z_SET_HOST_ON, Z_SET_HOST_PORT, Z_SET_HOST_PIN, Z_SET_HOST_NAME, Z_SET_HOST_TUNNEL, Z_SET_HOST_LAN, Z_SET_HOST_PANEL,
    Z_SET_HOST_COPYTUN, Z_SET_HOST_COPYLAN, Z_SET_HOST_ONLINE, Z_SET_HOST_QRCONF, Z_SET_HOST_IPV6,
    Z_HOST_CLOSE=22050, Z_HOST_TOGGLE, Z_HOST_TUNNEL, Z_HOST_HTML, Z_HOST_PASTA, Z_HOST_PORT, Z_HOST_PIN, Z_HOST_NAME, Z_HOST_LAN,
    Z_HOST_NEWLINK, Z_HOST_COPYTUN, Z_HOST_COPYLAN, Z_HOST_QRMODE, Z_HOST_QRNEW, Z_HOST_ONLINE, Z_HOST_QRCONF, Z_HOST_IPV6,
    Z_HOST_DEVLIB_BASE=22100, Z_HOST_DPLOK_BASE=22200,   // +100 aparelhos, +500 playlists de aparelhos
    Z_FX_BTN=23000, Z_FX_CLOSE, Z_FX_CLEAR, Z_FX_CANCEL, Z_FX_BASE=23010, Z_STEM_BASE=23020,   // efeitos (+5) e stems (+6)
    Z_SPAD_BTN=24000, Z_DC_BTN, Z_SET_SPAD, Z_SET_DC,   // SOUNDPAD e DISCORD (os paineis usam 24010..26800, app_panels.h)
    Z_DC_PLCARD_BASE=16500, Z_DC_CARD_BASE=6000000,      // botao ▶ DISCORD sobre a capa: playlist (+500) e faixa (+1 milhao)
    Z_HOST_ACCEPT_BASE=17000, Z_HOST_DENY_BASE=17100, Z_HOST_REVOKE_BASE=17200, Z_HOST_PL_BASE=17300, Z_HOST_PLDEV_BASE=17500,   // playlist*20+dispositivo (ate 21500)
    Z_HOST_DEVLINK_BASE=21600,   // +100: link permanente de cada aparelho (religar em qualquer endereco)
    // Estilo REMIX (1.6): lateral, tela inicial com fileiras e player embaixo.
    Z_RX_NAV_BASE=30000,          // +3: Início / Buscar / Sua biblioteca
    Z_RX_NOVAPL=30010, Z_RX_ATUALIZAR, Z_RX_FILA, Z_RX_VERTODAS, Z_RX_TOCAR, Z_RX_ALEATORIO, Z_RX_VOLTAR, Z_RX_BUSCARON, Z_RX_SIDEDRAG,
    Z_RX_LETRA=30030, Z_RX_SAIDA, Z_RX_PAINEL, Z_RX_HERO, Z_RX_BUSCAPL, Z_RX_BUSCAAL, Z_RX_LETRA_LINHA_BASE=30200,   // +200: clicar numa linha da letra pula para ela
    Z_RX_ATALHO_BASE=30100,       // +16: atalhos do topo do Início (mais ouvidos)
    Z_RX_SIDE_BASE=31000,         // +500: itens da lateral (0 = todas as músicas, depois as playlists)
    Z_RX_CARD_BASE=32000,         // +4000: cartões da tela inicial
    Z_RX_CARDPLAY_BASE=37000,     // +4000: play sobre a capa do cartão
    Z_RX_VERTUDO_BASE=42000,      // +100: "ver tudo" de cada fileira
    Z_COVER_BASE=2000000, Z_CARD_SEEK_BASE=3000000,
    Z_CARD_PREV_BASE=4000000, Z_CARD_NEXT_BASE=5000000, Z_WEB_CELL_BASE=7000,
    Z_ROW_UP_BASE=8000000, Z_ROW_DOWN_BASE=9000000, Z_FOLDER_ITEM_BASE=10000, Z_CTX_ITEM_BASE=11000,
    Z_PL_CARD_BASE=13000, Z_PL_PLAY_BASE=13500, Z_PL_SHUF_BASE=14000, Z_HK_KEY_BASE=14500, Z_HK_SCOPE_BASE=14600, Z_ON_PLAY_BASE=15000, Z_ON_DL_BASE=15500, Z_ON_ADD_BASE=16000
};

static RECT R_titlebar, R_art, R_seek, R_vol, R_play, R_prev, R_next, R_shuffle, R_repeat;
static RECT R_heart, R_gear, R_close, R_min, R_library;
static RECT R_modeSquare, R_modeCd, R_modeVertical;
static std::vector<RECT> R_themeCircles;
static std::vector<RECT> R_cardRects;
static std::vector<RECT> R_cardCoverButtons;
static std::vector<RECT> R_cardSeekRects;
static RECT R_settingsPanel, R_settingsDefault, R_settingsCustom, R_settingsClose;
static RECT R_settingsModeSquare, R_settingsModeCd, R_settingsModeVertical;
static RECT R_settingsStyle[UI_STYLE_COUNT];   // ESTILO: clássico / remix
static RECT R_setHostCopyTun, R_setHostCopyLan, R_setHostOnline, R_setHostQrConf, R_setHostIpv6;
static RECT R_setHostOn, R_setHostPort, R_setHostPin, R_setHostName, R_setHostTunnel, R_setHostLan, R_setHostPanel, R_hostBtn;   // HOST (docs/HOST.md)
static RECT R_fxBtn;   // EFEITOS (cabecalho)
static RECT R_spadBtn, R_dcBtn, R_setSpad, R_setDc;   // SOUNDPAD e DISCORD (cabecalho e configuracoes)
static std::vector<RECT> R_cardDcBtns, R_plDcBtns;   // ▶ DISCORD sobre a capa (so com o bot conectado)
struct FxPanelUI { bool open=false; RECT box{0,0,0,0}, btnClose{0,0,0,0}, btnClear{0,0,0,0}, btnCancel{0,0,0,0}, info{0,0,0,0}; RECT fx[5]{}; RECT stem[6]{}; };
static FxPanelUI g_fxp;
static RECT R_verticalCoverButton;
static RECT R_playerPanel;
static int g_panelArt = 300;
static RECT R_pencilPanel, R_pencilVert, R_editBox, R_editSave, R_editCancel;
static RECT R_setParticles, R_setGlitch, R_setRunnerToggle, R_runnerSlider, R_setEffect;
static RECT R_setAutoColor, R_setPerf, R_setBgClose, R_setSysMedia, R_setQuit;
static RECT R_libBar, R_tabTracks, R_tabPlaylists, R_searchBox, R_searchClear, R_plBack, R_plNew, R_hkReset;
static RECT R_hkKey[HK_COUNT], R_hkScope[HK_COUNT];
static std::vector<RECT> R_plCards, R_plPlay, R_plShuf;
static RECT R_tabOnline, R_plAdd, R_plMode, R_pickDone, R_pickCancel, R_activity;
static RECT R_setOnMode, R_setOnFmt, R_setOnSrc, R_setOnFolder, R_setOnRecheck, R_onlineInfo;
static RECT R_setCli, R_setCliBuscar;   // FONTES EXTERNAS: caminho da CLI e "procurar no sistema"
static RECT R_setWallChoose, R_setWallClear, R_setCoverBlur;
static std::vector<RECT> R_themeCirclesSettings, R_runColors, R_playColors, R_navColors;
static std::vector<RECT> R_partColors, R_ledColors;
static bool g_editArtist = false;            // editor de texto aberto
static int g_editMode = 0;                   // 0 = nome do artista, 1 = nome do arquivo
static int g_editTrack = -1;
static std::wstring g_editBuf;
static std::map<std::wstring,std::wstring> g_artistMap;
struct SetSlider { RECT r, hit; int id; int minv, maxv; };
static std::vector<SetSlider> g_setSliders;
static std::vector<std::pair<RECT,std::wstring>> g_setLabels;
static int g_setScroll = 0;
static int g_setContentH = 0;
static std::vector<std::pair<RECT,std::wstring>> g_setSections;
static RECT R_wavePanel, R_waveDragRect, R_shapeTgl, R_listBtn;
static int g_gridCols = 0, g_contentH = 0;
static int g_headerH = 0, g_sideW = 0;   // faixas solidas do cabecalho e da lateral (estilos novos)
// Zonas que nao podem se sobrepor (a 1.4.0 teve HOST dentro da faixa do ESTILO).
static_assert(Z_SETTINGS_STYLE_BASE+UI_STYLE_COUNT<=Z_ON_SRC_BASE+100 && Z_HOST_BTN>Z_HOST_PLDEV_BASE+4000, "zonas do host/estilo sobrepostas");
static_assert(Z_HOST_DPLOK_BASE+500<=Z_FX_BTN && Z_FX_CANCEL<Z_FX_BASE && Z_FX_BASE+5<=Z_STEM_BASE && Z_STEM_BASE+10<Z_COVER_BASE, "faixa dos efeitos invade outra");
static_assert(Z_STEM_BASE+10<=Z_SPAD_BTN && Z_ON_ADD_BASE+500<=Z_DC_PLCARD_BASE && Z_DC_PLCARD_BASE+500<=Z_HOST_ACCEPT_BASE && Z_CARD_NEXT_BASE+1000000<=Z_DC_CARD_BASE && Z_DC_CARD_BASE+1000000<=Z_ROW_UP_BASE, "botoes do discord invadem outra faixa");
static_assert(Z_HOST_PLDEV_BASE+4000<=Z_HOST_BTN && Z_SET_HOST_IPV6<Z_HOST_CLOSE && Z_HOST_IPV6<Z_HOST_DEVLIB_BASE && Z_HOST_DEVLIB_BASE+100<=Z_HOST_DPLOK_BASE && Z_HOST_DPLOK_BASE+500<Z_COVER_BASE, "faixa do host invade outra");
static_assert(Z_HOST_PLDEV_BASE+4000<=Z_HOST_DEVLINK_BASE && Z_HOST_DEVLINK_BASE+100<=Z_HOST_BTN, "faixa do link do aparelho invade outra");
static_assert(Z_RX_NAV_BASE>Z_SPAD_BTN+2800 && Z_RX_NAV_BASE+3<=Z_RX_NOVAPL && Z_RX_ATALHO_BASE+16<Z_RX_SIDE_BASE && Z_RX_VERTODAS<Z_RX_SIDE_BASE && Z_RX_SIDE_BASE+500<=Z_RX_CARD_BASE && Z_RX_CARD_BASE+4000<=Z_RX_CARDPLAY_BASE && Z_RX_CARDPLAY_BASE+4000<=Z_RX_VERTUDO_BASE && Z_RX_VERTUDO_BASE+100<Z_COVER_BASE, "faixa do estilo REMIX invade outra");
static std::vector<RECT> R_cardPlayBtns;   // estilos novos: botao de play sobre a capa do card (aparece com o mouse)
// ---- estilo REMIX (1.6): lateral com a biblioteca, tela inicial com fileiras, player embaixo ----
enum { RXP_INICIO=0, RXP_LISTA=1, RXP_DESCOBRIR=2 };   // LISTA = biblioteca/playlists/playlist aberta (usa g_view)
static int g_rxPag=RXP_INICIO;             // pagina da area principal
static int g_rxScroll=0, g_rxContentH=0;   // rolagem da tela inicial
static RECT R_rxTop{0,0,0,0}, R_rxSide{0,0,0,0}, R_rxMain{0,0,0,0}, R_rxBar{0,0,0,0};
static RECT R_rxNav[3];                    // Início / Descobrir / Sua biblioteca
static RECT R_rxNovaPl{0,0,0,0}, R_rxAtualizar{0,0,0,0}, R_rxVerTodas{0,0,0,0};
static RECT R_rxCab{0,0,0,0}, R_rxTocar{0,0,0,0}, R_rxAleat{0,0,0,0};   // cabecalho da pagina da biblioteca/playlist
static RECT R_rxVoltar{0,0,0,0}, R_rxBuscarOn{0,0,0,0};   // cabecalho da pagina Descobrir
static RECT R_rxBuscaPl{0,0,0,0}, R_rxBuscaAl{0,0,0,0};    // com texto na busca: procurar online, playlists e albuns
static std::wstring g_rxGenero, g_rxGeneroNome;           // genero aberto na pagina Descobrir (vazio = grade de generos)
static std::vector<RECT> R_rxSidePl;       // 0 = "Todas as músicas", depois as playlists (mais usadas primeiro)
static std::vector<int> g_rxSideOrdem;     // item da lateral -> índice real da playlist (-1 = todas as músicas)
// A ordem por uso e congelada: recalcular a cada clique fazia a lista "pular"
// debaixo do cursor (abrir uma playlist ja conta como uso). So muda quando a
// lista de playlists muda (criar, apagar, renomear) ou ao abrir o app.
static std::wstring g_rxOrdemAss;          // assinatura da lista de playlists
static std::vector<int> g_rxOrdemFixa;
static RECT R_rxSideDrag{0,0,0,0};         // divisória: arrasta para mudar a largura da lateral
// Atalhos do topo do Início: o que você mais ouviu por último (playlists e músicas).
struct RxAtalho { RECT r{0,0,0,0}; int tipo=0; int idx=0; std::wstring nome, sub, capa; };
static std::vector<RxAtalho> g_rxAtalhos;
// Destaque do topo do Início (a novidade da vez).
static RECT R_rxHero{0,0,0,0}, R_rxHeroBtn{0,0,0,0};
static desc::Item g_rxHeroItem; static bool g_rxHeroOk=false; static std::wstring g_rxHeroFileira;
// Barra de baixo: botoes de letra, fila e onde tocar.
static RECT R_rxLetra{0,0,0,0}, R_rxSaida{0,0,0,0}, R_rxPainel{0,0,0,0};
static bool g_rxLetraOn=false;         // letra ocupando a area principal
static bool g_rxPainelOn=false;        // painel da direita (tocando agora)
static RECT R_rxNp{0,0,0,0};           // area do painel da direita
static std::vector<RECT> R_rxLetraLinhas;
static int g_rxLetraScroll=0;
static ULONGLONG g_rxLetraMexeu=0;   // rolou com a mão: para de seguir a música por alguns segundos
struct RxCard { RECT r{0,0,0,0}, play{0,0,0,0}; int fila=0, item=0; };
static std::vector<RxCard> g_rxCards;      // cartoes visiveis da tela inicial
struct RxFila { RECT head{0,0,0,0}, verTudo{0,0,0,0}; int fonte=0; };   // fonte: -1 = local (recentes), >=0 = fileira do descobrir
static std::vector<RxFila> g_rxFilas;
static std::vector<int> g_rxAbertas;       // fileiras expandidas ("ver tudo")
static std::vector<int> g_rxRecentes;             // "tocados recentemente": indices na lista em tela
static std::vector<std::wstring> g_rxRecentesChave;   // ...e o caminho/URL de cada um
static std::vector<int> g_rxMistura;               // "Sua mistura": faixas escolhidas pelo seu gosto
static std::vector<std::wstring> g_rxMisturaChave;
static bool RxOn(){ return g_cfg.uiStyle!=UI_CLASSICO; }
// ---- duracao das musicas locais (para a lista mostrar o tempo) --------------
// O arquivo so diz a duracao depois de ser aberto: uma thread vai medindo as
// faixas que aparecem na tela e guarda o resultado (nao trava o desenho).
struct DurCache { std::mutex m; std::map<std::wstring,int> pronto; std::vector<std::wstring> fila; std::atomic<bool> rodando{false}; };
inline DurCache& DUR(){ static DurCache* d=new DurCache(); return *d; }
static void DurWorker();
static int DurSegundos(const std::wstring& path,int jaSabe){
    if(jaSabe>0) return jaSabe;
    if(path.empty()||IsUrlText(path)) return 0;
    DurCache& d=DUR();
    {
        std::lock_guard<std::mutex> lk(d.m);
        auto it=d.pronto.find(path); if(it!=d.pronto.end()) return it->second;
        if(d.fila.size()<400){ for(auto& q:d.fila) if(q==path) return 0; d.fila.push_back(path); }
    }
    if(!d.rodando.exchange(true)) std::thread([]{ RemixSafe("medir duracao",[]{ DurWorker(); }); }).detach();
    return 0;
}
// Novidades/generos chegam de uma thread: a tela precisa refazer o layout (as
// fileiras mudam de tamanho), nao so repintar.
static std::atomic<bool> g_rxRefazer{false};
static void RxAvisarNovidades(){ g_rxRefazer.store(true); AppPost(EV_REDRAW); }
static RECT R_autoTgl, R_sortBtn, R_folderBtn, R_volIcon;
static std::vector<RECT> R_rowUp, R_rowDown;
static RECT R_setAutoplay, R_setSort, R_setSortDir, R_setEqOn, R_setEqReset;
static std::vector<std::wstring> g_shortcutLines;   // secao ATALHOS (so texto)
static RECT R_shortcutsBox;
// menu "trocar imagem" (local / web)
static bool g_imgMenuOpen = false;
static int g_imgMenuTrack = -1;
static RECT R_imgBox, R_imgLocal, R_imgWeb;
// menu PASTA (recentes)
static bool g_folderMenuOpen = false;
static RECT R_folderBox;
static std::vector<RECT> R_folderItems;
static std::vector<std::wstring> g_folderItemPaths; // "" = escolher..., L"*" = todo o PC
// menu de contexto (botao direito numa faixa)
static bool g_ctxOpen = false;
static int g_ctxTrack = -1;
static RECT R_ctxBox;
static std::vector<RECT> R_ctxItems;
// menu de contexto generico: rotulos + acao de cada item (faixa, card de playlist, escolher playlist)
enum { CTX_TRACK=0, CTX_PLAYLIST=1, CTX_PICKPL=2, CTX_MENU=3, CTX_PICKON=4, CTX_SAIDA=5 };
enum { CA_PLAY=1, CA_COVER, CA_ARTIST, CA_RENAME, CA_FOLDER, CA_ADDPL, CA_REMOVEPL, CA_DELETE, CA_PL_PLAY, CA_PL_SHUF, CA_PL_RENAME, CA_PL_DELETE, CA_PL_HOST, CA_PICK_NEW,
       CA_ADD_LIB, CA_ADD_FILES, CA_ADD_FOLDERCOPY, CA_ADD_FOLDERLINK, CA_ADD_LINK, CA_ADD_SEARCH, CA_NEW_EMPTY, CA_NEW_FOLDER, CA_NEW_LINK, CA_NEW_SEARCH,
       CA_PLF_PICK, CA_PLF_UNLINK, CA_PLF_LIBRARY, CA_PL_FOLDER, CA_PL_SYNC, CA_PL_DLALL, CA_PL_MODE, CA_DOWNLOAD, CA_OPEN_URL, CA_ACT_CANCEL, CA_ACT_OPENDIR, CA_PICKON_NEW, CA_DC_PLAY, CA_DC_PLPLAY, CA_DC_PLTOGGLE,
       CA_PICK_BASE=100, CA_PLF_RECENT_BASE=200, CA_PICKON_BASE=300, CA_SAIDA_BASE=400 };
static int g_ctxKind=0, g_ctxArg=-1; static std::vector<std::wstring> g_ctxLabels; static std::vector<int> g_ctxActs; static std::vector<bool> g_ctxDanger;
// confirmacao (excluir)
static bool g_confirmOpen = false;
static std::wstring g_confirmText;
static int g_confirmTrack = -1;
static RECT R_confirmBox, R_confirmYes, R_confirmNo;
// conversao de formato em andamento
static bool g_converting = false;
static unsigned g_openGen = 0;
static bool g_pendingAutoplay = false;
static DWORD g_resumeMs = 0;   // troca de fonte (stem <-> completa): a faixa reabre nesse ponto
static std::wstring g_currentSource;          // arquivo realmente aberto (pode ser o WAV convertido)
// seletor de imagem da web (imagens sao void* por plataforma)
struct WebRes { std::wstring murl; std::wstring turl; void* img=nullptr; void* cpu=nullptr; };
struct WebPick {
    bool open=false; int track=-1; std::wstring query; bool editing=false;
    std::vector<WebRes> res; std::mutex m; std::atomic<int> gen{0};
    std::atomic<bool> searching{false}; std::atomic<bool> downloading{false};
    int sel=-1; int scroll=0; std::wstring status=L"Aguardando...";
    RECT box{}, qbox{}, btnSearch{}, btnUse{}, btnCancel{}, btnClose{};
    std::vector<RECT> cells;
};
static WebPick& WP(){ static WebPick* s=new WebPick(); return *s; }
struct WebDlState{std::mutex m;std::wstring path;bool ok=false;};
static WebDlState& WDS(){static WebDlState* s=new WebDlState();return *s;}

static bool PtIn(const RECT& r, int x, int y){ return x>=r.left && x<=r.right && y>=r.top && y<=r.bottom; }
static float g_dpiMul = 1.f;   // casca: densidade da tela (celular = 2x..4x); 1 no desktop
static float S(float v){ return v * g_cfg.uiScale / 100.0f * g_dpiMul; }
static int SI(int v){ return (int)std::lround(S((float)v)); }
static float TextScale(int base, int pct){ return S((float)base) * pct / 100.0f; }
static bool FxOn(){ return !g_cfg.perfMode&&!g_safeMode; }     // efeitos pesados (particulas, glitch, corredor, blur)
static void SetStatus(const std::wstring& s, int ms=2600){ g_status=s; g_statusUntil=GetTickCount64()+ms; }
static bool StatusVisible(){ return !g_status.empty() && GetTickCount64() < g_statusUntil; }

static BYTE LedAlpha(float phase,int minA,int maxA){
    float t=1.0f;
    if(g_cfg.ledEffect==L"pulso") t=powf((sinf(phase)+1.0f)/2.0f,3.0f);
    else if(g_cfg.ledEffect==L"respiracao") t=(sinf(phase)+1.0f)/2.0f;
    float bright=g_cfg.ledBrightness/100.0f;
    int a=(int)((minA+t*(maxA-minA))*bright); return (BYTE)std::max(0,std::min(255,a));
}
// Paleta brilhante: originais + claras + vermelho sangue etc (15 opcoes).
static const wchar_t* g_brightIds[15] = {
    L"#00ff66", L"#00e5ff", L"#ff2d95", L"#ff9500", L"#ffe600",
    L"#8a0303", L"#ff5e5e", L"#ffb3c6", L"#a7d8ff", L"#b9fbcf",
    L"#e4c7ff", L"#ffe3b3", L"#fff9b0", L"#7ff7e0", L"#c9ccd4"
};
static COLORREF ParseHexColor(const std::wstring& id){
    if(id.size()<7||id[0]!=L'#') return g_theme.accent;
    unsigned v=remix_parse_hex(id,1,6);
    return RGB((v>>16)&0xFF,(v>>8)&0xFF,v&0xFF);
}
static bool IsBrightId(const std::wstring& id){
    if(id.size()!=7||id[0]!=L'#') return false;
    for(auto b:g_brightIds) if(id==b) return true;
    return false;
}
static COLORREF ResolveCustom(const std::wstring& id){
    if(g_cfg.autoColor) return g_theme.accent;
    if(IsBrightId(id)) return ParseHexColor(id);
    if(!id.empty()&&id[0]==L'#') return ParseHexColor(id);
    if(id.empty()) return g_theme.accent;
    return FindTheme(g_themes,id).accent;
}

// ---- waves organicas --------------------------------------------------
static double g_streamAheadMs=0;   // ate onde o streaming ja decodificou (ms absolutos da musica); 0 = ainda nao chegou
static float Hash01(float n){ float s=sinf(n)*43758.5453f; return s-floorf(s); }
static float VNoise(float x){
    float i=floorf(x),f=x-i; f=f*f*(3.0f-2.0f*f);
    return Hash01(i)+(Hash01(i+1.0f)-Hash01(i))*f;
}
static float WaveIdleAt(int i,int count,float t){
    float p=count>1?(float)i/(count-1):0.f;
    float v=.5f+(VNoise(p*7.f+t*.55f)-.5f)*1.05f
             +(VNoise(p*19.f-t*1.15f+40.f)-.5f)*.5f
             +.10f*sinf(p*12.56f+t*.8f);
    return std::max(.10f,std::min(1.f,v));
}
static float AudioWaveBar(int i,int count,DWORD posMs,DWORD lenMs,float t){
    if(!lenMs) return WaveIdleAt(i,count,t);
    std::lock_guard<std::mutex> lock(WS().m);
    if(WS().data.empty()) return WaveIdleAt(i,count,t);
    float p=count>1?(float)i/(count-1):0.f;
    DWORD look=std::min<DWORD>(lenMs/1000*35,14000);
    DWORD back=std::min<DWORD>(lenMs/1000*6,2500);
    if(g_player.IsStream()){   // online: nao "espiar" trecho que o streaming ainda nao baixou/decodificou
        double avail=g_streamAheadMs-(double)posMs; if(avail<0)avail=0;
        look=(DWORD)std::min<double>((double)look,std::max(300.0,avail));
    }
    double center=(double)posMs-back+(double)look*p;
    float v=-1.f, band=1.f, pulse=0.f;
    {
        // Onda no ritmo: a altura vem da energia do espectro naquele instante (quadros de 25-50 ms) e o
        // "pulo" vem da batida detectada agora (UpdateSpecBands), mais forte perto do ponto que esta tocando.
        std::lock_guard<std::mutex> lk(WS().fm);
        int frames=(int)(WS().spec.size()/48); double hop=std::max(1.0,WS().specHopF);
        if(frames>0&&center>=0){
            int fi=(int)(center/hop);
            if(fi<frames){ const float* f=&WS().spec[(size_t)fi*48]; float e=0; for(int k=0;k<48;k++) e+=f[k]; e/=48.f; v=std::min(1.f,sqrtf(e/std::max(0.0005f,WS().energyMax))); }
        }
        if(WS().hasSpec){
            float q=powf(p,.7f);
            int bi=(int)(q*47.99f); if(bi<0)bi=0; if(bi>47)bi=47;
            band=.30f+1.15f*WS().bands[bi];
        }
        pulse=WS().pulse;
    }
    if(v<0){   // sem espectro ainda: os 320 pontos da musica inteira
        float norm=(float)(center/std::max<double>(1,(double)lenMs));
        norm=std::max(0.f,std::min(1.f,norm));
        size_t n=WS().data.size();
        size_t idx=(size_t)(norm*(float)(n-1));
        size_t span=std::max<size_t>(1,n/110);
        size_t a=idx>span?idx-span:0,b=std::min(n-1,idx+span);
        v=0; for(size_t j=a;j<=b;j++) v=std::max(v,WS().data[j]);
    }
    v=powf(std::max(0.f,v),.82f);
    float perto=1.f-std::min(1.f,(float)fabs(center-(double)posMs)/1800.f);   // ("near" e macro no Windows)
    float out=v*(1.f+.60f*pulse*perto)*band;
    float idle=(.07f+.13f*WaveIdleAt(i,count,t*.7f))*band;
    return std::max(.06f,std::min(1.f,std::max(out,idle)));
}
// ---- LED corredor: ponto sobre o perimetro de um retangulo arredondado -----
struct RectFC { float X, Y, Width, Height; };
static void PerimeterPoint(const RectFC& r,float rad,float u,float&x,float&y){
    float w=r.Width,h=r.Height;
    rad=std::min(rad,std::min(w,h)/2.f);
    const float PIf=3.14159265f;
    float L[4]={w-rad*2,h-rad*2,w-rad*2,h-rad*2};
    float Q=PIf*rad*.5f;
    float P=L[0]+Q+L[1]+Q+L[2]+Q+L[3]+Q;
    float d=u*P; d-=floorf(d/P)*P;
    auto arcPt=[](float cx,float cy,float a0,float dd,float rr,float&px,float&py){
        float ang=a0+dd/rr; px=cx+rr*cosf(ang); py=cy+rr*sinf(ang); };
    if(d<L[0]){ x=r.X+rad+d; y=r.Y; return; } d-=L[0];
    if(d<Q){ arcPt(r.X+w-rad,r.Y+rad,-PIf*.5f,d,rad,x,y); return; } d-=Q;
    if(d<L[1]){ x=r.X+w; y=r.Y+rad+d; return; } d-=L[1];
    if(d<Q){ arcPt(r.X+w-rad,r.Y+h-rad,0,d,rad,x,y); return; } d-=Q;
    if(d<L[2]){ x=r.X+w-rad-d; y=r.Y+h; return; } d-=L[2];
    if(d<Q){ arcPt(r.X+rad,r.Y+h-rad,PIf*.5f,d,rad,x,y); return; } d-=Q;
    if(d<L[3]){ x=r.X; y=r.Y+h-rad-d; return; } d-=L[3];
    arcPt(r.X+rad,r.Y+rad,PIf,d,rad,x,y);
}
static BYTE RunnerAlpha(){
    float bright=g_cfg.ledBrightness/100.0f;
    int a=(int)(200*bright); return (BYTE)std::max(0,std::min(255,a));
}
static bool GlitchNow(DWORD nowMs){
    if(!g_cfg.glitchOn||!FxOn()||!g_player.playing) return false;
    DWORD c=nowMs%2800; return c<190;
}
static std::wstring FormatTime(DWORD ms){
    int total=(int)(ms/1000), m=total/60, s=total%60; wchar_t b[32];
    swprintf(b,32,L"%d:%02d",m,s); return b;
}
static void ApplyTheme(){ g_theme=FindTheme(g_themes,g_cfg.theme); }

// ---- volume / mudo ------------------------------------------------------
static void ApplyVolume(){ g_player.SetVolume(g_muted?0:g_cfg.volume); }
static void SetVolumePercent(int v){ g_cfg.volume=std::max(0,std::min(100,v)); if(g_muted&&v>0) g_muted=false; ApplyVolume(); }
static void ToggleMute(){ g_muted=!g_muted; ApplyVolume(); SetStatus(g_muted?L"Mudo":L"Som ligado",1200); }

// ---- FFT + espectrograma --------------------------------------------------
static void FftMag(float* re,float* im,int N){
    for(int i=1,j=0;i<N;i++){int bit=N>>1;for(;j&bit;bit>>=1)j^=bit;j^=bit;
        if(i<j){std::swap(re[i],re[j]);std::swap(im[i],im[j]);}}
    for(int len=2;len<=N;len<<=1){
        float ang=-6.2831853f/len,wr=cosf(ang),wi=sinf(ang);
        for(int i=0;i<N;i+=len){
            float cr=1.f,ci=0.f;
            for(int k=0;k<len/2;k++){
                int a=i+k,b=i+k+len/2;
                float vr=re[b]*cr-im[b]*ci,vi=re[b]*ci+im[b]*cr;
                float ur=re[a],ui=im[a];
                re[a]=ur+vr;im[a]=ui+vi;re[b]=ur-vr;im[b]=ui-vi;
                float ncr=cr*wr-ci*wi;ci=cr*wi+ci*wr;cr=ncr;
            }
        }
    }
}
static void SpecBands(const float* mag,int nbins,int sr,float out[48]){
    const int NB=48;
    const float fmin=45.f,fmax=15000.f,fnyq=sr*0.5f;
    for(int k=0;k<NB;k++){
        float f0=fmin*powf(fmax/fmin,(float)k/NB);
        float f1=fmin*powf(fmax/fmin,(float)(k+1)/NB);
        int b0=(int)(f0/fnyq*nbins); if(b0<1)b0=1; if(b0>=nbins)b0=nbins-1;
        int b1=(int)(f1/fnyq*nbins)+1; if(b1<=b0)b1=b0+1; if(b1>=nbins)b1=nbins-1;
        float e=0.f; for(int j=b0;j<=b1;j++) if(mag[j]>e)e=mag[j];
        e*=1.f+1.1f*(float)k/NB;
        out[k]=std::min(1.f,powf(e*2.5f,.5f));
    }
}
// Analise da faixa atual em thread: onda (320 buckets) + espectrograma.
static void AnalyzeCurrentWave(){
    if(g_current<0 || g_current>=(int)g_tracks.size()) return;
    std::wstring path=g_currentSource.empty()?g_tracks[g_current].path:g_currentSource;
    unsigned long job=++WS().job;
    if(g_waveThread.joinable()) g_waveThread.detach();
    { std::lock_guard<std::mutex> lock(WS().m); WS().data.clear(); WS().path=path; }
    { std::lock_guard<std::mutex> lk(WS().fm); WS().spec.clear(); }
    g_waveThread=std::thread([path,job](){
        try{
            const int N=2048;
            std::vector<float> raw, bucket, hann(N), re(N), im(N), mag(N/2), ring(N,0.f), specLocal;
            for(int i=0;i<N;i++){float a=6.2831853f*i/2047.f;hann[i]=.5f-.5f*cosf(a);}
            size_t framesPerBucket=2205, hop=2205, sinceHop=0, rpos=0; bool first=true; int sr=44100;
            auto publishSpec=[&](){
                if(specLocal.empty()) return;
                std::lock_guard<std::mutex> lk(WS().fm);
                if(job==WS().job.load()){ WS().spec.insert(WS().spec.end(),specLocal.begin(),specLocal.end()); WS().specHopMs=(int)(hop*1000/std::max(1,sr)); WS().specHopF=hop*1000.0/std::max(1,sr); }
                specLocal.clear();
            };
            bool ok=Player::DecodeMono(path,[&](const float* f,size_t n,unsigned srr)->bool{
                if(job!=WS().job.load()) return false;
                if(first){ sr=(int)srr; framesPerBucket=std::max<size_t>(256,(size_t)sr/20); hop=std::max<size_t>(256,(size_t)sr/40); first=false; }   // espectro a cada 25 ms (ritmo)
                for(size_t i=0;i<n;i++){
                    float v=f[i];
                    bucket.push_back(fabsf(v));
                    if(bucket.size()>=framesPerBucket){float m=0.f;for(float b:bucket)m=std::max(m,b);raw.push_back(sqrtf(std::max(0.f,m)));bucket.clear();}
                    ring[rpos]=v; rpos=(rpos+1)%N;
                    if(++sinceHop>=hop){
                        sinceHop=0;
                        for(int k=0;k<N;k++){re[k]=ring[(rpos+k)%N]*hann[k];im[k]=0.f;}
                        FftMag(re.data(),im.data(),N);
                        for(int k=0;k<N/2;k++)mag[k]=sqrtf(re[k]*re[k]+im[k]*im[k])/(N/4);
                        float bands[48]; SpecBands(mag.data(),N/2,sr,bands);
                        specLocal.insert(specLocal.end(),bands,bands+48);
                        if(specLocal.size()>=48*20) publishSpec();
                    }
                }
                return true;
            });
            publishSpec();
            if(!bucket.empty()){float m=0.f;for(float b:bucket)m=std::max(m,b);raw.push_back(sqrtf(std::max(0.f,m)));}
            if(ok && !raw.empty() && job==WS().job.load()){
                const size_t buckets=320;
                std::vector<float> data(buckets,0.f);
                for(size_t i=0;i<buckets;++i){size_t a=(i*raw.size())/buckets,b=((i+1)*raw.size())/buckets;if(b<=a)b=std::min(raw.size(),a+1);float mx=0.f;for(size_t j=a;j<b;++j)mx=std::max(mx,raw[j]);data[i]=std::max(0.06f,std::min(1.f,mx));}
                std::lock_guard<std::mutex> lock(WS().m);
                if(job==WS().job.load()){ WS().data.swap(data); WS().path=path; }
            }
        } catch(...){}
    });
}
static void ClearWave(){ ++WS().job; { std::lock_guard<std::mutex> lk(WS().m); WS().data.clear(); WS().path.clear(); } { std::lock_guard<std::mutex> lk(WS().fm); WS().spec.clear(); WS().hasSpec=false; WS().pulse=0; WS().onsetMax=0.05f; WS().energyMax=0.02f; } }
static void UpdateSpecBands(bool playingNow,DWORD posMs,float dt){
    WaveState&W=WS();
    std::lock_guard<std::mutex> lk(W.fm);
    int frames=(int)(W.spec.size()/48);
    float tick=dt/0.03f;
    if(!playingNow||frames==0){
        bool any=false; float decay=powf(.80f,tick);
        for(float&b:W.bands){b*=decay;if(b>.004f)any=true;}
        W.pulse*=decay;
        if(!any){for(float&b:W.bands)b=0;W.hasSpec=false;W.pulse=0;}
        return;
    }
    // O que se ouve agora esta atrasado em relacao ao cursor (buffer da saida de som): antes a onda lia
    // 50 ms A FRENTE e ficava adiantada. Agora le o instante que esta saindo nos fones.
    static unsigned lat=0; static ULONGLONG latAt=0; ULONGLONG nowT=GetTickCount64();
    if(nowT-latAt>2000){ lat=Player::OutputLatencyMs(); latAt=nowT; }
    long long tms=(long long)posMs-(long long)lat; if(tms<0) tms=0;
    double hop=std::max(1.0,W.specHopF);
    int idx=(int)((double)tms/hop); if(idx>=frames)idx=frames-1; if(idx<0)idx=0;
    const float* v=&W.spec[(size_t)idx*48];
    // batida: quanto cada banda subiu desde o quadro anterior (graves pesam mais: bumbo/caixa)
    float flux=0, energy=0;
    for(int k=0;k<48;k++) energy+=v[k];
    energy/=48.f;
    if(idx>0){ const float* pv=&W.spec[(size_t)(idx-1)*48]; for(int k=0;k<48;k++){ float d=v[k]-pv[k]; if(d>0) flux+=d*(k<12?1.6f:(k<30?1.0f:0.5f)); } }
    W.onsetMax=std::max(W.onsetMax*powf(.995f,tick),flux);
    W.energyMax=std::max(W.energyMax*powf(.998f,tick),energy);
    float on=W.onsetMax>0.0001f?std::min(1.f,flux/W.onsetMax):0.f;
    on=on>0.35f?(on-0.35f)/0.65f:0.f;              // so subida forte conta como batida
    if(on>W.pulse) W.pulse=on; else W.pulse*=powf(.80f,tick);   // sobe na hora e cai em ~150 ms
    float rise=1.f-powf(1.f-.55f,tick), fall=1.f-powf(.86f,tick);
    for(int k=0;k<48;k++){float o=W.bands[k],t=v[k];W.bands[k]=(t>o)?o+(t-o)*rise:o+(t-o)*fall;}
    W.hasSpec=true;
}

// ---- analise incremental do streaming (a musica online mostra a onda/espectro real) ----
// O canal de streaming chama este aqui com o PCM que chega; a UI publica em WS() no Tick.
void StreamWavePump(StreamJob* jp,const int16_t* s16,size_t nSamples,uint64_t absFirstFrame){
    if(!jp||!jp->wa)return;
    StreamJob& j=*jp; StreamWave& w=*j.wa;
    std::lock_guard<std::mutex> lk(w.m);
    if(w.started&&absFirstFrame!=w.firstAbs+w.fed){   // seek/reinicio: o PCM novo nao emenda com o anterior
        w.started=false; w.env.clear(); w.spec.clear(); w.envAcc=0; w.envN=0; w.specSince=0; w.rpos=0; w.fed=0; ++w.gen;
    }
    if(!w.started){ w.firstAbs=absFirstFrame; w.rate=j.st&&j.st->rate?j.st->rate:48000; w.envHop=std::max(256u,(uint32_t)(w.rate/20)); w.specHop=w.envHop; w.started=true; }
    const int NB=48;
    for(size_t k=0;k+1<nSamples;k+=2){
        float l=(float)s16[k],r=(float)s16[k+1];
        float v=(l+r)*0.5f/32768.f; if(v<0.f)v=-v;
        if(v>w.envAcc)w.envAcc=v;
        ++w.envN;
        float mono=(l+r)*0.5f/32768.f;   // janela (mono) em [-1,1], mesma escala da musica local
        w.ring[w.rpos]=mono; w.rpos=(w.rpos+1)%2048;
        ++w.specSince;
        if(w.specSince>=(size_t)w.specHop){
            w.specSince=0;
            float re[2048],im[2048];
            for(int i=0;i<2048;i++){ float a=6.2831853f*(float)i/2047.f; re[i]=w.ring[(w.rpos+i)%2048]*(.5f-.5f*cosf(a)); im[i]=0.f; }
            FftMag(re,im,2048);
            float mag[1024]; for(int i=0;i<1024;i++)mag[i]=sqrtf(re[i]*re[i]+im[i]*im[i])/(2048.f/4.f);
            float bands[NB]; SpecBands(mag,1024,w.rate,bands);
            w.spec.insert(w.spec.end(),bands,bands+NB);
        }
    }
    if(w.envN>=(size_t)w.envHop){
        w.env.push_back(std::max(0.06f,std::min(1.f,sqrtf(std::max(0.f,w.envAcc)))));
        w.envN=0; w.envAcc=0;
    }
    w.fed+=nSamples/2;
}
// Publica o que ja chegou do streaming em WS() (mesmo formato da musica local). Throttled.
static int g_pubStreamId=0; static unsigned long g_pubGen=0; static size_t g_pubSpecN=(size_t)-1; static ULONGLONG g_pubDataAt=0;
static void PublishStreamWave(){
    if(!g_player.loaded||!g_player.IsStream()){ g_pubStreamId=0; g_pubDataAt=0; g_streamAheadMs=0; return; }
    std::shared_ptr<StreamJob> j=FindStreamId(g_curStreamId);
    if(!j||!j->wa)return;
    StreamWave& w=*j->wa;
    std::lock_guard<std::mutex> lk(w.m);
    if(!w.started)return;
    if(j->id!=g_pubStreamId||w.gen!=g_pubGen){ g_pubStreamId=j->id; g_pubGen=w.gen; g_pubSpecN=(size_t)-1; g_pubDataAt=0; }
    uint64_t rate=w.rate?w.rate:48000;
    uint64_t L0=j->st?j->st->lenFrames.load():0;          // duracao real (metadados), quando se sabe
    uint64_t received=w.firstAbs+(uint64_t)w.env.size()*(uint64_t)w.envHop;   // o que ja chegou
    if(L0==0&&received>0&&g_player.IsStream()){           // sem duracao (URL direta/radio): "duracao" = o que ja chegou
        g_player.SetStreamLengthMs((DWORD)std::min<uint64_t>(received*1000ULL/rate,0xFFFFFFFFULL));
    }
    uint64_t lenFrames=L0?L0:received;                    // denominador da onda: total real ou (crescente) o recebido
    g_streamAheadMs=(double)received*1000.0/(double)rate; // pontos de dados que ja existem em WS().data/spec
    // espectro: grade de ~50 ms a partir do inicio da musica (gridStart cobre pausa/seeks)
    if(!w.spec.empty()&&w.spec.size()!=g_pubSpecN){
        size_t gridStart=(size_t)((w.firstAbs*1000ULL/rate)/50);
        std::vector<float> whole(gridStart+w.spec.size());
        memcpy(&whole[gridStart],w.spec.data(),w.spec.size()*sizeof(float));
        { std::lock_guard<std::mutex> lkf(WS().fm); if(j->id==g_pubStreamId&&w.gen==g_pubGen){ WS().spec.swap(whole); WS().specHopMs=50; WS().specHopF=50.0; } }
        g_pubSpecN=w.spec.size();
    }
    // onda: 320 buckets do que ja chegou (com janela equivalente por fração)
    ULONGLONG now=GetTickCount64();
    if(!w.env.empty()&&lenFrames>0&&(g_pubDataAt==0||now-g_pubDataAt>400)){
        g_pubDataAt=now;
        const size_t buckets=320;
        std::vector<float> data(buckets,0.f);
        double flen=(double)lenFrames, first=(double)w.firstAbs, last=(double)w.firstAbs+(double)w.env.size()*(double)w.envHop;
        for(size_t i=0;i<buckets;i++){
            double c=buckets>1?(double)i/(double)(buckets-1):0.; c*=flen;
            double half=flen/(double)buckets*1.45, a=std::max(0.,c-half), b=std::min(flen,c+half);
            if(b<first)continue;
            size_t e0=a>first?(size_t)((a-first)/(double)w.envHop):0, e1=b<last?(size_t)((b-first)/(double)w.envHop):w.env.size();
            if(e1>w.env.size())e1=w.env.size();
            float mx=0.f; for(size_t e=e0;e<e1;e++)if(w.env[e]>mx)mx=w.env[e];
            if(mx>0.f)data[i]=std::max(0.06f,std::min(1.f,mx));
        }
        std::lock_guard<std::mutex> lock(WS().m);
        if(j->id==g_pubStreamId&&w.gen==g_pubGen){ WS().data.swap(data); WS().path=L"stream"; }
    }
}

// ---- biblioteca ------------------------------------------------------------
static void ApplySort();
static void PlayIndex(int idx,bool autoplay);
static void LoadFolderAndPlaylist(){
    if(g_cfg.musicFolder.empty()) { g_tracks.clear(); g_current=-1; g_listScroll=0; return; }
    g_tracks=ScanFolder(g_cfg.musicFolder);
    for(auto&t:g_tracks){auto it=g_artistMap.find(t.path);if(it!=g_artistMap.end())t.artist=it->second;}
    ApplySort();
    g_current=g_tracks.empty()?-1:0; g_listScroll=0;
}
// Varredura em thread. Um pedido novo durante uma varredura nao se perde mais: a
// thread em andamento roda de novo com o pedido mais recente (antes, trocar de pasta
// rapido deixava a lista vazia para sempre).
static void RunScanLoop(){
    for(;;){
        while(SS().again.exchange(false)){
            unsigned long gen=SS().gen.load(); bool computer; std::wstring folder;
            { std::lock_guard<std::mutex> lk(SS().m); computer=SS().computer; folder=SS().folder; }
            try{
                auto result=computer?ScanComputerMusic():ScanFolder(folder);
                if(gen==SS().gen.load()){ { std::lock_guard<std::mutex> lk(SS().m); SS().result.swap(result); } SS().ready=true; }
            } catch(...){}
        }
        SS().busy=false;
        if(!SS().again.load()||SS().busy.exchange(true)) return;
    }
}
static void RequestScan(bool computer,const std::wstring& folder){
    { std::lock_guard<std::mutex> lk(SS().m); SS().computer=computer; SS().folder=folder; }
    ++SS().gen; SS().ready=false; SS().again=true;
    if(SS().busy.exchange(true)) return;
    if(g_scanThread.joinable()) g_scanThread.join();
    g_scanThread=std::thread(RunScanLoop);
}
static void StartAutoScan(){ RequestScan(true,L""); }
static std::atomic<bool> g_refreshMode{false};
static void StartFolderRescan(){
    if(g_cfg.musicFolder.empty()) return;
    g_refreshMode=true;
    RequestScan(false,g_cfg.musicFolder);
}
static void PollFolderWatch(){
    if(g_cfg.musicFolder.empty()) return;
    if(!PlatformWatchTake(900)) return;
    StartFolderRescan();
}
// Capas embutidas extraidas ENQUANTO a varredura rodava: o aviso chegou antes da lista
// existir e se perdia. Ao receber a lista, aplica o que ja esta no indice.
static void RefreshEmbeddedCovers(std::vector<Track>& v){
    std::map<std::wstring,std::wstring> custom; bool loaded=false;
    for(auto& t:v){
        if(IsOnlineTrack(t)) continue;
        std::wstring emb;
        if(art::Lookup(t.path,LowerExt(std::filesystem::path(t.path)),emb)!=1||emb==t.coverPath) continue;
        if(!loaded){ custom=LoadCustomCovers(); loaded=true; }
        if(!custom.count(t.path)) t.coverPath=emb;
    }
}
static void ConsumeAutoScan(){
    if(!SS().ready.exchange(false)) return;
    if(g_view==2){   // playlist aberta: a biblioteca nova fica guardada, a lista em tela nao muda
        std::lock_guard<std::mutex> lk(SS().m); g_libTracks.swap(SS().result); g_libCached=true; g_refreshMode=false; RefreshEmbeddedCovers(g_libTracks);
        for(auto&t:g_libTracks){auto it=g_artistMap.find(t.path);if(it!=g_artistMap.end())t.artist=it->second;}
        return;
    }
    bool preserve = g_refreshMode.exchange(false);
    std::wstring curPath;
    bool wasPlaying = g_player.playing;
    if(preserve && g_current>=0 && g_current<(int)g_tracks.size()) curPath = g_tracks[g_current].path;
    std::vector<Track> keepOnline; for(auto&t:g_tracks) if(IsOnlineTrack(t)) keepOnline.push_back(t);   // musicas online abertas pela busca
    { std::lock_guard<std::mutex> lk(SS().m);
      if(preserve && !curPath.empty() && SS().result.size()==g_tracks.size()){
          // mesma lista? compara conjunto de caminhos (a ordem em tela pode ser outra)
          std::vector<std::wstring> a,b; for(auto&t:g_tracks)a.push_back(t.path); for(auto&t:SS().result)b.push_back(t.path);
          std::sort(a.begin(),a.end()); std::sort(b.begin(),b.end());
          if(a==b) return;
      }
      g_tracks.swap(SS().result);
    }
    for(auto& t:keepOnline) g_tracks.push_back(t);
    RefreshEmbeddedCovers(g_tracks);
    for(auto&t:g_tracks){auto it=g_artistMap.find(t.path);if(it!=g_artistMap.end())t.artist=it->second;}
    ApplySort();
    if(preserve && !curPath.empty()){
        int ni=-1;
        for(size_t i=0;i<g_tracks.size();++i) if(g_tracks[i].path==curPath){ ni=(int)i; break; }
        if(ni>=0){ g_current=ni; return; }
        if(wasPlaying) g_player.Pause();
        g_current=g_tracks.empty()?-1:0; g_listScroll=0;
        return;
    }
    g_current=g_tracks.empty()?-1:0; g_listScroll=0;
    if(g_current>=0) PlayIndex(g_current,false);
    if(g_tracks.empty()) SetStatus(L"Nenhuma musica encontrada nas pastas do usuario.",4000);
}
// Pastas recentes (menu PASTA)
static void AddRecentFolder(const std::wstring& f){
    if(f.empty()) return;
    auto& v=g_cfg.recentFolders;
    v.erase(std::remove_if(v.begin(),v.end(),[&](const std::wstring& x){ return _wcsicmp(x.c_str(),f.c_str())==0; }),v.end());
    v.insert(v.begin(),f);
    if(v.size()>6) v.resize(6);
}
static void SwitchFolder(const std::wstring& f){   // "" = padrao (pastas do usuario)
    g_pickMode=false; g_pickSel.clear();
    if(g_view==2||g_view==1){ g_libCached=false; g_openPl=-1; g_view=0; g_cfg.openPlaylist.clear(); }
    ++SS().gen; SS().ready=false;
    g_cfg.musicFolder=f;
    if(!f.empty()) AddRecentFolder(f);
    g_cfg.Save(); g_showSettings=false; g_folderMenuOpen=false;
    LoadFolderAndPlaylist();
    if(f.empty()){ PlatformWatchStop(); StartAutoScan(); SetStatus(L"Procurando musicas em Musicas, Downloads, Documentos e Area de trabalho...",4000); }
    else { PlatformWatchStart(f); if(g_current>=0) PlayIndex(g_current,false); }
}

// ---- reproducao -------------------------------------------------------------
static void OpenAndStart(const std::wstring& src,bool autoplay){
    if(g_player.Open(src)){
        g_currentSource=src;
        if(g_resumeMs){ g_player.SeekMs(g_resumeMs); g_resumeMs=0; }
        ApplyVolume();
        AnalyzeCurrentWave();
        if(autoplay) g_player.Play();
    } else SetStatus(Player::HasAudio()?L"Nao consegui abrir este arquivo.":L"Sem saida de audio (nenhum dispositivo de som encontrado).",3000);
}
static std::vector<std::wstring> g_saidas;   // dispositivos de som listados no menu "onde tocar"
// Troca onde o som sai (fone, caixa, HDMI...): fecha e reabre o audio e volta
// para o mesmo ponto da musica. Nome vazio = o que o sistema estiver usando.
static void TrocarSaida(const std::wstring& nome){
    if(Player::OutDeviceName()==nome) return;
    bool tocava=g_player.playing;
    DWORD pos=g_player.loaded?g_player.GetPositionMs():0;
    std::wstring src=g_currentSource;
    g_player.Close();
    Player::OutDeviceName()=nome;
    g_cfg.outDevice=nome; g_cfg.Save();
    Player::GlobalShutdown();
    Player::GlobalInit();
    if(!src.empty()){ g_resumeMs=pos; OpenAndStart(src,tocava); }
    SetStatus(nome.empty()?L"Som no dispositivo padrão do sistema.":L"Som em: "+nome,3000);
}
// ---- fila embaralhada ---------------------------------------------------------
// A lista visivel continua na ordem do usuario (inclusive a manual); o aleatorio
// so muda a ORDEM DE REPRODUCAO: cada faixa toca uma vez por ciclo e "anterior"
// volta pela fila. Clicar numa faixa recomeca a fila a partir dela.
static std::vector<int> g_shufQueue; static int g_shufPos=-1;
static void RebuildShuffleQueue(int startIdx){
    g_shufQueue.clear(); g_shufPos=-1;
    int n=(int)g_tracks.size(); if(n<=0) return;
    for(int i=0;i<n;i++) if(i!=startIdx) g_shufQueue.push_back(i);
    for(int i=(int)g_shufQueue.size()-1;i>0;i--){ int j=rand()%(i+1); std::swap(g_shufQueue[(size_t)i],g_shufQueue[(size_t)j]); }
    if(startIdx>=0&&startIdx<n){ g_shufQueue.insert(g_shufQueue.begin(),startIdx); g_shufPos=0; }
}
static void PlayOnlineIndex(int idx,bool autoplay,unsigned gen);
static std::wstring StemSourceFor(const Track& t); static int StemModeNow(); static void RequestStemsFor(int idx,bool front); static void StemsAhead();
static void PlayIndex(int idx,bool autoplay=true){
    if(g_tracks.empty()) return; if(idx<0)idx=(int)g_tracks.size()-1; if(idx>=(int)g_tracks.size())idx=0;
    // aleatorio: se a faixa nao e a proxima da fila (clique do usuario), recomeca a fila a partir dela
    if(g_cfg.shuffle&&(g_shufQueue.size()!=g_tracks.size()||g_shufPos<0||g_shufPos>=(int)g_shufQueue.size()||g_shufQueue[(size_t)g_shufPos]!=idx)) RebuildShuffleQueue(idx);
    g_current=idx; g_nowPlaying=g_tracks[(size_t)idx]; g_nowPlayingValid=true;
    const std::wstring path=g_tracks[idx].path;
    if(autoplay) desc::Registrar(g_tracks[(size_t)idx].artist,path);   // o que voce ouve alimenta as recomendacoes (so no seu PC)
    PlatformLoadCover(g_tracks[idx].coverPath);
    unsigned gen=++g_openGen;
    g_currentSource.clear();
    g_player.Close(); g_curStreamOpen=false; g_curStreamId=0; g_queueTick=0;   // o canal antigo sai na proxima UpdateStreamQueue (se nao for uma das proximas)
    {   // modo de stem ligado e ja separado: toca o stem (senao pede a separacao e toca a completa enquanto isso)
        std::wstring ss=StemSourceFor(g_tracks[(size_t)idx]);
        if(!ss.empty()){ ClearWave(); g_converting=false; OpenAndStart(ss,autoplay); StemsAhead(); return; }
        if(StemModeNow()!=stems::M_FULL){ RequestStemsFor(idx,true); StemsAhead(); }
    }
    if(IsOnlineTrack(g_tracks[(size_t)idx])){ PlayOnlineIndex(idx,autoplay,gen); return; }
    if(!Player::ProbeNative(path)){
        g_player.Close();
        if(!PlatformHaveFfmpeg()) {
            g_converting=false;
            bool nativeExt=IsNativeAudioExt(LowerExt(std::filesystem::path(path)));
            SetStatus(nativeExt?L"Nao consegui abrir este arquivo (danificado ou codec incomum).":L"Formato nao suportado (instale o ffmpeg para tocar este arquivo).",4000);
            return;
        }
        g_converting=true; g_pendingAutoplay=autoplay;
        std::thread([path,gen](){
            std::wstring dst; RemixSafe("conversao com ffmpeg",[&]{ dst=PlatformTranscodeToWav(path); });
            AppPost(EV_TRANSCODED,dst,(int)gen);
        }).detach();
        return;
    }
    g_converting=false;
    OpenAndStart(path,autoplay);
}
static void OnTranscoded(const std::wstring& dst,unsigned gen){
    if(gen!=g_openGen) return;
    g_converting=false;
    if(dst.empty()){ SetStatus(L"A conversao com o ffmpeg falhou.",3500); return; }
    OpenAndStart(dst,g_pendingAutoplay);
}
#include "app_online_a.h"
// ---- efeitos de audio e stems (PC) ------------------------------------------------------------
static void ApplyFxNow(){ Player::SetFx(g_cfg.fxSlow,g_cfg.fxSpeed,g_cfg.fxReverb,g_cfg.fxBass,g_cfg.fx8d); g_player.ApplyFx(); }
static const wchar_t* FxName(int i){ static const wchar_t* n[5]={L"SLOW",L"SPEED",L"REVERB",L"GRAVE",L"8D"}; return n[std::max(0,std::min(4,i))]; }
static int& FxLevel(int i){ switch(i){ case 0: return g_cfg.fxSlow; case 1: return g_cfg.fxSpeed; case 2: return g_cfg.fxReverb; case 3: return g_cfg.fxBass; default: return g_cfg.fx8d; } }
static bool AnyFxOn(){ return g_cfg.fxSlow||g_cfg.fxSpeed||g_cfg.fxReverb||g_cfg.fxBass||g_cfg.fx8d||!g_cfg.stemMode.empty(); }
static void CycleFx(int i){   // cada clique sobe um nivel (1, 2, 3) e o proximo desliga
    int& v=FxLevel(i); v=(v+1)%4;
    if(i==0&&v) g_cfg.fxSpeed=0;
    if(i==1&&v) g_cfg.fxSlow=0;
    g_cfg.Save(); ApplyFxNow();
    SetStatus(std::wstring(FxName(i))+(v?L": nível "+std::to_wstring(v):std::wstring(L": desligado")),1600);
}
static void ClearFx(){ g_cfg.fxSlow=g_cfg.fxSpeed=g_cfg.fxReverb=g_cfg.fxBass=g_cfg.fx8d=0; g_cfg.Save(); ApplyFxNow(); SetStatus(L"Efeitos desligados.",1600); }
static std::wstring StemIdOf(const Track& t){ return IsOnlineTrack(t)?t.url:t.path; }
static std::wstring StemKeyOf(const Track& t){ return stems::KeyFor(StemIdOf(t),IsOnlineTrack(t)); }
static std::wstring StemKeyCurrent(){   // para desenhar: guarda a chave da faixa atual (a de arquivo le tamanho/data)
    static std::wstring id,key;
    if(g_current<0||g_current>=(int)g_tracks.size()) return L"";
    const Track& t=g_tracks[(size_t)g_current];
    if(StemIdOf(t)!=id){ id=StemIdOf(t); key=StemKeyOf(t); }
    return key;
}
static int StemModeNow(){ return stems::ModeFromKey(g_cfg.stemMode); }
static std::wstring StemSourceFor(const Track& t){
    int m=StemModeNow(); if(m==stems::M_FULL) return L"";
    std::wstring f=stems::FileFor(StemKeyOf(t),m); std::error_code ec;
    return (!f.empty()&&std::filesystem::exists(std::filesystem::path(f),ec))?f:L"";
}
static bool IsStemFile(const std::wstring& p){ return !p.empty()&&Config::StartsI(Config::NormSep(p),Config::NormSep(stems::Root())+REMIX_SEP_STR); }
static void StemsNotify(const std::wstring& key,int st){ AppPost(EV_STEMS,key,st); }
static void RequestStemsFor(int idx,bool front){
    if(idx<0||idx>=(int)g_tracks.size()||!stems::Installed()) return;
    const Track& t=g_tracks[(size_t)idx];
    if(IsOnlineTrack(t)){ OTrack o=OTrackFor(t); stems::Request(t.url,true,o.play,o.title,o.artist,o.dur,front,StemsNotify); }
    else stems::Request(t.path,false,L"",t.title,t.artist,t.durSec,front,StemsNotify);
}
static void StemsAhead(){   // modo de stem ligado: as proximas 2 da fila ja vao separando
    if(StemModeNow()==stems::M_FULL||!stems::Installed()) return;
    for(int i:UpcomingIndices(2)) RequestStemsFor(i,false);
}
// Troca o que esta tocando para o modo escolhido, no mesmo ponto (se o stem ja existe).
static void SwitchStemSourceNow(){
    if(g_current<0||g_current>=(int)g_tracks.size()) return;
    const Track& t=g_tracks[(size_t)g_current];
    int m=StemModeNow(); bool was=g_player.playing;
    DWORD pos=g_player.GetPositionMs();
    if(m==stems::M_FULL){
        if(!IsStemFile(g_currentSource)) return;   // ja esta na completa
        g_resumeMs=pos?pos:1; PlayIndex(g_current,was); return;
    }
    std::wstring want=StemSourceFor(t);
    if(want.empty()){
        if(!stems::Installed()){ SetStatus(L"Para separar em stems instale o Demucs: instale o Demucs no PC (pip install demucs).",5000); return; }
        RequestStemsFor(g_current,true); StemsAhead();
        SetStatus(L"Separando esta música (na 1ª vez leva ~metade da duração). Enquanto isso toca a completa.",4500); return;
    }
    if(_wcsicmp(g_currentSource.c_str(),want.c_str())==0) return;
    g_player.Close(); g_curStreamOpen=false; g_curStreamId=0; g_converting=false;
    if(g_player.Open(want)){ g_currentSource=want; if(pos) g_player.SeekMs(pos); ApplyVolume(); AnalyzeCurrentWave(); if(was) g_player.Play(); }
    else SetStatus(L"Não consegui abrir o stem.",3000);
}
static void SetStemMode(int m){
    m=std::max(0,std::min((int)stems::M_COUNT-1,m));
    g_cfg.stemMode=stems::ModeKey(m); g_cfg.Save();
    if(m==stems::M_FULL) stems::CancelQueued(false);
    SwitchStemSourceNow();
}
static void OnStemsEvent(const std::wstring& key,int st){
    if(g_current<0||g_current>=(int)g_tracks.size()||StemKeyCurrent()!=key) return;
    if(st==stems::S_READY&&StemModeNow()!=stems::M_FULL){ SwitchStemSourceNow(); SetStatus(std::wstring(L"Stems prontos: ")+stems::ModeName(StemModeNow()),2500); }
    else if(st==stems::S_FAILED){ auto j=stems::Find(key); std::wstring e; if(j){ std::lock_guard<std::mutex> lk(j->m); e=j->err; } SetStatus(e.empty()?std::wstring(L"A separação falhou."):e,6000); }
}
static bool PlayFileDirect(const std::wstring& path,bool autoplay=true){
    std::error_code ec;
    if(path.empty() || !std::filesystem::exists(std::filesystem::path(path),ec)) return false;
    for(size_t i=0;i<g_tracks.size();++i) if(_wcsicmp(g_tracks[i].path.c_str(),path.c_str())==0){PlayIndex((int)i,autoplay);return true;}
    Track t; t.path=path; t.title=std::filesystem::path(path).stem().wstring(); t.artist=L"Artista desconhecido"; ResolveTrackCover(t,std::filesystem::path(path),LowerExt(std::filesystem::path(path)),LoadCustomCovers());
    t.fileTime=FileTimeOf(std::filesystem::path(path));
    auto tg=ReadTags(path,LowerExt(std::filesystem::path(path))); if(!tg.title.empty())t.title=tg.title; if(!tg.artist.empty())t.artist=tg.artist;
    {auto it=g_artistMap.find(t.path);if(it!=g_artistMap.end())t.artist=it->second;}
    g_tracks.push_back(t);
    if(g_cfg.sortMode!=L"manual") ApplySort();
    int idx=-1; for(size_t i=0;i<g_tracks.size();++i) if(g_tracks[i].path==path){idx=(int)i;break;}
    if(idx<0) return false;
    PlayIndex(idx,autoplay);
    return g_player.loaded||g_converting;
}
static int NextShuffled(){
    int n=(int)g_tracks.size(); if(n<=0) return -1;
    if((int)g_shufQueue.size()!=n) RebuildShuffleQueue(g_current);
    if(g_shufPos+1>=(int)g_shufQueue.size()){        // fim do ciclo: novo embaralhado, sem repetir a ultima
        int last=g_current; RebuildShuffleQueue(-1);
        if(n>1&&g_shufQueue[0]==last) std::swap(g_shufQueue[0],g_shufQueue[1]);
        g_shufPos=0; return g_shufQueue[0];
    }
    return g_shufQueue[(size_t)++g_shufPos];
}
static int PrevShuffled(){
    if(g_shufPos>0&&g_shufPos<(int)g_shufQueue.size()) return g_shufQueue[(size_t)--g_shufPos];
    return g_current;
}
static void NextTrack(){
    if(g_tracks.empty())return;
    int n=(int)g_tracks.size(); int idx;
    if(g_cfg.shuffle&&n>1) idx=NextShuffled();
    else idx=g_current+1;
    PlayIndex(idx,true);
}
static void PrevOrRestart(){ if(g_tracks.empty())return; if(g_player.GetPositionMs()>3000)g_player.Restart();else PlayIndex(g_cfg.shuffle?PrevShuffled():g_current-1,true); }
static void TogglePlayPause(){
    if(g_player.playing) g_player.Pause();
    else if(g_player.loaded) g_player.Play();
    else if(g_current>=0&&!g_converting) PlayIndex(g_current,true);
}

// ---- ordem da playlist ------------------------------------------------------
static std::wstring SortModeName(const std::wstring& m){
    if(m==L"artist") return L"ARTISTA"; if(m==L"file") return L"ARQUIVO"; if(m==L"date") return L"DATA"; if(m==L"manual") return L"MANUAL"; return L"TÍTULO";
}
static void ApplySort(){
    std::wstring cur=(g_current>=0&&g_current<(int)g_tracks.size())?g_tracks[g_current].path:L"";
    if(!(g_view==2&&g_cfg.sortMode==L"manual")) SortTracks(g_tracks,g_cfg.sortMode,g_cfg.sortDesc);   // playlist: manual = ordem do playlist.json
    if(!cur.empty()) for(size_t i=0;i<g_tracks.size();++i) if(g_tracks[i].path==cur){g_current=(int)i;break;}
    RebuildShuffleQueue(g_current);   // os indices mudaram
}
static void CycleSortMode(){
    const wchar_t* modes[]={L"title",L"artist",L"file",L"date",L"manual"};
    int i=0; for(int k=0;k<5;k++) if(g_cfg.sortMode==modes[k]) i=k;
    g_cfg.sortMode=modes[(i+1)%5];
    if(g_cfg.sortMode==L"manual") SaveCustomOrder(g_tracks);
    ApplySort(); g_cfg.Save();
    SetStatus(L"Ordem: "+SortModeName(g_cfg.sortMode),1500);
}
static void MoveTrack(int i,int dir){
    if(i<0||i>=(int)g_tracks.size()) return;
    int j=i+dir; if(j<0||j>=(int)g_tracks.size()) return;
    if(g_cfg.sortMode!=L"manual"){ g_cfg.sortMode=L"manual"; }
    std::swap(g_tracks[i],g_tracks[j]);
    if(g_current==i) g_current=j; else if(g_current==j) g_current=i;
    if(g_view==2){ std::vector<std::wstring> ps; for(auto&t:g_tracks) ps.push_back(t.path); PlaylistSetOrder(g_openPl,ps); } else SaveCustomOrder(g_tracks);
    g_cfg.Save();
}

// ---- renomear / excluir arquivo -------------------------------------------
// Troca a chave 'oldPath' por 'newPath' em covers.ini, artists.ini e order.ini.
static void RekeyAssociations(const std::wstring& oldPath,const std::wstring& newPath){
    { auto m=LoadCustomCovers(); auto it=m.find(oldPath); if(it!=m.end()){ std::wstring v=it->second; m.erase(it); if(!newPath.empty()) m[newPath]=v;
        std::vector<std::wstring> ls; for(auto&kv:m) ls.push_back(Config::ToPortable(kv.first)+L"="+Config::ToPortable(kv.second)); WriteAllUtf8Lines(Config::CoversPath(),ls); } }
    { auto m=LoadCustomArtists(); auto it=m.find(oldPath); if(it!=m.end()){ std::wstring v=it->second; m.erase(it); if(!newPath.empty()) m[newPath]=v;
        std::vector<std::wstring> ls; for(auto&kv:m) if(!kv.second.empty()) ls.push_back(Config::ToPortable(kv.first)+L"="+kv.second); WriteAllUtf8Lines(Config::ArtistsPath(),ls); } }
    { auto o=LoadCustomOrder(); bool ch=false; for(auto& p:o) if(p==oldPath){ p=newPath; ch=true; } if(ch){ o.erase(std::remove(o.begin(),o.end(),std::wstring()),o.end()); std::vector<std::wstring> ls; for(auto&p:o) ls.push_back(Config::ToPortable(p)); WriteAllUtf8Lines(Config::OrderPath(),ls); } }
    { auto it=g_artistMap.find(oldPath); if(it!=g_artistMap.end()){ std::wstring v=it->second; g_artistMap.erase(it); if(!newPath.empty()) g_artistMap[newPath]=v; } }
    PlaylistsRekey(oldPath,newPath);
}
static bool RenameTrackFile(int i,const std::wstring& newStem){
    if(i<0||i>=(int)g_tracks.size()) return false;
    std::wstring stem=newStem;
    for(auto& c:stem) if(c==L'/'||c==L'\\'||c==L':'||c==L'*'||c==L'?'||c==L'"'||c==L'<'||c==L'>'||c==L'|') c=L'-';
    stem=Config::Trim(stem);
    if(stem.empty()) return false;
    namespace fs=std::filesystem;
    fs::path oldP(g_tracks[i].path);
    fs::path newP=oldP.parent_path()/(stem+oldP.extension().wstring());
    if(newP==oldP) return true;
    std::error_code ec;
    if(fs::exists(newP,ec)){ SetStatus(L"Ja existe um arquivo com esse nome.",3000); return false; }
    bool wasCurrent=(i==g_current);
    DWORD pos=0; bool wasPlaying=false;
    if(wasCurrent&&g_player.loaded){ pos=g_player.GetPositionMs(); wasPlaying=g_player.playing; g_player.Close(); } // Windows nao renomeia arquivo aberto
    fs::rename(oldP,newP,ec);
    if(ec){ SetStatus(L"Nao consegui renomear o arquivo.",3000); if(wasCurrent) PlayIndex(i,wasPlaying); return false; }
    std::wstring oldPath=g_tracks[i].path, newPath=newP.wstring();
    bool titleWasStem=(g_tracks[i].title==oldP.stem().wstring());
    g_tracks[i].path=newPath;
    if(titleWasStem) g_tracks[i].title=stem;
    RekeyAssociations(oldPath,newPath);
    if(g_cfg.sortMode==L"manual") SaveCustomOrder(g_tracks);
    if(wasCurrent){ PlayIndex(i,wasPlaying); if(pos>0) g_player.SeekMs(pos); }
    SetStatus(L"Arquivo renomeado.",2000);
    return true;
}
static void DeleteTrack(int i){
    if(i<0||i>=(int)g_tracks.size()) return;
    std::wstring path=g_tracks[i].path;
    bool wasCurrent=(i==g_current);
    if(wasCurrent){ g_player.Close(); g_currentSource.clear(); }
    if(!PlatformTrash(path)){ SetStatus(L"Nao consegui mover para a lixeira.",3000); if(wasCurrent) PlayIndex(i,false); return; }
    RekeyAssociations(path,L"");
    PlatformEvictThumb(g_tracks[i].coverPath);
    g_tracks.erase(g_tracks.begin()+i);
    if(g_cfg.sortMode==L"manual") SaveCustomOrder(g_tracks);
    if(g_tracks.empty()){ g_current=-1; PlatformLoadCover(L""); }
    else if(wasCurrent){ g_current=std::min(i,(int)g_tracks.size()-1); PlayIndex(g_current,false); }
    else if(g_current>i) g_current--;
    SetStatus(L"Movido para a lixeira.",2500);
}

// ---- Host (host_server.h): cola definida mais abaixo; aqui so as assinaturas ----
static bool HostRunningNow();
static std::string HostTargetsOf(const std::wstring& slug);   // "" = playlist nao hosteada
static unsigned g_hostFolderGen=0;   // sobe quando uma pasta vinculada muda (o Host republica)
static void HostStopNow();
static bool HostStartFromCfg();
static std::wstring HostPlLabel(const std::wstring& slug);
// ---- editor de texto (artista / nome do arquivo) --------------------------
static void StartArtistEdit(int idx){
    if(idx<0||idx>=(int)g_tracks.size()) return;
    g_editArtist=true; g_editMode=0; g_editTrack=idx; g_editBuf=g_tracks[idx].artist;
}
static void StartFileRename(int idx){
    if(idx<0||idx>=(int)g_tracks.size()) return;
    g_editArtist=true; g_editMode=1; g_editTrack=idx; g_editBuf=std::filesystem::path(g_tracks[idx].path).stem().wstring();
}
static void StartPlaylistNameEdit(int mode,int pl){ g_editArtist=true; g_editMode=mode; g_editTrack=pl; g_editBuf=(mode==3&&pl>=0&&pl<(int)g_playlists.size())?g_playlists[(size_t)pl].name:L"";
    if(mode==4||mode==5){ std::wstring c=Config::Trim(PlatformClipboardText()); if(IsUrlText(c)&&c.size()<600) g_editBuf=c; } }   // link na area de transferencia ja vem colado
static void CancelArtistEdit(){ g_editArtist=false; g_editTrack=-1; g_editBuf.clear(); g_editMode=0; }
// host: 6 = porta, 7 = PIN, 8 = nome do PC
static void StartHostEdit(int mode){ g_editArtist=true; g_editMode=mode; g_editTrack=-1; g_editBuf=mode==6?std::to_wstring(g_cfg.hostPort):(mode==7?g_cfg.hostPin:g_cfg.hostName); }
// Discord (discord_host.h entra mais abaixo, junto do Host): pontes usadas antes dele.
static bool DcSetToken(const std::wstring& t,std::wstring& err);
static void DcSetDjRole(const std::wstring& v);
static bool DcReadyNow();
static int DcPlaylistOnBot(const std::wstring& slug);   // -1 = sem token (nao mostra), 0 = nao, 1 = liberada
static void CommitArtistEdit(){
    std::wstring s=g_editBuf;
    while(!s.empty()&&(s.back()==L' '||s.back()==L'\r'||s.back()==L'\n')) s.pop_back();
    if(g_editMode==1){ RenameTrackFile(g_editTrack,s); CancelArtistEdit(); return; }
    if(g_editMode==9){   // token do bot: sai da memoria da tela na hora
        std::wstring err; bool ok=DcSetToken(s,err); std::fill(g_editBuf.begin(),g_editBuf.end(),L'\0'); std::fill(s.begin(),s.end(),L'\0'); CancelArtistEdit();
        if(!ok){ SetStatus(err,5000); return; }
        SetStatus(L"Token salvo neste PC. Agora toque em LIGAR BOT.",3500);
        return;
    }
    if(g_editMode==10){ std::wstring v=Config::Trim(s); if(v.size()>100) v.resize(100); CancelArtistEdit(); DcSetDjRole(v); SetStatus(v.empty()?L"Sem cargo DJ: só admins e o dono pulam sem votação.":L"Cargo DJ: "+v,2800); return; }
    if(g_editMode==12){   // caminho do programa de linha de comando (fonte externa)
        std::wstring v=Config::Trim(s);
        CancelArtistEdit();
        g_cfg.mediaCli=v; g_cfg.Save(); fonte::Reconferir();
        if(v.empty()) SetStatus(L"Fonte externa desligada. O player local e as playlists continuam iguais.",3800);
        else if(fonte::Configurada()) SetStatus(L"Fonte externa pronta: "+fonte::Versao(),4000);
        else SetStatus(L"Não achei esse arquivo (ou ele não é executável). Confira o caminho.",4500);
        return;
    }
    if(g_editMode==11){   // Rich Presence: Application ID (so numero)
        std::wstring v; for(wchar_t c:Config::Trim(s)) if(c>=L'0'&&c<=L'9') v.push_back(c);
        CancelArtistEdit();
        if(!v.empty()&&(v.size()<15||v.size()>25)){ SetStatus(L"Application ID: são uns 18 números (Developer Portal > seu app > Application ID).",4500); return; }
        g_cfg.rpcAppId=v; g_cfg.Save(); drpc::SetAppId(v);
        SetStatus(v.empty()?L"Rich Presence desligado.":L"Rich Presence ligado: o Discord vai mostrar o que você está ouvindo.",3800);
        return;
    }
    if(g_editMode>=6&&g_editMode<=8){   // host: porta / PIN / nome
        int mode=g_editMode; CancelArtistEdit(); std::wstring v=Config::Trim(s);
        if(mode==6){ int p=_wtoi(v.c_str()); if(p<1024||p>65535){ SetStatus(L"Porta: use um número de 1024 a 65535.",3200); return; } g_cfg.hostPort=p; }
        else if(mode==7){ bool ok=v.size()>=4&&v.size()<=12; for(wchar_t c:v) if(c<L'0'||c>L'9') ok=false; if(!ok){ SetStatus(L"PIN: só números, de 4 a 12 dígitos.",3200); return; } g_cfg.hostPin=v; }
        else g_cfg.hostName=v.size()>40?v.substr(0,40):v;
        g_cfg.Save(); if(HostRunningNow()){ HostStopNow(); HostStartFromCfg(); }
        SetStatus(mode==6?L"Porta salva.":mode==7?L"PIN salvo.":L"Nome salvo.",2200); BuildLayout(); return;
    }
    if(g_editMode==2){
        int pi=CreatePlaylistSafe(s.empty()?L"Playlist":s); CancelArtistEdit();
        if(pi>=0){
            if(!g_pendingAddPath.empty()){
                for(auto& t:g_tracks) if(t.path==g_pendingAddPath){ if(IsOnlineTrack(t)){ std::vector<OTrack> v{OTrackFor(t)}; AddOnlineItemsToPlaylist(pi,v); } else PlaylistAddTrack(pi,t); break; }
                SetStatus(L"Adicionada à playlist \""+g_playlists[(size_t)pi].name+L"\".",2500);
            } else { OpenPlaylistView(pi); SetStatus(L"Playlist criada. Use + ADICIONAR para colocar músicas (biblioteca, arquivos, pasta ou link).",4500); }
        } else SetStatus(L"Nao consegui criar a playlist.",3000);
        g_pendingAddPath.clear(); BuildLayout(); return;
    }
    if(g_editMode==4||g_editMode==5){   // link colado: adiciona na playlist (4) ou cria uma nova (5)
        int mode=g_editMode, pl=g_editTrack; std::wstring url=Config::Trim(s); CancelArtistEdit();
        if(!IsUrlText(url)){ SetStatus(L"Cole um link que comece com https://",3000); return; }
        ResolveLinkAsync(url,(mode==4&&pl>=0&&pl<(int)g_playlists.size())?g_playlists[(size_t)pl].slug:L"");
        SetStatus(L"Lendo o link... (alguns segundos; o Spotify pode levar mais)",5000);
        return;
    }
    if(g_editMode==3){ int pl=g_editTrack; RenamePlaylist(pl,s); if(g_openPl>=0) g_cfg.openPlaylist=g_playlists[(size_t)std::min(g_openPl,(int)g_playlists.size()-1)].slug; CancelArtistEdit(); BuildLayout(); return; }
    if(g_editTrack>=0&&g_editTrack<(int)g_tracks.size()){
        if(s.empty()) s=L"Artista desconhecido";
        g_tracks[g_editTrack].artist=s;
        SaveCustomArtist(g_tracks[g_editTrack].path,s);
        g_artistMap[g_tracks[g_editTrack].path]=s;
    }
    CancelArtistEdit();
}
static void AskDeleteTrack(int idx){
    if(idx<0||idx>=(int)g_tracks.size()) return;
    g_confirmOpen=true; g_confirmKind=0; g_confirmTrack=idx;
    g_confirmText=L"Mover \""+std::filesystem::path(g_tracks[idx].path).filename().wstring()+L"\" para a lixeira?";
}

// ---- menus flutuantes (geometria calculada ao abrir) --------------------
static void OpenImgMenu(int i,const RECT& anchor){
    int bw=(int)S(280),bh=(int)S(112);
    int bx=std::max(6,std::min((int)anchor.left-bw/2,g_winW-bw-6));
    int by=std::max(6,std::min((int)anchor.bottom+6,g_winH-bh-6));
    R_imgBox={bx,by,bx+bw,by+bh};
    R_imgLocal={(LONG)(bx+S(16)),(LONG)(by+S(14)),(LONG)(bx+bw-S(16)),(LONG)(by+S(48))};
    R_imgWeb={(LONG)(bx+S(16)),(LONG)(by+S(58)),(LONG)(bx+bw-S(16)),(LONG)(by+S(98))};
    g_imgMenuTrack=i; g_imgMenuOpen=true;
}
static void OpenFolderMenu(){
    g_folderItemPaths.clear(); R_folderItems.clear();
    g_folderItemPaths.push_back(L"");   // escolher pasta...
    g_folderItemPaths.push_back(L"*");  // pastas do usuario (padrao)
    for(auto& f:g_cfg.recentFolders) if(Config::DirExists(f)) g_folderItemPaths.push_back(f);
    int rowH=(int)S(34), bw=std::min((int)S(440),g_winW-12), bh=(int)S(14)+rowH*(int)g_folderItemPaths.size()+(int)S(10);
    int bx=std::max(6,std::min((int)R_folderBtn.left,g_winW-bw-6)), by=std::min((int)R_folderBtn.bottom+4,g_winH-bh-6);
    R_folderBox={bx,by,bx+bw,by+bh};
    for(size_t i=0;i<g_folderItemPaths.size();++i){ int y=by+(int)S(8)+(int)i*rowH; R_folderItems.push_back({bx+(int)S(8),y,bx+bw-(int)S(8),y+rowH-(int)S(4)}); }
    g_folderMenuOpen=true;
}
static void OpenCtxMenuGeneric(int x,int y,int kind,int arg,const std::vector<std::wstring>& labels,const std::vector<int>& acts,const std::vector<bool>& danger,int width){
    int rowH=(int)S(32), bw=width, n=(int)labels.size(), bh=(int)S(10)+rowH*n+(int)S(8);
    int bx=std::max(6,std::min(x,g_winW-bw-6)), by=std::max(6,std::min(y,g_winH-bh-6));
    R_ctxBox={bx,by,bx+bw,by+bh}; R_ctxItems.clear();
    for(int i=0;i<n;i++){ int yy=by+(int)S(6)+i*rowH; R_ctxItems.push_back({bx+(int)S(6),yy,bx+bw-(int)S(6),yy+rowH-(int)S(2)}); }
    g_ctxKind=kind; g_ctxArg=arg; g_ctxLabels=labels; g_ctxActs=acts; g_ctxDanger=danger; g_ctxTrack=(kind==CTX_TRACK)?arg:-1; g_ctxOpen=true;
}
static void OpenSaidaMenu(int x,int y){
    g_saidas=Player::OutDevices();
    std::vector<std::wstring> l; std::vector<int> a;
    std::wstring atual=Player::OutDeviceName();
    l.push_back(atual.empty()?L"✓  Padrão do sistema":L"Padrão do sistema"); a.push_back(CA_SAIDA_BASE);
    for(size_t i=0;i<g_saidas.size()&&i<12;i++){ l.push_back((atual==g_saidas[i]?L"✓  ":L"")+g_saidas[i]); a.push_back(CA_SAIDA_BASE+1+(int)i); }
    if(g_saidas.empty()) l[0]=L"Nenhum dispositivo de som encontrado";
    OpenCtxMenuGeneric(x,y,CTX_SAIDA,-1,l,a,std::vector<bool>(l.size(),false),(int)S(330));
}
static void OpenCtxMenu(int track,int x,int y){   // menu de uma faixa
    if(track<0||track>=(int)g_tracks.size()) return;
    if(IsOnlineTrack(g_tracks[(size_t)track])){
        std::vector<std::wstring> l={L"Tocar (streaming)",L"Salvar cópia",L"Trocar capa...",L"Abrir o link no navegador",L"Adicionar à playlist..."};
        std::vector<int> a={CA_PLAY,CA_DOWNLOAD,CA_COVER,CA_OPEN_URL,CA_ADDPL}; std::vector<bool> d={false,false,false,false,false};
        if(DcReadyNow()){ l.push_back(L"Tocar no bot do Discord"); a.push_back(CA_DC_PLAY); d.push_back(false); }
        if(g_view==2){ l.push_back(L"Remover da playlist"); a.push_back(CA_REMOVEPL); d.push_back(true); }
        OpenCtxMenuGeneric(x,y,CTX_TRACK,track,l,a,d,(int)S(250));
        return;
    }
    std::vector<std::wstring> l={L"Tocar",L"Trocar capa...",L"Renomear artista...",L"Renomear arquivo...",L"Abrir pasta",L"Adicionar à playlist..."};
    std::vector<int> a={CA_PLAY,CA_COVER,CA_ARTIST,CA_RENAME,CA_FOLDER,CA_ADDPL};
    std::vector<bool> d={false,false,false,false,false,false};
    if(DcReadyNow()){ l.push_back(L"Tocar no bot do Discord"); a.push_back(CA_DC_PLAY); d.push_back(false); }
    if(g_view==2){ l.push_back(L"Remover da playlist"); a.push_back(CA_REMOVEPL); d.push_back(true); }
    l.push_back(L"Excluir arquivo (lixeira)"); a.push_back(CA_DELETE); d.push_back(true);
    OpenCtxMenuGeneric(x,y,CTX_TRACK,track,l,a,d,(int)S(240));
}
static void OpenPlaylistCtxMenu(int pl,int x,int y){   // menu de um card de playlist
    if(pl<0||pl>=(int)g_playlists.size()) return;
    const Playlist& p=g_playlists[(size_t)pl];
    std::vector<std::wstring> l={L"Tocar em ordem",L"Tocar aleatório",p.folder.empty()?L"Vincular uma pasta...":L"Trocar a pasta vinculada..."};
    std::vector<int> a={CA_PL_PLAY,CA_PL_SHUF,CA_PL_FOLDER}; std::vector<bool> d={false,false,false};
    if(!p.link.empty()){ l.push_back(L"Sincronizar com o link"); a.push_back(CA_PL_SYNC); d.push_back(false); }
    int on=0; for(auto& e:p.entries) if(!e.url.empty()&&e.path.empty()) ++on;
    if(on>0){ l.push_back(L"Salvar cópia das "+std::to_wstring(on)+L" músicas online"); a.push_back(CA_PL_DLALL); d.push_back(false); }
    l.push_back(p.mode.empty()?L"Modo online: padrão das configurações":(p.mode==L"download"?L"Fonte externa: salvar uma cópia":L"Modo online: streaming")); a.push_back(CA_PL_MODE); d.push_back(false);
    l.push_back(HostPlLabel(p.slug)); a.push_back(CA_PL_HOST); d.push_back(false);
    if(DcReadyNow()){ l.push_back(L"Tocar no bot do Discord"); a.push_back(CA_DC_PLPLAY); d.push_back(false); }
    { int onb=DcPlaylistOnBot(p.slug); if(onb>=0){ l.push_back(onb?L"No bot do Discord: liberada (tirar)":L"No bot do Discord: liberar"); a.push_back(CA_DC_PLTOGGLE); d.push_back(false); } }
    l.push_back(L"Renomear..."); a.push_back(CA_PL_RENAME); d.push_back(false);
    l.push_back(L"Excluir playlist"); a.push_back(CA_PL_DELETE); d.push_back(true);
    OpenCtxMenuGeneric(x,y,CTX_PLAYLIST,pl,l,a,d,(int)S(280));
}
static void OpenPickPlaylistMenu(int track,int x,int y){   // "adicionar a playlist": escolher qual
    if(track<0||track>=(int)g_tracks.size()) return;
    std::vector<std::wstring> l; std::vector<int> a; std::vector<bool> d;
    for(size_t i=0;i<g_playlists.size();++i){ l.push_back(g_playlists[i].name); a.push_back(CA_PICK_BASE+(int)i); d.push_back(false); }
    l.push_back(L"+ Nova playlist..."); a.push_back(CA_PICK_NEW); d.push_back(false);
    OpenCtxMenuGeneric(x,y,CTX_PICKPL,track,l,a,d,(int)S(260));
}
static void AskDeletePlaylist(int i){ if(i<0||i>=(int)g_playlists.size()) return; g_confirmOpen=true; g_confirmKind=1; g_confirmTrack=i; g_confirmText=L"Excluir a playlist \""+g_playlists[(size_t)i].name+L"\"? (as músicas continuam no disco)"; }
static void OpenConfirm(){ // geometria da caixa de confirmacao
    int bw=std::min(520,g_winW-40), bh=170, bx=(g_winW-bw)/2, by=(g_winH-bh)/2;
    R_confirmBox={bx,by,bx+bw,by+bh};
    R_confirmYes={bx+bw-250,by+bh-56,bx+bw-135,by+bh-18};
    R_confirmNo={bx+bw-125,by+bh-56,bx+bw-20,by+bh-18};
}
static void ApplyCoverPick(int i,const std::wstring& imgPath){
    if(imgPath.empty()||i<0||i>=(int)g_tracks.size()) return;
    auto oldCover=g_tracks[i].coverPath;
    auto saved=SaveCustomCover(g_tracks[i].path,imgPath);
    if(!saved.empty()){g_tracks[i].coverPath=saved;PlatformEvictThumb(oldCover);PlatformEvictThumb(saved);if(i==g_current)PlatformLoadCover(saved);}
}
static void ApplyEqNow(){ Player::SetEq(g_cfg.eq,g_cfg.eqOn); g_player.ApplyEq(); }
// ---- segundo plano ---------------------------------------------------------
// Botao X / Alt+F4 / fechar pelo sistema: com musica tocando e a opcao ligada,
// a janela some e o Remix segue tocando. Relancar o app (instancia unica), a
// bandeja (Windows) ou os controles de midia (MPRIS no Linux) trazem a janela
// de volta. Quando a musica acaba sozinha, o app fecha (g_bgAutoQuit).
static void HideToBackground(bool autoQuit){ g_hiddenToBg=true; g_bgAutoQuit=autoQuit; g_bgIdleSec=0; PlatformHide(); }
static void ShowFromBackground(){ g_hiddenToBg=false; g_bgAutoQuit=false; PlatformRestoreAndFocus(); }
static void QuitApp(){ PlatformClose(); }
static void RequestClose(){
    if(g_cfg.bgOnClose&&g_player.playing&&!g_hiddenToBg){ HideToBackground(true); return; }
    PlatformClose();
}
// Pausa/toca vindos de fora (bandeja, teclas de midia, MPRIS): lembra que foi o
// usuario que pausou, para nao fechar sozinho em segundo plano.
static void RemotePlayPause(){ TogglePlayPause(); g_userPaused=g_player.loaded&&!g_player.playing; }
static void RemotePlay(){ if(!g_player.playing) RemotePlayPause(); }
static void RemotePause(){ if(g_player.playing) RemotePlayPause(); }
static void RemoteStop(){ if(g_player.playing) TogglePlayPause(); if(g_player.loaded) g_player.SeekMs(0); g_userPaused=true; }

// ---- vistas: biblioteca / cards de playlists / faixas de uma playlist ----------------
static void SyncCurrentToList(){   // acha a faixa tocando na lista em tela (ou -1)
    g_current=-1;
    if(g_nowPlayingValid) for(size_t i=0;i<g_tracks.size();++i) if(_wcsicmp(g_tracks[i].path.c_str(),g_nowPlaying.path.c_str())==0){ g_current=(int)i; break; }
    RebuildShuffleQueue(g_current);
}
static void ApplyArtistMap(std::vector<Track>& v){ for(auto&t:v){ auto it=g_artistMap.find(t.path); if(it!=g_artistMap.end()) t.artist=it->second; } }
static void EnterLibraryView(){
    if(g_view==2){
        if(g_libCached){ g_tracks=g_libTracks; g_libCached=false; } else LoadFolderAndPlaylist();
        g_openPl=-1; g_cfg.openPlaylist.clear(); g_cfg.Save(); ApplySort(); SyncCurrentToList();
    }
    g_view=0; g_listScroll=0; g_searchFocus=false; BuildLayout();
}
static void ShowPlaylistCards(){ g_view=1; g_listScroll=0; g_searchFocus=false; BuildLayout(); }
static void OpenPlaylistView(int pi){
    if(pi<0||pi>=(int)g_playlists.size()) return;
    if(!g_libCached&&g_view!=2){ g_libTracks=g_tracks; g_libCached=true; }
    const std::vector<Track>& lib=g_libCached?g_libTracks:g_tracks;
    if(g_pickMode){ g_pickMode=false; g_pickSel.clear(); }
    int missing=0; std::vector<Track> tr=PlaylistTracks(pi,lib,missing);
    ApplyArtistMap(tr);
    if(g_playlists[(size_t)pi].coverPath.empty()){ for(auto& t:tr) if(!t.coverPath.empty()){ g_playlists[(size_t)pi].coverPath=t.coverPath; break; } }
    g_tracks=tr; g_openPl=pi; g_view=2; g_cfg.openPlaylist=g_playlists[(size_t)pi].slug; g_cfg.Save();
    desc::RegistrarPlaylist(g_playlists[(size_t)pi].slug);   // playlist usada sobe na lateral
    ApplySort(); SyncCurrentToList(); g_listScroll=0; g_searchFocus=false; BuildLayout();
    if(missing>0) SetStatus(std::to_wstring(missing)+(missing==1?L" faixa não encontrada (o arquivo mudou de lugar ou foi apagado).":L" faixas não encontradas (arquivos mudaram de lugar ou foram apagados)."),4000);
}
static void PlayPlaylist(int pi,bool shuffle){   // "tocar" direto do card (pi<0 = todas as musicas)
    if(pi<0) EnterLibraryView(); else OpenPlaylistView(pi);
    if(g_tracks.empty()){ SetStatus(pi<0?L"Biblioteca vazia.":L"Playlist vazia: use + ADICIONAR (biblioteca, arquivos, pasta ou link).",3500); return; }
    if(g_cfg.shuffle!=shuffle){ g_cfg.shuffle=shuffle; g_cfg.Save(); }
    int start=0; if(shuffle){ RebuildShuffleQueue(-1); start=g_shufQueue.empty()?0:g_shufQueue[0]; }
    PlayIndex(start,true);
}
static void RestoreOpenPlaylist(){   // ao abrir o app: volta para a playlist que estava aberta, com a 1a faixa carregada (pausada)
    if(g_cfg.openPlaylist.empty()) return;
    int i=FindPlaylistBySlug(g_cfg.openPlaylist); if(i<0){ g_cfg.openPlaylist.clear(); return; }
    OpenPlaylistView(i);
    if(g_player.playing||g_current>=0||g_tracks.empty()) return;
    for(size_t k=0;k<g_tracks.size();++k) if(!g_lastOnlineUrl.empty()&&g_tracks[k].path==g_lastOnlineUrl){   // lembra a musica online; o streaming recomeca no play
        PlayIndex((int)k,false);
        SetStatus(L"Última música online: "+g_tracks[k].title+L" (aperte play: o streaming recomeça).",4500);
        return;
    }
    PlayIndex(0,false);
}
static void AddTrackToPlaylist(int pi,int track){
    if(track<0||track>=(int)g_tracks.size()) return;
    if(pi<0){ g_pendingAddPath=g_tracks[(size_t)track].path; StartPlaylistNameEdit(2,-1); return; }
    if(pi<(int)g_playlists.size()&&IsOnlineTrack(g_tracks[(size_t)track])){
        std::vector<OTrack> v{OTrackFor(g_tracks[(size_t)track])};
        SetStatus(AddOnlineItemsToPlaylist(pi,v)>0?L"Adicionada à playlist \""+g_playlists[(size_t)pi].name+L"\".":std::wstring(L"Essa música já está na playlist."),2500);
        return;
    }
    if(PlaylistAddTrack(pi,g_tracks[(size_t)track])) SetStatus(L"Adicionada à playlist \""+g_playlists[(size_t)pi].name+L"\".",2500);
    else SetStatus(L"Essa faixa já está na playlist \""+g_playlists[(size_t)pi].name+L"\".",2500);
}
static void RemoveTrackFromOpenPlaylist(int track){
    if(g_view!=2||track<0||track>=(int)g_tracks.size()) return;
    if(!PlaylistRemovePath(g_openPl,g_tracks[(size_t)track].path)){ SetStatus(L"Essa música vem da pasta vinculada: desvincule a pasta (menu PASTA DA PLAYLIST) ou tire o arquivo de lá.",4500); return; }
    g_tracks.erase(g_tracks.begin()+track);
    if(g_current==track) g_current=-1; else if(g_current>track) g_current--;
    RebuildShuffleQueue(g_current); BuildLayout();
}
#include "app_online_b.h"
#include "app_online_c.h"
#include "host_server.h"
#include "discord_host.h"
#include "soundpad.h"
static bool DcSetToken(const std::wstring& t,std::wstring& err){ return dc::SetToken(t,err); }
static void DcSetDjRole(const std::wstring& v){ dc::UpdateCfg([&](dc::Cfg& c){ c.djRole=v; }); }
static bool DcReadyNow(){ return dc::Ready(); }
static int DcPlaylistOnBot(const std::wstring& slug){ dc::View v=dc::GetView(); if(!v.hasToken) return -1; return v.cfg.pls.count(slug)?1:0; }
// ---- Host: cola entre a UI e o servidor -----------------------------------------
static std::wstring g_hostReq;   // pedido de pareamento em confirmacao na tela
static std::string AccentHex(){ char b[16]; snprintf(b,sizeof b,"#%02x%02x%02x",GetRValue(g_theme.accent),GetGValue(g_theme.accent),GetBValue(g_theme.accent)); return b; }
static bool HostRunningNow(){ return host::Running(); }
// Biblioteca que o Host publica: com uma playlist aberta (g_view 2) e sem a biblioteca guardada, g_tracks e a
// playlist; nesse caso usa a ultima biblioteca conhecida (as playlists sao publicadas do mesmo jeito).
static std::vector<Track> g_hostLib; static bool g_hostLibOk=false;
static const std::vector<Track>* HostLibNow(){ if(g_libCached) return &g_libTracks; if(g_view!=2) return &g_tracks; return g_hostLibOk?&g_hostLib:nullptr; }
// Playlists com as musicas da pasta vinculada junto (o app mostra as duas coisas): o que o Host e o Discord recebem.
static std::vector<Playlist> ExpandedPlaylists(){
    std::vector<Playlist> pls=g_playlists;
    std::map<std::wstring,const Track*> known;   // titulo/artista ja lidos (biblioteca e playlist aberta): sem ler tag de novo
    for(auto& t:g_hostLib) known[t.path]=&t;
    if(g_libCached) for(auto& t:g_libTracks) known[t.path]=&t;
    if(const std::vector<Track>* lib=HostLibNow()) for(auto& t:*lib) known[t.path]=&t;
    if(g_view==2) for(auto& t:g_tracks) known[t.path]=&t;
    for(auto& p:pls){
        for(auto& e:p.entries){   // faixa adicionada sem ler as tags: usa o que a biblioteca ja sabe
            if(e.path.empty()||!e.url.empty()) continue;
            auto k=known.find(e.path); if(k==known.end()) continue;
            if(e.title.empty()) e.title=k->second->title;
            if(e.artist.empty()) e.artist=k->second->artist;
            if(e.dur<=0) e.dur=k->second->durSec;
        }
        if(p.folder.empty()||!Config::DirExists(p.folder)) continue;
        std::set<std::wstring> have; for(auto& e:p.entries) if(!e.path.empty()) have.insert(e.path);
        std::vector<PlEntry> extra;
        WalkFiles(p.folder,[&](const std::filesystem::path& f){
            if(!IsAudioExt(LowerExt(f))) return; std::wstring w=f.wstring(); if(!have.insert(w).second) return;
            PlEntry e; e.path=w; e.file=f.filename().wstring();
            auto k=known.find(w); if(k!=known.end()){ e.title=k->second->title; e.artist=k->second->artist; }
            extra.push_back(e);
        });
        std::sort(extra.begin(),extra.end(),[](const PlEntry& a,const PlEntry& b){ return _wcsicmp(a.file.c_str(),b.file.c_str())<0; });
        p.entries.insert(p.entries.begin(),extra.begin(),extra.end());
    }
    return pls;
}
static void HostPublishNow(){
    const std::vector<Track>* lib=HostLibNow();
    if(lib&&lib!=&g_hostLib){ g_hostLib=*lib; g_hostLibOk=true; }
    static const std::vector<Track> vazio;
    host::Publish(lib?*lib:vazio,ExpandedPlaylists());
}
// Impressao digital do que o Host mostra: caminho, titulo, artista, capa e duracao de cada faixa e de cada
// entrada das playlists. Renomear, editar ou trocar faixa (mesmo sem mudar a quantidade) republica.
static unsigned long long HostFingerprint(){
    unsigned long long h=1469598103934665603ULL;
    auto mix=[&](const std::wstring& w){ for(wchar_t c:w){ h^=(unsigned long long)c; h*=1099511628211ULL; } h^=0xFF; h*=1099511628211ULL; };
    auto num=[&](long long v){ h^=(unsigned long long)v; h*=1099511628211ULL; };
    const std::vector<Track>* lib=HostLibNow(); num(lib?(long long)lib->size():-1);
    if(lib) for(auto& t:*lib){ mix(t.path); mix(t.title); mix(t.artist); mix(t.coverPath); num(t.durSec); }
    num((long long)g_playlists.size()); num((long long)g_hostFolderGen);
    for(auto& p:g_playlists){ mix(p.slug); mix(p.name); mix(p.folder); num((long long)p.entries.size()); for(auto& e:p.entries){ mix(e.path); mix(e.url); mix(e.play); mix(e.title); mix(e.artist); mix(e.thumb); num(e.dur); } }
    return h;
}
static host::Options HostOptsFromCfg(){ host::Options o; o.port=g_cfg.hostPort; o.lanOk=g_cfg.hostLan; o.lan6=g_cfg.hostIPv6; o.pin=WideToUtf8(g_cfg.hostPin); o.name=g_cfg.hostName; o.accent=AccentHex(); o.onlineOk=g_cfg.hostOnline; o.qrConfirm=g_cfg.hostQrConfirm; return o; }
static bool HostStartFromCfg(){
    std::string err=host::Start(HostOptsFromCfg());
    if(!err.empty()){ SetStatus(L"Host: "+Utf8ToWide(err),4500); return false; }
    HostPublishNow(); if(g_cfg.hostTunnel) host::TunnelStart(g_cfg.hostPort);
    SetStatus(L"Host ligado na porta "+std::to_wstring(g_cfg.hostPort)+L".",3000); return true;
}
static void HostStopNow(){ host::Stop(); SetStatus(L"Host desligado.",2500); }
static void HostToggle(){
    if(host::Running()){ HostStopNow(); g_cfg.hostOn=false; g_cfg.Save(); return; }
    if(g_cfg.hostPin.empty()){ SetStatus(L"Defina um PIN (4 a 12 números) antes de ligar o Host.",3500); return; }
    g_cfg.hostOn=HostStartFromCfg(); g_cfg.Save();
}
static void HostAskPair(const std::wstring& reqId);
// Proximo pedido de vinculo esperando (o mais antigo), se nao tiver outro dialogo aberto.
static void HostNextPending(){
    if(g_confirmOpen) return;
    host::View v=host::GetView(); const host::PairReq* best=nullptr;
    for(auto& q:v.pending) if(!best||q.created<best->created) best=&q;
    if(best) HostAskPair(Utf8ToWide(best->id));
}
static void HostTick(){
    static unsigned long long last=0; static ULONGLONG lastMs=0;
    if(!host::Running()) return;
    ULONGLONG now=GetTickCount64(); if(now-lastMs<2000) return; lastMs=now;
    unsigned long long f=HostFingerprint(); if(f!=last){ last=f; HostPublishNow(); }
    if(g_confirmOpen&&g_confirmKind==2){   // pedido na tela expirou ou ja foi respondido pelo painel: fecha e mostra o proximo
        host::PairReq q=host::FindReq(WideToUtf8(g_hostReq));
        if(q.id.empty()||q.estado!=0){ g_confirmOpen=false; g_confirmKind=0; g_hostReq.clear(); SetStatus(L"O pedido de vínculo expirou.",2500); }
    }
    HostNextPending();
}
// Discord: publica as playlists quando mudam (so com o bot ligado) e reconecta se o processo caiu.
// Soundpad: solta os sons que acabaram e redesenha o progresso com o painel aberto.
static bool SpadPanelIsOpen();                        // app_panels.h (depois do layout)
static void OnPickedSoundpad(const std::wstring& s);
static void OnSpadAdded(const std::wstring& err,int n);
static void ExtrasTick(){
    dc::Poll();
    static ULONGLONG lastPub=0; static unsigned long long lastFp=0; static unsigned lastRun=0;
    ULONGLONG now=GetTickCount64();
    if(now-lastPub>=2000){
        lastPub=now;
        if(dc::St().running.load()){
            unsigned long long f=HostFingerprint(); unsigned rg=dc::St().runGen.load();
            if(f!=lastFp||rg!=lastRun){ lastFp=f; lastRun=rg; dc::Publish(ExpandedPlaylists()); }
        }
    }
    bool changed=spad::Tick();
    if(changed||(SpadPanelIsOpen()&&spad::PlayingCount()>0)) PlatformRedraw();
}
static void HostAskPair(const std::wstring& reqId){
    host::PairReq q=host::FindReq(WideToUtf8(reqId)); if(q.id.empty()||q.estado!=0) return;
    if(g_confirmOpen) return;   // ja tem um dialogo na tela; o HostTick mostra este quando ele fechar
    g_hostReq=reqId; g_confirmOpen=true; g_confirmKind=2; g_confirmTrack=-1;
    g_confirmText=L"\""+Utf8ToWide(q.name)+L"\" ("+Utf8ToWide(q.ip)+(q.viaTunnel?L", pela internet":L", rede local")+(q.viaQr?L", pelo QR":L"")+L") quer se conectar. Aceitando, ele não vê nada até você liberar a biblioteca ou hostear playlists.";
}
static void HostCopy(bool tunnel){
    std::string u=tunnel?host::TunnelUrl():host::LanUrl();
    if(u.empty()){ SetStatus(tunnel?(host::Running()?L"O link do túnel ainda não está pronto (ele é testado antes de aparecer).":L"Ligue o Host primeiro."):(host::Running()?L"Sem link de rede local (ligue REDE LOCAL).":L"Ligue o Host primeiro."),3500); return; }
    if(PlatformSetClipboardText(Utf8ToWide(u))) SetStatus(L"Link copiado: "+Utf8ToWide(u),3500); else SetStatus(L"Não consegui copiar. Link: "+Utf8ToWide(u),6000);
}
// Link permanente do aparelho: copia e joga no QR grande. Com ele o celular volta a ser reconhecido
// em qualquer endereco (o vinculo mora aqui no PC; o cookie do navegador e so um atalho).
static void HostCopyDeviceLink(size_t i){
    host::PanelUI& p=host::PU(); if(i>=p.v.devs.size()) return;
    const std::string id=p.v.devs[i].id; const std::wstring nome=Utf8ToWide(p.v.devs[i].name);
    bool isTun=false; std::string u=host::DeviceLinkUrl(id,p.qrTunnel,isTun);
    if(u.empty()){ SetStatus(host::Running()?L"Sem link ainda (ligue REDE LOCAL ou espere o túnel).":L"Ligue o Host primeiro.",3500); return; }
    p.qrDev=id;
    if(PlatformSetClipboardText(Utf8ToWide(u))) SetStatus(L"Link de \""+nome+L"\" copiado (e no QR ao lado). Abrindo nele, o aparelho volta sem PIN.",5000);
    else SetStatus(L"Não consegui copiar; o QR ao lado agora é o link de \""+nome+L"\".",5000);
}
static void HostSetOnline(bool on){ g_cfg.hostOnline=on; g_cfg.Save(); host::SetOptions(g_cfg.hostOnline,g_cfg.hostQrConfirm); SetStatus(on?L"O celular pode buscar e ouvir online (o PC faz o trabalho).":L"Online no celular desligado.",2800); }
static void HostSetQrConfirm(bool on){ g_cfg.hostQrConfirm=on; g_cfg.Save(); host::SetOptions(g_cfg.hostOnline,g_cfg.hostQrConfirm); SetStatus(on?L"Vincular pelo QR agora também pede ACEITAR aqui no PC.":L"Vincular pelo QR não pede confirmação no PC (o QR só aparece na sua tela e vale uma vez).",3500); }
static void HostSetIpv6(bool on){ g_cfg.hostIPv6=on; g_cfg.Save(); if(host::Running()){ HostStopNow(); HostStartFromCfg(); } SetStatus(on?L"IPv6 ligado (só rede local: link-local, ULA ou o mesmo /64).":L"IPv6 desligado: só IPv4 e túnel.",3200); }
static void HostConfirm(bool ok){
    bool valeu=!g_hostReq.empty()&&host::Approve(WideToUtf8(g_hostReq),ok); g_hostReq.clear();
    if(!valeu) SetStatus(L"Esse pedido expirou: peça para o celular tentar de novo.",3500);
    else SetStatus(ok?L"Dispositivo aceito. Libere no painel HOST o que ele pode ouvir.":L"Pedido recusado.",3000);
}
static void HostMakeHtml(){ std::wstring p=host::WriteConnectHtml(); if(p.empty()){ SetStatus(L"Não consegui gravar o arquivo.",3000); return; } SetStatus(L"Remix-conectar.html gravado na pasta do Remix: mande pelo WhatsApp.",4500); PlatformOpenFolder(Config::BaseDir()); }
static std::wstring HostPlLabel(const std::wstring& slug){ return host::Targets(slug).empty()?L"Hostear no celular":L"Parar de hostear no celular"; }
static std::string HostTargetsOf(const std::wstring& slug){ return host::Targets(slug); }
static void HostTogglePlaylist(int pl){ if(pl<0||pl>=(int)g_playlists.size()) return; const std::wstring& slug=g_playlists[(size_t)pl].slug; bool on=host::Targets(slug).empty(); host::SetTargets(slug,on?"ALL":""); HostPublishNow(); SetStatus(on?L"Playlist hosteada para todos os dispositivos (ajuste no painel HOST).":L"Playlist não aparece mais nos celulares.",3200); }
static void ConfirmYes(){
    int t=g_confirmTrack; int kind=g_confirmKind; g_confirmOpen=false; g_confirmKind=0;
    if(kind==2){ HostConfirm(true); return; }
    if(kind==1){
        if(t>=0&&t<(int)g_playlists.size()) host::SetTargets(g_playlists[(size_t)t].slug,"");   // playlist nova com o mesmo nome nao herda o "hostear"
        bool wasOpen=(g_openPl==t); DeletePlaylistDir(t);
        if(wasOpen){ g_openPl=-1; EnterLibraryView(); } else if(g_openPl>t) g_openPl--;
        if(g_openPl>=0&&g_openPl<(int)g_playlists.size()) g_cfg.openPlaylist=g_playlists[(size_t)g_openPl].slug;
        SetStatus(L"Playlist excluída.",2000); if(host::Running()) HostPublishNow();
    } else DeleteTrack(t);
    BuildLayout();
}
// ---- busca (filtra a lista em tela; nao muda a fila de reproducao) --------------
static std::wstring FoldText(const std::wstring& s){
    std::wstring o; o.reserve(s.size());
    for(wchar_t c:s){ wchar_t l=(wchar_t)towlower(c);
        switch(l){ case L'á': case L'à': case L'â': case L'ã': case L'ä': l=L'a'; break; case L'é': case L'è': case L'ê': case L'ë': l=L'e'; break;
        case L'í': case L'ì': case L'î': case L'ï': l=L'i'; break; case L'ó': case L'ò': case L'ô': case L'õ': case L'ö': l=L'o'; break;
        case L'ú': case L'ù': case L'û': case L'ü': l=L'u'; break; case L'ç': l=L'c'; break; case L'ñ': l=L'n'; break; default: break; }
        o.push_back(l); }
    return o;
}
static bool TrackMatchesSearch(const Track& t){
    if(g_searchBuf.empty()) return true;
    std::wstring q=FoldText(g_searchBuf);
    if(FoldText(t.title).find(q)!=std::wstring::npos) return true;
    if(FoldText(t.artist).find(q)!=std::wstring::npos) return true;
    return FoldText(std::filesystem::path(t.path).filename().wstring()).find(q)!=std::wstring::npos;
}
static void FocusSearch(){ if(g_cfg.displayMode==L"vertical") return; if(g_showSettings) g_showSettings=false; if(g_view==1) EnterLibraryView(); g_searchFocus=true; }
static void ClearSearch(){ g_searchFocus=false; if(!g_searchBuf.empty()){ g_searchBuf.clear(); BuildLayout(); } }
// ---- atalhos configuraveis -----------------------------------------------------------
static int FindHotkey(int kc,int mods){ if(!kc) return -1; for(int a=0;a<HK_COUNT;a++) if(g_cfg.hk[a].key==kc&&g_cfg.hk[a].mods==mods) return a; return -1; }
static void RunHotkeyAction(int a){
    switch(a){
    case HK_PLAYPAUSE: RemotePlayPause(); break;
    case HK_NEXT: NextTrack(); break;
    case HK_PREV: PrevOrRestart(); break;
    case HK_VOLUP: SetVolumePercent(g_cfg.volume+5); break;
    case HK_VOLDOWN: SetVolumePercent(g_cfg.volume-5); break;
    case HK_MUTE: ToggleMute(); break;
    case HK_RESTART: g_player.Restart(); break;
    case HK_SHUFFLE: g_cfg.shuffle=!g_cfg.shuffle; g_cfg.Save(); RebuildShuffleQueue(g_current); SetStatus(g_cfg.shuffle?L"Aleatório ligado: a lista fica na sua ordem, só a reprodução muda.":L"Aleatório desligado.",2200); break;
    case HK_REPEAT: g_cfg.repeat=!g_cfg.repeat; g_cfg.Save(); SetStatus(g_cfg.repeat?L"Repetir a faixa: ligado":L"Repetir a faixa: desligado",1800); break;
    case HK_SEARCH: FocusSearch(); break;
    case HK_DELETE: if(!g_showSettings&&g_current>=0){ AskDeleteTrack(g_current); OpenConfirm(); } break;
    case HK_RENAME: if(!g_showSettings&&g_current>=0) StartFileRename(g_current); break;
    case HK_MOVEUP: if(!g_showSettings){ MoveTrack(g_current,-1); BuildLayout(); } break;
    case HK_MOVEDOWN: if(!g_showSettings){ MoveTrack(g_current,+1); BuildLayout(); } break;
    case HK_CLOSE:
        if(g_hkCapture>=0) g_hkCapture=-1;
        else if(OU().open){ OU().open=false; OU().editing=false; }
        else if(g_imgMenuOpen) g_imgMenuOpen=false;
        else if(g_searchFocus||!g_searchBuf.empty()) ClearSearch();
        else if(g_showSettings) g_showSettings=false;
        else if(g_pickMode) FinishPickMode(false);
        else if(g_view==2) ShowPlaylistCards();
        else if(g_view==1) EnterLibraryView();
        break;
    case HK_QUIT: QuitApp(); break;
    case HK_SHOWHIDE: if(g_hiddenToBg) ShowFromBackground(); else HideToBackground(false); break;
    default: break;
    }
}
static bool RunHotkeyByName(const std::wstring& id){
    if(_wcsicmp(id.c_str(),L"show")==0){ ShowFromBackground(); return true; }
    if(_wcsicmp(id.c_str(),L"hide")==0){ HideToBackground(false); return true; }
    int a=HkIndexById(id); if(a<0) return false; RunHotkeyAction(a); return true;
}

static void HandleCommand(const std::wstring& cmd){
    if(cmd.rfind(L"CMD|",0)==0){ if(!RunHotkeyByName(cmd.substr(4))) SetStatus(L"Comando desconhecido: "+cmd.substr(4),2500); return; }   // remix --cmd <acao>
    const std::wstring p=L"PLAYFILE|";
    if(cmd.rfind(p,0)==0){
        std::wstring path=cmd.substr(p.size());
        if(!path.empty()) PlayFileDirect(path,true);
        ShowFromBackground();
    }
}
static void HandleEvent(int type,const std::wstring& s,int n){
    if(g_rxRefazer.exchange(false)&&RxOn()) BuildLayout();   // fileiras novas: refaz a geometria
    switch(type){
    case EV_NEXT_TRACK: NextTrack(); break;
    case EV_WEB_DOWNLOAD_DONE: ConsumeWebDownload(); break;
    case EV_THUMBS_INVALIDATE: PlatformClearThumbs(); break;
    case EV_COMMAND: HandleCommand(s); break;
    case EV_HOST_PEDIDO: HostAskPair(s); break;
    case EV_HOST_STATUS: SetStatus(s,n?6000:4000); break;
    case EV_STEMS: OnStemsEvent(s,n); break;
    case EV_PICK_SOUNDPAD: OnPickedSoundpad(s); break;
    case EV_PICK_CLI: {
        std::wstring v=Config::Trim(s);
        if(v.empty()) break;
        g_cfg.mediaCli=v; g_cfg.Save(); fonte::Reconferir();
        SetStatus(fonte::Configurada()?(L"Fonte externa pronta: "+fonte::Versao()):std::wstring(L"Esse arquivo não é executável."),4000);
    } break;
    case EV_SPAD_ADDED: OnSpadAdded(s,n); break;
    case EV_TRANSCODED: OnTranscoded(s,(unsigned)n); break;
    case EV_PICK_FOLDER: if(!s.empty()) SwitchFolder(s); break;
    case EV_PICK_IMAGE: ApplyCoverPick(n,s); break;
    case EV_PICK_WALL: if(!s.empty()){g_cfg.bgWallpaper=s;g_cfg.Save();} break;
    case EV_ART_READY: OnArtReady(s); break;
    case EV_ONLINE_META: OnStreamMeta(s,n); break;
    case EV_ONLINE_READY: OnStreamReady(n,_wtoi(s.c_str())); break;
    case EV_ONLINE_FAIL: OnStreamFail(s,n); break;
    case EV_ONLINE_THUMB: OnOnlineThumb(s); break;
    case EV_ONLINE_RESOLVED: OnLinkResolved(s,n); break;
    case EV_ONLINE_JOB: OnDownloadJob(n); break;
    case EV_PICK_PL_FOLDER: OnPickedPlaylistFolder(s); break;
    case EV_PICK_PL_FILES: OnPickedFiles(s,n==1); break;
    case EV_PICK_PL_ADDFOLDER: OnPickedAddFolder(s); break;
    case EV_PICK_NEWPL_FOLDER: OnPickedNewPlaylistFolder(s); break;
    case EV_PICK_DLFOLDER: if(!s.empty()){ g_cfg.downloadFolder=s; g_cfg.Save(); SetStatus(L"As cópias vão para: "+s,3000); } break;
    case EV_PICK_DLONCE:
        if(!s.empty()){ g_cfg.downloadFolder=s; g_cfg.Save(); QueueDownloadsInto(g_pendingDl.items,g_pendingDl.plName,s); }
        else SetStatus(L"Cópia cancelada.",2000);
        g_pendingDl=PendingDownloads{};
        break;
    case EV_ONLINE_SEARCH: {   // miniaturas dos resultados em segundo plano
        std::vector<std::pair<std::wstring,std::wstring>> v;
        { std::lock_guard<std::mutex> lk(OU().m); for(auto& t:OU().res) if(!t.thumb.empty()) v.push_back({t.url,t.thumb}); }
        FetchThumbsAsync(v); } break;
    default: break;   // EV_TOOLS_READY: so redesenhar
    }
    PlatformRedraw();
}
// Diario do online: no maximo 1 gravacao por segundo, so com streaming/download ativo
// (parado, grava uma vez so para lembrar a ultima musica online).
static void TickJournal(){
    ULONGLONG now=GetTickCount64(); if(now-g_journalLast<1000) return; g_journalLast=now;
    int waiting=0; float pct=0; std::wstring dt; bool dl=DownloadActivity(waiting,pct,dt);
    bool memo=g_nowPlayingValid&&IsOnlineTrack(g_nowPlaying);
    int pos=0,dur=memo?g_nowPlaying.durSec:0;
    if(memo&&g_player.IsStream()){ pos=(int)(g_player.GetPositionMs()/1000); DWORD l=g_player.GetLengthMs(); if(l) dur=(int)(l/1000); }
    // so grava se algo mudou (posicao, fase/buffer de um canal, download)
    std::wstring sig=(memo?g_nowPlaying.path:std::wstring())+L"|"+std::to_wstring(pos)+L"|"+std::to_wstring(g_curStreamId);
    for(auto& c:StreamSnapshot()) sig+=L"|"+std::to_wstring(c.id)+L":"+std::to_wstring(c.phase)+L":"+std::to_wstring(c.end/std::max<uint32_t>(1,c.rate));
    if(dl) sig+=L"|dl:"+std::to_wstring((int)pct)+L":"+std::to_wstring(waiting)+dt;
    if(sig==g_journalSig) return;
    g_journalSig=sig;
    WriteJournal(false,memo,memo?g_nowPlaying.title:L"",memo?g_nowPlaying.path:L"",pos,dur,g_curStreamId);
}
// Chamado a cada frame/tick. dt em segundos.
static void Tick(float dt){
    // escondido em 2o plano e a musica acabou sozinha (nao foi pausa do usuario): encerra
    if(g_hiddenToBg&&g_bgAutoQuit&&!g_player.playing&&!g_converting&&!g_userPaused){ g_bgIdleSec+=dt; if(g_bgIdleSec>3.f){ g_bgIdleSec=0; PlatformClose(); return; } }
    else g_bgIdleSec=0;
    if(g_player.playing) g_userPaused=false;
    HostTick();
    ExtrasTick();
    if(g_showSplash){ if(GetTickCount64()-g_splashStart>=SPLASH_MS) g_showSplash=false; }
    else {
        ConsumeAutoScan(); PollFolderWatch(); PollLinkedFolders(); UpdateStreamQueue(); TickJournal();
        float sm=.4f+(g_cfg.ledSpeed/100.f)*1.6f;
        float k=dt/0.04f;
        if(g_player.playing&&g_cfg.artShape==L"cd")g_rotation+=1.2f*sm*k*(g_cfg.cdSpeed/100.f);
        if(g_rotation>360)g_rotation-=360;
        g_glowPhase+=.08f*sm*k;
        g_runnerPhase+=(.0022f+.0135f*(g_cfg.runnerSpeed/100.f))*sm*k;
        if(g_runnerPhase>1.f)g_runnerPhase-=1.f;
        if(g_runnerPhase<0.f)g_runnerPhase=0.f;
        if(g_player.ReachedEnd()){
            if(g_cfg.repeat) g_player.Restart();
            else if(!g_cfg.autoplay) g_player.Pause();
            else if(!g_tracks.empty()&&(g_cfg.shuffle||g_current+1<(int)g_tracks.size())) NextTrack();
            else g_player.Pause();
        }
    }
    if(RxOn()&&g_rxLetraOn&&g_player.playing) BuildLayout();   // a letra acompanha a musica
    PublishStreamWave();
    {   // Rich Presence do Discord: o que esta tocando (so com o Application ID preenchido)
        const Track* ct=(g_current>=0&&g_current<(int)g_tracks.size())?&g_tracks[(size_t)g_current]:(g_nowPlayingValid?&g_nowPlaying:nullptr);
        int pos=g_player.loaded?(int)(g_player.GetPositionMs()/1000):0, len=g_player.loaded?(int)(g_player.GetLengthMs()/1000):0;
        drpc::Atualizar(ct?ct->title:L"",ct?ct->artist:L"",g_player.playing,pos,len);
    }
    UpdateSpecBands(g_player.loaded&&g_player.playing, g_player.loaded?g_player.GetPositionMs():0, dt);
}
// Inicializacao comum (depois de carregar config e antes da janela).
static void DurWorker(){
    DurCache& d=DUR();
    for(;;){
        std::wstring path;
        { std::lock_guard<std::mutex> lk(d.m); if(d.fila.empty()){ d.rodando.store(false); return; } path=d.fila.back(); d.fila.pop_back(); }
        int dur=0;
        ma_decoder_config dc=ma_decoder_config_init(ma_format_unknown,0,0); ma_decoder dec;
#ifdef _WIN32
        bool ok=ma_decoder_init_file_w(path.c_str(),&dc,&dec)==MA_SUCCESS;
#else
        bool ok=ma_decoder_init_file(WideToUtf8(path).c_str(),&dc,&dec)==MA_SUCCESS;
#endif
        if(ok){ ma_uint64 fr=0; if(ma_decoder_get_length_in_pcm_frames(&dec,&fr)==MA_SUCCESS&&dec.outputSampleRate) dur=(int)(fr/dec.outputSampleRate); ma_decoder_uninit(&dec); }
        { std::lock_guard<std::mutex> lk(d.m); if(d.pronto.size()>8000) d.pronto.clear(); d.pronto[path]=dur; }
        AppPost(EV_REDRAW);
    }
}
static void CoreInit(){
    art::St().onReady=[](const std::wstring& a,const std::wstring& b){ AppPost(EV_ART_READY,OPack({a,b})); };   // capa embutida pronta
    g_themes=LoadAllThemes(); ApplyTheme(); g_cfg.HealMusicFolder(); MigrateIniAssociations(g_cfg.musicFolder); g_artistMap=LoadCustomArtists();
    ExtraFormatsEnabled()=PlatformHaveFfmpeg();
    Player::OutDeviceName()=g_cfg.outDevice;   // onde tocar, lembrado da última vez
    Player::GlobalInit();
    Player::SetEq(g_cfg.eq,g_cfg.eqOn);
    ApplyFxNow();
    LoadPlaylists();
    g_shortcutLines={ L"Espaço: tocar / pausar", L"← / →: anterior / próxima", L"↑ / ↓ ou + / -: volume", L"M: mudo   R: reinicia a faixa",
                      L"Del: excluir (lixeira)   F2: renomear arquivo", L"Ctrl+↑ / Ctrl+↓: mover na ordem manual", L"Botão direito numa faixa: menu", L"Esc: fecha painéis",
                      L"Fechar a janela tocando: continua em 2º plano (abra o app de novo para voltar)", L"Ctrl+Q: sair de vez" };
    { std::wstring msg=OnlineStartupCleanup(g_lastOnlineUrl,g_lastOnlineTitle); if(!msg.empty()) SetStatus(msg,5000); }   // restos de download interrompido
    if(g_cfg.hostOn&&!g_cfg.hostPin.empty()) HostStartFromCfg();   // host ligado na ultima vez: volta sozinho
    dc::St().onChange=[]{ AppPost(EV_REDRAW); };
    if(dc::AutoStartWanted()) dc::StartAsync();                     // bot do Discord ligado na ultima vez
    spad::Load();
    if(spad::GetView().on) spad::SetOnAsync(true,[]{ AppPost(EV_REDRAW); });   // microfone virtual ligado na ultima vez
    drpc::SetAppId(g_cfg.rpcAppId);   // Rich Presence do Discord (so liga com o Application ID preenchido)
}
static void CoreShutdown(){
    g_cfg.Save();
    host::Stop();
    dc::Stop();
    spad::Stop(true);   // o microfone virtual some junto com o Remix
    PlatformWatchStop();
    if(g_waveThread.joinable())g_waveThread.detach();
    if(g_scanThread.joinable())g_scanThread.detach();
    bool memo=g_nowPlayingValid&&IsOnlineTrack(g_nowPlaying);
    g_player.Close(); StopAllStreams();   // canais de streaming: a atual e os da fila
    if(CancelAllDownloads()>0){ for(int k=0;k<40;k++){ int w=0; float pc=0; std::wstring t; if(!DownloadActivity(w,pc,t)) break; std::this_thread::sleep_for(std::chrono::milliseconds(50)); } }
    bool anyDl=false; { std::lock_guard<std::mutex> lk(DQ().m); anyDl=!DQ().all.empty(); }
    std::error_code ec;
    if(memo||anyDl||std::filesystem::exists(std::filesystem::path(JournalPath()),ec))
        WriteJournal(true,memo,memo?g_nowPlaying.title:L"",memo?g_nowPlaying.path:L"",0,memo?g_nowPlaying.durSec:0,0);
    Player::GlobalShutdown();
}
