#!/usr/bin/env bash
# Remix Player - Linux: roda o app; compila antes se o binario nao existir ou o
# codigo (aqui e em ../comum) for mais novo. Rodando de dentro da arvore do
# projeto, config/capas/Musica ficam na pasta de cima (a "casa" do app); no zip
# portatil ficam nesta mesma pasta.
cd "$(dirname "$0")"
newer_src=0
if [ -x ./remix ]; then
  for f in *.cpp *.h ../comum/*.h ../comum/*.c; do
    [ -e "$f" ] && [ "$f" -nt ./remix ] && newer_src=1
  done
fi
if [ ! -x ./remix ] || [ "$newer_src" = 1 ]; then
  if [ -f build.sh ]; then
    echo "Compilando o Remix (primeira vez ou codigo alterado)..."
    bash fetch-deps.sh && bash build.sh || { echo "Falhou a compilacao. Veja README-LINUX.md."; exit 1; }
  fi
fi
chmod +x ./remix 2>/dev/null || true   # zips feitos no Windows perdem a permissao de execucao
# Num terminal, abre em segundo plano e devolve o prompt (fechar o terminal nao
# mata o player). ./RODAR.sh --fg (ou REMIX_FOREGROUND=1) mantem preso ao terminal
# para ver as mensagens.
if [ "${1:-}" = "--fg" ]; then shift; exec ./remix "$@"; fi
if [ -t 1 ] && [ -z "${REMIX_FOREGROUND:-}" ]; then
  setsid nohup ./remix "$@" >/dev/null 2>&1 < /dev/null &
  echo "Remix aberto em segundo plano (./RODAR.sh --fg mostra as mensagens no terminal)."
  exit 0
fi
exec ./remix "$@"
