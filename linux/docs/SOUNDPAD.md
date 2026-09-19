# Soundpad: sons no seu microfone

O **Soundpad** do Remix toca sons (buzina, aplausos, memes, trilhas) **no seu microfone**, para quem
está com você no Discord, num jogo ou numa chamada ouvir. É **só no PC** (Windows e Linux).

Abra pelo botão **SOUNDPAD** no cabeçalho ou em Configurações > SOUNDPAD E DISCORD.

## Linux

O Remix cria um **microfone virtual** no PipeWire ou no PulseAudio (com o `pactl`):

- uma saída chamada **Remix Soundpad**, onde os sons tocam, e
- uma entrada chamada **Remix Microfone**, que é o que os outros ouvem.

1. **LIGAR MICROFONE**.
2. No Discord (Configurações > Voz e vídeo > Dispositivo de entrada), no jogo ou na chamada, escolha
   **Remix Microfone**.
3. Deixe **MINHA VOZ: SIM** para a sua voz ir junto com os sons.

Ao **desligar** (ou fechar o Remix) os dois dispositivos somem. Se o Remix fechar à força, na
próxima vez ele reaproveita os que ficaram. Precisa do `pactl` (pacote `pulseaudio-utils`, ou
`libpulse` no Arch): instale pela sua distro se faltar.

## Windows

Criar um microfone de verdade no Windows exige um driver, então o Remix usa um **cabo de áudio
virtual** já instalado:

1. Instale o **VB-CABLE** (gratuito, da VB-Audio): botão **BAIXAR VB-CABLE** no painel ou
   <https://vb-audio.com/Cable/>. Reinicie o PC se o instalador pedir.
2. No Remix: **LIGAR MICROFONE** (ele acha o "CABLE Input" sozinho).
3. No Discord/jogo, escolha **CABLE Output** como microfone.

VoiceMeeter também funciona. Em **SAÍDA** dá para escolher outro dispositivo à mão.

## Opções

| Botão | O que faz |
|---|---|
| **MINHA VOZ** | Mistura o seu microfone de verdade com os sons. O Remix nunca usa o próprio microfone virtual como "voz" (seria eco infinito), mesmo se ele for o padrão do sistema. |
| **OUVIR NO FONE** | Toca os sons também na sua saída normal, para você saber o que está tocando. |
| **VOLUME** | Volume geral dos sons (100 → 75 → 50 → 25%), com a mesma curva segura do player. |
| **SAÍDA** | Onde os sons tocam: AUTOMÁTICA (Remix Soundpad / CABLE Input) ou outro dispositivo. |
| **MICROFONE** | Qual microfone de verdade vai como "minha voz": PADRÃO ou outro. |
| **PARAR TODOS** | Para todos os sons tocando. |
| **ABRIR PASTA** | Pasta onde os sons ficam guardados. |

## Sons

- **+ ADICIONAR SONS**: escolha vários arquivos. Eles são **copiados** para a pasta
  `soundpad` do Remix (o original pode ser apagado ou movido). MP3, WAV, OGG e FLAC entram direto;
  M4A, Opus, WMA e outros são convertidos para WAV com o ffmpeg. Até 500 sons, 200 MB cada.
- **Clique** num som para tocar; clique de novo para parar. Vários tocam ao mesmo tempo.
- **Teclas 1 a 9** tocam os nove primeiros sons enquanto o painel está aberto.
- **%** no canto do som muda o volume só dele (100 → 75 → 50 → 25%).
- **✕** remove o som (apaga a cópia).
- Com o microfone desligado, clicar num som toca só no seu fone (bom para testar).

## Dicas

- A **supressão de ruído** do Discord (Krisp) pode cortar sons que não parecem voz: desligue em
  Voz e vídeo > Supressão de ruído se os sons sumirem.
- Deixe a **sensibilidade de entrada** automática (ou use "Pressione para falar" com a tecla segurada
  enquanto o som toca).
- A sua voz só é capturada com **MINHA VOZ: SIM** e o microfone ligado.

## Arquivos

- `soundpad/soundpad.ini`: opções e a lista de sons (id, volume, arquivo, nome).
- `soundpad/<id>.<ext>`: as cópias dos sons.
