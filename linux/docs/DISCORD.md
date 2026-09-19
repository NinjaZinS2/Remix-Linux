# Bot de música do Discord

O Remix do PC pode rodar **o seu próprio bot de música** no Discord. Quem está no servidor usa
comandos como `/tocar`, `/fila` e `/pular` — igual aos bots de música conhecidos — mas **quem faz o
trabalho é o seu PC**, com os mesmos mecanismos do Remix: as suas playlists, a busca online
(yt-dlp), os links do Spotify/Deezer/Apple Music e os efeitos (slow, speed, reverb, grave, 8D).

É **só no PC** (Windows e Linux). O **Host do celular** é outra coisa, com outras regras: o bot do
Discord não mexe nos aparelhos vinculados nem no que eles podem ver.

## Como ligar (uma vez só)

1. **Instale o bot no PC.** Rode o instalador de dependências e escolha o bot do Discord:
   - Linux: `bash instalar-dependencias.sh --discord` (usa o Node.js do sistema se for 22.12 ou mais
     novo; senão baixa o Node 22 oficial, conferindo o SHA-256, em `~/.local/share/remix/node`).
     Os pacotes do bot ficam em `~/.local/share/remix/discord`.
   - Windows: `INSTALAR-DEPENDENCIAS.bat /discord` (Node.js 22 portátil e os pacotes do bot dentro
     da pasta do Remix, em `assets\tools\node` e `assets\tools\discord`).
   - Se você já tem o Node.js 22.12+, dá para instalar pelo próprio app: painel **DISCORD** >
     **INSTALAR BOT**.
2. **Crie o bot no Discord.** Painel **DISCORD** > **ABRIR O PORTAL** (ou
   <https://discord.com/developers/applications>) > **New Application** > dê um nome > aba **Bot** >
   **Reset Token** > **Copy**. Se só você for convidar o bot, desligue **Public Bot** nessa aba.
   Não precisa ligar nenhuma "Privileged Gateway Intent".
3. **Cole o token no Remix:** **DEFINIR TOKEN...** > Ctrl+V > Enter. Ele aparece só como bolinhas,
   fica guardado neste PC e nunca vai para a tela, o registro ou outro lugar.
4. **LIGAR BOT.** Quando conectar, **COPIAR CONVITE** copia o link de convite (com as permissões
   certas: ver canais, mandar mensagens, links embutidos, conectar e falar). Abra no navegador,
   escolha o servidor e autorize.
5. No servidor, digite `/`: os comandos do Remix aparecem (na primeira vez pode levar um minuto).

O bot volta sozinho quando você abre o Remix, se estava ligado quando você fechou.

## O que o dono decide (painel DISCORD)

| Opção | O que faz |
|---|---|
| **FILA PÚBLICA** | SIM: qualquer um no servidor põe músicas (busca e links). NÃO: só DJ, admins e você. |
| **PLAYLISTS PARA TODOS** | SIM: qualquer um toca as playlists que você liberou. NÃO: só DJ, admins e você. |
| **MÚSICAS ONLINE** | NÃO: o bot só toca as playlists liberadas (nada de YouTube/SoundCloud). |
| **VOTAÇÃO** | 50%, 66%, 75%, 100% de quem está na chamada, ou DESLIGADA (aí só DJ/admins/dono controlam). |
| **CARGO DJ** | Nome do cargo do servidor (padrão `DJ`). Quem tem o cargo não precisa de votação. |
| **LIMITE** | Quantas músicas cada pessoa pode ter na fila (10 a 200). |
| **PLAYLISTS NO BOT** | Marque **NO BOT: SIM** nas playlists que o bot pode tocar. As outras não existem para o Discord. |

Também dá para liberar uma playlist pelo botão direito no card da playlist ("No bot do Discord: liberar").

## Tocar pelo PC

Com o bot conectado:

- **▶ DISCORD em cima da capa** (passe o mouse num card de música ou de playlist) ou botão direito >
  **Tocar no bot do Discord**: toca **agora** no servidor onde **você está numa chamada** (ou onde o
  bot já está tocando). A playlist substitui a fila. O canal recebe o aviso "O dono pôs para tocar
  pelo PC".
- No painel, cada servidor tocando tem **pausar/continuar**, **PULAR**, **REPETIR** e **PARAR**.

## Comandos

Cada pessoa vê os nomes no idioma do Discord dela (em inglês: `/play`, `/queue`, `/skip`...).

| Comando | O que faz | Quem pode sem votação |
|---|---|---|
| `/tocar musica [proxima]` | Nome ou link (YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music, Bandcamp). Álbum e playlist entram inteiros (até o limite). `proxima`: logo depois da atual (DJ). | fila pública: todos |
| `/buscar musica [fonte]` | Mostra os resultados (as playlists liberadas primeiro) com um menu para escolher. | fila pública: todos |
| `/playlist nome [aleatorio]` | Toca uma playlist liberada (com autocompletar). | playlists para todos: todos |
| `/playlists` | Lista as playlists liberadas. | todos |
| `/fila [pagina]` · `/agora` | Fila com páginas; o que está tocando com a barra de tempo e botões. | todos |
| `/pular` | Pula a música. | DJ, admins, dono, quem pediu a música |
| `/pausar` · `/continuar` | | DJ, admins, dono, quem pediu |
| `/avancar tempo` | Vai para `1:30` (ou segundos). | DJ, admins, dono, quem pediu |
| `/efeito tipo [nivel]` | Slow, speed, reverb, grave, 8D (nível 1 a 3; sem nível liga/desliga). | DJ, admins, dono, quem pediu |
| `/remover posicao` | Tira da fila. | a sua própria música; a dos outros: DJ/votação |
| `/mover de para` · `/embaralhar` · `/limpar` · `/repetir modo` | | DJ, admins, dono |
| `/parar` · `/sair` | Para, limpa a fila e sai da chamada. | DJ, admins, dono |
| `/volume porcento` | 10% a 150% (cada pessoa também pode baixar o bot no próprio Discord). | só DJ, admins, dono |
| `/ajuda` | Comandos e as regras deste bot. | todos |

A mensagem **Tocando agora** tem botões (pausar, pular, parar, repetir, fila) que seguem as mesmas
regras. **Quem está sozinho com o bot na chamada controla tudo.**

### Votação

Para quem não é DJ, as ações que atrapalham quem está ouvindo (pular a música dos outros, pausar,
parar, embaralhar, limpar, mover, repetir, efeitos) abrem uma **votação** com botão. Só vota quem
está **na mesma chamada** que o bot; cada um vota uma vez; vale por **1 minuto**. Precisa de
**mais que a porcentagem** escolhida: com 50%, 2 pessoas na chamada = as 2; 3 pessoas = 2;
4 pessoas = 3. Um DJ clicando em **Votar** aprova na hora. Votação de pular ou pausar some quando a
música troca.

## Limites (para não pesar no seu PC)

- Até **3 servidores** tocando ao mesmo tempo; fila de até **500** músicas por servidor.
- Cada pessoa: **15 comandos a cada 30 s** e **uma busca por vez**; o PC faz no máximo **4 buscas**
  ao mesmo tempo.
- O bot sai da chamada depois de **3 min sozinho** ou **5 min parado** sem nada na fila.
- Cinco músicas seguidas que não tocam: o bot para e avisa (normalmente yt-dlp ou ffmpeg velhos).

## Como funciona

```
Discord  <->  node bot.mjs (discord.js + @discordjs/voice)  <-- JSON por stdin/stdout -->  Remix
                        ^                                                                    |
                        '---- áudio Ogg/Opus: http://127.0.0.1:<porta aleatória>/<segredo>/<ticket>'
```

- O Remix grava o `bot.mjs` (embutido no programa) na pasta do bot a cada início e roda o Node.js.
  O bot é só uma ponte: recebe comandos, repassa ao Remix e mostra as respostas que o Remix manda.
- **Toda a lógica fica no Remix** (`comum/discord_host.h`): fila por servidor, votação, permissões,
  busca/links (os mesmos do app), playlists liberadas, efeitos (o mesmo grafo do ffmpeg do Host) e
  o áudio. O ffmpeg converte para **Opus 96 kbps, 48 kHz** em Ogg e o bot só repassa os pacotes
  para o Discord (sem codificar nada no Node).
- Cada música tocada ganha um **ticket de uso único** (vale 1 minuto) numa porta aleatória que só
  aceita conexões do próprio PC. Seek, efeitos e volume recomeçam o áudio do ponto certo.
- A criptografia ponta a ponta de voz que o Discord exige (DAVE) é feita pelo `@discordjs/voice`
  (com o `@snazzah/davey`).
- Pacotes: `discord.js` 14.27.0 e `@discordjs/voice` 0.19.2 (licença Apache-2.0), baixados do npm
  na instalação — não vêm dentro do Remix.

## Segurança

- **Token:** `discord.ini` na pasta do Remix. No Linux o arquivo tem permissão 600 (só o seu
  usuário lê); no Windows o token é protegido com a DPAPI do seu usuário. Vai para o bot só pelo
  stdin (nunca por argumento ou variável de ambiente) e é apagado do registro se aparecer.
  **APAGAR TOKEN** remove do PC. Se o token vazar: Developer Portal > Bot > **Reset Token**.
- **Links de quem usa o bot:** só de fontes conhecidas (YouTube, YouTube Music, SoundCloud, Spotify,
  Deezer, Apple Music, Bandcamp). Endereço qualquer (site, IP da sua rede) é recusado: o yt-dlp do
  seu PC não vira um "leitor de URL" para estranhos.
- **Arquivos do PC:** só os das playlists marcadas **NO BOT: SIM**. Nada fora delas aparece.
- As mensagens do bot **nunca mencionam ninguém** (um título com `@everyone` não pinga o servidor).
- O bot pede só as intents padrão (servidores e estados de voz): não lê mensagens nem a lista de
  membros.
- Aviso: tocar YouTube por bot pode ir contra os termos do YouTube (bots grandes já foram
  derrubados por isso). Use por sua conta; as playlists com arquivos do seu PC não têm esse problema.

## Rich Presence: aparecer no seu perfil (1.6)

Isso é **outra coisa** do bot: não entra em servidor nenhum, não precisa de token e não toca
nada. É só o seu perfil do Discord mostrando **"Ouvindo \<música\> — \<artista\>"** enquanto o
Remix toca, com a barrinha de tempo.

1. Em <https://discord.com/developers/applications>, **New Application** e dê o nome que você
   quer que apareça no perfil (por exemplo `Remix`). Se você já criou o app do bot, serve o mesmo.
2. Copie o **Application ID** (uns 18 números) da página **General Information**.
3. No Remix: **DISCORD** > **RICH PRESENCE: DEFINIR APPLICATION ID...**, cole e dê Enter.

Pronto. O campo vazio desliga. O ID fica no `config.ini` (`RpcAppId=`).

- Funciona no **Windows** (`\\.\pipe\discord-ipc-0`) e no **Linux em qualquer distro**: o Remix
  procura o soquete do Discord no `XDG_RUNTIME_DIR`, no `/tmp` e também nas pastas do **Flatpak**
  (`app/com.discordapp.Discord`) e do **Snap** (`snap.discord`), inclusive Canary e PTB.
- Só fala com o Discord que já está aberto no seu PC; nada vai para a internet. Com o Discord
  fechado, o Remix tenta de novo de tempos em tempos, sem incomodar.
- Para aparecer uma **imagem** junto, suba uma arte chamada `remix` em Rich Presence > Art Assets
  do seu app (o Remix já pede essa imagem pelo nome).

## Problemas comuns

- **"Token inválido"**: gere outro (Reset Token) e cole de novo.
- **"Node.js 22.12+ não encontrado"**: rode o instalador de dependências com a opção do Discord.
- **"Falta instalar o bot"**: **INSTALAR BOT** no painel (ou o instalador).
- **Os comandos não aparecem**: espere um minuto e reabra o Discord (Ctrl+R). Confira se o convite
  incluiu `applications.commands` (o **COPIAR CONVITE** do Remix já inclui).
- **"Não tenho permissão para entrar/falar"**: dê ao cargo do bot as permissões Conectar e Falar no
  canal de voz.
- **O registro** (fim do painel) mostra o que o bot está fazendo, sem o token.
- **Rich Presence não aparece**: confira se o Discord está aberto, se o Application ID é o do
  **seu** app e se o Discord está com "Exibir atividade atual como mensagem de status" ligado
  (Configurações > Atividade). O painel mostra `RICH PRESENCE: LIGADO ●` quando conectou.

## Testes (para quem mexe no código)

- `REMIX_DISCORD_BOT=<script.mjs>` troca o bot por outro script com o mesmo protocolo (um bot falso
  que simula pessoas, votos e canais). Ações de teste: `dc`, `dctoken:<t>`, `dc:on`, `dc:off`,
  `dcpl:<playlist>`, `dcplay:<faixa>`, `dcplpl:<playlist>`, `dccfg:fila0|fila1|pls0|pls1|on0|on1|vot<n>|lim<n>`,
  `dcdump`.
