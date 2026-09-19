#!/usr/bin/env bash
# Compila o Remix para Linux -> build/remix (e copia para src/shell/remix)
# Codigo comum aos dois sistemas em src/core/, casca Linux em src/shell/.
#   src/shell/build.sh            build otimizado
#   src/shell/build.sh --debug    com simbolos e sem otimizacao
#   src/shell/build.sh --no-wayland   raylib so com X11
#   REMIX_TOOLCHAIN=gcc src/shell/build.sh   forca o g++ do sistema (padrao: zig se
#                                        existir em third_party/zig -> binario
#                                        compativel com glibc >= REMIX_GLIBC, 2.27)
# Dependencias: zig (src/shell/fetch-deps.sh baixa) OU g++ com C++20; headers
# X11/Wayland (pacotes -devel do sistema OU o sysroot de src/shell/fetch-deps.sh).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
SYSROOT="$ROOT/third_party/sysroot"
RL="$ROOT/third_party/raylib-5.5/src"
[ -d "$RL" ] || { echo "raylib nao encontrado em third_party/. Rode: src/shell/fetch-deps.sh"; exit 1; }

DEBUG=0
for a in "$@"; do
  case "$a" in
    --debug) DEBUG=1 ;;
    --no-wayland) export REMIX_NO_WAYLAND=1 ;;
  esac
done
TOOLCHAIN="${REMIX_TOOLCHAIN:-auto}"
ZIG="$ROOT/third_party/zig/zig"
GLIBC_TARGET="${REMIX_GLIBC:-2.27}"
if [ "$TOOLCHAIN" = auto ]; then if [ -x "$ZIG" ]; then TOOLCHAIN=zig; else TOOLCHAIN=gcc; fi; fi
export REMIX_TOOLCHAIN="$TOOLCHAIN"
if [ "$TOOLCHAIN" = zig ]; then RL_LIB="build/raylib-zig/libraylib.a"; else RL_LIB="build/raylib/libraylib.a"; fi
for a in "$@"; do [ "$a" = --rebuild-raylib ] && rm -f "$RL_LIB"; done
[ -f "$RL_LIB" ] || bash src/shell/build-raylib.sh
mkdir -p build

OPT="-O2"; [ "$DEBUG" = 1 ] && OPT="-O0 -g"
INC=(-I"$ROOT/src/core" -I"$ROOT/src/shell" -I"$RL" -I"$RL/external")

# backend de audio (miniaudio + stb_vorbis) e C puro: compila separado
if [ "$TOOLCHAIN" = zig ]; then CCC=("$ZIG" cc -target "x86_64-linux-gnu.$GLIBC_TARGET" -fno-sanitize=undefined); else CCC=("${CC:-gcc}"); fi
"${CCC[@]}" $OPT -w -c -I"$ROOT/src/core" src/core/audio_backend.c -o build/audio_backend.o
if [ "$TOOLCHAIN" = zig ]; then
  # zig: libc++ estatica embutida, glibc alvo fixa (nada de shims).
  CXX=("$ZIG" c++ -target "x86_64-linux-gnu.$GLIBC_TARGET" -fno-sanitize=undefined)
  SOURCES=(src/shell/main_linux.cpp build/audio_backend.o)
  LINK=(-lm -lpthread -ldl)
  echo "[remix] compilando com zig (glibc >= $GLIBC_TARGET, debug=$DEBUG)"
else
  CXX=("${CXX:-g++}")
  SOURCES=(src/shell/main_linux.cpp build/audio_backend.o -fno-builtin src/shell/compat_glibc.cpp)
  # libstdc++ estatica: a do sistema (pacote libstdc++-static) ou a extraida no sysroot.
  LIBDIRS=()
  sys_a="$("${CXX[@]}" -print-file-name=libstdc++.a)"
  if [ ! -f "$sys_a" ]; then
    alt="$(find "$SYSROOT/usr/lib/gcc" -name libstdc++.a 2>/dev/null | head -1 || true)"
    if [ -n "$alt" ]; then LIBDIRS+=(-L"$(dirname "$alt")"); else echo "[aviso] libstdc++.a nao encontrada: linkando libstdc++ dinamica"; fi
  fi
  LINK=("${LIBDIRS[@]}" -static-libgcc)
  if [ -f "$sys_a" ] || [ ${#LIBDIRS[@]} -gt 0 ]; then LINK+=(-static-libstdc++); fi
  # libm: amarra sqrtf/fmod & cia a versao base da glibc (ver compat_glibc.cpp)
  case "$(uname -m)" in x86_64) LINK+=(-Wl,--wrap=sqrtf,--wrap=atan2f,--wrap=asinf,--wrap=acosf,--wrap=fmod,--wrap=fmodf) ;; esac
  LINK+=(-lpthread -ldl -lm -Wl,--as-needed -Wl,-O1)
  echo "[remix] compilando com g++ (debug=$DEBUG)"
fi
"${CXX[@]}" -std=gnu++20 $OPT -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
  -Wno-missing-field-initializers -Wno-sign-compare -Wno-unused-variable -Wno-unused-but-set-variable \
  "${INC[@]}" -o build/remix "${SOURCES[@]}" "$RL_LIB" "${LINK[@]}"
cp build/remix src/shell/remix && chmod +x src/shell/remix   # binario pronto na pasta src/shell/ (RODAR.sh)
command -v strip >/dev/null && strip --strip-unneeded src/shell/remix || true
echo "[remix] ok -> build/remix ($(du -h build/remix | cut -f1))"
echo "[remix] glibc minima exigida: $(objdump -T build/remix | grep -oE 'GLIBC_[0-9.]+' | sort -t. -k2,2n -k3,3n -u | tail -1)"
