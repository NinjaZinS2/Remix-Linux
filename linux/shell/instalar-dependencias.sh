#!/usr/bin/env bash
# Remix Player - instala o que o Remix usa alem do proprio programa:
#   yt-dlp, ffmpeg e um JavaScript runtime (Deno 2.3+ ou Node.js 22+)  ->  buscar, tocar e baixar musica online
#   zenity (ou kdialog no KDE)                                        ->  janelas de escolher pasta e arquivo
#   libcurl                                                           ->  capas e links do Spotify, Deezer e Apple Music
#   pactl (pulseaudio-utils / libpulse)                               ->  Soundpad: o microfone virtual "Remix Microfone"
#   opcional: Node.js 22.12+ e o discord.js (--discord)               ->  bot de musica do Discord (~60 MB)
# Descobre a distro pelo /etc/os-release e usa o caminho certo para cada uma:
#   Debian, Ubuntu, Mint, Pop!_OS, Zorin...   apt      yt-dlp e Deno oficiais (os do repositorio sao antigos)
#   Fedora, Nobara, RHEL, Rocky, Alma...      dnf      ffmpeg ou ffmpeg-free; sem nenhum, o ffmpeg estatico oficial
#   Arch, Manjaro, EndeavourOS, CachyOS...    pacman
#   openSUSE Tumbleweed e Leap                zypper
#   Void Linux e Solus                        xbps-install e eopkg
#   Bazzite, Silverblue, Kinoite, SteamOS e outras imutaveis, ou sem sudo: tudo na sua pasta
#     pessoal (~/.local/bin e ~/.deno/bin), sem mexer no sistema
#   Gentoo: yt-dlp, Deno e ffmpeg na pasta pessoal (sem compilar); NixOS: mostra o comando do nix
# O Remix procura as ferramentas nessas pastas sozinho.
#
#   bash instalar-dependencias.sh             mostra o que falta e pergunta antes de instalar
#   bash instalar-dependencias.sh --sim       instala sem perguntar
#   bash instalar-dependencias.sh --mostrar   so mostra o que faria (nao instala nada)
#   bash instalar-dependencias.sh --stems     tambem instala o separador de stems (Demucs, ~1 GB, opcional)
#   bash instalar-dependencias.sh --discord   tambem instala o bot do Discord (Node.js 22 + discord.js, opcional)
set -u
AUTO=0; SHOW=0; STEMS=ask; DISCORD=ask
for a in "$@"; do
  case "$a" in
    --sim|-y) AUTO=1 ;;
    --mostrar|--dry-run) SHOW=1 ;;
    --stems) STEMS=1 ;;
    --sem-stems) STEMS=0 ;;
    --discord) DISCORD=1 ;;
    --sem-discord) DISCORD=0 ;;
    -h|--help) sed -n '2,24p' "$0"; exit 0 ;;
  esac
done
STEMS_DIR="$HOME/.local/share/remix/stems"
NODE_DIR="$HOME/.local/share/remix/node"        # Node.js oficial so para o bot (quando o do sistema e antigo)
BOT_DIR="$HOME/.local/share/remix/discord"      # discord.js e @discordjs/voice (o Remix grava o bot.mjs sozinho)
export PATH="$HOME/.local/bin:$HOME/.deno/bin:$PATH"
# Testes: REMIX_DEPS_FINGIR_FALTA=1 finge que nada esta instalado; REMIX_DEPS_OS_RELEASE=arquivo no lugar
# do /etc/os-release; REMIX_DEPS_PM=apt-get|dnf|pacman|zypper|xbps-install|eopkg|nenhum; REMIX_DEPS_IMUTAVEL=1
FAKE="${REMIX_DEPS_FINGIR_FALTA:-0}"

have() { [ "$FAKE" = 1 ] && return 1; command -v "$1" >/dev/null 2>&1; }
run() { if [ "$SHOW" = 1 ]; then echo "   [faria] $*"; return 0; fi; echo "   \$ $*"; "$@"; }
vnum() { "$@" 2>/dev/null | head -n1 | grep -oE '[0-9]+(\.[0-9]+)+' | head -n1; }
vge() { [ -n "$1" ] && [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]; }

ok_ytdlp() { have yt-dlp && vge "$(vnum yt-dlp --version)" 2025.11.0; }
ok_js() {
  if have deno && vge "$(vnum deno --version)" 2.3.0; then return 0; fi
  if have node && vge "$(vnum node --version)" 22.0.0; then return 0; fi
  if have bun && vge "$(vnum bun --version)" 1.2.11; then return 0; fi
  return 1
}
ok_ffmpeg() { have ffmpeg; }
ok_cloudflared() { have cloudflared || [ -x "$HOME/.local/bin/cloudflared" ]; }
ok_dialog() { have zenity || have kdialog; }
ok_pactl() { have pactl; }
ok_curl() {
  [ "$FAKE" = 1 ] && return 1
  { ldconfig -p 2>/dev/null || /sbin/ldconfig -p 2>/dev/null || /usr/sbin/ldconfig -p 2>/dev/null; } | grep -qE 'libcurl(-gnutls)?\.so\.4'
}
line() { if "$2"; then printf '   [ok]     %s\n' "$1"; else printf '   [falta]  %s\n' "$1"; MISSING=1; fi; }
report() {
  MISSING=0
  local y="" j=""
  have yt-dlp && y="$(vnum yt-dlp --version)"
  have deno && j="$j deno $(vnum deno --version)"
  have node && j="$j node $(vnum node --version)"
  line "yt-dlp 2025.11 ou mais novo${y:+ (tem $y)}" ok_ytdlp
  line "ffmpeg" ok_ffmpeg
  line "cloudflared (tunel do Host: celular pela internet)" ok_cloudflared
  line "Deno 2.3+ ou Node.js 22+${j:+ (tem$j)}" ok_js
  line "zenity ou kdialog (janelas de escolher pasta)" ok_dialog
  line "libcurl (capas e links)" ok_curl
  line "pactl (Soundpad: microfone virtual)" ok_pactl
}

# ---- qual distro ----
OSR="${REMIX_DEPS_OS_RELEASE:-/etc/os-release}"
OS_ID=""; OS_LIKE=""; OS_NAME=""; OS_VARIANT=""
if [ -r "$OSR" ]; then
  while IFS='=' read -r k v; do
    v="${v%\"}"; v="${v#\"}"
    case "$k" in ID) OS_ID="$v" ;; ID_LIKE) OS_LIKE="$v" ;; PRETTY_NAME) OS_NAME="$v" ;; VARIANT_ID) OS_VARIANT="$v" ;; esac
  done < "$OSR"
fi
FAMILY=""
for x in $OS_ID $OS_LIKE; do
  case "$x" in
    debian|ubuntu|linuxmint|pop|elementary|zorin|kali|raspbian|neon|deepin|pureos|mx) FAMILY=apt ;;
    fedora|rhel|centos|rocky|almalinux|nobara|ultramarine|ol|amzn) FAMILY=dnf ;;
    arch|manjaro|endeavouros|cachyos|garuda|artix|arcolinux|steamos) FAMILY=pacman ;;
    opensuse*|suse|sles|sled) FAMILY=zypper ;;
    void) FAMILY=xbps ;;
    solus) FAMILY=eopkg ;;
    gentoo) FAMILY=gentoo ;;
    nixos) FAMILY=nixos ;;
    alpine) FAMILY=alpine ;;
  esac
  [ -n "$FAMILY" ] && break
done
IMUTAVEL=0
[ "${REMIX_DEPS_IMUTAVEL:-0}" = 1 ] && IMUTAVEL=1
[ -z "${REMIX_DEPS_OS_RELEASE:-}" ] && [ -e /run/ostree-booted ] && IMUTAVEL=1
case "$OS_ID" in steamos|bazzite|aurora|bluefin|endless) IMUTAVEL=1 ;; esac
case "$OS_VARIANT" in silverblue|kinoite|sericea|onyx|*-atomic) IMUTAVEL=1 ;; esac

PM=""
case "$FAMILY" in apt) PM=apt-get ;; dnf) PM=dnf ;; pacman) PM=pacman ;; zypper) PM=zypper ;; xbps) PM=xbps-install ;; eopkg) PM=eopkg ;; esac
if [ -z "$FAMILY" ]; then for p in apt-get dnf pacman zypper xbps-install eopkg; do command -v "$p" >/dev/null 2>&1 && { PM="$p"; break; }; done; fi
if [ -n "${REMIX_DEPS_PM:-}" ]; then PM="$REMIX_DEPS_PM"; [ "$PM" = nenhum ] && PM=""
elif [ -n "$PM" ] && ! command -v "$PM" >/dev/null 2>&1; then PM=""; fi
[ "$IMUTAVEL" = 1 ] && PM=""
SUDO=""; SEMSUDO=0
if [ -n "$PM" ] && [ "$(id -u)" -ne 0 ]; then
  if command -v sudo >/dev/null 2>&1; then SUDO="sudo"; elif command -v doas >/dev/null 2>&1; then SUDO="doas"; else PM=""; SEMSUDO=1; fi
fi
ARCH="$(uname -m)"

pm_install() {
  case "$PM" in
    dnf) run $SUDO dnf install -y "$@" ;;
    apt-get) run $SUDO apt-get install -y "$@" ;;
    pacman) run $SUDO pacman -S --needed --noconfirm "$@" ;;
    zypper) run $SUDO zypper --non-interactive install "$@" ;;
    xbps-install) run $SUDO xbps-install -Sy "$@" ;;
    eopkg) run $SUDO eopkg install -y "$@" ;;
    *) return 1 ;;
  esac
}
install_one() {   # tenta os pacotes na ordem e para no primeiro que instalar
  local p
  for p in "$@"; do
    if [ "$PM" = apt-get ] && command -v apt-cache >/dev/null 2>&1 && ! apt-cache show "$p" >/dev/null 2>&1; then continue; fi
    pm_install "$p" && return 0
  done
  return 1
}
fetch() {   # url destino
  if command -v curl >/dev/null 2>&1; then run curl -fL --retry 3 -o "$2" "$1"
  elif command -v wget >/dev/null 2>&1; then run wget -O "$2" "$1"
  else echo "   [erro] precisa do curl ou do wget para baixar"; return 1; fi
}
# ---- separador de stems (opcional): Demucs num Python isolado, so para o Remix ----
ok_stems() { [ "$FAKE" = 1 ] && return 1; ls -d "$STEMS_DIR"/venv/lib/python3*/site-packages/demucs >/dev/null 2>&1; }
user_stems() {
  echo "Separador de stems: Demucs (Meta, codigo aberto) + PyTorch de CPU em $STEMS_DIR (~1 GB)"
  local uvb a tmp
  uvb="$(command -v uv 2>/dev/null)"
  if [ -z "$uvb" ]; then
    case "$ARCH" in x86_64) a=x86_64 ;; aarch64) a=aarch64 ;; *) echo "   [erro] processador $ARCH sem o uv oficial"; return 1 ;; esac
    tmp="$(mktemp -d)"
    fetch "https://github.com/astral-sh/uv/releases/latest/download/uv-$a-unknown-linux-gnu.tar.gz" "$tmp/uv.tar.gz" || return 1
    run tar -xzf "$tmp/uv.tar.gz" -C "$tmp" || return 1
    run mkdir -p "$HOME/.local/bin"
    if [ "$SHOW" != 1 ]; then uvb="$(find "$tmp" -type f -name uv -perm -u+x | head -n1)"; [ -n "$uvb" ] || { echo "   [erro] nao achei o uv no pacote"; return 1; }; run cp "$uvb" "$HOME/.local/bin/uv"; fi
    uvb="$HOME/.local/bin/uv"
  fi
  run mkdir -p "$STEMS_DIR"
  run "$uvb" venv --allow-existing --python 3.12 "$STEMS_DIR/venv" || return 1
  run "$uvb" pip install --python "$STEMS_DIR/venv/bin/python" "torch==2.5.1" "torchaudio==2.5.1" --index-url https://download.pytorch.org/whl/cpu || return 1
  run "$uvb" pip install --python "$STEMS_DIR/venv/bin/python" "demucs==4.0.1" soundfile || return 1
  echo "   baixando o modelo htdemucs (~80 MB)"
  if [ "$SHOW" != 1 ]; then TORCH_HOME="$STEMS_DIR/torch" "$STEMS_DIR/venv/bin/python" -c "from demucs.pretrained import get_model; get_model('htdemucs')" || return 1; fi
}
# ---- bot do Discord (opcional): Node.js 22.12+ e os pacotes do bot, so para o Remix ----
node22() {   # imprime o node 22.12+ a usar (o do sistema ou o da pasta do Remix)
  local n
  for n in "$NODE_DIR/bin/node" "$(command -v node 2>/dev/null)"; do
    [ -n "$n" ] && [ -x "$n" ] || continue
    [ "$FAKE" = 1 ] && [ "$n" != "$NODE_DIR/bin/node" ] && continue
    if vge "$(vnum "$n" --version)" 22.12.0; then echo "$n"; return 0; fi
  done
  return 1
}
ok_discord() { [ "$FAKE" = 1 ] && return 1; node22 >/dev/null && [ -f "$BOT_DIR/node_modules/discord.js/package.json" ] && [ -f "$BOT_DIR/node_modules/@discordjs/voice/package.json" ]; }
user_node() {
  local a tmp f sum
  case "$ARCH" in x86_64) a=x64 ;; aarch64) a=arm64 ;; *) echo "   [erro] processador $ARCH sem o Node.js oficial"; return 1 ;; esac
  echo "Node.js 22 oficial em $NODE_DIR (so para o bot do Discord)"
  tmp="$(mktemp -d)"
  fetch "https://nodejs.org/dist/latest-v22.x/SHASUMS256.txt" "$tmp/SHASUMS256.txt" || { rm -rf "$tmp"; return 1; }
  if [ "$SHOW" = 1 ]; then echo "   [faria] baixar node-v22.*-linux-$a.tar.xz, conferir o SHA-256 e extrair"; rm -rf "$tmp"; return 0; fi
  f="$(grep -oE "node-v22\.[0-9.]+-linux-$a\.tar\.xz" "$tmp/SHASUMS256.txt" | head -n1)"
  [ -n "$f" ] || { echo "   [erro] nao achei o Node 22 para $a"; rm -rf "$tmp"; return 1; }
  fetch "https://nodejs.org/dist/latest-v22.x/$f" "$tmp/$f" || { rm -rf "$tmp"; return 1; }
  sum="$(grep " $f\$" "$tmp/SHASUMS256.txt" | cut -d' ' -f1)"
  if [ -z "$sum" ] || [ "$(sha256sum "$tmp/$f" | cut -d' ' -f1)" != "$sum" ]; then echo "   [erro] o arquivo baixado nao confere (SHA-256)"; rm -rf "$tmp"; return 1; fi
  run rm -rf "$NODE_DIR"
  run mkdir -p "$NODE_DIR"
  run tar -xJf "$tmp/$f" -C "$NODE_DIR" --strip-components=1 || { rm -rf "$tmp"; return 1; }
  rm -rf "$tmp"
}
user_discord() {
  local n cli
  n="$(node22)" || { user_node || return 1; n="$NODE_DIR/bin/node"; }
  [ "$SHOW" = 1 ] && [ ! -x "$n" ] && { echo "   [faria] npm install discord.js@14.27.0 @discordjs/voice@0.19.2 em $BOT_DIR"; return 0; }
  echo "Bot do Discord: discord.js + @discordjs/voice em $BOT_DIR (Node $(vnum "$n" --version))"
  run mkdir -p "$BOT_DIR"
  if [ "$SHOW" != 1 ]; then
    printf '%s\n' '{' '  "name": "remix-discord-bot",' '  "private": true,' '  "type": "module",' '  "description": "Ponte do bot do Discord do Remix (instalada pelo app)",' '  "dependencies": { "discord.js": "14.27.0", "@discordjs/voice": "0.19.2" }' '}' > "$BOT_DIR/package.json"
  fi
  cli=""
  for c in "$(dirname "$n")/../lib/node_modules/npm/bin/npm-cli.js" "$(dirname "$(readlink -f "$n")")/../lib/node_modules/npm/bin/npm-cli.js"; do [ -f "$c" ] && { cli="$c"; break; }; done
  if [ -n "$cli" ]; then run "$n" "$cli" install --prefix "$BOT_DIR" --omit=dev --no-audit --no-fund --no-update-notifier --loglevel=error || return 1
  elif command -v npm >/dev/null 2>&1; then run npm install --prefix "$BOT_DIR" --omit=dev --no-audit --no-fund --no-update-notifier --loglevel=error || return 1
  else echo "   [erro] nao achei o npm junto do Node.js (instale o pacote npm da distro)"; return 1; fi
}
arch_ok() {
  case "$ARCH" in x86_64|aarch64) return 0 ;; esac
  echo "   [erro] processador $ARCH: nao ha versao oficial pronta, instale $1 pela sua distro"; return 1
}
# ---- versoes oficiais na pasta pessoal (sem sudo) ----
user_ytdlp() {
  arch_ok yt-dlp || return 1
  echo "yt-dlp: versao oficial em ~/.local/bin"
  run mkdir -p "$HOME/.local/bin"
  if command -v python3 >/dev/null 2>&1 && python3 -c 'import sys; sys.exit(0 if sys.version_info >= (3, 10) else 1)'; then
    fetch "https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp" "$HOME/.local/bin/yt-dlp" || return 1
  else
    local b="yt-dlp_linux"; [ "$ARCH" = aarch64 ] && b="yt-dlp_linux_aarch64"
    fetch "https://github.com/yt-dlp/yt-dlp/releases/latest/download/$b" "$HOME/.local/bin/yt-dlp" || return 1
  fi
  run chmod +x "$HOME/.local/bin/yt-dlp"
}
user_deno() {
  arch_ok Deno || return 1
  echo "Deno: versao oficial em ~/.deno/bin"
  local z="deno-x86_64-unknown-linux-gnu.zip" tmp
  [ "$ARCH" = aarch64 ] && z="deno-aarch64-unknown-linux-gnu.zip"
  tmp="$(mktemp -d)"
  fetch "https://github.com/denoland/deno/releases/latest/download/$z" "$tmp/deno.zip" || { rm -rf "$tmp"; return 1; }
  run mkdir -p "$HOME/.deno/bin"
  if command -v unzip >/dev/null 2>&1; then run unzip -o -q "$tmp/deno.zip" -d "$HOME/.deno/bin"; else run python3 -m zipfile -e "$tmp/deno.zip" "$HOME/.deno/bin"; fi
  run chmod +x "$HOME/.deno/bin/deno"
  rm -rf "$tmp"
}
user_cloudflared() {
  echo "cloudflared: binario oficial da Cloudflare em ~/.local/bin (tunel do Host)"
  local a="amd64"; [ "$ARCH" = aarch64 ] && a="arm64"
  run mkdir -p "$HOME/.local/bin"
  fetch "https://github.com/cloudflare/cloudflared/releases/latest/download/cloudflared-linux-$a" "$HOME/.local/bin/cloudflared" || return 1
  run chmod +x "$HOME/.local/bin/cloudflared"
}
user_ffmpeg() {
  arch_ok ffmpeg || return 1
  echo "ffmpeg: versao estatica oficial (BtbN, cerca de 100 MB) em ~/.local/bin"
  local a="linux64" tmp
  [ "$ARCH" = aarch64 ] && a="linuxarm64"
  tmp="$(mktemp -d)"
  fetch "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/ffmpeg-master-latest-$a-gpl.tar.xz" "$tmp/ff.tar.xz" || { rm -rf "$tmp"; return 1; }
  run tar -xJf "$tmp/ff.tar.xz" -C "$tmp"
  run mkdir -p "$HOME/.local/bin"
  run cp "$tmp/ffmpeg-master-latest-$a-gpl/bin/ffmpeg" "$HOME/.local/bin/ffmpeg"
  run chmod +x "$HOME/.local/bin/ffmpeg"
  rm -rf "$tmp"
}

echo "=============================================================="
echo "  Remix Player - dependencias"
echo "=============================================================="
if [ "$FAMILY" = nixos ]; then MODO="NixOS: instala pelo nix"
elif [ "$IMUTAVEL" = 1 ]; then MODO="distro imutavel: tudo na sua pasta pessoal"
elif [ -n "$PM" ]; then MODO="usa o $PM"
elif [ "$SEMSUDO" = 1 ]; then MODO="sem sudo: tudo na sua pasta pessoal"
else MODO="gerenciador de pacotes nao reconhecido: tudo na sua pasta pessoal"; fi
echo "  Sistema: ${OS_NAME:-${OS_ID:-desconhecido}}   ($MODO)"
if [ "$FAMILY" = alpine ] || { [ -z "${REMIX_DEPS_OS_RELEASE:-}" ] && ldd --version 2>&1 | grep -qi musl; }; then
  echo "  Aviso: esta distro usa a musl (como o Alpine); o Remix e feito para glibc e nao roda aqui."
fi
echo
report
if [ "$MISSING" = 0 ] && { ok_stems || [ "$STEMS" != 1 ]; } && { ok_discord || [ "$DISCORD" != 1 ]; }; then
  echo; echo "Tudo pronto: e so abrir o Remix."
  if ! ok_stems; then echo "(opcional: separador de stems com: bash instalar-dependencias.sh --stems)"; fi
  if ! ok_discord; then echo "(opcional: bot do Discord com: bash instalar-dependencias.sh --discord)"; fi
  exit 0
fi
echo
if [ "$FAMILY" = nixos ]; then
  echo "No NixOS programas baixados soltos nao rodam. Instale pelo nix:"
  echo "   nix-shell -p yt-dlp ffmpeg deno zenity        (so nesta sessao do terminal)"
  echo "   ou ponha yt-dlp, ffmpeg, deno e zenity em environment.systemPackages e rode: sudo nixos-rebuild switch"
  exit 1
fi
if [ -n "$PM" ]; then
  echo "Plano: o que a distro tem em dia vem pelo $PM (pede a senha do $SUDO); o que faltar ou for antigo"
  echo "vem da versao oficial para a sua pasta pessoal."
else
  echo "Plano: versoes oficiais do yt-dlp, do Deno e do ffmpeg em ~/.local/bin e ~/.deno/bin (sem sudo)."
  if [ "$FAMILY" = gentoo ]; then echo "  Gentoo, se preferir pelo sistema: sudo emerge --ask media-video/ffmpeg net-misc/yt-dlp dev-lang/deno-bin gnome-extra/zenity"; fi
fi
if [ "$SHOW" = 0 ] && [ "$AUTO" = 0 ]; then
  printf 'Instalar o que falta agora? [S/n] '
  read -r resp || resp=n
  case "${resp:-s}" in s|S|sim|Sim|SIM|y|Y|yes) ;; *) echo "Nada foi instalado."; exit 0 ;; esac
fi

if [ -n "$PM" ]; then
  [ "$PM" = apt-get ] && run $SUDO apt-get update
  if ! ok_dialog; then case "${XDG_CURRENT_DESKTOP:-}" in *KDE*) install_one kdialog zenity ;; *) install_one zenity kdialog ;; esac; fi
  if ! ok_curl; then case "$PM" in dnf|xbps-install) install_one libcurl ;; apt-get) install_one libcurl4t64 libcurl4 ;; pacman|eopkg) install_one curl ;; zypper) install_one libcurl4 ;; esac; fi
  if ! ok_ffmpeg; then case "$PM" in dnf) install_one ffmpeg ffmpeg-free ;; zypper) install_one ffmpeg-7 ffmpeg-6 ffmpeg ;; *) install_one ffmpeg ;; esac; fi
  if ! ok_pactl; then case "$PM" in pacman) install_one libpulse ;; eopkg) install_one pulseaudio ;; *) install_one pulseaudio-utils ;; esac; fi
  # yt-dlp e runtime JavaScript do repositorio so onde costumam estar em dia (no apt sao antigos)
  if [ "$PM" != apt-get ] && ! ok_ytdlp; then install_one yt-dlp; fi
  if ! ok_js; then case "$PM" in pacman|xbps-install) install_one deno nodejs ;; dnf) install_one nodejs deno ;; zypper) install_one nodejs22 ;; esac; fi
  hash -r 2>/dev/null
  if [ "$SHOW" = 1 ]; then echo "   Se ainda faltar algo depois do $PM:"; fi
else
  if ! ok_dialog; then echo "   Aviso: sem zenity ou kdialog o Remix nao abre a janela de escolher pasta; instale pela loja de apps da distro."; fi
  if ! ok_curl; then echo "   Aviso: falta a libcurl (capas e links); quase toda distro ja traz, instale pela loja de apps se precisar."; fi
  if ! ok_pactl; then echo "   Aviso: sem o pactl o Soundpad nao cria o microfone virtual (pacote pulseaudio-utils ou libpulse)."; fi
fi
ok_ytdlp || user_ytdlp
ok_js || user_deno
ok_ffmpeg || user_ffmpeg
ok_cloudflared || user_cloudflared
if ! ok_stems; then
  if [ "$STEMS" = ask ] && [ "$SHOW" = 0 ] && [ "$AUTO" = 0 ]; then
    echo
    echo "Opcional: separador de stems (so vocal, so musica, bateria, baixo...). Baixa ~1 GB e na CPU"
    echo "leva cerca de metade da duracao de cada musica na primeira vez."
    printf 'Instalar o separador de stems tambem? [s/N] '
    read -r resp || resp=n
    case "${resp:-n}" in s|S|sim|Sim|SIM|y|Y|yes) STEMS=1 ;; *) STEMS=0 ;; esac
  fi
  if [ "$STEMS" = 1 ] || { [ "$SHOW" = 1 ] && [ "$STEMS" != 0 ]; }; then user_stems || echo "   [aviso] o separador de stems nao foi instalado (o resto do Remix funciona igual)."; fi
fi
if ! ok_discord; then
  if [ "$DISCORD" = ask ] && [ "$SHOW" = 0 ] && [ "$AUTO" = 0 ]; then
    echo
    echo "Opcional: bot de musica do Discord (o SEU bot toca as suas playlists e buscas no servidor,"
    echo "com fila e votacao). Baixa o discord.js (~30 MB) e, se o Node.js do sistema for antigo, o Node 22."
    printf 'Instalar o bot do Discord tambem? [s/N] '
    read -r resp || resp=n
    case "${resp:-n}" in s|S|sim|Sim|SIM|y|Y|yes) DISCORD=1 ;; *) DISCORD=0 ;; esac
  fi
  if [ "$DISCORD" = 1 ] || { [ "$SHOW" = 1 ] && [ "$DISCORD" != 0 ]; }; then user_discord || echo "   [aviso] o bot do Discord nao foi instalado (o resto do Remix funciona igual)."; fi
fi
echo
if [ "$SHOW" = 1 ]; then echo "(--mostrar: nada foi instalado)"; exit 0; fi
echo "Conferindo:"
report
echo
if ok_stems; then echo "   [ok]     separador de stems (Demucs)"; else echo "   [opcional] separador de stems: bash instalar-dependencias.sh --stems"; fi
if ok_discord; then echo "   [ok]     bot do Discord (Node $(vnum "$(node22)" --version) + discord.js)"; else echo "   [opcional] bot do Discord: bash instalar-dependencias.sh --discord"; fi
if [ "$MISSING" = 0 ]; then echo "Tudo pronto: e so abrir o Remix."; else echo "Ainda falta algo acima. Veja as mensagens e rode de novo."; exit 1; fi
