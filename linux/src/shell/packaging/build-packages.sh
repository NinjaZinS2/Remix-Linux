#!/usr/bin/env bash
# Gera os pacotes Linux a partir de build/remix:
#   dist/remix_<ver>-<rel>_amd64.deb   (Debian 12+, Ubuntu 22.04+, Mint 21+...)
#   dist/remix-<ver>-<rel>.x86_64.rpm  (Fedora, Nobara, RHEL-like)
#   dist/Remix-<ver>-x86_64.AppImage    (qualquer distro: um arquivo so)
# Requer: dpkg-deb (pacote dpkg) e rpmbuild (pacote rpm-build). Nao precisa de root.
#   REMIX_VERSION=1.6.1 REMIX_RELEASE=1 src/shell/packaging/build-packages.sh
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
cd "$ROOT"
VER="${REMIX_VERSION:-1.6.1}"
REL="${REMIX_RELEASE:-1}"
ARCH_DEB=amd64
[ -x build/remix ] || bash src/shell/build.sh
# Arquivo de texto enviado pelo site do GitHub a partir do Windows chega com CRLF. O .rpm (spec),
# o .deb (control), o .desktop e os scripts .sh quebram com isso: todo texto que entra num pacote
# passa por aqui sem o \r. Uso: semcr <modo> <origem> <destino>
semcr() { install -Dm"$1" /dev/null "$3" && tr -d '\r' < "$2" > "$3"; }
# glibc minima exigida pelo binario (vira o Depends do .deb)
MINGLIBC="$(objdump -T build/remix | grep -oE 'GLIBC_[0-9.]+' | sed 's/GLIBC_//' | sort -t. -k1,1n -k2,2n -k3,3n | tail -1)"
echo "[pkg] glibc minima: $MINGLIBC"

# ---------------------------------------------------------------- staging --
STAGE="$ROOT/build/stage"
rm -rf "$STAGE"; mkdir -p "$STAGE"
install -Dm755 build/remix "$STAGE/usr/bin/remix"
strip --strip-unneeded "$STAGE/usr/bin/remix"
install -Dm644 assets/branding/splash.png "$STAGE/usr/share/remix/assets/branding/splash.png"
install -Dm644 assets/branding/open.wav   "$STAGE/usr/share/remix/assets/branding/open.wav"
install -Dm644 assets/branding/icon.png   "$STAGE/usr/share/remix/assets/branding/icon.png"
for f in assets/fonts/*.ttf; do install -Dm644 "$f" "$STAGE/usr/share/remix/assets/fonts/$(basename "$f")"; done
for f in assets/themes/*.ini; do semcr 644 "$f" "$STAGE/usr/share/remix/assets/themes/$(basename "$f")"; done
semcr 644 src/shell/remix.desktop "$STAGE/usr/share/applications/remix.desktop"
for d in src/shell/icons/*/; do sz="$(basename "$d")"; install -Dm644 "$d/remix.png" "$STAGE/usr/share/icons/hicolor/$sz/apps/remix.png"; done
semcr 644 src/shell/README-LINUX.md "$STAGE/usr/share/doc/remix/README-LINUX.md"
semcr 644 src/shell/packaging/copyright "$STAGE/usr/share/doc/remix/copyright"
[ -f /usr/share/licenses/dejavu-sans-fonts/LICENSE ] && install -Dm644 /usr/share/licenses/dejavu-sans-fonts/LICENSE "$STAGE/usr/share/doc/remix/licenses/DejaVu-LICENSE" || true
[ -f /usr/share/licenses/google-droid-sans-fonts/NOTICE ] && install -Dm644 /usr/share/licenses/google-droid-sans-fonts/NOTICE "$STAGE/usr/share/doc/remix/licenses/Droid-NOTICE" || true
mkdir -p dist
echo "[pkg] staging pronto: $(du -sh "$STAGE" | cut -f1)"

# -------------------------------------------------------------------- .deb --
if command -v dpkg-deb >/dev/null; then
  DEB="$ROOT/build/deb"; rm -rf "$DEB"; mkdir -p "$DEB"
  cp -a "$STAGE/." "$DEB/"
  mkdir -p "$DEB/DEBIAN"
  SIZE_KB="$(du -sk --exclude=DEBIAN "$DEB" | cut -f1)"
  tr -d '\r' < src/shell/packaging/deb/control.in | sed -e "s/@VERSION@/$VER-$REL/" -e "s/@SIZE@/$SIZE_KB/" -e "s/@ARCH@/$ARCH_DEB/" -e "s/@GLIBC@/$MINGLIBC/" > "$DEB/DEBIAN/control"
  (cd "$DEB" && find . -type f ! -path './DEBIAN/*' -exec md5sum {} + | sed 's| \./| |' > DEBIAN/md5sums)
  chmod 0755 "$DEB/DEBIAN"; chmod 0644 "$DEB/DEBIAN/control" "$DEB/DEBIAN/md5sums"
  OUT_DEB="dist/remix_${VER}-${REL}_${ARCH_DEB}.deb"
  dpkg-deb --build --root-owner-group "$DEB" "$OUT_DEB" >/dev/null
  echo "[pkg] deb -> $OUT_DEB"
else
  echo "[pkg] dpkg-deb nao encontrado: pulando .deb"
fi

# -------------------------------------------------------------------- .rpm --
if command -v rpmbuild >/dev/null; then
  RPMTOP="$ROOT/build/rpmbuild"; rm -rf "$RPMTOP"
  mkdir -p "$RPMTOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS,BUILDROOT}
  tar -C "$STAGE" -czf "$RPMTOP/SOURCES/remix-stage.tar.gz" usr
  tr -d '\r' < src/shell/packaging/rpm/remix.spec > "$RPMTOP/SPECS/remix.spec"
  rpmbuild -bb "$RPMTOP/SPECS/remix.spec" \
    --define "_topdir $RPMTOP" --define "remix_version $VER" --define "remix_release $REL" \
    --define "dist %{nil}" >"$RPMTOP/rpmbuild.log" 2>&1 || { tail -40 "$RPMTOP/rpmbuild.log"; exit 1; }
  cp "$RPMTOP"/RPMS/x86_64/remix-*.rpm dist/
  echo "[pkg] rpm -> $(ls dist/remix-*.rpm | tail -1)"
else
  echo "[pkg] rpmbuild nao encontrado: pulando .rpm"
fi
# ------------------------------------------------------------ zip portatil --
# Pasta autossuficiente: extrair e rodar ./remix (ou RODAR.sh). Config e capas
# ficam na propria pasta (config.ini ao lado do binario = modo portatil).
PORT="$ROOT/build/portable/remix-$VER-linux-x86_64"
rm -rf "$ROOT/build/portable"; mkdir -p "$PORT/Musica" "$PORT/assets/covers"
install -m755 "$STAGE/usr/bin/remix" "$PORT/remix"
cp -a assets/branding assets/fonts assets/themes "$PORT/assets/"
rm -f "$PORT/assets/branding/app.ico"
for f in "$PORT"/assets/themes/*.ini; do semcr 644 "assets/themes/$(basename "$f")" "$f"; done
semcr 755 src/shell/RODAR.sh "$PORT/RODAR.sh"
semcr 644 src/shell/portatil/MUSICA-LEIA-ME.txt "$PORT/Musica/LEIA-ME.txt"
semcr 644 src/shell/README-LINUX.md "$PORT/README-LINUX.md"
semcr 644 src/shell/packaging/copyright "$PORT/LICENCAS.txt"
semcr 644 src/shell/LEIA-ME-PORTATIL.txt "$PORT/LEIA-ME.txt"
printf '[General]\nMusicFolder=Musica\n' > "$PORT/config.ini"
OUT_ZIP="dist/remix-$VER-linux-x86_64-portable.zip"
rm -f "$OUT_ZIP"
(cd "$ROOT/build/portable" && zip -qr "$ROOT/$OUT_ZIP" "remix-$VER-linux-x86_64")
echo "[pkg] zip portatil -> $OUT_ZIP"
# ---------------------------------------------------------------- AppImage --
bash src/shell/build-appimage.sh || echo "[pkg] AppImage falhou (opcional)"
ls -la dist/
