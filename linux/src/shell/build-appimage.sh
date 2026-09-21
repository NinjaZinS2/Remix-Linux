#!/usr/bin/env bash
# Gera dist/Remix-<ver>-x86_64.AppImage a partir de build/remix.
# Um arquivo so, roda em qualquer distro x86_64 com glibc >= 2.27: da permissao de
# execucao e abre. Config/capas ficam em ~/.config/remix (como no .deb/.rpm).
# Ferramentas (appimagetool + runtime estatico) ficam em third_party/appimage/
# (ja incluidas no projeto; se faltarem, baixa do GitHub). Nao precisa de root.
#   REMIX_VERSION=1.6.1 src/shell/build-appimage.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"; cd "$ROOT"
VER="${REMIX_VERSION:-1.6.1}"
TOOLDIR="$ROOT/third_party/appimage"
TOOL="$TOOLDIR/appimagetool-x86_64.AppImage"; RUNTIME="$TOOLDIR/runtime-x86_64"
mkdir -p "$TOOLDIR"
if [ ! -f "$TOOL" ]; then
  echo "[appimage] baixando appimagetool..."
  curl -fsSL -o "$TOOL" https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage || { echo "[appimage] sem appimagetool (offline?): pulando"; exit 0; }
fi
if [ ! -f "$RUNTIME" ]; then
  echo "[appimage] baixando runtime estatico..."
  curl -fsSL -o "$RUNTIME" https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64 || { echo "[appimage] sem runtime (offline?): pulando"; exit 0; }
fi
chmod +x "$TOOL"
[ -x build/remix ] || bash src/shell/build.sh
# texto enviado pelo site do GitHub a partir do Windows chega com CRLF; o .desktop quebra com isso
semcr() { install -Dm"$1" /dev/null "$3" && tr -d '\r' < "$2" > "$3"; }

APP="$ROOT/build/AppDir"; rm -rf "$APP"
install -Dm755 build/remix "$APP/usr/bin/remix"; strip --strip-unneeded "$APP/usr/bin/remix" 2>/dev/null || true
for f in splash.png open.wav icon.png; do install -Dm644 "assets/branding/$f" "$APP/usr/share/remix/assets/branding/$f"; done
for f in assets/fonts/*.ttf; do install -Dm644 "$f" "$APP/usr/share/remix/assets/fonts/$(basename "$f")"; done
for f in assets/themes/*.ini; do semcr 644 "$f" "$APP/usr/share/remix/assets/themes/$(basename "$f")"; done
semcr 644 src/shell/remix.desktop "$APP/usr/share/applications/remix.desktop"
semcr 644 src/shell/remix.desktop "$APP/remix.desktop"
for d in src/shell/icons/*/; do sz="$(basename "$d")"; install -Dm644 "$d/remix.png" "$APP/usr/share/icons/hicolor/$sz/apps/remix.png"; done
install -Dm644 src/shell/icons/256x256/remix.png "$APP/remix.png"
ln -sf remix.png "$APP/.DirIcon"
semcr 644 src/shell/packaging/copyright "$APP/usr/share/doc/remix/copyright"
cat > "$APP/AppRun" <<'RUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/remix" "$@"
RUN
chmod +x "$APP/AppRun"

# O appimagetool e ele mesmo um AppImage: sem FUSE (containers, sandbox), extrai e roda direto.
RUN="$TOOL"
if ! "$TOOL" --version >/dev/null 2>&1; then
  if [ ! -x "$TOOLDIR/squashfs-root/AppRun" ]; then (cd "$TOOLDIR" && "$TOOL" --appimage-extract >/dev/null); fi
  RUN="$TOOLDIR/squashfs-root/AppRun"
fi
mkdir -p dist; OUT="dist/Remix-$VER-x86_64.AppImage"; rm -f "$OUT"
ARCH=x86_64 "$RUN" --no-appstream --runtime-file "$RUNTIME" "$APP" "$OUT" >"$ROOT/build/appimage.log" 2>&1 || { tail -20 "$ROOT/build/appimage.log"; exit 1; }
chmod +x "$OUT"
echo "[appimage] ok -> $OUT ($(du -h "$OUT" | cut -f1))"
