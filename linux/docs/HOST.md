# Host — ouvir as músicas do PC no celular

O Remix do PC pode virar um **servidor para o celular**. O telefone abre um site (pela
rede local ou por um túnel Cloudflare), se vincula ao PC — escaneando um **QR code** ou
com o **PIN** — e passa a ouvir **só o que o PC liberar para aquele aparelho**: playlists
hosteadas, a biblioteca inteira (se você ligar) e músicas **online** (o PC busca e
converte; o celular só recebe o áudio). Cada celular tem as **playlists dele**, separadas
dos outros aparelhos.

Mudanças da **1.5.0** em relação à 1.4.0 no fim deste documento.

## Como usar (PC)

1. **Configurações > HOST** ou o botão **HOST** no cabeçalho (abre o painel).
   - **PIN**: 4 a 12 números. Obrigatório para ligar (é o caminho de quem não tem o QR).
   - **Porta**: padrão **49875** (TCP vai só até 65535). Troque se quiser.
   - **Nome**: como o PC aparece no celular.
   - **Túnel Cloudflare**: acesso pela internet (precisa do `cloudflared`; os instaladores de
     dependências baixam em `assets/tools`).
   - **Rede local**: acesso direto pelo mesmo roteador (Wi-Fi ou cabo).
   - **Online no celular**: deixa o celular buscar e ouvir online usando o yt-dlp/ffmpeg do PC.
   - **QR pede aceite**: por padrão o QR vincula direto; ligado, pede ACEITAR no PC também.
   - **IPv6**: desligado por padrão. Ligado, aceita IPv6 **só da rede local** (link-local,
     ULA ou o mesmo /64 do PC).
2. **LIGAR**. O painel mostra o **QR code**, os links e os botões **COPIAR LINK DO TÚNEL** e
   **COPIAR LINK LOCAL** (também nas configurações).
3. No celular, **escaneie o QR com a câmera**. A página pede **só um nome** e a opção
   "Lembrar este aparelho neste navegador". Pronto: o aparelho fica vinculado.
   - Sem o QR: abra o link, digite nome + **PIN** e aceite o pedido que aparece no PC.
   - O QR vale **10 minutos e uma vez só**; **NOVO QR** invalida o anterior. O código vai no
     fragmento da URL (`#q=...`), que o navegador nunca envia a servidor nenhum, e a página
     apaga o fragmento da barra de endereço assim que lê.
4. **Libere o que o aparelho pode ouvir** (por padrão ele não vê nada):
   - **BIBLIOTECA: SIM/NÃO** em cada aparelho do painel → a biblioteca inteira do PC.
   - Playlists do PC: botão direito na playlist > **Hostear no celular** (padrão: todos os
     aparelhos, inclusive os que forem vinculados depois), ou no painel escolha **quais**
     aparelhos (lista fixa: aparelho novo não entra sozinho).
   - Playlists dos celulares são **só do dono**. Se o dono tocar em "compartilhar com outros
     aparelhos", o painel mostra **LIBERAR** — só depois disso os outros aparelhos veem. Se o
     dono renomear ou adicionar faixa depois de liberada, ela sai dos outros até você liberar de
     novo. E a compartilhada só mostra o que o **dono** ainda pode ouvir: tirou a biblioteca
     dele, as faixas somem da compartilhada também.
5. **REMOVER** um aparelho revoga o acesso na hora: downloads e streams em andamento são
   cortados (o mesmo vale para BIBLIOTECA: NÃO, parar de hostear, ONLINE NO CELULAR desligado
   e desligar o Host). As playlists dele são apagadas.
   - Aparelho vinculado **sem "Lembrar"** aparece como *(temporário)*: fica só na memória
     (não vai para o `host.ini`), some ao fechar o Remix ou depois de 12 h sem uso. Os
     outros saem sozinhos depois de **180 dias** sem uso.
   - Vários pedidos de vínculo ao mesmo tempo entram em fila: o próximo aparece quando você
     responde o atual. Pedido com mais de 3 minutos expira (o PC avisa em vez de "aceito").
6. **HTML P/ WHATSAPP** grava `Remix-conectar.html` (só links; sem PIN, sem QR, sem IP
   público): abrindo no celular, ele acha o PC na rede local sozinho ou usa o túnel. Os links
   já vêm escritos no HTML, então aparecem até na pré-visualização do WhatsApp/Arquivos do
   iPhone, que não roda JavaScript.

Para "instalar" no celular: menu do navegador > **Adicionar à tela inicial** (a página é PWA).

## O celular (a página)

Interface no estilo dos apps de música: **Início** (atalhos das playlists liberadas e da
biblioteca, se liberada), **Buscar** (no PC ou online: YouTube Music, YouTube, SoundCloud),
**Sua Biblioteca** (playlists do PC, suas e compartilhadas), tela da **playlist** com voltar,
tocar e aleatório, e **Tocando agora** em tela cheia (capa, seek, aleatório/repetir,
adicionar a playlist). O botão Voltar do celular fecha as telas antes de sair.
Controles na tela de bloqueio (Media Session).

**iPhone (Safari e apps que usam o WebKit):** todos os botões dão retorno ao toque (o iOS só
aplica `:active` com um ouvinte de toque na página); a barra de posição aceita tocar ou arrastar
em qualquer ponto (o controle nativo só arrasta começando em cima do polegar); linhas de música
são dois botões lado a lado (nada de botão dentro de botão); folhas abrem o teclado no mesmo toque,
sobem acima dele e travam a rolagem de trás; toque duplo não aciona o que acabou de abrir; depois
de um erro o play recarrega a música (o WebKit deixa o áudio "sem pausa" e ignorava o play); a
tela bloqueada mostra anterior/próxima (e não ±10 s); campos sem autocorreção e sem zoom; se o
`app.js` não carregar, aparece "Toque aqui para recarregar" em vez da rodinha eterna; cookies
bloqueados geram uma explicação em vez de voltar ao PIN. A URL do `app.js`/`app.css` leva a versão
e um resumo do código (`?v=1.5.0-xxxxxxxx`), então o celular nunca fica com o arquivo velho do cache.

- Arquivos locais tocam direto do PC (com avanço/retrocesso por `Range`).
- Online: o PC resolve com o yt-dlp e converte ao vivo com o ffmpeg para MP3 192 kbps (ou
  AAC se o ffmpeg não tiver MP3). Avançar reabre o áudio do ponto escolhido. Playlists do PC
  que têm músicas online também tocam assim no celular. As capas online passam pelo PC:
  **o celular nunca fala com YouTube/SoundCloud**.
- **Efeitos e stems no celular** (botão **Efeitos e stems** no Tocando agora): os mesmos Slow, Speed, Reverb, Grave e
  8D do PC (3 níveis) e os modos de stem. Quem aplica é o PC com o ffmpeg (reverb por convolução, `bass`, `apulsator`,
  limitador), e o celular recebe um MP3 comum: a tela bloqueada do iPhone continua funcionando. Com efeito ou stem a
  música vem como stream (avançar recomeça com `?t=`), e o tempo mostrado considera a velocidade.
- **Onda no ritmo no celular:** o PC calcula energia e batidas a cada 25 ms (`/api/ritmo`) e a página desenha a onda
  do Tocando agora e faz os ícones de "tocando" pularem na batida — sem Web Audio (que pararia com a tela bloqueada).
- Começo rápido: a extração do yt-dlp (~3 s) fica em cache por 25 min, compartilhada com o
  player e os downloads do PC, e os 3 primeiros resultados de cada busca já são extraídos em
  segundo plano. Medido: o primeiro áudio chega em ~0,3–0,5 s (antes ~3 s).

## Segurança e privacidade

| Risco | Como é tratado |
|---|---|
| Alguém na internet acessar a porta direto | O servidor só conversa com **loopback** (o túnel chega por 127.0.0.1) e com a **rede local** (IPv4 privado; IPv6 só se ligado e só local). Qualquer outra origem é derrubada antes de ler o pedido — vale mesmo se o PC tiver IP público. |
| Aparelho vinculado ver o que não devia | Autorização **por aparelho e por item**: biblioteca só com BIBLIOTECA ligado; playlists do PC só se hosteadas para ele; playlists de outro aparelho só se o dono compartilhar **e** o PC liberar; online só o que ele buscou ou está em playlist que ele vê. Vale para listar, tocar, baixar capa e adicionar em playlist. Item não autorizado responde 404 (nem confirma que existe). |
| Adivinhar PIN / QR | 5 erros travam a origem por 60 s, dobrando até 16 min; **30 erros em 10 min de qualquer origem travam o PIN para todos por 10 min** (trocar de IP não ajuda; o QR continua funcionando). Comparação em tempo constante. QR: 128 bits, uso único, 10 min. PIN ainda exige ACEITAR no PC. |
| Token roubado | 256 bits (CSPRNG do sistema) em cookie `HttpOnly; SameSite=Strict` (`Secure` no túnel). "Lembrar" desligado = cookie de sessão **e** aparelho temporário no servidor (só na memória, 12 h sem uso). Os outros expiram com 180 dias sem uso. `host.ini` com permissão 600 no Linux. REMOVER revoga e corta o que estiver tocando. |
| Site de fora usando o navegador do celular/PC ("DNS rebinding") | O servidor só aceita `Host` com IP, `localhost`, nome da máquina na rede (`.local`, `.lan`, sem domínio) e o link do túnel; qualquer outro nome recebe 421. `/api/ping` para outra origem responde só `{"app":"remix"}` (sem nome do PC nem versão). |
| CSRF | Todo POST exige `X-Remix: 1` + cookie SameSite=Strict. |
| XSS | CSP `script-src 'self'` sem inline, nada de `innerHTML` com dados, JSON escapa `< > &`, `X-Frame-Options: DENY`, `nosniff`. |
| Injeção (SQL/comando) | Não há SQL. Processos (yt-dlp/ffmpeg) sem shell, argumentos em vetor; todo link vai depois de `--` (nunca vira opção do yt-dlp) e só link `http(s)` toca; a busca vai depois de `ytsearch:`/URL codificada; o celular nunca manda URL — só ids que o PC gerou. ffmpeg com `-protocol_whitelist` (nada de arquivo local). Miniaturas: só https, sem usuário/senha/porta/IP no link, só CDNs conhecidos, **sem seguir redirecionamento**, e só imagem de verdade (JPEG/PNG/WebP/GIF pelos bytes) chega ao ffmpeg, com o formato fixo. |
| Cabeçalho malicioso | Nome de cabeçalho fora do padrão (ex.: `Transfer-Encoding :`), `Content-Length`/`Host` repetidos e linhas sem `:` são recusados (400); `Transfer-Encoding` é recusado. O IP do túnel (`CF-Connecting-IP`) só é aceito com o túnel ligado, em conexão local e se for um IP válido. |
| Path traversal | Só ids opacos (hash); `..`, `%`, `\` e caracteres de controle na URL são recusados. |
| DoS | 90 pedidos/10 s por origem e 600 só para aparelho com token **válido** (IPv6 contado por /64); 64 conexões, no máximo 12 por origem da rede local; o pedido inteiro tem 15 s para chegar (quem manda um byte por vez é derrubado); cabeçalho 16 KB, corpo 64 KB; 1 busca online por aparelho (3 no total, cancelada se o celular desistir); 1 stream por aparelho (4 no total; trocar de faixa não conta a antiga). As respostas são montadas com a trava e enviadas sem ela (celular lento não congela o PC). |
| Privacidade | Nada de rastreamento: o celular só guarda um código aleatório (e só se "lembrar" estiver ligado); nenhum dado do aparelho é lido. Pelo túnel: HTTPS, sem porta aberta, sem IP do PC. |

**Rede local = HTTP.** Quem está no mesmo roteador pode ver o tráfego. Para cifrar mesmo em
casa, desligue REDE LOCAL e use só o túnel.

## Túnel Cloudflare e o erro de DNS

O `cloudflared` mostra o link `https://…trycloudflare.com` **antes** de o nome existir no
DNS (medido: 1 a 2 s de NXDOMAIN). Quem abre nessa janela faz o roteador guardar "esse site
não existe" por até **30 minutos** (SOA mínimo do trycloudflare.com) — era o
`DNS_PROBE_POSSIBLE` no Wi-Fi que funcionava nos dados móveis.

Desde a 1.5.0 o Remix espera o túnel registrar a conexão, espera mais 8 s, **testa o link
de verdade** e só então mostra, copia e põe no QR ("TESTANDO O LINK..." enquanto isso).
Se um roteador já tiver guardado o erro, **NOVO LINK** gera outro nome na hora.

O link do quick tunnel muda cada vez que o túnel sobe; o cookie de um aparelho vale para o
endereço em que ele se vinculou (rede local é estável). Para um endereço fixo pela internet,
use um túnel nomeado na conta Cloudflare apontando para `http://127.0.0.1:<porta>`.

## API (JSON)

Todo POST: `X-Remix: 1`, corpo JSON. 401 = não vinculado; 429 = calma; erros `{"erro":"…"}`.
Faixa: `{id, t, a, d, c (tem capa), o (1 = online)}`.

| Rota | Auth | O que faz |
|---|---|---|
| `GET /api/ping` | não | `{app, v, nome}`; de outra origem (CORS) só `{app}` |
| `POST /api/parear {pin, nome, lembrar}` | não | pedido; o PC recebe ACEITAR/RECUSAR |
| `GET /api/parear/estado?req=` | não | `pendente` / `aceito` (entrega o cookie uma vez) / `recusado` / `expirado` |
| `POST /api/parear/qr {token, nome, lembrar}` | não | `aceito` (cookie) ou `pendente` + `req` se QR pede aceite |
| `GET /api/estado` | cookie | `{host, dispositivo, id, v, biblioteca, online, faixas, temporario}` |
| `GET /api/biblioteca` | cookie | faixas da biblioteca (vazio sem liberação) |
| `GET /api/playlists` | cookie | `{pc, minhas (compartilhar, liberada), compartilhadas (dono)}` com `itens` já filtrados |
| `POST /api/minhas {acao, slug, nome, id, valor}` | cookie | `criar`, `renomear`, `apagar`, `add`, `remover`, `compartilhar` |
| `GET /api/faixa/<id>` | cookie | arquivo local com `Range`; com `?fx=slow:2,reverb:1&stem=vocal&t=<s>` vem convertido (stream) |
| `GET /api/capa/<id>` | cookie | capa local ou miniatura online (via PC) |
| `GET /api/online/buscar?q=&fonte=0\|1\|2` | cookie | busca pelo yt-dlp do PC |
| `GET /api/online/ouvir/<id>?t=<s>` | cookie | stream MP3/AAC convertido ao vivo (aceita `fx` e `stem`) |
| `GET/POST /api/stems/<id>` | cookie | estado da separação (`pronto`, `fila`, `baixando`, `separando`, `falhou`); POST começa (no máximo 4 na fila) |
| `GET /api/ritmo/<id>?stem=` | cookie | `{hop:25, e:[energia 0..255], b:[batida 0..255]}` (2 análises por vez) |
| `POST /api/sair` | cookie | desvincula este aparelho |

### Testes sem celular

```bash
build/remix --home <pasta> --no-splash --after 500:hostpin:2468 --after 800:host:on --after 1500:hostqr --exit-after 60000 &
curl -s http://127.0.0.1:49875/api/ping
curl -s -c ck -X POST http://127.0.0.1:49875/api/parear/qr -H 'X-Remix: 1' -d '{"token":"<do log: qr: ...#q=>","nome":"teste"}'
curl -s -b ck http://127.0.0.1:49875/api/estado
```

Ações de teste: `hostpin:`, `hostport:`, `host:on|off`, `hostok`, `hostno`, `hostpanel`,
`hosthtml`, `hostqr` (imprime o link do QR), `hostlib:<aparelho>:<0|1>`,
`hostplall:<playlist>`, `hostdplok:<i>`, `hostonline:<0|1>`, `tunnel:on|off`; `dump` mostra
o estado do host.

## Por dentro

- `comum/host_net.h` — sockets IPv4/IPv6 (Winsock2/POSIX), `PeerAllowed` (filtro de origem),
  `WaitAccept` (poll nos dois sockets), endereços locais (cabo/Wi-Fi; docker/VM/VPN fora).
- `comum/host_server.h` — rotas, vínculo (PIN/QR), autorização (`CanSee`), registro de
  músicas online, streaming (`ServeOnlineStream`), `host.ini`, túnel (`VerifyTunnel`),
  estado do painel.
- `comum/host_web.h` — a página do celular (HTML/CSS/JS embutidos) e o `Remix-conectar.html`.
- `comum/qrcode.h` — gerador de QR code sem dependências (verificado com zxing-cpp).
- Núcleo/UI: `[Host]` no config.ini (`HostOn, HostPort, HostPin, HostTunnel, HostLan,
  HostName, HostOnline, HostQrConfirm, HostIPv6`), seção HOST das configurações, painel
  (`LayoutHostPanel`/`DrawHostPanel` nas duas cascas), copiar link
  (`PlatformSetClipboardText`).

## O que mudou na 1.6

- **Letra da música no celular**: no player, botão **Letra**. O PC procura no LRCLIB (banco
  público, sem conta) e guarda no disco para sempre; quando existe versão sincronizada, a linha
  atual fica em destaque e tocar numa linha pula para aquele ponto (`POST /api/letra`).
- **Procurar playlists e álbuns prontos**: na aba Buscar > Online, as opções **Playlists** e
  **Álbuns** procuram no catálogo público e abrem a lista ali mesmo, com Tocar tudo e Salvar como
  playlist (`POST /api/online/listas`). Não precisa mais sair do app para achar uma playlist.

- **Início com novidades**: o celular mostra as mesmas fileiras do PC (o melhor do artista que
  você ouve, parecidos, álbuns recentes, bombando, playlists da semana, álbuns e artistas em
  alta), abaixo da biblioteca e das playlists. Detalhes em [DESCOBRIR.md](DESCOBRIR.md).
  - `GET /api/descobrir`: as fileiras já montadas (só com o online ligado).
  - `GET /api/desccapa/<id>`: a capa. O PC baixa e guarda; o celular nunca fala com a internet
    direto (o CSP da página só deixa carregar imagem do próprio PC).
  - Tocar um cartão de música resolve o link no PC e põe na fila; álbum, playlist ou artista
    abre na aba Buscar, com TOCAR TUDO e SALVAR COMO PLAYLIST.

## O que mudou na 1.5.6

- **Corrigido: a música voltava para o começo no meio do streaming** (com o relógio parado no tempo
  antigo), principalmente no iPhone — e também em música baixada **com efeito ligado**, porque aí ela
  passa pelo mesmo caminho de conversão. O stream era mandado **sem tamanho e sem "Range"**: quando o
  Safari precisava pedir o áudio de novo (engasgo de rede, fim de um pedaço, volta do bloqueio) ele
  recebia o arquivo desde o primeiro byte, mas o contador continuava de onde estava. Agora, quando o PC
  sabe a duração e o ffmpeg tem o mp3 (192 kbps, taxa fixa = 24000 bytes por segundo), o Host manda
  `Content-Length` e aceita `Range: bytes=N-`: o byte pedido vira tempo (`N/24000 × velocidade do efeito`)
  e o ffmpeg recomeça exatamente ali. O celular passa a tratar como arquivo normal.
- Com isso: **arrastar a barra é instantâneo** (o navegador pula sozinho, sem pedir a música inteira de
  novo), a **tela bloqueada do iPhone** mostra a duração certa e os controles de faixa, e a duração de
  arquivos sem tag aparece (o PC mede o arquivo e guarda em cache).
- Pedido curto com fim definido (`bytes=a-b`, o navegador só espiando o cabeçalho ou o fim) **não derruba
  mais** o áudio que está tocando naquele aparelho.
- **Fila adiantada:** a página avisa o PC quais são as **próximas 2 músicas online** (`POST /api/preparar`)
  e ele já extrai o link (cache de 25 min): trocar de faixa deixa de esperar os ~3 s do yt-dlp.
- **Efeitos sem sustos:** vários toques seguidos viram uma recarga só (350 ms), e se o áudio falhar logo
  depois de uma troca de efeito/stem ou de um avanço, a página **tenta a mesma música de novo** em vez de
  pular para a seguinte.
- **Volta onde parou:** a fila e o ponto ficam guardados no aparelho; se a página recarregar (o iOS
  descarrega a aba quando falta memória), ela volta na mesma música, pausada.
- **Arrastar o mini player** (a barrinha de baixo) para a esquerda passa para a próxima e para a direita
  volta — agora na barra inteira, com o dedo arrastando junto (antes só a parte do título respondia).
- **Playlist de outras plataformas pelo celular:** cole um link de playlist ou álbum (Spotify, YouTube,
  YouTube Music, Deezer, Apple Music, SoundCloud, Bandcamp) na busca online — o botão vira **ABRIR LINK**,
  o PC resolve com os mesmos mecanismos do app (`POST /api/online/link`) e o celular mostra as músicas com
  **TOCAR TUDO** e **SALVAR COMO PLAYLIST** (playlist do próprio aparelho, criada com todas de uma vez pela
  ação `addvarios` de `/api/minhas`). Link de fora dessas fontes é recusado (o yt-dlp do PC não vira leitor
  de URL qualquer).
- **O vínculo agora mora no PC, não no navegador.** Cada aparelho ganha uma **chave permanente** (32
  caracteres aleatórios, guardada em `host.ini` ao lado do token). O cookie continua sendo o atalho do
  dia a dia, mas ele vale só naquele endereço: trocar o link do túnel, abrir pela rede local ou instalar
  na tela de início fazia o celular aparecer como desconhecido de novo. Com a chave isso acabou:
  - no painel HOST, cada aparelho tem o botão **LINK**: ele copia `<endereço>/#a=<chave>` e joga esse
    mesmo link no QR grande ("QR para religar"). **NOVO QR** volta o quadrado para o QR normal de vincular;
  - no celular, **Aparelho > Copiar link deste aparelho** dá o mesmo link;
  - abrindo esse link em **qualquer** endereço do PC, o aparelho entra direto, sem PIN e sem aparecer
    pedido novo no PC (`POST /api/entrar {chave}`, com a mesma trava de tentativas do PIN);
  - a página guarda a chave no aparelho e, se o cookie sumir sozinho (o Safari limpa dados de sites que
    você não abre há um tempo), ela **religa sem você perceber** em vez de voltar para a tela de PIN;
  - aparelhos vinculados antes da 1.5.6 ganham a chave na primeira vez que o Host liga, sem precisar
    vincular de novo. **REMOVER** continua apagando tudo: aí a chave antiga não vale mais.

## O que mudou na 1.5.2

- Efeitos (Slow, Speed, Reverb, Grave, 8D) e stems (Demucs) também no celular, aplicados pelo PC; onda no ritmo real
  calculada pelo PC; rotas `/api/stems` e `/api/ritmo`.

## O que mudou na 1.5.1

- Playlist hosteada com **pasta vinculada**: o celular recebe também as músicas da pasta (antes
  só as adicionadas à mão), com os títulos que o PC já leu, e vê em poucos segundos quando uma
  música entra ou sai da pasta.

## O que mudou na 1.5.0

- **Corrigido:** os botões do Host usavam os mesmos códigos internos do seletor de estilo
  (clicar em LIGAR O HOST trocava para "Spotify + LED"; o botão HOST do cabeçalho trocava para
  "Limpo"). As zonas do Host mudaram de faixa e o build agora falha se duas faixas se sobrepuserem.
- **Corrigido:** link do túnel com erro de DNS (ver acima) e sem como copiar.
- **Novo:** QR code para vincular sem PIN; autorização por aparelho (nada liberado por padrão);
  playlists isoladas por aparelho com compartilhamento liberado pelo PC; busca e streaming
  online no celular; nova interface do celular; filtro de origem (só loopback e rede local);
  IPv6 opcional só na rede local; opções ONLINE NO CELULAR e QR PEDE ACEITE.
- **Segurança (revisão da 1.5.0):** aparelho sem "Lembrar" vira temporário e ninguém fica
  vinculado para sempre; revogar/desligar corta downloads e streams na hora; playlist
  compartilhada não vira porta dos fundos; renomear/editar faixa republica para o celular;
  "Hostear no celular" com o Host desligado não apaga mais os aparelhos do `host.ini`; links do
  yt-dlp com `--`; capas sem SSRF; defesa contra DNS rebinding, slowloris e cabeçalhos
  ambíguos; trava geral de PIN; pedidos de vínculo em fila.
