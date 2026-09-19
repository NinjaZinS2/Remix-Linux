# Remix Player — versão Linux

Porte nativo do Remix (player C++ Windows/GDI+/Media Foundation) para Linux,
com a mesma interface e os mesmos recursos: modos quadrado / CD / vertical,
grade de cards ou lista, capas personalizadas (arquivo local ou busca na
internet), temas, partículas, glitch, LED e linha corredora, onda de áudio
real e espectro por frequência, monitor da pasta em tempo real, instância
única com `--play-file`.

## Novidades: música online, playlists e capas embutidas

- **Playlists de verdade**: na playlist aberta, **+ ADICIONAR** → marcar músicas da
  biblioteca (clique nos cards e CONCLUIR), escolher arquivos, adicionar todas de uma
  pasta, **vincular uma pasta** (a playlist mostra sempre o conteúdo atual dela), colar
  link ou buscar online. Com uma playlist aberta, **PASTA DA PLAYLIST** mexe só nela —
  antes o botão trocava a pasta da biblioteca inteira e a playlist continuava vazia.
  **+ NOVA PLAYLIST**: vazia, de uma pasta, de um link ou da busca.
- Lista vazia: o aviso fica no meio da área da lista, com um botão (não cobre mais a barra).
- **Música online** (aba ONLINE): busca no YouTube Music / YouTube / SoundCloud com animação
  de carregando, links do Spotify (página pública, sem chave), Deezer, Apple Music, YouTube
  e SoundCloud, **streaming só na memória** ou **download**, diário com limpeza automática
  ao abrir. Detalhes no [README principal](../../../README.md#fontes-externas-buscar-e-reproduzir).
  Precisa de uma CLI de mídia compatível, do `ffmpeg` e de Deno 2.3+ (ou Node.js 22+), todos
  instalados por você pela sua distro — o Remix não instala nem baixa nada. O caminho da CLI vai em
  Configurações > PROGRAMA DE LINHA DE COMANDO (ou na chave `MediaCli=` do config.ini); sem isso,
  as telas online só avisam que falta configurar e o resto do app segue igual.
- **Fila de streaming**: a música atual e as próximas 2 carregam ao mesmo tempo, cada uma no
  seu canal (CLI + ffmpeg + buffer na memória). Pular ou acabar a música toca a próxima na
  hora; na lista aparece TOCANDO / FILA: PRONTA / FILA: CARREGANDO.
- **Capas embutidas**: a capa de dentro do MP3, M4A, FLAC, OGG/Opus e WAV aparece sozinha
  (extraída em segundo plano para `~/.cache/remix/art/`). M4A também mostra título e artista.
- Correções: clicar em algumas faixas (a 11ª, por exemplo) trocava a pasta da biblioteca ou
  mexia em escala/LED/modo de exibição; trocar de pasta com uma varredura em andamento podia
  deixar a lista vazia; `--home` agora abre uma instância separada.

## Novidades da 1.2

- **Windows e Linux agora rodam a mesma lógica** (`app_core.h`, `app_layout.h`,
  `app_input.h`): tudo abaixo vale para os dois. O Windows também passou a usar
  miniaudio (FLAC/OGG nativos, EQ, `ffmpeg.exe` opcional ao lado do exe).
- **Volume** com ícone de alto-falante: clique (ou Ctrl+M) muta/desmuta,
  porcentagem ao lado, cor segue o tema.
- **Atalhos** com duas teclas (não disparam sem querer): Ctrl+Espaço (play/pause),
  Ctrl+←/→ (anterior/próxima), Ctrl+↑/↓ (volume), Ctrl+M (mudo), Ctrl+S (aleatório),
  Ctrl+R (repetir), Ctrl+0 (reiniciar), Ctrl+Del (excluir), Alt+↑/↓ (mover), F2, Esc.
  Lista e troca em Configurações > ATALHOS.
- **PASTA** no cabeçalho: troca a biblioteca sem abrir as configurações
  (pastas recentes, escolher outra, voltar ao padrão).
- **Clique direito** numa música: tocar, trocar capa, renomear artista,
  **renomear o arquivo no disco** (capa/artista/ordem acompanham), abrir a
  pasta no gerenciador de arquivos, **excluir para a lixeira** (com confirmação).
- **MODO LEVE** (Configurações > EFEITOS): menos partículas/LED e 30 fps.
- **Varredura padrão** só nas pastas do usuário (Músicas, Downloads,
  Documentos, Área de trabalho, mídias removíveis), pulando Steam/jogos/
  node_modules/.git — mais rápida e sem efeitos sonoros de jogo na lista.
- Windows: botões **minimizar/fechar** no cabeçalho da janela sem borda;
  nomes de arquivo com acento ("Poça") agora abrem.
- Linux: corrigido travamento ao abrir a janela em sessões Wayland (GLFW
  esperava um evento do XWayland que nunca chegava).
- **Segundo plano**: fechar a janela (X / Alt+F4) com música tocando esconde
  o player e ele continua tocando. Abrir o Remix de novo (menu, ícone,
  `remix` no terminal) traz a janela de volta; quando a música acaba sozinha,
  o app encerra. Minimizado também continua tocando e avançando de faixa
  (antes, no Linux, o loop parava minimizado). Ctrl+Q ou o botão **SAIR DO
  REMIX** encerram de vez.
- **Controles do sistema** (Linux: MPRIS — applet de mídia do KDE/GNOME,
  teclas de mídia, `playerctl`; Windows: ícone na bandeja + teclas de mídia).
  Os dois recursos vêm **ligados** e podem ser desligados em Configurações >
  REPRODUÇÃO ("FECHAR: CONTINUA TOCANDO" e "CONTROLES DO SISTEMA").
- **AppImage**: `dist/Remix-1.6.0-x86_64.AppImage`, um arquivo só que roda em
  qualquer distro (veja a tabela abaixo).
- `./RODAR.sh` num terminal abre o player em segundo plano e devolve o prompt
  (`./RODAR.sh --fg` mantém preso ao terminal para ver as mensagens).
- **Atalhos configuráveis** (Configurações > ATALHOS): cada ação tem sua tecla
  (clique no botão e pressione a combinação; Backspace limpa) e um escopo:
  **FOCO** (padrão: só com a janela do Remix ativa, não atrapalha jogos) ou
  **GLOBAL** (funciona em segundo plano ou com outro programa na frente).
  Global no Linux usa XGrabKey, ou seja, sessão X11; numa sessão Wayland
  configure no atalho do sistema o comando `remix --cmd next` (também
  `playpause`, `prev`, `volup`, `voldown`, `mute`, `shuffle`, `repeat`,
  `show`, `hide`, `quit`) — funciona em qualquer desktop.
- **Busca**: caixa acima da lista (ou Ctrl+F): filtra por nome, artista ou
  arquivo, sem acento/maiúscula; Enter toca a primeira; Esc limpa.
- **Playlists**: aba PLAYLISTS acima da lista. Cada playlist é uma pasta em
  `~/.config/remix/playlists/<nome>/` (ou na pasta do app, no modo portátil)
  com um `playlist.json` que guarda só o **caminho** das músicas — nada é
  copiado nem apagado. Botão direito numa faixa > "Adicionar à playlist";
  no card: TOCAR (em ordem) / ALEATÓRIO, clique abre a lista, botão direito
  renomeia/exclui. Se um arquivo mudou de pasta, o app procura pelo nome e
  tamanho na biblioteca e corrige o caminho sozinho; se não achar, avisa.
  A playlist aberta é lembrada ao reabrir o app.
- **Aleatório** agora é uma fila: a lista fica na SUA ordem (inclusive a
  manual) e só a ordem de reprodução muda — cada faixa toca uma vez por ciclo
  e "anterior" volta pela fila.
- Correções: o X das configurações funciona depois de rolar; brilho e
  velocidade do LED valem para o contorno do painel em qualquer modo; o
  layout se refaz sozinho se o compositor aplicar outro tamanho na troca de
  modo (quadrado ↔ vertical ↔ CD).

## Novidades da 1.1

- **AUTO** (autoplay) no cabeçalho e nas configurações: ligado, ao acabar uma
  música toca a próxima (ordem da lista, ou aleatório com ⇄); desligado, toca só
  a que você escolheu e para.
- **ORDEM** da playlist: título, artista, arquivo, data ou **manual**. No
  manual aparecem setas ▲▼ em cada faixa (ou Ctrl+↑/↓ na faixa atual) e a
  ordem fica salva em `order.ini`.
- **Equalizador** de 8 bandas (60 Hz a 15 kHz, ±12 dB) nas configurações.
- **Formatos**: MP3, WAV, FLAC e OGG Vorbis nativos; M4A/AAC, Opus, WMA, AIFF,
  APE, WavPack, MKA/MP4/WebM (áudio) quando o `ffmpeg` está instalado
  (`sudo dnf install ffmpeg` / `sudo apt install ffmpeg`). Tags lidas de
  ID3 (MP3) e Vorbis comments (FLAC/OGG/Opus).
- **Qualidade**: a saída de áudio é reaberta na taxa do arquivo (44.1k, 48k…),
  então o player não faz resampling nenhum; volume linear, sem processamento
  (o EQ só entra quando ligado).
- A janela lembra o tamanho do modo normal e abre proporcional à tela.
- Correções: o botão play dos cards às vezes caía na zona de seek e carregava a
  faixa pausada; ao trocar de modo (vertical ↔ normal) a grade podia ficar
  rolada fora da tela.

## Três jeitos de usar

| Forma | Arquivo | Como |
|---|---|---|
| Portátil (sem instalar) | `dist/remix-1.6.0-linux-x86_64-portable.zip` | extrair e rodar `./remix` (ou `./RODAR.sh`). Config, capas e a pasta `Musica/` ficam ali dentro, igual ao Windows |
| Fedora / Nobara / RHEL 8+ | `dist/remix-1.6.0-1.x86_64.rpm` | `sudo dnf install ./dist/remix-1.6.0-1.x86_64.rpm` |
| Qualquer distro (AppImage) | `dist/Remix-1.6.0-x86_64.AppImage` | `chmod +x Remix-1.6.0-x86_64.AppImage` e abrir (duplo clique ou `./Remix-1.6.0-x86_64.AppImage`). Config e capas em `~/.config/remix`. Precisa de FUSE (`libfuse2` ou `libfuse3`, já vem na maioria das distros); sem FUSE: `./Remix-...AppImage --appimage-extract-and-run` |
| Debian 10+ / Ubuntu 18.04+ / Mint 19+ / Pop!_OS | `dist/remix_1.6.0-1_amd64.deb` | `sudo apt install ./dist/remix_1.6.0-1_amd64.deb` |

A própria pasta do projeto também é portátil: tem `remix` (binário pronto),
`RODAR.sh`, `config.ini` e `assets/` na raiz. Baixou o zip do repositório,
extraiu, `./RODAR.sh` (ou duplo clique em `remix`). Se o zip perdeu a permissão
de execução: `chmod +x remix RODAR.sh`.

O mesmo binário serve para todos: é compilado com o **zig** contra a glibc
2.27 (a do Ubuntu 18.04), embute a libc++ e só depende de `libc`, `libm`,
`libpthread` e `libdl`. X11 / Wayland / OpenGL, libcurl (busca de capas) e
PulseAudio / PipeWire / ALSA são carregados em tempo de execução, então não há
dependência dura de pacote. Recomendados: `libcurl` (busca de capa na
internet) e `zenity` ou `kdialog` (diálogos de escolher pasta / imagem).
Só x86_64.

Depois de instalar, "Remix Player" aparece no menu de aplicativos; no terminal:

```bash
remix
remix --play-file "/caminho/Minha Musica.mp3"   # reusa a instância aberta
```

## Onde ficam os dados

| Modo | Quando | Config / .inis / capas |
|---|---|---|
| Instalado | `remix` em `/usr/bin` | `~/.config/remix/` (`config.ini`, `covers.ini`, `artists.ini`, `assets/covers/`, `assets/themes/`) |
| Portátil | existe `config.ini` ao lado do binário | a própria pasta do binário (igual ao Windows) |
| Forçado | `REMIX_HOME=/pasta remix` ou `remix --home /pasta` | a pasta indicada |

Os `.ini` do Windows funcionam sem conversão: caminhos relativos com `\` são
normalizados para `/`. Para trazer sua biblioteca do Windows, copie a pasta
inteira (`Musica/`, `assets/covers/`, `config.ini`, `covers.ini`,
`artists.ini`) para `~/.config/remix/` ou use o modo portátil.

Com `MusicFolder=` vazio ("PADRÃO / TODO PC"), a varredura cobre a pasta
pessoal (`$HOME`, pulando pastas ocultas), `/media/$USER`, `/run/media/$USER`
e `/mnt`.

## Diferenças em relação ao Windows

Desde a 1.2 a lógica é a mesma nos dois sistemas; o que muda é só a casca:

- A janela usa a decoração do sistema (título/bordas do seu desktop). No
  Windows a janela era sem borda com arrasto próprio.
- Por padrão o app abre via X11 (XWayland em sessões Wayland), que funciona
  igual em GNOME, KDE, XFCE etc. `REMIX_WAYLAND=1` ou `--wayland` usa o
  backend Wayland nativo (GLFW). Em Wayland nativo, a decoração depende do
  compositor (KDE desenha; GNOME precisa de `libdecor`).
- Diálogos de pasta/imagem usam `kdialog` (KDE) ou `zenity` (GNOME e outros).
- Fontes: DejaVu Sans (texto e símbolos ⚙ ✎ ⇄ ⟳ ✕) e Droid Sans Japanese
  empacotadas; coreano/chinês caem para fontes do sistema via fontconfig.
- O espectro em tempo real é pré-calculado junto com a onda (uma passagem de
  decodificação por faixa, em thread), em vez de um decodificador ao vivo.
- WebP não é decodificado (mesma limitação do Windows sem codec).

## Compilar TUDO de uma vez (Windows + Linux)

| Onde você está | Script | O que sai |
|---|---|---|
| Windows (PowerShell) | `windows\BUILD-TUDO.ps1` | `Remix.exe`, binário Linux `remix`, zips portáteis dos dois; `.deb/.rpm` se houver WSL |
| Linux / WSL | `bash linux/build-all.sh` | binário Linux, `.deb`, `.rpm`, zips portáteis Linux e Windows, `Remix.exe` cross-compilado |

O `Remix.exe` é compilado com o **MinGW-w64 (GCC)**, o mesmo compilador do MSYS2
que o `windows\COMPILAR.bat` usa (no Linux, `linux/build-windows.sh` usa o MinGW do
sistema ou baixa um em `third_party/mingw` com `linux/fetch-mingw.sh`, sem root). O
binário Linux continua sendo feito com o **zig** (glibc 2.27). No Windows:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
cd windows; .\BUILD-TUDO.ps1   # ou -SoWindows / -SoLinux
```

Por que MinGW e não zig no Windows: executável feito com zig é alvo frequente de
falso positivo de antivírus ("Trojan:Win32/Wacatac!ml" e parecidos). O `Remix.exe`
do MinGW só importa DLLs do sistema (UCRT, GDI+, WinHTTP, DWM), leva informações de
versão (`windows/app.rc`) e manifesto (`windows/app.manifest` via `manifest.rc` ->
`default-manifest.o`: o GCC do MinGW linka esse nome sozinho e o `-B` faz ele usar o
nosso no lugar do padrão, então o exe tem um manifesto só em qualquer MinGW).
Forçar o zig mesmo assim: `REMIX_WIN_TOOLCHAIN=zig bash linux/build-windows.sh`.

## Compilar do zero

Tudo que o build usa fica em `third_party/` (raylib, zig, headers): nada é
instalado no sistema e não precisa de root.

```bash
linux/fetch-deps.sh     # baixa raylib 5.5, o zig 0.13 e, se faltar, os headers -devel
linux/build.sh          # -> build/remix e ./remix (copia portátil na raiz)
./build/remix --home test --no-splash   # roda com a biblioteca de teste em test/
```

Toolchains: com `third_party/zig/zig` presente o build usa o zig
(`-target x86_64-linux-gnu.2.27`, mude com `REMIX_GLIBC=2.31`); com
`REMIX_TOOLCHAIN=gcc` usa o `g++` do sistema (C++20). No caminho gcc o
binário exige glibc ≥ 2.35 e `linux/compat_glibc.cpp` entra para não puxar
símbolos ainda mais novos.

Pacotes de desenvolvimento, se preferir instalar em vez do sysroot:

```bash
# Fedora / Nobara
sudo dnf install gcc-c++ libX11-devel libXcursor-devel libXrandr-devel libXinerama-devel libXi-devel libXrender-devel libXext-devel libXfixes-devel libxkbcommon-devel wayland-devel wayland-protocols-devel libstdc++-static
# Debian / Ubuntu
sudo apt install g++ libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libxrender-dev libxext-dev libxfixes-dev libxkbcommon-dev libwayland-dev wayland-protocols libwayland-bin
```

`linux/build-raylib.sh` compila o raylib 5.5 como biblioteca estática
(GLFW com X11 + Wayland, sem raudio/rmodels, JPG/BMP habilitados). O GLFW
carrega as libs X11/Wayland/GL por `dlopen`, então nada disso entra no link.

Opções de `linux/build.sh`: `--debug`, `--no-wayland`, `--rebuild-raylib`.

### Gerar os pacotes

```bash
linux/packaging/build-packages.sh    # -> dist/*.deb, *.rpm, zip portatil e AppImage (sem root)
REMIX_VERSION=1.6.0 REMIX_RELEASE=1 linux/packaging/build-packages.sh
```

O `Depends` do `.deb` é calculado do próprio binário (`libc6 (>= 2.27)` no
build com zig). O zip portátil sai do mesmo script.

## Estrutura do código

| Arquivo | Papel |
|---|---|
| `comum/platform.h` | tipos Windows (`BYTE`, `DWORD`, `COLORREF`, `RECT`...) para o núcleo compilar nos dois sistemas |
| `comum/config.h`, `playlist.h`, `theme.h` | núcleo compartilhado com o Windows (INI, ID3, scan, temas, caminhos portáteis) |
| `comum/app_core.h` | **estado e lógica do player** (playlist, reprodução, ordem, menus, renomear/excluir, EQ, eventos) + hooks `Platform*` que cada casca implementa — compartilhado |
| `comum/app_layout.h`, `app_input.h` | geometria (BuildLayout / LayoutSettings) e entrada (hit-test, mouse, teclado, ações `--after`) — compartilhados |
| `comum/app_web_common.h` | parte comum da busca de capa (URLs, parse do HTML) |
| `comum/app_keys.h` | teclas independentes de plataforma (KeyCode), nomes, atalhos configuráveis (ações, padrões, parse/formatação) |
| `comum/app_playlists.h` | playlists: pasta + `playlist.json` por playlist, JSON mínimo, validação/correção de caminhos |
| `linux/hotkeys_x11.h` | atalhos GLOBAIS via XGrabKey (libX11 por dlopen, conexão própria) |
| `linux/mpris_linux.h` | MPRIS2 (controles de mídia do desktop) via libdbus por dlopen |
| `comum/player_ma.h`, `audio_backend.c` | áudio miniaudio nos dois sistemas: play/pause/seek/volume, EQ, decodificação para análise |
| `linux/gfx.h` | desenho 2D (raylib) com a "cara" do GDI+, fontes com fallback por glifo, clip |
| `linux/sys_linux.h` | eventos thread→UI, diálogos, inotify, socket de instância única, libcurl via dlopen |
| `linux/app_state.h` | imagens/capas da casca Linux e implementação dos hooks `Platform*` (diálogos, lixeira, ffmpeg, monitor de pasta) |
| `linux/app_draw.h`, `linux/app_draw2.h` | desenho dos modos, configurações e overlays (raylib) |
| `linux/app_web.h` | miniaturas da busca de capa (libcurl) |
| `linux/main_linux.cpp` | janela, loop principal, CLI |
| `windows/main.cpp`, `win_sys.h`, `win_web.h`, `win_draw.h` | casca Windows (Win32/GDI+/WinHTTP): mesma lógica, só desenho e sistema |
| `linux/compat_glibc.cpp` | compatibilidade com glibc antiga (só no build com g++) |
| `linux/build-windows.sh`, `windows/BUILD-TUDO.ps1`, `linux/build-all.sh` | builds cruzados / completos |
| `linux/packaging/` | `.deb`, `.rpm`, zip portátil (e chama o AppImage) |
| `linux/RODAR.sh` | roda (compila antes se o binário não existir ou o código mudou) |

Opções de linha de comando: `remix --help`.

## Problemas comuns

- **"nao foi possivel abrir a janela"**: sem OpenGL 3.3. Verifique o driver da
  GPU; para testar com renderização por software:
  `LIBGL_ALWAYS_SOFTWARE=1 __GLX_VENDOR_LIBRARY_NAME=mesa remix`.
- **Sem som**: o miniaudio tenta PulseAudio/PipeWire e depois ALSA. Confira
  se `pipewire-pulse` (ou `pulseaudio`) está rodando.
- **Botões "ESCOLHER PASTA" / imagem não abrem nada**: instale `zenity` ou
  `kdialog`.
- **Busca de capa na internet não funciona**: instale `libcurl` (`libcurl4`
  no Debian/Ubuntu).
- **M4A/AAC/Opus não aparecem na lista**: instale o `ffmpeg`.
- **AppImage não abre** ("dlopen(): error loading libfuse.so.2"): instale
  `libfuse2` (Debian/Ubuntu: `sudo apt install libfuse2`; Fedora: `sudo dnf
  install fuse-libs`) ou rode `./Remix-...AppImage --appimage-extract-and-run`.
- **Fechei e continuou tocando**: é o modo segundo plano. Abra o Remix de novo
  para ver a janela, use o applet de mídia (Raise/Quit) ou Ctrl+Q; para
  desligar, Configurações > REPRODUÇÃO > "FECHAR: ...".
- **Atalho GLOBAL não funciona**: em sessão Wayland o XGrabKey só vale com um
  app X11 em foco. Use o atalho do sistema (KDE: Configurações > Atalhos >
  Atalhos personalizados; GNOME: Teclado > Atalhos personalizados) chamando
  `remix --cmd playpause` etc. Em X11, se outro programa já pegou a mesma
  tecla, escolha outra.
- **Não aparece nos controles de mídia / teclas de mídia não funcionam**:
  precisa da `libdbus-1.so.3` e de uma sessão D-Bus (todo desktop tem); confira
  com `busctl --user list | grep mpris`. `REMIX_NO_MPRIS=1` desliga.
- **A janela não aparece numa sessão Wayland** (versões antes da 1.2): o GLFW
  ficava esperando o XWayland. Atualize, ou rode com `REMIX_WAYLAND=1`.
- Testes automatizados (sem clicar): `remix --home test --no-splash --after 1200:vertical --after 3000:shot:/tmp/a.png --exit-after 4000`
  (ações: vertical, square, cd, list, grid, settings, next, play, autoplay,
  sort, movedown, eq, mute, volup, ctx, foldermenu, confirm, rename, perf,
  scrollend, shot:<png>, track:<n>, key:<tecla ou Ctrl+F9>, type:<texto>,
  close, show, quit, minimize, size:<L>x<A>, click:<x>,<y>, search:<texto>,
  tab:playlists|tracks, plnew:<nome>, pladd:<n>, plopen:<n>, plplay:<n>,
  plshuf:<n>, back, hkset:<acao>=<combo>|global, hkcap:<acao>). As mesmas
  flags funcionam no `Remix.exe`.
- Testar o `Remix.exe` sem Windows: com o `wine` instalado, copie o exe e a
  pasta `assets/` para uma pasta de teste e rode
  `wine Remix.exe --no-splash --after "4000:shot:$(winepath -w "$PWD")\\tela.png" --exit-after 7000`.
