# Segurança de arquivos — plano "só o reproduzível + a capa"

> **Status: PLANO.** Nada daqui está implementado. Descreve o código da **1.4.0**
> (commit `81d48fc`) e propõe como proteger quem usa o Remix contra arquivos de
> música maliciosos ou corrompidos — os que já estão no PC, os que chegam pelos
> downloads online, as capas e o que o Host manda para o celular.
>
> Ideia do dono: **extrair só o que toca e a capa, e ignorar o resto**. Só que isso
> mexe no caminho de todas as músicas, então vai por fases, com uma chave que começa
> **desligada**, e com volta para o comportamento de hoje se algo der errado.

**Resumo em cinco linhas**

1. Hoje todos os decodificadores (dr_mp3, dr_flac, dr_wav, stb_vorbis, stb_image, GDI+)
   rodam **dentro do processo da interface**. Se um arquivo explorar um bug de memória
   neles, o código roda com as permissões do usuário, ou o app fecha, às vezes toda vez que abre.
2. Parte do caminho já está certa: os formatos "extras" e o streaming já chegam ao
   player como **PCM gerado pelo ffmpeg em outro processo**. Falta generalizar isso, limitar
   tempo e memória, e isolar esse processo.
3. Proposta: um **trabalhador** (o próprio `remix`/`Remix.exe` em modo `--higienizar`,
   ou o ffmpeg) roda isolado, identifica o arquivo **pelo conteúdo**, entrega só
   **áudio** (PCM, ou um remux limpo) e **uma capa JPEG nova de até 512 px**, e joga fora o resto.
4. O original **nunca é alterado**. A cópia limpa fica num cache indexado por SHA-256.
   O que falha vai para a **quarentena** (só um registro, sem copiar o arquivo).
5. O player, as miniaturas e o Host passam a abrir **só** o que saiu do trabalhador.
   Os identificadores continuam sendo o caminho original, então playlists,
   `covers.ini`, `artists.ini` e os ids do Host não mudam.

---

## Sumário

1. [Modelo de ameaça](#1-modelo-de-ameaça)
2. [O que já protege e o que não protege](#2-o-que-já-protege-e-o-que-não-protege)
3. [A proposta: só o reproduzível + a capa](#3-a-proposta-só-o-reproduzível--a-capa)
4. [Isolamento do processo que toca o arquivo não confiável](#4-isolamento-do-processo-que-toca-o-arquivo-não-confiável)
5. [Riscos de quebrar o app e como mitigar](#5-riscos-de-quebrar-o-app-e-como-mitigar)
6. [Plano de testes](#6-plano-de-testes)
7. [Fases de implementação](#7-fases-de-implementação)
8. [Perguntas em aberto para o dono](#8-perguntas-em-aberto-para-o-dono)
9. [Apêndice A — endurecimentos baratos que não dependem do plano](#apêndice-a--endurecimentos-baratos-que-não-dependem-do-plano)

Vocabulário usado aqui:

- **original**: o arquivo do usuário (biblioteca, pasta vinculada, download, capa da pasta).
- **trabalhador**: processo separado e limitado que é o único a ler bytes não confiáveis.
- **cópia limpa**: o que o trabalhador produz (áudio + capa + um manifesto pequeno).
- **higienizar**: produzir a cópia limpa.
- **quarentena**: registro dos arquivos que o trabalhador recusou ou que o derrubaram.

---

## 1. Modelo de ameaça

### 1.1 O que proteger

| Bem | Por que importa no Remix |
|---|---|
| **Execução de código no PC** | O Remix roda como o usuário: lê a pasta pessoal, o `host.ini` (com os tokens dos celulares), cookies de navegador etc. |
| **Disponibilidade** | Um arquivo que derruba o app na abertura vira **laço de crash**. No Windows o modo seguro (`win_diag.h`) desliga splash, efeitos, bandeja e atalhos, mas **não** desliga capas nem a leitura de tags. E `main.cpp` ainda chama `PlayIndex(g_current,false)` na abertura, o que roda `ProbeNative`, `Open`, `AnalyzeCurrentWave` e `PlatformLoadCover` na última faixa. A grade de capas também decodifica imagens logo que abre. |
| **Os arquivos do usuário** | O app nunca deve alterar nem apagar música por causa de um arquivo hostil (renomear e lixeira só por ação do usuário). |
| **O celular (Host)** | `/api/faixa/<id>` manda o arquivo **byte a byte** para o navegador do celular, que tem os próprios decodificadores. |
| **O disco e a memória** | Conversões sem limite (`TranscodeToWav`) e alocações pedidas pelo cabeçalho podem encher o disco ou esgotar a RAM. |

### 1.2 Por onde entra um arquivo hostil

| Entrada | Caminho no código | Quem lê os bytes hoje (processo) |
|---|---|---|
| Pasta de música / varredura "padrão" (Downloads, Documentos, Área de trabalho, pendrive) | `ScanFolder`, `ScanComputerMusic`, `WalkFiles` (`playlist.h`) | `ReadTags` → `ReadId3Tags`/`ReadFlacTags`/`ReadOggTags`/`ReadMp4Tags` (**UI/varredura**) |
| Capa embutida | `art::Lookup` → thread `art::Work` (`cover_art.h`) | `FromId3`, `FromPictureBlock`, `FromOgg`, `FindCovr` + `ShrinkCoverInto` (**UI, thread própria**) |
| Imagem solta na pasta | `FindCoverInFolder` (`playlist.h`) | stb_image via `gfx::LoadImg` (Linux) / GDI+ `GetThumb`, `PlatformLoadCover` (Windows) (**UI**) |
| Tocar | `PlayIndex` → `Player::ProbeNative`, `Player::Open`, `AnalyzeCurrentWave` → `Player::DecodeMono` | miniaudio + dr_libs + stb_vorbis (**UI**, três aberturas do mesmo arquivo) |
| Formato "extra" (m4a, opus, wma, ape…) | `PlatformTranscodeToWav` (`sys_linux.h`/`win_sys.h`) | **ffmpeg (outro processo)** → WAV → miniaudio |
| `--play-file`, IPC, arrastar | `PlayFileDirect` (`app_core.h`) | igual a "Tocar" |
| Arquivos escolhidos para uma playlist | `OnPickedFiles` → `PlaylistAddFile` (`app_online_b.h`, `app_playlists.h`) | igual a "Tocar". **Aceita qualquer arquivo regular** (o diálogo tem o filtro "Todos") |
| `playlist.json` recebido de alguém | `LoadPlaylistFile` (`app_playlists.h`) | `JParser` (recursivo), `Config::FromPortable` em `path`/`folder` |
| Download online | `RunDownload` (`online_play.h`) | yt-dlp + ffmpeg (outros processos). O arquivo final depois é lido como "Tocar" e "Capa embutida" |
| Streaming online | `StreamThread` (`online_play.h`) | yt-dlp + **ffmpeg → PCM s16le** (o player só vê PCM) |
| Miniatura online (YouTube/Deezer/Apple/Spotify) | `FetchThumb` (`online_resolve.h`) | ffmpeg (outro processo). **Sem ffmpeg**: `ShrinkCoverInto` no processo da UI |
| Busca de capa na internet | `WebSearchAsync`/`WebDownloadSelectedAsync` (`linux/app_web.h`, `windows/win_web.h`) | `LoadImageFromMemory` (stb_image) / `Gdiplus::Image` (**UI, threads**) |
| Papel de parede | `g_cfg.bgWallpaper` → `gfx::LoadImg` / `GetWallpaperImage` | stb_image / GDI+ (**UI**) |
| Host → celular | `host::Serve` → `SendFile` (`host_server.h`) | navegador do celular |

### 1.3 Classes de falha, com exemplos reais

> Os CVEs abaixo são **exemplos da classe**, para pesquisa. Não são receita, e nem todos
> afetam as versões que o Remix usa. Confira o número no NVD antes de citar em outro lugar.
> Versões no repositório: miniaudio **0.11.21** (dr_mp3/dr_flac/dr_wav embutidos),
> stb_vorbis **1.22**, stb_image **2.30** (dentro do raylib 5.5).

| Classe | Exemplos conhecidos | Onde bate no Remix |
|---|---|---|
| **Bug de memória em decodificador de áudio** | CVE-2019-13217…13223 (estouros no stb_vorbis, achados por fuzzing); lote do GitHub Security Lab de 2023 contra o stb_vorbis 1.22 (a mesma versão de `comum/audio/stb_vorbis.c`); CVE-2018-5146 (libvorbis, usado no Pwn2Own 2018 contra o Firefox); CVE-2020-0499 (libFLAC, leitura fora dos limites); CVE-2015-1538 "Stagefright" (MP4 no Android) | stb_vorbis e dr_libs no processo da UI; `DecodeMono` decodifica o arquivo **inteiro** em segundo plano assim que a faixa abre; o navegador do celular pelo Host |
| **Bug em decodificador de imagem** | CVE-2023-4863 (libwebp, explorado em ataques reais); CVE-2004-0200 / MS04-028 (JPEG no GDI+) e o boletim MS08-052 (EMF/WMF/GIF no GDI+); CVE-2021-28021 e a série CVE-2023-45661…45667 (stb_image) | stb_image (Linux: PNG, JPG, BMP e GIF ligados em `build-raylib.sh`); GDI+ no Windows; ffmpeg nas miniaturas |
| **Arquivo poliglota** | GIFAR (GIF que também é JAR); MP3 válido com ZIP, HTML ou executável colado no fim; ImageTragick CVE-2016-3714 (o formato decidido pelo conteúdo, não pelo nome) | dr_mp3 procura sincronismo e aceita lixo antes e depois; o Host manda o arquivo inteiro (o `nosniff` ajuda, mas o conteúdo segue lá) |
| **Extensão mentirosa** | `.jpg` que é EMF, GIF ou WebP; `.mp3` que é Ogg; miniatura WebP "chamada" `.jpg` (o YouTube faz isso) | `ma_decoder_init_file` tenta a extensão e depois **os quatro** decodificadores por tentativa; o GDI+ identifica pelo conteúdo e processa EMF/WMF/TIFF; raylib escolhe pela extensão, e o stb_image confere o conteúdo |
| **Cabeçalho com tamanho gigante** | Contagens e tamanhos que viram alocação ou laço (padrão clássico de estouro de inteiro, como no Stagefright) | ID3 declarando ~256 MB, WAV com `data` = 0xFFFFFFFF, átomo MP4 de 64 bits, STREAMINFO do FLAC com taxa ou total absurdos |
| **Metadados enormes** | APIC de dezenas de MB, milhares de comentários Vorbis, letra gigante; texto com caracteres de controle ou de direção (U+202E) | `ReadId3Tags` aloca o tamanho declarado **antes** de ler; `art::MAXB` = 64 MB por capa; título e artista vão para a UI e para o celular |
| **Bomba de descompressão** | PNG ou JPEG de poucos KB que declara 20000×20000 px; áudio curto que "declara" horas | `gfx::LoadImg` na thread da UI (um PNG de 16k×16k vira ~1 GB RGBA; sem `STBI_MAX_DIMENSIONS`); `TranscodeToWav` sem `-fs` pode gerar WAV de GB |
| **Nome de arquivo e traversal** | yt-dlp CVE-2024-38519 (extensão não validada no download, corrigido na 2024.07.01); "zip slip" como classe; nomes com U+202E, `CON`, `NUL`, `-` no começo | `-o audio.%(ext)s` no download; `pasta_temp` no diário (`OnlineStartupCleanup`); `path` relativo com `..` em `playlist.json`; `SafeFileName` |
| **Links simbólicos e arquivos especiais** | link para um arquivo sensível; FIFO ou dispositivo com nome de música ou de capa | `WalkFiles` pula links, mas `FindCoverInFolder`, `PlaylistTracks`, `PlayFileDirect` e `OnPickedFiles` usam `exists`/`is_regular_file`, que **seguem** links; um FIFO chamado `cover.jpg` trava o `fopen` |
| **Download com formato inesperado** | `-f bestaudio/best` pode trazer vídeo; site genérico entrega HLS/DASH; ffmpeg lendo arquivo local por playlist HLS (CVE-2016-1897/1898, CVE-2017-9993) | `StreamThread` passa `mi.url` do JSON do yt-dlp direto para `-i`; `FetchThumb` passa um arquivo baixado de URL arbitrária para `ffmpeg -i` sem forçar o formato |
| **Travamento (DoS de tempo)** | Arquivo que prende o decodificador em laço | `TranscodeToWav` espera para sempre (`WaitForSingleObject(INFINITE)` / `system()`); `DecodeMono` sem limite de tempo |

### 1.4 Quem é o atacante, e o que fica fora do modelo

Dentro do modelo:

- quem distribui **música** (torrent, grupo de WhatsApp, pendrive emprestado, pasta compartilhada);
- um **site** que o extrator genérico do yt-dlp abre, ou um CDN de capa comprometido
  (na busca de capa, a `murl` é a imagem do site original, não do Bing);
- quem manda um **link** ou um **`playlist.json`**/pasta de playlist;
- um **celular pareado** de outra pessoa (o Host permite vários aparelhos).

Fora do modelo (outros documentos, se for o caso):

- malware que já roda como o usuário, ou alguém com escrita na pasta do app (pode trocar o `.exe`);
- cadeia de suprimentos do próprio yt-dlp, ffmpeg, cloudflared ou deno (versão e origem dos binários);
- sequestro de `PATH` no Linux (o app procura as ferramentas no `PATH`; no Windows só em `assets\tools`).

---

## 2. O que já protege e o que não protege

### 2.1 Já protege

| Onde | O que faz |
|---|---|
| `WalkFiles` (`playlist.h`) | Usa `symlink_status` e só visita arquivo **regular**; pula links, junctions/reparse points, pastas ocultas (Linux), `node_modules`, pastas do sistema e do Steam. |
| `art::*` (`cover_art.h`) | Limites: `MAXB` = 64 MB, FLAC até 128 blocos, Ogg até 1024 páginas e 4 pacotes, MP4 até 6 níveis e 256 átomos no topo; confere `pos + tamanho > total` antes de copiar; pula quadros ID3 comprimidos ou criptografados; `IsImage` exige JPEG ou PNG antes de gravar. |
| `ReadFlacTags` / `ReadOggTags` / `ReadMp4Tags` (`playlist.h`) | Tetos de 8 MB, 16 MB e 16 MB; laços com `guard`. Numa leitura rápida não achei estouro óbvio nesses parsers próprios. O risco maior está nos decodificadores de terceiros e nos limites de memória (2.2). |
| `ShrinkCoverInto` (`playlist.h` no Windows, `main_linux.cpp` no Linux) | Capa **maior** que 512 px (`COVER_MAX_DIM`) é reencodada. |
| `PlatformTranscodeToWav` (`sys_linux.h`, `win_sys.h`) | Formatos extras são decodificados pelo **ffmpeg em outro processo**, com `-vn -map_metadata -1`, e o player só abre um WAV PCM. É o "só o reproduzível" em miniatura. |
| `StreamThread` (`online_play.h`) | Streaming: ffmpeg → `-f s16le -ac 2 -ar 48000 pipe:1` → `PcmStream`. **Nenhum contêiner remoto** passa pelo miniaudio. |
| `FetchThumb` (`online_resolve.h`) | Com ffmpeg, a miniatura vira JPEG quadrado de até 512 px, com timeout de 30 s. |
| `app_proc.h` (`Proc`, `RunCapture`) | Sem shell, argumentos em vetor (`QuoteArgW` correto); Windows: Job Object `KILL_ON_JOB_CLOSE` e herança só dos pipes (`PROC_THREAD_ATTRIBUTE_HANDLE_LIST`); Linux: `posix_spawn` num grupo próprio e pipes `O_CLOEXEC`; `RunCapture` com timeout, cancelamento e teto de 96 MB na saída. |
| Download (`RunDownload`, `SafeFileName`) | yt-dlp com `--ignore-config`; saída numa pasta temporária `job-<pid>-<n>`; só move arquivo com extensão de áudio (`LooseAudioExt`); nome final limpo (barras, `:*?"<>` e a barra vertical, controles, 140 caracteres, pontos no fim). |
| HTTP (`sys::HttpGet`, `HttpGetBytes`) | Teto de 40 MB e timeouts. |
| Host (`host_server.h`, `host_web.h`) | Só ids do mapa, `..` rejeitado, `X-Content-Type-Options: nosniff`, CSP sem script inline, a página usa `textContent`. Um título malicioso **não** vira XSS no celular. |
| Windows (`win_diag.h`) | Log de crash e modo seguro depois de uma abertura que não terminou. |

### 2.2 Não protege

1. **Decodificação no processo da UI.** `Player::ProbeNative`, `Player::Open` e
   `Player::DecodeMono` (`player_ma.h`) abrem o original três vezes, a última
   decodificando o arquivo inteiro para a onda e o espectro. Qualquer bug de memória em dr_mp3,
   dr_flac, dr_wav ou stb_vorbis acontece no processo que tem a janela, o Host e os tokens.
2. **Tentativa e erro no miniaudio.** `ma_decoder_init_file` sem `encodingFormat` tenta a
   extensão e depois WAV, FLAC, MP3 e Vorbis. Uma extensão mentirosa expõe os quatro parsers, e o dr_mp3
   aceita quase qualquer coisa como MP3.
3. **Imagens no processo da UI.** Linux: `gfx::LoadImg` → raylib `LoadImage` → stb_image
   sem `STBI_MAX_DIMENSIONS` (o limite padrão é 2^24 por lado). Windows: `GetThumb` (`win_draw.h`),
   `PlatformLoadCover` (`main.cpp`) e `GetWallpaperImage` usam `Gdiplus::Image(path)`, que identifica pelo
   conteúdo (EMF/WMF/TIFF/GIF com nome `.jpg`), sem limite de dimensão e na thread de desenho.
4. **Capa pequena fica como veio.** Quando a imagem tem até 512 px, `ShrinkCoverInto` devolve vazio,
   `art::Work` renomeia o `.raw.jpg`/`.raw.png` e `EnsureInternalCover` faz `copy_file`. Os
   bytes originais vão para o cache e são decodificados cada vez que a capa aparece na grade.
   Uma "imagem" com magic de JPEG que falha no decodificador também fica gravada.
5. **Imagem da pasta sem filtro.** `FindCoverInFolder` aceita qualquer `.jpg/.jpeg/.png/.bmp/.webp`
   e não confere o tipo de arquivo (FIFO ou link).
6. **Alocação pedida pelo arquivo.** `ReadId3Tags` faz `std::vector<BYTE> tag(tagSize)` com até ~256 MB
   **antes** de ler: um arquivo de 10 bytes pede 256 MB. `art::FromMp3`/`ReadN` fazem o mesmo até 64 MB.
7. **ffmpeg sem limites e sem lista de permitidos.** `TranscodeToWav` não tem timeout (Linux usa `system()`
   com `ffmpeg` do `PATH`; Windows usa `CreateProcessW` + `INFINITE`, fora do `Proc`/Job), nem `-fs`,
   `-protocol_whitelist` ou `-format_whitelist`. `FetchThumb` entrega a `ffmpeg -i` um arquivo baixado,
   e o ffmpeg decide o formato pelo conteúdo. `StreamThread` usa `mi.url` do JSON do yt-dlp como entrada.
8. **Processos filhos com todos os direitos do usuário:** rede, disco inteiro, criar
   outros processos. O Job só serve para "morrer junto". No Linux não há limite nenhum.
9. **Download traz "extras".** `-x --embed-metadata --embed-thumbnail` põe dentro do arquivo final a
   miniatura remota e metadados arbitrários. A etapa "tagged" usa `-map 0 -c copy` e copia **todos** os
   fluxos (capa, anexos, dados). Nada confere se o conteúdo bate com a extensão, e o app não exige
   yt-dlp ≥ 2024.07.01.
10. **Host serve o original.** `SendFile` manda os bytes do arquivo como estão, e o decodificador do celular
    recebe o arquivo hostil. `host::Publish` inclui entradas de playlist que podem não ser áudio
    (`OnPickedFiles` não filtra; `playlist.json` pode apontar para qualquer caminho). `IdFor` é
    FNV-1a do caminho: não é segredo. `/api/faixa/<id>` só confere se o id está no mapa (não se a
    playlist foi hosteada para aquele aparelho), então um aparelho pareado que conhece o caminho consegue pedir o arquivo.
11. **Diário confiado.** `OnlineStartupCleanup` apaga `pasta_temp` se o texto começar com a pasta
    do cache (`Config::StartsI`), mas não normaliza `..`. Um diário adulterado leva a `remove_all`
    fora do cache.
12. **`JParser::parse` recursivo sem limite de profundidade.** Um `playlist.json` com `[[[[…` estoura a pilha.
13. **Arquivo temporário com nome fixo** no Linux: `WebDownloadSelectedAsync` grava
    `$TMPDIR/remix_cover_full.<ext>` com `fopen("wb")`, que segue link. Em `/tmp` compartilhado,
    a proteção depende de `fs.protected_symlinks`.
14. **`SafeFileName`** não trata `CON`, `PRN`, `AUX`, `NUL`, `COM1…9`, `LPT1…9` nem os
    caracteres de direção (U+202A…202E, U+2066…2069) que disfarçam a extensão.
15. **Chaves de cache não criptográficas:** FNV-1a do conteúdo em `art::Fnv`, `std::hash` do caminho
    em `TranscodeToWav` e `EnsureInternalCover`. Colisão fabricada pode trocar a capa
    de outro álbum ou reaproveitar o WAV de outro arquivo.
16. **`try/catch` não pega segfault.** `art::Work` e `AnalyzeCurrentWave` estão protegidos contra exceção
    C++, não contra corrupção de memória.

### 2.3 Mapa: quem lê bytes não confiáveis hoje

```
                         processo do Remix (janela, Host, tokens)
 ┌───────────────────────────────────────────────────────────────────────────────┐
 │ varredura ─ ReadTags (ID3/Vorbis/MP4)                                         │
 │ art::Work ─ FromId3/FromPictureBlock/FromOgg/FindCovr ─ ShrinkCoverInto       │
 │ tocar ─ ProbeNative ─ Open ─ DecodeMono  (dr_mp3/dr_flac/dr_wav/stb_vorbis)   │
 │ capas/miniaturas/papel de parede ─ stb_image (Linux) / GDI+ (Windows)         │
 │ busca de capa ─ LoadImageFromMemory / Gdiplus::Image                          │
 │ playlist.json / diário / respostas HTTP ─ JParser                             │
 │ Host ─ SendFile(original) ────────────────────────────────► navegador celular │
 └───────────────────────────────────────────────────────────────────────────────┘
   outros processos, SEM limites:  ffmpeg (TranscodeToWav, FetchThumb, streaming,
                                   download), yt-dlp, spotdl, deno, cloudflared
```

---

## 3. A proposta: só o reproduzível + a capa

### 3.1 Princípios

1. **Um único leitor de bytes não confiáveis:** o trabalhador. O processo da UI só lê o que o
   trabalhador escreveu, em formatos que nós controlamos.
2. **Identificar pelo conteúdo.** A extensão só decide se o arquivo *aparece* na biblioteca, nunca como ele é lido.
3. **Produzir, não filtrar.** A saída é gerada do zero (PCM, contêiner remontado, JPEG reencodado).
   O que não é áudio nem capa não tem por onde passar.
4. **Nunca alterar o original.** O original é aberto só para leitura, compartilhado
   (`FILE_SHARE_READ|WRITE|DELETE` no Windows, para não travar o arquivo do usuário), e nada é escrito ao lado dele.
5. **Falhar fechado, com saída.** Arquivo recusado vai para a quarentena. Por configuração, o usuário
   pode tocar o original com aviso (o padrão é pergunta em aberto, seção 8).
6. **Identidade não muda.** `Track.path` continua sendo o caminho original.

### 3.2 Visão geral

```
original ──► [1 identificar] ──► [2 áudio] ──────────► PCM por pipe (tocar)
 (só leitura)   pelo conteúdo       remux ou PCM/FLAC ─► <cache>/limpo/v1/audio/<sha>.<fmt>
                    │                                          (Host, download, offline)
                    ├──────────► [3 capa] ────────────► <cache>/limpo/v1/capas/<sha>-512.jpg
                    │               decodifica com limite, reduz, JPEG novo
                    └──────────► [4 metadados] ────────► manifesto: título, artista, álbum,
                                    parser limitado        faixa, ano, duração, taxa, canais, bits
        tudo dentro do TRABALHADOR (processo separado, tempo/memória limitados, isolado)
                    │
               falhou/caiu ─────────────────────────────► quarentena.tsv (só registro)
```

### 3.3 Etapa 1 — identificar pelo conteúdo

O trabalhador lê os primeiros 64 KB (e, para MP4, os cabeçalhos de átomos) e classifica:

| Conteúdo | Assinatura | Aceito como |
|---|---|---|
| MP3 | `ID3` + ≥ 3 quadros MPEG consecutivos válidos logo depois da tag, ou quadros desde o byte 0 | `mp3` |
| AAC ADTS | `FF F1` / `FF F9` com quadros consecutivos | `aac` |
| FLAC | `fLaC` + STREAMINFO com valores plausíveis | `flac` |
| Ogg | `OggS` + 1º pacote `\x01vorbis`, `OpusHead` ou `\x7fFLAC` | `vorbis` / `opus` / `oggflac` |
| WAV | `RIFF????WAVE` + `fmt ` com formato PCM/float/extensible | `wav` |
| AIFF | `FORM????AIFF` / `AIFC` | `aiff` |
| MP4/M4A | `ftyp` no offset 4 com marca `M4A `, `M4B `, `mp42`, `isom`, `dash` e trilha de áudio | `mp4` |
| Matroska/WebM | `1A 45 DF A3` + DocType `webm`/`matroska` | `mkv` |
| APE / WavPack / Musepack / WMA | `MAC `, `wvpk`, `MPCK`/`MP+`, GUID ASF `30 26 B2 75 8E 66 CF 11` | `ape` / `wv` / `mpc` / `wma` |

Regras:

- **Recusa imediata** se o começo for de outro tipo: `MZ` (PE), `\x7fELF`, `#!`, `PK\x03\x04`,
  `%PDF`, `<html`/`<!DOCTYPE`, `{`/`[` (JSON), `#EXTM3U` (playlist HLS). Motivo: "não é áudio".
- **Extensão ≠ conteúdo, mas o conteúdo é áudio**: aceita pelo conteúdo e grava
  `aviso=extensao` no manifesto (a UI pode mostrar um selo discreto).
- **Poliglota**: nada fora do fluxo de áudio sobrevive às etapas 2 e 3, então os bytes extras
  morrem ali. Mesmo assim, registra `aviso=bytes_extras` quando sobram mais de 64 KB depois do último
  quadro ou página.
- Com ffmpeg, a classificação é confirmada com `ffprobe` restrito (os limites da seção 4 continuam valendo):

```
ffprobe -v error -hide_banner -protocol_whitelist file \
  -format_whitelist mp3,aac,flac,ogg,wav,aiff,mov,matroska,ape,wv,mpc,mpc8,asf \
  -probesize 5000000 -analyzeduration 20000000 \
  -show_entries format=format_name,duration:stream=index,codec_type,codec_name,sample_rate,channels,bits_per_raw_sample,disposition \
  -of compact file:/caminho/do/original
```

`file:` explícito impede que um nome como `concat:…` ou `subfile,…` seja lido como protocolo.

### 3.4 Etapa 2 — áudio

Dois níveis. O dono escolhe o padrão (seção 8).

**Nível 1 — remux (rápido, sem perda, protege contêiner e metadados).** O fluxo de áudio é copiado
quadro a quadro e o contêiner é remontado sem nada além do essencial. O processo da UI ainda
decodifica o *codec* (dr_mp3, dr_flac, stb_vorbis), mas não vê mais tags, capas, anexos, átomos
nem bytes colados.

```
ffmpeg -nostdin -hide_banner -v error -protocol_whitelist file -format_whitelist <fmt detectado> \
  -max_alloc 268435456 -i file:ORIGINAL \
  -map 0:a:0 -vn -sn -dn -map_metadata -1 -map_chapters -1 \
  -c:a copy -fflags +bitexact -flags:a +bitexact -fs <teto> \
  [-id3v2_version 0 -write_id3v1 0]        # quando sai mp3
  <cache>/limpo/tmp/<job>/audio.<mp3|flac|ogg|wav>
```

- Só vale para o que o miniaudio toca nativamente: **mp3 → mp3, flac → flac, ogg vorbis → ogg, wav/aiff PCM → wav**.
  O resto (m4a, opus, wma, ape, wv, mpc, webm, mka) vai sempre para o nível 2, como já acontece hoje com o `TranscodeToWav`.
- **Sem ffmpeg**, o remux dos quatro nativos é feito pelo próprio trabalhador. Para MP3: pular ID3v2, APEv2 e ID3v1 e
  copiar só os quadros com cabeçalho válido, mantendo o quadro Xing/LAME (atraso e preenchimento).
  Para FLAC: STREAMINFO novo + quadros com CRC-16 conferido. Para Ogg: páginas refeitas com
  CRC e o pacote de comentários vazio. Para WAV: cabeçalho de 44 bytes novo + `data`.

**Nível 2 — PCM (protege também o codec).** O trabalhador decodifica e entrega **amostras**. O que o
processo da UI recebe foi gerado por nós: não sobra estrutura que o atacante controle, só
o valor das amostras.

- **Para tocar, sem disco**: o mesmo modelo do streaming online. O trabalhador escreve PCM no
  stdout; `PcmStream` + `Player::OpenStream` tocam; seek reinicia com `-ss`, como o `canRestart`
  já faz em `StreamThread`; a onda e o espectro vêm de `StreamWavePump`, sem `DecodeMono` no original.

```
ffmpeg ... -i file:ORIGINAL -map 0:a:0 -vn -sn -dn -map_metadata -1 \
  -f f32le -ar <taxa original> -ac <canais> pipe:1        # ou s16le enquanto PcmStream for s16
```

- **Para cache** (Host, download, sem ffmpeg na hora de tocar): FLAC sem perda produzido pelo
  ffmpeg (`-c:a flac -compression_level 5`, `-sample_fmt s16` ou `s32` com `bits_per_raw_sample` = 24
  conforme a origem) ou WAV escrito pelo trabalhador próprio (miniaudio decodifica e grava um cabeçalho
  WAV de 44 bytes; não precisa tirar o `MA_NO_ENCODING`).
- **Sem ffmpeg**, o trabalhador próprio decodifica os quatro nativos com o mesmo miniaudio de hoje, só que
  **dentro do processo isolado** e com `ma_decoder_config.encodingFormat` fixado pelo resultado da etapa 1
  (acaba a tentativa e erro).

Tetos (valores iniciais, configuráveis): duração máxima **12 h** (`-t`; audiolivros `.m4b` passam de 6 h),
saída máxima **2 GB** (`-fs`), taxa entre 8 kHz e 384 kHz, até 8 canais. Fora disso, vai para a quarentena
com o motivo.

### 3.5 Etapa 3 — capa

Fontes, todas no trabalhador: capa embutida (hoje `art::Extract`), imagem da pasta
(`FindCoverInFolder`), capa escolhida pelo usuário (`SaveCustomCover`), miniatura online
(`FetchThumb`), busca de capa (`app_web.h`/`win_web.h`) e papel de parede.

1. **Extrair** com os parsers de `cover_art.h`, refatorados para receber `(const uint8_t*, size_t)`
   em vez de caminho (também necessário para o fuzzing, seção 6), com teto de **16 MB** por imagem (hoje 64 MB).
2. **Checar o cabeçalho antes de decodificar**: `stbi_info_from_memory`, ou `-max_pixels` no ffmpeg.
   Recusa acima de **8192×8192** ou **40 megapixels**.
3. **Decodificar** com stb_image compilado com `STBI_MAX_DIMENSIONS 8192`. No Windows o trabalhador
   **também usa stb_image**, nunca GDI+, para ter um único decodificador para fuzzar e porque o GDI+ aceita EMF/WMF.
   Com ffmpeg:

```
ffmpeg -nostdin -v error -protocol_whitelist file -f <image2|jpeg_pipe|png_pipe|webp_pipe conforme o magic> \
  -max_pixels 40000000 -i file:IMAGEM \
  -frames:v 1 -vf "scale=512:512:force_original_aspect_ratio=decrease,format=yuvj420p" \
  -c:v mjpeg -q:v 3 -fflags +bitexact -f image2pipe pipe:1
```

   Para capa embutida via ffmpeg: `-map 0:v:0` (a capa entra como fluxo de vídeo `attached_pic`).
4. **Reduzir** para no máximo 512 px (`COVER_MAX_DIM`) e **sempre** reencodar, inclusive as menores
   (acaba o "copia crua" de `EnsureInternalCover` e `art::Work`). JPEG baseline, qualidade 88
   (`COVER_JPEG_QUALITY`), transparência achatada sobre a cor de fundo neutra (pergunta 9).
5. **O processo da UI confere antes de abrir**: magic `FF D8`, marcador SOF0 com ≤ 512×512, até 256 KB.
   Qualquer outra coisa é recusada sem chamar stb_image nem GDI+.

### 3.6 Etapa 4 — metadados que ficam

Só o que a UI e o Host usam: **título, artista, álbum, artista do álbum, faixa, ano, duração**.
Técnicos: **taxa, canais, bits, atraso/preenchimento** (gapless).

- Lidos no trabalhador pelos parsers atuais (`ReadId3Tags`, `ParseVorbisComments`, `Mp4WalkTags`),
  com o teto de tag baixado para **1 MB** e a checagem "tamanho declarado ≤ tamanho do arquivo" **antes** de alocar.
- Texto saneado: UTF-8 válido, sem caracteres de controle, sem U+202A…202E / U+2066…2069,
  sem zero-width em excesso, **até 256 caracteres** por campo.
- Não sobrevivem: letra, comentários, capítulos, URL, "encoded by", ReplayGain, capas extras,
  anexos, fluxos de dados (pergunta 7 para decidir se algum desses fica).
- Nomes que o usuário mudou (`artists.ini`) continuam valendo por cima, chaveados pelo caminho original.

### 3.7 Cache

```
<pasta de cache>/limpo/
  v1/                                   <- versão do pipeline; mudar regra = v2 (v1 vira lixo e é apagada)
    indice.tsv                          chave-rápida \t sha256 \t estado \t formato \t último-uso
    audio/ab/<sha256>.flac|.mp3|.ogg|.wav
    capas/ab/<sha256>-512.jpg           (sha256 da imagem ORIGINAL extraída)
    manifesto/ab/<sha256>.txt           chave=valor, uma por linha, valores percent-encoded
    tmp/<pid>-<n>/                      única pasta onde o trabalhador escreve
  quarentena.tsv
```

- **Chave rápida** (sem reler o arquivo): `caminho | tamanho | mtime | id do arquivo` (inode no Linux,
  `FILE_ID_INFO` no Windows). **Chave de conteúdo**: SHA-256 calculado pelo trabalhador enquanto lê
  (deduplica cópias do mesmo álbum e acaba com as colisões fabricáveis de FNV/`std::hash`). Hoje o projeto
  não tem SHA-256: entra uma implementação de domínio público de arquivo único (~150 linhas).
- **Tamanho**: `LimpoCacheMB` (padrão sugerido 2048) com LRU por `último-uso`. Capas e manifestos quase
  nunca saem (são pequenos). Com o nível 2 por pipe, o áudio de reprodução nem passa pelo cache.
- **Onde**: hoje `Config::CacheDir()` é `%TEMP%\remix-cache` no Windows (a Limpeza de Disco e o Sensor
  de Armazenamento apagam) e `~/.cache/remix` no Linux. Para o áudio limpo, sugestão:
  `%LOCALAPPDATA%\Remix\cache` (pergunta 3).
- **Escrita atômica**: o trabalhador escreve em `tmp/`; o processo da UI confere (magic, tamanho,
  dimensão) e faz `rename` para o lugar final. Nada é lido direto de `tmp/`.

### 3.8 Quarentena

- `quarentena.tsv`: `caminho \t tamanho \t mtime \t sha256 \t motivo \t data \t versão`.
  **Não guarda cópia do arquivo**: copiar espalharia o problema e faria o antivírus reclamar do cache.
- Motivos padronizados: `nao_e_audio`, `extensao_executavel`, `tempo_esgotado`, `memoria`,
  `trabalhador_caiu:<sinal ou código>`, `saida_grande`, `duracao`, `sem_audio`, `capa_grande`, `capa_invalida`.
- Na UI: faixa com ícone de bloqueio e menu **Tentar de novo**, **Mostrar na pasta** e, se a
  configuração permitir, **Tocar o original mesmo assim**.
- Mudança de versão do pipeline ou atualização do ffmpeg libera a nova tentativa automaticamente
  (a versão faz parte da linha).

### 3.9 Onde entra

| Ponto | Hoje | Com o plano (chave ligada) |
|---|---|---|
| **Importação da biblioteca** (`ScanFolder`, `ScanComputerMusic`, `PlaylistTracks`) | `ReadTags` + `ResolveTrackCover` na thread da varredura | A varredura só lista (stat): título = nome do arquivo, artista = "Artista desconhecido". Pede ao trabalhador **metadados + capa** em lotes de 32 e atualiza por evento (`EV_LIMPO_META`, igual ao `EV_ART_READY` de hoje). O áudio **não** é higienizado na varredura. |
| **Tocar** (`PlayIndex`, `PlayFileDirect`, `OnTranscoded`) | `ProbeNative`/`Open` no original; extras via `TranscodeToWav` | `ResolverTocavel(track)`: nível 2 por pipe → `OpenStream`; nível 1 → arquivo remuxado do cache. Pré-higieniza as próximas 2 da fila, como o `SPool` do streaming. |
| **Onda e espectro** (`AnalyzeCurrentWave`) | `DecodeMono(original)` | Usa o PCM do pipe (`StreamWavePump`) ou o arquivo limpo. |
| **Capas e miniaturas** (`art::Lookup`, `FindCoverInFolder`, `GetThumb`, `g_thumbCache`, `PlatformLoadCover`) | stb_image/GDI+ no original ou na cópia crua | Só `capas/<sha>-512.jpg`, depois da checagem de SOF0. |
| **Capa escolhida / busca na web** (`SaveCustomCover`, `WebSearchAsync`, `WebDownloadSelectedAsync`) | Decodifica na UI; grava cópia crua se for pequena | Bytes vão para o trabalhador; miniaturas da busca também (lote); `covers.ini` passa a apontar para a capa limpa. |
| **Papel de parede** | `gfx::LoadImg` / `GetWallpaperImage` | Mesmo pipeline, com teto de 2560 px em vez de 512. |
| **Download online** (`RunDownload`) | yt-dlp `-x --embed-metadata --embed-thumbnail`, depois `-map 0 -c copy` | yt-dlp **sem** `--embed-thumbnail`/`--embed-metadata`; o arquivo de `job-*` passa pelo trabalhador (conteúdo × extensão, remux `-map 0:a:0`, capa limpa anexada, só título/artista/álbum). **O que vai para a pasta do usuário é a versão limpa** (pergunta 6). Exigir yt-dlp ≥ 2024.07.01. |
| **Streaming online** (`StreamThread`) | ffmpeg → PCM (já bom), mas sem restrição | Mesmo formato de saída; ffmpeg com `-protocol_whitelist https,tls,tcp,crypto,hls,http` (em modo URL) e os limites da seção 4; `mi.url` só é aceita se começar com `https://`. |
| **Miniatura online** (`FetchThumb`) | ffmpeg sem formato forçado; sem ffmpeg, decodifica na UI ou grava cru | Formato forçado pelo magic (`-f jpeg_pipe`/`png_pipe`/`webp_pipe`); sem ffmpeg vai para o trabalhador próprio (stb_image). WebP sem ffmpeg continua sem capa, como hoje. |
| **Host** (`/api/faixa/<id>`, `/api/capa/<id>`) | `SendFile(original)` | Serve o **remux** do cache (menor que PCM pelo túnel) com `Content-Type` do formato limpo. Se ainda não existe: `503` + `Retry-After: 3` e a higienização começa na hora. Ao ligar o Host, pré-higieniza as playlists hosteadas. `host::Publish` descarta entradas que não sejam áudio pelo conteúdo. Capa: só o JPEG limpo. |

### 3.10 Como o player passa a abrir só a cópia limpa

Um ponto único de decisão, consultado pela UI, que **nunca lê o original**:

```cpp
// comum/limpo.h (proposto) — so consulta indice/estado; quem le bytes e o trabalhador
enum class Fonte { Original, PcmIsolado, ArquivoLimpo, Pendente, Bloqueado };
struct Tocavel { Fonte fonte; std::wstring caminho; uint32_t taxa = 0; uint16_t canais = 0; uint8_t bits = 0; std::wstring motivo; };
Tocavel ResolverTocavel(const Track& t);          // chave desligada -> Fonte::Original (comportamento de hoje)
std::wstring CapaLimpa(const std::wstring& fonte); // "" enquanto nao estiver pronta
```

Travas de defesa em profundidade (só com `Higienizar=estrito`):

- `Player::Open` recusa caminho fora de `<cache>/limpo/` e passa `encodingFormat` fixo ao miniaudio.
- `gfx::LoadImg`, `GetThumb`, `PlatformLoadCover` e `GetWallpaperImage` recusam imagem fora de
  `<cache>/limpo/v*/capas/` e de `Config::AssetDir()` (splash, ícones e marca vêm com o app).
- `host::SendFile` recusa caminho fora de `<cache>/limpo/` e de `assets/branding`.
- Um teste de regressão (seção 6.3) falha se algum desses recebe caminho da biblioteca.

O que **não** muda: `Track.path`, `covers.ini`, `artists.ini`, `order.ini`, `playlist.json`,
`host::IdFor`, o índice `art/index.tsv` durante a transição, renomear e mandar para a lixeira
(continuam agindo no original, por ação do usuário).

### 3.11 Protocolo com o trabalhador

- Mesmo binário: `remix --higienizar` / `Remix.exe --higienizar` (sem janela, sem GLFW/GDI+, sem
  Host, sem config), então não há um `.exe` novo para o antivírus estranhar. Lançado por `Proc`
  (`app_proc.h`), com os limites da seção 4.
- **Entrada (stdin)**, uma linha por tarefa: `CAPA <id> <caminho-percent-encoded>`,
  `META <id> …`, `AUDIO <id> <nivel> <formato-saida> …`, `PCM <id> <inicio-ms> …`.
- **Saída (stdout)**, linhas de até 16 KB: `COMECA <id>`, `OK <id> sha256=… formato=… taxa=… canais=…
  bits=… dur_ms=… titulo=<pct> artista=<pct> …`, `FALHA <id> <motivo>`. Parser de campos fixos no
  processo da UI, **não** o `JParser`. No modo `PCM`, o stdout é o próprio áudio.
- **Queda**: se o processo morre, a tarefa com `COMECA` sem `OK` vai para a quarentena com
  `trabalhador_caiu:<código>`; um novo trabalhador segue com o resto do lote.
- **Tempo por tarefa**: capa 10 s; metadados 5 s; remux 30 s + 2 s/MB; FLAC 30 s + 0,5 × duração.
  Estourou → `Kill` (já existe) → quarentena `tempo_esgotado`.

---

## 4. Isolamento do processo que toca o arquivo não confiável

Só **mudar de processo** já resolve metade: um crash não fecha o app e travamento vira timeout. O
isolamento reduz o que um exploit bem-sucedido consegue fazer lá dentro.

### 4.1 Limites comuns (valores iniciais)

| Tarefa | Memória | CPU | Tempo total | Saída | Rede | Processos filhos |
|---|---|---|---|---|---|---|
| capa / metadados | 512 MB | 10 s | 15 s | 2 MB | não | não |
| remux | 512 MB | 60 s | 30 s + 2 s/MB | 1,1 × original (≤ 2 GB) | não | não |
| PCM para cache (FLAC/WAV) | 1 GB | 0,5 × duração | duração + 30 s | 2 GB | não | não |
| PCM por pipe (tocar) | 1 GB | vigia: sem PCM por 10 s com demanda → mata | — | pipe | não | não |
| ffmpeg do streaming | 1 GB | vigia igual | — | pipe | **sim** (só https) | não |
| yt-dlp (download/busca) | 2 GB | — | como hoje | pasta `job-*` | **sim** | **sim** (ffmpeg, deno) |

No ffmpeg, somar `-max_alloc 268435456` (nenhuma alocação isolada acima de 256 MB).

### 4.2 Windows

| Opção | O que impede | Custo de implementar | Custo em execução | Risco de quebrar |
|---|---|---|---|---|
| **Job Object com limites** (em `Proc::Start`: `JOB_OBJECT_LIMIT_PROCESS_MEMORY`, `JOB_OBJECT_LIMIT_PROCESS_TIME`, `JOB_OBJECT_LIMIT_ACTIVE_PROCESS`=1, `JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION`; `JobObjectBasicUIRestrictions` com `UILIMIT_HANDLES`, `READCLIPBOARD`, `WRITECLIPBOARD`, `GLOBALATOMS`, `DESKTOP`, `EXITWINDOWS`) | Esgotar memória e CPU, criar filhos, mexer na área de transferência e em janelas de outros processos | **Pequeno**: o Job já existe em `app_proc.h`; é preencher a struct por perfil | ~0 | Baixo. `ACTIVE_PROCESS=1` **não** vale para o yt-dlp (ele chama ffmpeg e deno). |
| **Criação mitigada** (`PROC_THREAD_ATTRIBUTE_CHILD_PROCESS_POLICY` = `PROCESS_CREATION_CHILD_PROCESS_RESTRICTED`, Win10 1709+; `MITIGATION_POLICY` com DEP/ASLR forçados; `WIN32K_SYSTEM_CALL_DISABLE` **só no trabalhador próprio**) | Filhos mesmo fora do Job; boa parte das syscalls de kernel gráfico (superfície histórica de elevação) | Pequeno (a lista de atributos já é montada em `Proc::Start`) | ~0 | Médio: desligar win32k no ffmpeg quebra (ele carrega `user32`). Só no nosso trabalhador, que não usa GDI+. |
| **Token restrito + integridade Baixa** (`CreateRestrictedToken` com `DISABLE_MAX_PRIVILEGE` + SIDs negados; `SetTokenInformation(TokenIntegrityLevel, S-1-16-4096)`; `CreateProcessAsUserW`, que dispensa privilégio porque o token deriva do próprio processo) | Escrever nos arquivos do usuário, no registro HKCU e em processos de integridade Média. Leitura continua possível (é o padrão do Windows) | **Médio**: a pasta `limpo\tmp` precisa de rótulo de integridade Baixa (`SetNamedSecurityInfo` + ACE `SYSTEM_MANDATORY_LABEL`), ou a saída vai só pelo pipe | ~10 ms por processo | Médio: antivírus às vezes estranha processo de integridade Baixa; o ffmpeg estático funciona; testar caminho com acento e japonês. |
| **AppContainer** (`CreateAppContainerProfile` + `PROC_THREAD_ATTRIBUTE_SECURITY_CAPABILITIES` sem capacidades) | Rede, leitura de arquivos sem ACL para o contêiner, a maior parte do sistema | **Grande**: dar RX a `ALL APPLICATION PACKAGES` em `assets\tools`, passar o original por handle herdado ou pipe (o ffmpeg precisa de seek em MP4: handle, não pipe), perfil no registro | 20–50 ms na primeira vez | Alto: não existe no Windows 7 (o manifesto ainda lista o 7), pendrive exFAT e pasta de rede precisam de teste, instalação portátil em pasta sem permissão de ACL. |
| Windows Sandbox / Hyper-V | Tudo | Fora de escala | Segundos | — |

**Importante:** o Wine **não** reproduz integridade nem AppContainer com fidelidade. Essa fase precisa
de teste em Windows 10 e 11 de verdade (VM).

### 4.3 Linux

O `Proc` usa `posix_spawnp`, que não aplica rlimit, seccomp nem landlock. A saída barata é um
**trampolim no próprio binário**: `remix --isolar <perfil> -- ffmpeg …` aplica as restrições em
si mesmo e faz `execve`. rlimits, `no_new_privs`, filtros seccomp e regras landlock **sobrevivem ao exec**.

| Opção | O que impede | Custo de implementar | Custo em execução | Risco de quebrar |
|---|---|---|---|---|
| **rlimits + `prctl`** (`RLIMIT_AS` ou `RLIMIT_DATA`, `RLIMIT_CPU`, `RLIMIT_FSIZE`, `RLIMIT_NOFILE`, `RLIMIT_CORE`=0; `PR_SET_NO_NEW_PRIVS`; `PR_SET_PDEATHSIG`=SIGKILL) | Esgotar memória, CPU e disco; ganhar privilégio por setuid; ficar vivo sem o Remix | **Pequeno** (trampolim ~80 linhas) | ~0 | Baixo. `RLIMIT_AS` conta espaço virtual (threads do ffmpeg reservam muito): preferir `RLIMIT_DATA`, ou AS ≥ 2 GB. **Não** usar `RLIMIT_NPROC` (é por usuário e quebraria o desktop). |
| **Landlock** (kernel ≥ 5.13; rede TCP a partir de 6.7) | Ler fora de: original, `/usr`, `/lib*`, `/etc/ld.so.cache`, binário do ffmpeg; escrever fora de `limpo/tmp/<job>`; `connect` TCP (ABI 4) | **Médio**: syscalls e structs escritas à mão (o alvo glibc 2.27 do zig não tem os headers), como já foi feito com o dbus | ~0 | Baixo, se for "melhor esforço": kernel sem suporte segue sem. ffmpeg de distro precisa ler as libs; ffmpeg estático em `~/.local/bin` também funciona. |
| **seccomp-bpf, lista de proibidas** (`socket` AF_INET/AF_INET6/AF_NETLINK, `ptrace`, `process_vm_readv/writev`, `perf_event_open`, `bpf`, `userfaultfd`, `keyctl`/`add_key`, `mount`, `unshare`, `setns`, `init_module`, `kexec_load`) | Rede e as primitivas de kernel mais usadas em exploits | Médio (BPF à mão, sem libseccomp) | ~0 | Médio: diferenças entre arquiteturas. Não bloquear `clone`/`execve` no ffmpeg (ele usa threads; o exec vem do trampolim). |
| seccomp, lista de permitidas | Quase tudo | Grande | ~0 | **Alto**: muda com glibc e versão do ffmpeg. Só para o trabalhador próprio, e mais tarde. |
| **namespace de usuário + rede** (`unshare(CLONE_NEWUSER\|CLONE_NEWNET)` no trampolim) | Qualquer rede (só existe `lo`, desligada) | Pequeno | ~1 ms | Médio: Ubuntu 23.10+/24.04 restringe userns sem perfil AppArmor → cair para landlock/seccomp. |
| **bubblewrap, se existir** (`bwrap --unshare-all --die-with-parent --new-session --ro-bind /usr /usr … --ro-bind <original> /in --bind <tmp> /out`) | Visão do sistema de arquivos + rede + PIDs | Pequeno no código, **médio** nos testes | 5–20 ms | Médio-alto: nem toda distro tem; setuid em algumas; não aninha dentro de Flatpak ou Snap; AppImage depende do sistema. |
| `systemd-run --user --scope -p MemoryMax=… -p CPUQuota=…` | Memória e CPU por cgroup (mais fiel que rlimit) | Pequeno | 50–100 ms por processo | Médio: sem sessão systemd de usuário (WSL, algumas distros) → não usar para tarefas curtas. |

### 4.4 Recomendação

1. **Primeiro**: processo separado + timeouts + limites de memória, CPU e saída (Job com limites /
   trampolim com rlimits) + `-protocol_whitelist`/`-format_whitelist`/`-max_alloc`/`-fs` no ffmpeg.
   Barato, pega DoS, crash e bomba.
2. **Depois**: Windows com integridade Baixa + filhos restritos (+ win32k desligado no trabalhador
   próprio); Linux com landlock + seccomp por lista de proibidas + `unshare` de rede quando der.
3. **Opcional**: AppContainer (Win10+) e bubblewrap quando presente, como camada extra
   ligada por configuração, nunca como requisito.

### 4.5 O que o isolamento não resolve

- **O celular.** O Host precisa servir a cópia limpa (seção 3.9); o isolamento do PC não protege o navegador do telefone.
- **O codec no nível 1.** O remux não impede um bug do dr_mp3 com dados de quadro malformados. Só o nível 2 impede.
- **ffmpeg e yt-dlp desatualizados.** O isolamento limita o estrago, não corrige a falha.
  Mostrar a versão em Configurações > ONLINE (já mostra) e avisar abaixo de um mínimo.

---

## 5. Riscos de quebrar o app e como mitigar

| Risco | Mitigação |
|---|---|
| **Tudo para de tocar** por bug no pipeline | Chave `[Seguranca] Higienizar=0` (padrão **desligado**); valores `0` desligado, `capas`, `avisar` (tenta limpar; se falhar, toca o original com aviso), `estrito` (bloqueia). Fases independentes (capas antes do áudio). |
| **Falha legítima** (arquivo raro que o ffmpeg não abre) | Em `avisar`: toca o original e mostra "Tocando o original sem higienizar: <motivo>". Menu da faixa com "Tentar de novo". Contador na tela de configurações ("N arquivos na quarentena"). |
| **Biblioteca grande** (20 mil faixas, pendrive, pasta de rede) | Varredura só lista; metadados e capas em lotes no trabalhador, com resultado em cache; **áudio só ao tocar** (nível 2 por pipe não espera nada); pré-higieniza só as próximas 2. Medir antes e depois (6.4). Pasta de rede: pergunta 12. |
| **Latência para começar a tocar** | Nível 2 por pipe: o primeiro bloco de PCM sai em dezenas de ms (o modelo do streaming já faz isso). Nível 1: remux de MP3 de 5–10 MB leva dezenas de ms. FLAC para cache só em segundo plano (Host, download). |
| **Custo de criar processo** (Windows ~20–50 ms) | Trabalhador de longa vida por lote (32 tarefas); um processo por faixa só no modo pipe. |
| **Formato que o ffmpeg não tem** (build "free" de distro, como o `ffmpeg-free` do Fedora; ffmpeg ausente) | Detectar com `ffmpeg -decoders` uma vez (junto de `ProbeOnlineTools`); os quatro nativos pelo trabalhador próprio (miniaudio isolado); o resto segue a regra de hoje ("instale o ffmpeg"), com o motivo certo. |
| **FLAC e alta resolução** (24 bits / 96–192 kHz / multicanal) | Hoje `PcmStream` é **s16 estéreo** (`PcmDS_Format`). Antes de ligar o pipe para essas faixas: generalizar para `f32` e N canais, e passar a taxa nativa para `EnsureEngineRate`; até lá, usar o nível 1 (FLAC remuxado) ou FLAC em cache. Teste de bit a bit em 6.3. |
| **Memória do pipe** | `STREAM_AHEAD_SEC` = 6 min foi pensado para rede; para arquivo local, 20–30 s basta (a 96 kHz/2 canais/f32, 6 min = ~276 MB). Parâmetro por origem. |
| **Gapless e início cortado** | O app não faz gapless nem crossfade hoje, mas o remux de MP3 **não pode** perder o atraso do LAME (clique ou silêncio no começo). ffmpeg aplica o atraso na decodificação; conferir que o muxer regrava o quadro Xing/LAME; teste de primeiras e últimas amostras (6.3). M4A: manter a edit list (priming do AAC). |
| **Seek** | Modo pipe: reinício com `-ss` (já implementado para URL em `StreamThread`). Testar arrastar a barra nos 3 primeiros e 3 últimos segundos. |
| **Tags que o usuário quer manter** | Parser limitado (3.6) extrai título, artista, álbum, faixa e ano **antes** de descartar; `artists.ini` continua por cima; pergunta 7 para letra, ReplayGain e capítulos. |
| **`covers.ini` apontando para cópias cruas antigas** em `assets/covers` | Migração única (como `MigrateOversizedCovers`): reencoda tudo pelo trabalhador, guarda o original em `_originais/` como hoje, e só troca o `.ini` depois de conferir. |
| **Disco** | LRU com teto; nível 2 por pipe não grava; mostrar o tamanho do cache e o botão "Limpar" em configurações. |
| **Antivírus** (histórico do projeto com falso positivo) | Mesmo binário, sem `CREATE_SUSPENDED` (o `Proc` já evita), sem código injetado; integridade Baixa e AppContainer são APIs documentadas usadas por navegadores. Testar com Defender em VM antes de lançar. |
| **Modo seguro do Windows** | Com a chave em `capas`/`avisar`/`estrito`, o laço de crash por capa some. Mesmo com a chave desligada, fazer o modo seguro pular capas embutidas e miniaturas (Apêndice A). |
| **Host lento pelo túnel** | Servir o remux (tamanho parecido com o original), não PCM; `503 + Retry-After` enquanto prepara. |
| **Compatibilidade de configuração** | Chaves novas só em `[Seguranca]`; `config.ini` antigo sem a seção = desligado. |

---

## 6. Plano de testes

### 6.1 Corpus de arquivos corrompidos

Gerado por script (`test/malicioso/gerar.py`, proposto), **sem exploits reais**: só malformações
estruturais. Os arquivos gerados não vão para o git (só o script). Base de mídia válida: a biblioteca
`test/Musica` (`Solta`, `Album1`, `Formatos` com mp3, wav, ogg, opus, m4a, flac, `capa.png`, `cover.jpg`)
e, se precisar de variedade, a *FATE suite* pública do FFmpeg.

| Grupo | Casos |
|---|---|
| Truncados | cada arquivo de `test/Musica` cortado em 1 byte, 10 %, 50 %, 99 %; arquivo de 0 byte |
| Tamanhos declarados | ID3 declarando 256 MB num arquivo de 10 bytes; APIC de 60 MB de lixo; FLAC PICTURE com 65535×65535; WAV com `data` 0xFFFFFFFF e com `fmt` canais=0, taxa=0, bits=0; STREAMINFO com total de amostras máximo; átomo MP4 tamanho 0 e 64 bits gigante; 1000 níveis de `udta`; página Ogg com 255 segmentos cheios repetidos; 100 mil comentários Vorbis vazios |
| Imagens | PNG de 30000×30000 com IDAT de zeros (poucos KB); JPEG progressivo com centenas de scans; GIF com nome `.jpg`; EMF e WMF com nome `.jpg` (Windows); WebP com nome `.jpg`; PNG com CRC errado; imagem de 1×1; imagem de 8193×1 |
| Poliglotas e extensão | MP3 válido + ZIP colado no fim; MP3 válido + HTML no fim; cabeçalho `MZ` com nome `.mp3`; Ogg com nome `.mp3`; `#EXTM3U` com nome `.jpg` e `.mp3` |
| Nomes e caminhos | `..`, U+202E no meio do nome, `CON.mp3`, `NUL.flac`, 255 bytes, quebra de linha (Linux), `-i.mp3`, `concat:x.mp3`, `file:x.mp3` |
| Tipos especiais (Linux) | link para `/etc/passwd` com nome `x.mp3`; FIFO `cover.jpg`; link para `/dev/zero`; arquivo esparso de 10 GB `.wav` (`truncate -s`) |
| JSON | `playlist.json` com 100 mil `[` aninhados; `path` com `../../../.ssh/id_rsa`; diário com `pasta_temp` usando `..` |
| Download (simulado) | pasta `job-*` com `audio.mp3` que é PE; `audio.webm` com vídeo e 3 trilhas de áudio; miniatura WebP com nome `.jpg` |

**Critérios**, para cada caso e nos dois sistemas:

- o **processo do Remix não cai nem trava** (UI responde em < 250 ms durante o teste);
- nenhum arquivo fora de `<cache>/limpo` e de `<cache>/online` é criado ou alterado (conferir por hash
  antes e depois em toda a pasta de teste);
- o resultado é `OK` com saída válida **ou** quarentena com o motivo esperado;
- nenhum pico de memória do processo da UI acima de +100 MB (Linux: `/proc/<pid>/status` VmHWM;
  Windows: `PeakWorkingSetSize` no log de teste).

### 6.2 Fuzzing

Pré-requisito (fase 0): separar leitura de arquivo e parsing, com funções que recebem `(const uint8_t*, size_t)`,
em `cover_art.h` (`FromId3`, `FromPictureBlock`, `FindCovr`, `FromOgg` a partir de buffer) e em `playlist.h`
(`ReadId3Tags`, `ParseVorbisComments`, `Mp4WalkTags`). O caminho de hoje (`ifstream`) vira um invólucro fino.

Alvos (pasta `test/fuzz/`, fora do build normal):

| Alvo | O que chama | Dicionário / sementes |
|---|---|---|
| `fuzz_capa_embutida` | `art::FromId3`, `FromPictureBlock`, `FindCovr`, `FromOgg` em memória | tokens `ID3`, `APIC`, `PIC`, `fLaC`, `OggS`, `covr`, `data`, `moov`, `udta`, `ilst`, `meta`; capas de `test/Musica` |
| `fuzz_tags` | `ReadId3Tags` (buffer), `ParseVorbisComments`, `Mp4WalkTags`, `DecodeID3Text` | `TIT2`, `TPE1`, `TITLE=`, `ARTIST=`, `©nam`, `©ART` |
| `fuzz_dr_mp3` / `fuzz_dr_flac` / `fuzz_dr_wav` / `fuzz_stb_vorbis` | `ma_decoder_init_memory` com `encodingFormat` fixo + leitura de até 10 s de frames | faixas de `test/Musica/Formatos` |
| `fuzz_stb_image` | `stbi_load_from_memory` com `STBI_MAX_DIMENSIONS 8192` (a mesma config do raylib: PNG, JPG, BMP, GIF) | `cover.jpg`, `capa.png`, `big_photo.jpg` |
| `fuzz_identificar` | a classificação da etapa 1 | magic numbers da tabela 3.3 |
| `fuzz_jparser` | `JParser::parse` (com o limite de profundidade da fase 0) | `playlist.json` reais |
| `fuzz_manifesto` | parser das linhas `OK`/`FALHA` do trabalhador | saídas reais gravadas |
| `fuzz_host_req` | `host::ReadReq` refatorado para ler de string | pedidos do `docs/HOST.md` |

Como rodar:

```bash
# libFuzzer (clang do sistema; o zig do projeto nao e necessario aqui)
clang++ -std=c++20 -g -O1 -fsanitize=fuzzer,address,undefined -Icomum \
  test/fuzz/fuzz_capa_embutida.cpp -o build/fuzz/fuzz_capa_embutida
build/fuzz/fuzz_capa_embutida -max_len=4000000 -rss_limit_mb=1024 -timeout=5 \
  -dict=test/fuzz/capa.dict build/fuzz/corpus/capa test/Musica

# AFL++ no trabalhador inteiro (caixa-preta do pipeline)
afl-fuzz -i test/fuzz/sementes -o build/fuzz/afl -m 1024 -t 5000 -- \
  build/remix-afl --higienizar-arquivo @@ /dev/null
```

Rotina: 1 h por alvo antes de qualquer versão que mexa em parser, miniaudio, raylib ou stb;
os achados viram casos fixos em `test/malicioso` (minimizados com `-minimize_crash=1`). Build Linux com
ASan/UBSan (`REMIX_SANITIZE=1 bash linux/build.sh`, proposto) para rodar o corpus da 6.1.

### 6.3 Regressão com a biblioteca de teste

Com os ganchos de teste que já existem (`--home`, `--after`, `dump`, `--exit-after`) e novos
(`higienizar:<n>`, `quarentena`, `limpo:dump`):

| Verificação | Como |
|---|---|
| Mesma biblioteca | `dump` com a chave desligada e ligada: mesma contagem de faixas, títulos, artistas, presença de capa |
| Áudio idêntico | Nível 1 (FLAC e WAV): PCM do original = PCM da cópia **bit a bit**. MP3/Ogg: PCM do ffmpeg no original = no remux. Nível 2: PCM do pipe = `ffmpeg -f f32le` do original |
| Taxa, canais, bits, duração | Iguais ao original (duração ±1 quadro); `Player::EngineRate()` igual à taxa nativa |
| Início e fim | As primeiras e últimas 4096 amostras batem (pega atraso do LAME e priming do AAC) |
| Seek | `seek:<ms>` em 0, 1000, meio, fim −3000: posição reportada ±50 ms |
| Capas | Toda capa exibida vem de `limpo/v1/capas`, com ≤ 512 px e SOF0 válido; teste falha se `gfx::LoadImg`/`GetThumb` recebe caminho da biblioteca |
| Host | `curl /api/faixa/<id>` devolve a cópia limpa (hash conferido) e `Content-Type` do formato limpo; `/api/capa/<id>` é JPEG ≤ 512 |
| Download | Fluxo de download com um arquivo local servido por HTTP de teste (sem internet): resultado na pasta do usuário é a versão limpa |
| Windows | As mesmas ações sob Wine (Xephyr) para lógica e UI; **isolamento só em VM Windows 10/11** |
| Travas | Com `Higienizar=estrito`, um teste chama `Player::Open` num original e espera recusa |

### 6.4 Desempenho

- Biblioteca sintética: 1000 e 20000 arquivos gerados com `ffmpeg -f lavfi -i sine` em mp3, flac,
  m4a e ogg, cada um com capa de 1500 px.
- Medir (chave desligada × ligada): tempo da primeira varredura e da segunda (cache), tempo até o
  primeiro som (p50/p95), CPU e memória do processo da UI, tamanho do cache.
- Metas iniciais: segunda varredura ≤ 1,2× a de hoje; primeiro som ≤ +100 ms no p95 (modo pipe);
  memória da UI ≤ a de hoje (ela deixa de decodificar capa grande).

### 6.5 Windows de verdade

Integridade Baixa, AppContainer, Job com limites e Defender não são fiéis no Wine. Uma VM Windows 10 22H2
e uma Windows 11 com: pasta com acento e japonês, pendrive exFAT, pasta de rede SMB, usuário
sem admin, Defender ligado, `assets\tools` com o ffmpeg do `INSTALAR-DEPENDENCIAS.bat` (winget) e com
um ffmpeg solto.

---

## 7. Fases de implementação

Esforço **relativo** (pontos): 1 = ajuste pequeno e local; 3 = um componente novo pequeno; 8 =
mexe em vários pontos das duas cascas e precisa de teste em VM. Numeração de versão só sugerida.

| Fase | Versão | O que entra | Pontos | Visível para o usuário |
|---|---|---|---|---|
| **0 — endurecimento sem mudar a arquitetura** | 1.4.x | Tudo do Apêndice A: tetos de alocação nas tags, `STBI_MAX_DIMENSIONS`, checagem de dimensão antes do GDI+, `encodingFormat` pelo magic, `TranscodeToWav` via `RunCapture` com timeout/`-fs`/whitelists (e no `Proc` no Windows), limites no Job, `is_regular_file` sem seguir link em capas e playlists, filtro de áudio em `OnPickedFiles`, profundidade no `JParser`, `pasta_temp` normalizada, `SafeFileName`, temp com nome único, modo seguro sem capas | **5** | Quase nada (mensagens melhores) |
| **0.5 — base de testes** | 1.4.x | Separar leitura/parsing em `cover_art.h` e nas tags; `test/malicioso/gerar.py`; alvos de fuzzing; build com sanitizers; primeiras horas de fuzzing e correções | **5** | Não |
| **1 — trabalhador + capas limpas** | 1.5.0 | `remix --higienizar` (capas e metadados), protocolo, `limpo/v1/capas`, quarentena de capa, todas as fontes de imagem pelo trabalhador, fim da cópia crua, migração de `assets/covers`. Chave `Higienizar=capas`. Só processo separado + limites (sem isolamento de SO) | **8** | Capas pequenas levemente reencodadas; capa que falhar some com aviso |
| **2 — isolamento de SO** | 1.6.0 | Windows: integridade Baixa + filhos restritos + win32k desligado no trabalhador; Linux: trampolim `--isolar` com rlimits, `no_new_privs`, landlock, seccomp por proibição, `unshare` de rede quando der; ffmpeg das capas e do `TranscodeToWav` sob o mesmo perfil. Testes em VM | **8** | Não (se der certo) |
| **3 — áudio limpo para tocar** | 1.7.0 | Etapa 1 completa; nível 2 por pipe com `PcmStream` generalizado (f32, N canais, taxa nativa); nível 1 como alternativa; `ResolverTocavel`; onda pelo PCM; travas `estrito`; UI de quarentena. Chaves `avisar` e `estrito` (padrão continua `0`) | **13** | Ícone de bloqueio, "tocar mesmo assim", tamanho do cache |
| **4 — downloads e Host** | 1.8.0 | Download grava a versão limpa (sem `--embed-*`, conteúdo × extensão, versão mínima do yt-dlp); streaming com `protocol_whitelist` e limites; Host serve remux do cache (`503`/`Retry-After`), pré-higieniza as playlists hosteadas, descarta não áudio no `Publish`; LRU e botão "Limpar" | **8** | Downloads sem capítulos e tags extras; Host demora um pouco na primeira vez |
| **5 — opcionais e decisão do padrão** | 2.0 | AppContainer (Win10+) e bubblewrap como camada extra; fuzzing contínuo; decidir se `avisar` vira padrão depois de uma versão beta | **5** | Talvez o padrão mude |

Ordem de valor por esforço: **0 → 1 → 2 → 4 (Host) → 3 → 5**. O Host e as capas expõem mais gente
(celular, laço de crash) com menos risco de quebrar do que trocar o caminho de reprodução. Se
preferir, a parte do Host da fase 4 pode vir logo depois da fase 2.

---

## 8. Perguntas em aberto para o dono

1. **Nível padrão do áudio** quando a chave for ligada: **remux** (rápido, sem perda, protege tags e
   contêiner, não protege o codec) ou **PCM** (protege também o codec, custa CPU e, em cache, disco)?
2. **Tocar por pipe** (sem disco, começa na hora, como o streaming) ou **arquivo em cache** (sem
   processo vivo durante a música)? Sugestão: pipe para tocar, cache só para Host e download.
3. **Onde fica o cache** do áudio limpo e **de que tamanho**: `%TEMP%\remix-cache` (hoje; o Windows
   limpa sozinho) ou `%LOCALAPPDATA%\Remix\cache`? 2 GB serve?
4. **Sem ffmpeg**: higienizar só os quatro nativos com o trabalhador próprio, ou exigir ffmpeg quando a chave estiver ligada?
5. **Quando falha**: bloquear ou tocar o original com aviso? E o usuário pode liberar uma faixa para sempre?
6. **Downloads**: aceitar que o arquivo gravado na pasta do usuário **perca** o que o yt-dlp embute hoje
   (descrição, URL, capítulos, miniatura original)? Fica só título, artista, álbum e a capa limpa.
7. **Quais tags ficam** além de título, artista, álbum, artista do álbum, faixa e ano? Letra? ReplayGain
   (o app não usa hoje)? Capítulos de audiolivro (`.m4b`)?
8. **Host**: servir o remux (formato original, tamanho parecido) ou transcodificar para um formato que todo
   celular toca (AAC/Opus) quando a origem é WMA, APE ou FLAC de alta resolução? Isso pesa na banda do túnel.
9. **Capas**: reencodar **sempre**, mesmo as de até 512 px (hoje ficam intactas)? Transparência: achatar
   sobre a cor de fundo ou manter PNG (com um PNG gerado por nós)?
10. **Windows 7** (ainda no manifesto): aceitar isolamento menor nele (só Job + integridade Baixa) ou
    desligar a função lá?
11. **Linux**: usar bubblewrap quando existir, ou manter só o que o próprio binário faz (landlock e seccomp)
    para ter o mesmo comportamento em toda distro?
12. **Pasta em rede ou pendrive lento**: higienizar metadados e capas na varredura (lento na primeira vez)
    ou só quando a faixa aparece na tela ou toca?
13. **Sem telemetria**: como saber de falsos positivos? Sugestão: botão "Copiar relatório" na quarentena, com motivo,
    formato e versão do ffmpeg, **sem caminho** do arquivo.
14. **Versões mínimas**: bloquear download e streaming com yt-dlp < 2024.07.01, ou só avisar? E ffmpeg velho?
15. **Quarentena** guarda só o registro (sugerido) ou também uma cópia do arquivo, para o usuário mandar para análise?
16. **Limite de duração**: 12 h está bom (audiolivros), ou deve ser configurável por pasta?

---

## Apêndice A — endurecimentos baratos que não dependem do plano

Cada item é pequeno, local e não muda o comportamento para arquivos normais. É a fase 0.

| # | Onde | Mudança |
|---|---|---|
| A1 | `ReadId3Tags` (`playlist.h`) | Teto de 16 MB (ou `min(tagSize, tamanho do arquivo − 10)`) **antes** do `vector(tagSize)`; conferir `gcount`. |
| A2 | `art::FromMp3`, `FromChunks`, `FromFlac`, `FromMp4` (`cover_art.h`) | `MAXB` 64 → 16 MB; comparar o tamanho declarado com o tamanho do arquivo antes do `ReadN`. |
| A3 | `linux/build-raylib.sh` | `-DSTBI_MAX_DIMENSIONS=8192`. |
| A4 | Windows `GetThumb`, `PlatformLoadCover`, `GetWallpaperImage`, `SaveResized` | Ler as dimensões pelo cabeçalho (JPEG SOF, PNG IHDR, BMP) antes do `Gdiplus::Image`; recusar acima de 8192 px ou se o magic não for JPEG/PNG/BMP (sem EMF/WMF/TIFF/ICO). |
| A5 | `Player::ProbeNative`, `Open`, `DecodeMono` (`player_ma.h`) | Definir `encodingFormat` pelo magic (MP3/FLAC/WAV/Ogg); acaba a tentativa com os quatro decodificadores. |
| A6 | `sys::TranscodeToWav`, `PlatformTranscodeToWav` | Trocar `system()`/`CreateProcessW` por `RunCapture` com timeout (ex.: 120 s + 1 s/MB); `-protocol_whitelist file,pipe`, `-i file:<caminho>`, `-max_alloc 268435456`, `-fs 2G`; no Windows passa a ter o Job. Linux: ffmpeg achado pelo `OFindTool` (mesma regra do online). |
| A7 | `FetchThumb` (`online_resolve.h`) | Forçar o demuxer pelo magic (`-f jpeg_pipe`/`png_pipe`/`webp_pipe`), recusar o resto; `-protocol_whitelist file`, `-max_pixels 40000000`; sem ffmpeg, nunca gravar a imagem crua. |
| A8 | `StreamThread` (`online_play.h`) | Aceitar `mi.url` só com `https://` (ou `http://` explícito); `-protocol_whitelist https,tls,tcp,http,crypto,hls` na entrada. |
| A9 | `Proc::Start` (`app_proc.h`) | Parâmetro de perfil: memória, CPU e "sem filhos" no Job (Windows); no Linux, anotar para o trampolim da fase 2. |
| A10 | `FindCoverInFolder`, `PlaylistTracks`, `PlayFileDirect`, `OnPickedFiles`, `ResolveTrackCover` | `symlink_status` + arquivo regular (sem seguir link), igual ao `WalkFiles`; `OnPickedFiles` só aceita `IsAudioExt`. |
| A11 | `host::Publish` (`host_server.h`) | Só publica entradas com extensão de áudio e arquivo regular; `/api/capa` só `.jpg`/`.png` do cache de capas ou de `assets/covers`. |
| A12 | `JParser::parse` (`app_playlists.h`) | Limite de profundidade (64) e de tamanho de entrada (ex.: 32 MB). |
| A13 | `OnlineStartupCleanup` (`online_play.h`) | `lexically_normal` + recusar `..`, e exigir nome `job-<pid>-<n>` exatamente abaixo de `OnlineCacheDir()` antes do `remove_all`. |
| A14 | `SafeFileName` (`online_play.h`) | Remover U+202A…202E e U+2066…2069; prefixar `_` em `CON`, `PRN`, `AUX`, `NUL`, `COM1…9`, `LPT1…9` (com ou sem extensão). |
| A15 | `linux/app_web.h` `WebDownloadSelectedAsync` | Arquivo em `<cache>` com nome aleatório, aberto com `O_CREAT\|O_EXCL\|O_NOFOLLOW` (em vez de `$TMPDIR/remix_cover_full.<ext>`). |
| A16 | `art::Work`, `EnsureInternalCover` | Quando `ShrinkCoverInto` recusa por ser pequena, reencodar mesmo assim; se a decodificação falhar, **não** gravar a cópia crua. |
| A17 | Modo seguro (`win_diag.h`, `windows/main.cpp`, `app_core.h`) | Em modo seguro: não chamar `PlayIndex(g_current,false)` na abertura (hoje ele abre e analisa a última faixa), não extrair capa embutida, não carregar miniaturas nem papel de parede, não rodar `AnalyzeCurrentWave`. Tira o laço de crash por arquivo ou imagem. |
| A18 | `RunDownload` (`online_play.h`) | Etapa "tagged" com `-map 0:a:0` em vez de `-map 0`; conferir o magic do `produced` contra a extensão; avisar se o yt-dlp for anterior a 2024.07.01. |
