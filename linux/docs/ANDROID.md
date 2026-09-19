# Remix Player no Android — estrutura para o porte

Este documento descreve como fazer a versão Android do Remix **reaproveitando o
núcleo que já existe** e com um build que **não depende de Android Studio, Gradle
nem de atualização de biblioteca**: tudo que o build usa fica pinado dentro do
repositório (`third_party/android/`), e só muda quando alguém decidir mudar.

O zip `docs/android-exemplo.zip` traz a pasta `android/` de exemplo (manifesto,
scripts de download e de build, e a casca esqueleto). Ele é um ponto de partida:
foi escrito seguindo o Makefile Android do próprio raylib, mas **ainda não foi
compilado num NDK** — a primeira coisa a fazer é rodar os dois scripts e corrigir
o que o compilador apontar.

## 1. Por que dá para portar sem reescrever

O app já está dividido em duas partes:

| Pasta | O que é | No Android |
|---|---|---|
| `comum/` | Toda a lógica: biblioteca, playlists, player (miniaudio), online, layout e cliques (`app_layout.h`, `app_input.h`), configuração, temas, estilos (`app_ui.h`) | **Reaproveita inteiro**, sem mudar |
| `linux/gfx.h`, `linux/app_state.h`, `linux/app_draw.h`, `linux/app_draw2.h` | A casca de desenho em **raylib** (a mesma API "cara de GDI+" que o Windows usa) | **Reaproveita**: raylib roda no Android (NativeActivity + OpenGL ES 2) |
| `linux/sys_linux.h` | Diálogos (zenity), inotify, socket de instância única, libcurl, ffmpeg | **Troca** por `android/sys_android.h` (mesma API `sys::`) |
| `linux/main_linux.cpp` | Janela, laço, CLI, MPRIS, atalhos X11 | **Troca** por `android/main_android.cpp` (laço com toque, densidade, permissões) |
| `windows/` | Win32 + GDI+ | Não entra |
| `comum/audio_backend.c` (miniaudio) | Saída de som | **Reaproveita**: miniaudio tem backend AAudio e OpenSL ES |

`linux/app_state.h` passa a incluir `sys_android.h` quando `__ANDROID__` está
definido (uma linha, já feita no repositório). Fora isso, o núcleo tem dois
ganchos pequenos pensados para celular:

- `g_dpiMul` (em `app_core.h`): multiplicador da escala geral. A casca Android
  põe a densidade da tela aí (`sys::ScreenDensityScale()`), então os botões ficam
  do tamanho certo sem mexer no `uiScale` da pessoa.
- Modo **vertical** = o layout do celular em pé (já existe, é o mesmo do desktop);
  em paisagem (tablet) usa o modo normal.

## 2. Estrutura de pastas proposta

```
remix/
├── comum/                  núcleo (compartilhado)
├── linux/                  casca raylib: gfx.h, app_state.h, app_draw*.h  <- o Android inclui daqui
├── android/                (no zip de exemplo; crie a pasta no repositório quando começar)
│   ├── README-ANDROID.md   este texto
│   ├── AndroidManifest.xml NativeActivity, hasCode="false", permissões
│   ├── fetch-android.sh    baixa SDK/NDK PINADOS para third_party/android (uma vez)
│   ├── build-android.sh    compila raylib + Remix por ABI e monta o APK (aapt2/zipalign/apksigner)
│   ├── sys_android.h       camada de sistema (eventos, assets, permissões, densidade)
│   ├── main_android.cpp    laço principal: toque, densidade, pastas, Platform*
│   ├── updater/            tools_update.h (C++) e AtualizadorRemix.java: auto-atualização (seção 8)
│   └── res/values/strings.xml
└── third_party/android/    sdk/ (cmdline-tools, platforms, build-tools, ndk), debug.keystore  [fora do git]
```

Não existe `build.gradle`, não existe projeto do Android Studio, não existe
código Java: o APK é **só nativo** (`android:hasCode="false"`), então não precisa
de `d8`/dex nem de AndroidX.

## 3. Toolchain pinado (a parte "sem ficar atualizando")

`android/fetch-android.sh` baixa, para dentro de `third_party/android/sdk`:

| Componente | Versão | Para quê |
|---|---|---|
| cmdline-tools | 11076708 | `sdkmanager` (instala o resto) |
| platforms | android-34 | `android.jar` (só para o aapt2 validar o manifesto) |
| build-tools | 34.0.0 | `aapt2`, `zipalign`, `apksigner` |
| NDK | 27.2.12479018 | `clang` para arm64-v8a, armeabi-v7a, x86_64 |
| platform-tools | (atual) | `adb` para instalar e ver o log |
| raylib | 5.5 (o mesmo do Linux, `third_party/raylib-5.5`) | janela/GLES/entrada |
| miniaudio | 0.11.21 (já em `comum/audio/`) | som |

Pré-requisito único no sistema: **Java 17** (`sdkmanager` e `apksigner` são .jar).
Fedora/Nobara: `sudo dnf install java-17-openjdk-headless`; Debian/Ubuntu:
`sudo apt install openjdk-17-jdk-headless`. Espaço: ~2,5 GB.

Regra: **ninguém atualiza nada por acaso**. Se um dia for preciso um NDK mais novo
(ex.: Android 16 exigir algo), muda-se o número no `fetch-android.sh`, roda de novo,
compila, testa — e só então commita. O `minSdkVersion` é 24 (Android 7.0+) e o
`targetSdkVersion` 34; subir o target é decisão explícita (a loja pede o target do
ano anterior, no mínimo).

## 4. Build e instalação

```bash
bash android/fetch-android.sh                 # uma vez (ou quando mudar uma versão)
bash android/build-android.sh                 # -> dist/Remix-<versao>-android.apk
REMIX_ABIS="arm64-v8a" bash android/build-android.sh      # só 64 bits, mais rápido
REMIX_ABIS="x86_64" bash android/build-android.sh         # para o emulador
third_party/android/sdk/platform-tools/adb install -r dist/Remix-1.4.0-android.apk
third_party/android/sdk/platform-tools/adb logcat -s remix raylib                 # log do app
```

O que o `build-android.sh` faz, na ordem:

1. Para cada ABI: compila `rcore rshapes rtextures rtext utils` do raylib com
   `-DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2`, o `android_native_app_glue.c`
   do NDK, o `comum/audio_backend.c` e o `android/main_android.cpp`, e linka tudo
   numa `libremix.so` (`-u ANativeActivity_onCreate` mantém o ponto de entrada;
   `-Wl,-z,max-page-size=16384` atende os aparelhos com página de 16 KB do
   Android 15). Guarda uma cópia com símbolos em `build/android/` para ler crash.
2. Copia `assets/branding`, `assets/fonts` e `assets/themes` para dentro do APK.
3. Monta os `mipmap` do ícone a partir de `linux/icons/*.png` e troca a versão no
   manifesto (`versionCode` = 1.4.0 → 10400).
4. `aapt2 compile/link` → `zip` das libs → `zipalign -p 4` → `apksigner`.

Assinatura: o `fetch-android.sh` cria uma `debug.keystore` só para instalar no seu
aparelho. Para loja, use uma keystore própria **fora do git** e passe
`REMIX_KEYSTORE`, `REMIX_KS_PASS`, `REMIX_KEY_ALIAS` para o script.

## 5. O que a casca Android implementa

Os ganchos que o núcleo exige (`Platform*`, declarados em `comum/app_core.h`) e a
API `sys::` (mesma de `linux/sys_linux.h`). O esqueleto do zip já traz o
comportamento da coluna "fase 1".

| Gancho | Fase 1 (esqueleto) | Depois |
|---|---|---|
| Pastas/assets | `REMIX_HOME` = `internalDataPath`; assets copiados do APK na 1ª abertura (`sys::ExtractAssets`) | — |
| Biblioteca | `/storage/emulated/0/Music` (e Download) com permissão `READ_MEDIA_AUDIO` pedida por JNI | escolher pasta pelo SAF (JNI) |
| `PlatformPick*` (pasta/imagem/arquivos) | responde "cancelado" | SAF |
| `PlatformHttpGet` / `sys::HttpGet` | desligado (sem busca online, sem capa da internet) | `HttpURLConnection` por JNI, ou libcurl estática pinada |
| `PlatformHaveFfmpeg` / transcodificação | não (só mp3, ogg, wav, flac nativos) | ffmpeg estático no APK (`lib/<abi>/libffmpeg.so` executável) — pesado, avaliar |
| Online (yt-dlp) | não existe no Android (Python) | **Host do PC** (`docs/HOST.md`) ou `youtubedl-android` com auto-atualização (seção 8) |
| `PlatformTrash` | não (apagar de vez é perigoso; devolve falso) | MediaStore delete por JNI |
| `PlatformOpenFolder`, clipboard, atalhos globais, minimizar/esconder, MPRIS | vazios | clipboard por JNI |
| Instância única / IPC | sempre "sou a única" | — |
| Segundo plano (tela apagada) | o som continua enquanto o Android não congelar o processo; **não** avança de faixa em segundo plano | serviço em primeiro plano + `MediaSession` (precisa de uma classe Java pequena, compilada com `javac` + `d8`, sem Gradle) |

### Toque, tela e botão Voltar

- raylib entrega o toque como mouse (`GetMousePosition`, `IsMouseButtonPressed(0)`).
  O laço do `main_android.cpp` trata: **arrastar** na lista = rolagem
  (`OnWheel`), **soltar sem arrastar** = clique (`OnLButtonDown` + `OnLButtonUp`),
  e nos controles de arrasto (seek, volume, sliders) o toque vai direto para
  `OnLButtonDown`/`OnMouseDrag` para não quebrar o arrastar.
- Densidade: `g_dpiMul = sys::ScreenDensityScale()` (420 dpi ≈ 2,6×).
- Orientação: em pé → `displayMode = vertical`; deitado → normal. O
  `configChanges` do manifesto evita recriar a Activity ao girar; o raylib
  redimensiona e o `BuildLayout()` recalcula.
- Botão Voltar: raylib mapeia para `KEY_BACK`; o laço converte em `Esc`
  (`HK_CLOSE`: fecha menus/painéis; na tela principal, sai para o launcher).
- Teclado virtual: raylib não abre o teclado do Android sozinho. Para a busca,
  chame `showSoftInput` por JNI ao focar o campo (fase 2); até lá a busca só
  funciona com teclado físico.

## 6. Como testar sem aparelho

```bash
SDK=third_party/android/sdk
$SDK/cmdline-tools/latest/bin/sdkmanager --sdk_root=$SDK "system-images;android-34;google_apis;x86_64" "emulator"
$SDK/cmdline-tools/latest/bin/avdmanager create avd -n remix -k "system-images;android-34;google_apis;x86_64"
$SDK/emulator/emulator -avd remix &
REMIX_ABIS="x86_64" bash android/build-android.sh && $SDK/platform-tools/adb install -r dist/Remix-*-android.apk
```

Crash nativo: `adb logcat -s DEBUG` mostra o endereço; traduza com
`$NDK/ndk-stack -sym build/android/` (usa a `libremix-<abi>-sym.so` guardada pelo build).

## 7. Alternativa mais curta: o celular como cliente do PC

O recurso **Host** (`docs/HOST.md`) transforma o Remix do PC num servidor: o
celular abre um site (túnel Cloudflare ou LAN) para ouvir as músicas do PC, com
playlists próprias. Um app Android que é só um *wrapper* desse site (WebView
apontando para a página, ou a PWA "instalada" pelo Chrome) sai em um dia e não
precisa de nada disto — pode ser a primeira versão na loja enquanto o porte
nativo amadurece. Os dois caminhos não se excluem.

## 8. Ferramentas que quebram sozinhas: atualização automática (obrigatória)

Toolchain pinado (seção 3) é uma coisa; **ferramentas de runtime** são outra. yt-dlp
quebra toda vez que o YouTube muda algo, e a versão de ontem para de funcionar sem o
app ter culpa. Então a regra é:

- **Build**: nada muda sozinho (NDK, raylib, miniaudio pinados).
- **Runtime**: o que fala com serviços de fora **se atualiza sozinho**, sem esperar
  uma versão nova do app.

### 8.1 O que precisa se atualizar

| Peça | Por que quebra | Como se atualiza sozinha |
|---|---|---|
| yt-dlp | YouTube/SoundCloud mudam a página e a assinatura | `yt-dlp -U` (ele se substitui) ou baixar o binário/`.zip` mais novo do GitHub Releases |
| Deno / Node (runtime JS do yt-dlp) | o yt-dlp passa a exigir versão mínima nova | checar a versão mínima que o yt-dlp pede e baixar a release |
| ffmpeg | raramente; só quando um formato novo aparece | baixar a build estática mais nova |
| cloudflared (túnel do Host) | protocolo do túnel evolui | GitHub Releases da Cloudflare |
| **o próprio app** | correções do Remix | checar `releases/latest` do repositório e avisar |

### 8.2 Como fazer no Android sem Gradle e sem loja

O Android não roda Python, então **não existe yt-dlp "de verdade" no celular**. As duas
saídas, da mais simples para a mais completa:

1. **Deixar o online com o PC (Host).** O celular usa o site do Host (`docs/HOST.md`):
   quem roda yt-dlp é o PC, e o PC já atualiza o yt-dlp sozinho (o Remix tenta
   `yt-dlp -U` quando uma busca falha por erro de extração, e o instalador baixa sempre
   a última versão). O app Android v1 não precisa de nada disso — é o que recomendamos.
2. **yt-dlp no aparelho com `youtubedl-android`** (yausername): empacota um Python
   embutido e expõe `YoutubeDL.getInstance().updateYoutubeDL(context)` — o yt-dlp se
   atualiza por dentro do app, sem versão nova na loja. Custo: é uma biblioteca Java/AAR
   (o APK vira `hasCode="true"`, precisa de uma classe Java pequena e do `d8`, mas
   continua sem Gradle: baixe o `.aar` pinado, extraia `classes.jar` + `jni/`, compile
   com `javac` e junte com `d8`). O `build-android.sh` do zip já tem o lugar marcado
   para esse passo.

### 8.3 Estrutura do atualizador (vale para os dois caminhos)

```
android/updater/
├── AtualizadorRemix.java   checa GitHub Releases, baixa para o cache e avisa
└── tools_update.h          (C++) a mesma lógica pelo HTTP da casca, quando não houver Java
```

Regras do atualizador (estão no esqueleto do zip, em `tools_update.h`):

- **Quando**: ao abrir (no máximo 1x por dia, `ultima_checagem` em `config.ini`) e
  **na hora que uma ferramenta falha** (erro de extração do yt-dlp = "atualiza e tenta
  de novo uma vez").
- **De onde**: só de fontes fixas e HTTPS — `api.github.com/repos/<dono>/<repo>/releases/latest`
  dos projetos oficiais (yt-dlp/yt-dlp, denoland/deno, cloudflare/cloudflared,
  EchoGroupStudio/Remix). Nunca de um link vindo de fora.
- **Como**: baixa em `<cache>/tools/<nome>.part`, confere o tamanho (e o SHA-256 quando
  o release publica), troca por rename atômico, guarda a versão anterior em
  `<nome>.antigo` para voltar se a nova não abrir.
- **Silencioso para ferramentas, com aviso para o app**: ferramenta nova instala sem
  perguntar; app novo mostra "Tem versão nova (1.x.y): abrir a página" — no Android sem
  loja o app não se substitui sozinho (precisaria de `REQUEST_INSTALL_PACKAGES` e um
  `FileProvider`, que é código Java); abrir a página de download é o caminho sem código.
- **Falhou?** Continua com a versão que tem e tenta de novo no dia seguinte. Nunca
  bloqueia o app por causa de atualização.

O mesmo atualizador serve para o **Windows e o Linux**: o Remix do PC passa a rodar
`yt-dlp -U` sozinho (a partir da 1.4.x, ao falhar uma busca) — a regra "runtime se
atualiza sozinho" vale em todo lugar.

## 9. Versões novas do Android (o que já está previsto)

O manifesto e o build do zip já consideram o que o Android tem exigido a cada versão:

| Android | Exigência | O que o exemplo faz |
|---|---|---|
| 15 (API 35) | `.so` alinhado em **16 KB** em aparelhos novos | `-Wl,-z,max-page-size=16384` no link (NDK r28+ já faz por padrão) |
| 15 | **edge-to-edge** obrigatório para target 35 (conteúdo por baixo das barras) | tema `Theme.NoTitleBar.Fullscreen` + o layout usa `viewport-fit`/insets (`AConfiguration` + `ANativeWindow` dão a área segura; ajustar `g_headerH` pela barra de status) |
| 14 (API 34) | serviço em 1º plano precisa declarar o **tipo** (`mediaPlayback`) | permissões já comentadas no manifesto (`FOREGROUND_SERVICE_MEDIA_PLAYBACK`) para a fase 2 |
| 13 (API 33) | `READ_MEDIA_AUDIO` no lugar de `READ_EXTERNAL_STORAGE`; `POST_NOTIFICATIONS` | `sys::AudioPermissionName()` escolhe pela versão; notificação só na fase 2 |
| 13+ | **botão Voltar preditivo** (`enableOnBackInvokedCallback`) | fica desligado no manifesto até a casca tratar o gesto; o `KEY_BACK` continua chegando |
| 12 (API 31) | `android:exported` obrigatório na Activity | já está |
| 11 (API 30) | scoped storage: sem acesso livre ao disco | biblioteca em `Music/` e `Download/` via `READ_MEDIA_AUDIO`; pasta livre só pelo SAF (fase 2) |
| Play Store | target = API do ano anterior no mínimo; **AAB** e assinatura própria | `targetSdkVersion` num único lugar do manifesto; `bundletool` (jar pinado) gera o AAB |

Regra para subir o `targetSdkVersion`: mude o número, rode no emulador da versão nova
(`sdkmanager "system-images;android-<N>;google_apis;x86_64"`), confira permissões,
áudio em segundo plano e o botão Voltar, e só então commite. O `minSdkVersion` 24
cobre ~99% dos aparelhos ativos; não há motivo para subir.

## 10. Decisões em aberto

- Nome do pacote: `com.echogroupstudio.remix` (troque no manifesto se quiser outro).
- Loja: o Play exige AAB (bundletool, também um .jar pinável) e assinatura própria.
- Quais formatos extras valem o peso do ffmpeg no APK.
- Segundo plano: quando implementar o serviço, o `hasCode` vira `true` e o build
  ganha um passo `javac` + `d8` (continua sem Gradle).
