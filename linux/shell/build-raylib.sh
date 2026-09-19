#!/usr/bin/env bash
# Compila o raylib 5.5 (GLFW X11 + Wayland, sem raudio/rmodels) como lib estatica.
# Sem cmake: compila os poucos .c diretamente. Headers X11/Wayland podem vir do
# sistema (pacotes -devel) ou do sysroot extraido em third_party/sysroot.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
RL="$ROOT/third_party/raylib-5.5/src"
SYSROOT="$ROOT/third_party/sysroot"
# Toolchain: zig (third_party/zig) gera binario para glibc antiga (REMIX_GLIBC,
# padrao 2.27 = Ubuntu 18.04 / Debian 10); gcc do sistema como alternativa.
TOOLCHAIN="${REMIX_TOOLCHAIN:-auto}"
ZIG="$ROOT/third_party/zig/zig"
GLIBC_TARGET="${REMIX_GLIBC:-2.27}"
if [ "$TOOLCHAIN" = auto ]; then if [ -x "$ZIG" ]; then TOOLCHAIN=zig; else TOOLCHAIN=gcc; fi; fi
if [ "$TOOLCHAIN" = zig ]; then
  CC=("$ZIG" cc -target "x86_64-linux-gnu.$GLIBC_TARGET" -fno-sanitize=undefined)
  AR=("$ZIG" ar)
  OUT="$ROOT/build/raylib-zig"
else
  CC=("${CC:-gcc}")
  AR=(ar)
  OUT="$ROOT/build/raylib"
fi
mkdir -p "$OUT/wl"
INC=(-I"$RL" -I"$RL/external/glfw/include" -I"$OUT/wl")
[ -d "$SYSROOT/usr/include" ] && INC+=(-isystem "$SYSROOT/usr/include")
WAYLAND=1
SCANNER="$(command -v wayland-scanner || true)"
[ -z "$SCANNER" ] && [ -x "$SYSROOT/usr/bin/wayland-scanner" ] && SCANNER="$SYSROOT/usr/bin/wayland-scanner"
PREGEN="$ROOT/linux/wl-protocols"
if [ "${REMIX_NO_WAYLAND:-0}" = "1" ]; then WAYLAND=0
elif [ -z "$SCANNER" ]; then
  if [ -f "$PREGEN/xdg-shell-client-protocol.h" ]; then cp "$PREGEN"/*.h "$OUT/wl/"; SCANNER=pregen; else WAYLAND=0; fi
fi
DEFS=(-DPLATFORM_DESKTOP -DPLATFORM_DESKTOP_GLFW -DGRAPHICS_API_OPENGL_33 -D_GLFW_X11
      -DSUPPORT_FILEFORMAT_JPG=1 -DSUPPORT_FILEFORMAT_BMP=1 -D_GNU_SOURCE)
if [ "$WAYLAND" = 1 ]; then
  DEFS+=(-D_GLFW_WAYLAND)
fi
if [ "$WAYLAND" = 1 ] && [ "$SCANNER" != pregen ]; then
  WLP="$RL/external/glfw/deps/wayland"
  gen(){ "$SCANNER" client-header "$WLP/$1.xml" "$OUT/wl/$2.h"; "$SCANNER" private-code "$WLP/$1.xml" "$OUT/wl/$2-code.h"; }
  gen wayland wayland-client-protocol
  gen xdg-shell xdg-shell-client-protocol
  gen xdg-decoration-unstable-v1 xdg-decoration-unstable-v1-client-protocol
  gen viewporter viewporter-client-protocol
  gen relative-pointer-unstable-v1 relative-pointer-unstable-v1-client-protocol
  gen pointer-constraints-unstable-v1 pointer-constraints-unstable-v1-client-protocol
  gen fractional-scale-v1 fractional-scale-v1-client-protocol
  gen xdg-activation-v1 xdg-activation-v1-client-protocol
  gen idle-inhibit-unstable-v1 idle-inhibit-unstable-v1-client-protocol
fi
X11C="$RL/external/glfw/src/x11_window.c"   # correcao do Remix no GLFW (XWayland), caso o raylib tenha vindo sem ela
if [ -f "$X11C" ] && ! grep -q 'Remix: prazo absoluto' "$X11C"; then
  patch -s -p1 --forward -d "$ROOT/third_party/raylib-5.5" < "$ROOT/linux/patches/glfw-x11-visibility-timeout.patch" || echo "[raylib] aviso: nao consegui aplicar o patch do GLFW"
fi
CFLAGS=(-O2 -DNDEBUG -std=gnu99 -fPIC -w)
echo "[raylib] toolchain=$TOOLCHAIN wayland=$WAYLAND"
for f in rcore rshapes rtextures rtext utils rglfw; do
  echo "[raylib] cc $f.c"
  "${CC[@]}" -c "${CFLAGS[@]}" "${DEFS[@]}" "${INC[@]}" "$RL/$f.c" -o "$OUT/$f.o"
done
rm -f "$OUT/libraylib.a"
"${AR[@]}" rcs "$OUT/libraylib.a" "$OUT"/*.o
echo "[raylib] ok -> $OUT/libraylib.a ($(du -h "$OUT/libraylib.a" | cut -f1))"
echo "$WAYLAND" > "$OUT/wayland.flag"
