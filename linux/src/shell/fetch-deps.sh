#!/usr/bin/env bash
# Baixa o que o build Linux precisa e nao esta no repositorio (nada e instalado
# no sistema; tudo fica em third_party/):
#   1) raylib 5.5 (fonte + src/shell/patches) -> third_party/raylib-5.5
#   2) zig 0.13 (toolchain C/C++ que gera binario p/ glibc antiga, >= 2.27)
#                                        -> third_party/zig   (REMIX_NO_ZIG=1 pula)
#   3) headers X11/Wayland, se os pacotes -devel nao estiverem instalados
#      (dnf download / apt-get download + extracao) -> third_party/sysroot
# Uso: src/shell/fetch-deps.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TP="$ROOT/third_party"
mkdir -p "$TP"

if [ ! -d "$TP/raylib-5.5/src" ]; then
  echo "[deps] baixando raylib 5.5..."
  curl -sSL --retry 3 -o "$TP/raylib-5.5.tar.gz" https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz
  tar -C "$TP" -xzf "$TP/raylib-5.5.tar.gz"
fi
# correcao do Remix no GLFW do raylib (a janela travava abrindo no XWayland): aplica uma vez
X11C="$TP/raylib-5.5/src/external/glfw/src/x11_window.c"
if [ -f "$X11C" ] && ! grep -q 'Remix: prazo absoluto' "$X11C"; then
  echo "[deps] aplicando src/shell/patches/glfw-x11-visibility-timeout.patch..."
  if command -v patch >/dev/null; then patch -s -p1 --forward -d "$TP/raylib-5.5" < "$ROOT/linux/patches/glfw-x11-visibility-timeout.patch"
  else (cd "$TP/raylib-5.5" && git apply "$ROOT/linux/patches/glfw-x11-visibility-timeout.patch"); fi
fi

ZIGV="${REMIX_ZIG_VERSION:-0.13.0}"
if [ "${REMIX_NO_ZIG:-0}" != 1 ] && [ ! -x "$TP/zig/zig" ]; then
  echo "[deps] baixando zig $ZIGV (~47 MB; toolchain para binario compativel com glibc antiga)..."
  curl -sSL --retry 3 -o "$TP/zig.tar.xz" "https://ziglang.org/download/$ZIGV/zig-linux-x86_64-$ZIGV.tar.xz"
  rm -rf "$TP/zig" "$TP/zig-linux-x86_64-$ZIGV"
  tar -C "$TP" -xJf "$TP/zig.tar.xz"
  mv "$TP/zig-linux-x86_64-$ZIGV" "$TP/zig"
  rm -f "$TP/zig.tar.xz"
fi
[ -x "$TP/zig/zig" ] && echo "[deps] zig $("$TP/zig/zig" version) ok" || echo "[deps] sem zig: o build usa o g++ do sistema (binario exige glibc >= 2.35)"

need_headers=0
for h in X11/Xlib.h X11/Xcursor/Xcursor.h X11/extensions/Xrandr.h X11/extensions/Xinerama.h X11/extensions/XInput2.h xkbcommon/xkbcommon.h wayland-client.h; do
  [ -f "/usr/include/$h" ] || [ -f "$TP/sysroot/usr/include/$h" ] || need_headers=1
done
if [ "$need_headers" = 0 ]; then echo "[deps] headers X11/Wayland ok"; exit 0; fi

mkdir -p "$TP/pkgs" "$TP/sysroot"
if command -v dnf >/dev/null; then
  echo "[deps] Fedora/RHEL: baixando pacotes -devel (sem instalar)..."
  PK="libX11-devel libXcursor-devel libXrandr-devel libXinerama-devel libXi-devel libXrender-devel libXext-devel libXfixes-devel xorg-x11-proto-devel libxkbcommon-devel wayland-devel wayland-protocols-devel libstdc++-static"
  (cd "$TP/pkgs" && dnf download --destdir . $PK >/dev/null)
  for f in "$TP"/pkgs/*.x86_64.rpm "$TP"/pkgs/*.noarch.rpm; do [ -f "$f" ] && bsdtar -C "$TP/sysroot" -xf "$f"; done
elif command -v apt-get >/dev/null; then
  echo "[deps] Debian/Ubuntu: baixando pacotes -dev (sem instalar)..."
  PK="libx11-dev libxcursor-dev libxrandr-dev libxinerama-dev libxi-dev libxrender-dev libxext-dev libxfixes-dev x11proto-dev libxkbcommon-dev libwayland-dev wayland-protocols libwayland-bin"
  (cd "$TP/pkgs" && apt-get download $PK >/dev/null)
  for f in "$TP"/pkgs/*.deb; do dpkg -x "$f" "$TP/sysroot"; done
  # Debian instala os headers em /usr/include/<triplet> tambem
  [ -d "$TP/sysroot/usr/include/x86_64-linux-gnu" ] && cp -an "$TP/sysroot/usr/include/x86_64-linux-gnu/." "$TP/sysroot/usr/include/" || true
else
  echo "[deps] instale os headers de X11/Wayland (libX11, Xcursor, Xrandr, Xinerama, Xi, Xrender, Xext, Xfixes, xkbcommon, wayland) e rode de novo."
  exit 1
fi
echo "[deps] ok -> $TP/sysroot"
