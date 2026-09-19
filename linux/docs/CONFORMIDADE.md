# Conformidade — o que já está feito e o que falta

Lista de verificação do projeto como player de música: identidade, integração com programas de
terceiros, conteúdo do repositório e linguagem da interface. A arquitetura que sustenta isso está em
[ARQUITETURA.md](ARQUITETURA.md); o que a versão Windows precisa fazer está em
[MUDANCAS-PARA-O-WINDOWS.md](MUDANCAS-PARA-O-WINDOWS.md).

Data desta revisão: **19/09/2026**.

---

## Feito

### Identidade
- [x] README com título "Remix — Player de música multiplataforma" e descrição objetiva.
- [x] Reprodução local e busca por link como funções principais; fontes externas como recurso
      opcional e secundário.
- [x] Nada de "projeto de estudo", "fins de pesquisa" ou "não incentiva pirataria" em lugar nenhum.
- [x] Licença Apache 2.0 (`LICENSE`), citada no README.

### Integração com programa de terceiros
- [x] Nunca como biblioteca: só processo separado, `argv` em vetor, sem shell
      (`src/core/fonte_externa.h`).
- [x] Caminho informado pela pessoa (`MediaCli=` no `config.ini`, botão **ESCOLHER O PROGRAMA...**).
- [x] O app testa o arquivo antes de usar (`fonte::ExecutavelOk`) e mostra a versão encontrada.
- [x] Degradação graciosa: sem configuração, `fonte::MsgFalta()` explica e o resto do app funciona.
- [x] Estado vazio na busca online com botão **CONFIGURAR FONTE EXTERNA**.
- [x] Trava: `fonte::Rodar` / `fonte::Abrir` recusam qualquer comando cujo primeiro argumento não
      seja o caminho configurado.
- [x] Sem descoberta automática do programa de mídia, sem auto-update (`-U`), sem segundo programa
      de reserva.
- [x] O contrato (opções que a CLI precisa aceitar) aparece nas Configurações no lugar de um nome.

### Repositório
- [x] Zero menção a nome de programa de terceiros em código, README, LEIA-ME, docs, `.spec`,
      `control`, `copyright` e scripts.
- [x] Instalador de dependências removido (`instalar-dependencias.sh`) — inclusive do zip portátil.
- [x] `Recommends:`/`Suggests:` do `.deb` e do `.rpm` sem programa de mídia (só `ffmpeg`, `zenity`,
      `libcurl`, `nodejs`/`deno` — utilitários genéricos).
- [x] Nenhum binário de terceiros versionado; `linux/dist/`, `linux/build/` e `linux/third_party/`
      fora do git.
- [x] Estrutura: `linux/src/`, `linux/config/` (com `config.exemplo.ini` comentado), `linux/dist/`,
      `README.md`, `LICENSE`.

### Linguagem da interface
- [x] Seções **FONTES EXTERNAS** e **PROGRAMA DE LINHA DE COMANDO (OPCIONAL)**.
- [x] Ordem dos botões nos resultados: **▶ tocar · + playlist · ↓ salvar cópia**.
- [x] "AO TOCAR: STREAMING / SALVAR CÓPIA"; menu de contexto com "Tocar (streaming)" antes de
      "Salvar cópia".
- [x] Em nenhum lugar a CLI é chamada de "baixador".
- [x] Painel do Discord: **BIBLIOTECAS DO BOT (npm)** deixa claro que roda o `npm` da pessoa.

---

## Publicado (19/09/2026)

- [x] Limpeza publicada em `NinjaZinS2/Remix-Linux` (commit `324c771`).
- [x] Repositório antigo `DevelopersOpenSource/Remix` esvaziado: ficaram só `README.md` e `LICENSE`,
      com os links dos repositórios de cada sistema e os créditos. O histórico continua lá (o código
      da versão Windows está no commit `3e0eafd`) e as releases seguem publicadas, com os arquivos
      intactos.
- [x] Descrição do repositório antigo no GitHub trocada (citava o programa pelo nome) e apontando
      para o Remix-Linux.
- [x] Textos das releases antigas (1.2.0, 1.2.1, 1.5.0, 1.5.1, 1.6.0) sem nome de programa de
      terceiros — só o texto mudou; nenhum download foi removido.

---

## Falta

1. **Release nova do `Remix-Linux`** com os quatro pacotes gerados por `build-packages.sh`
   (`.deb`, `.rpm`, AppImage e zip portátil), falando de player, biblioteca e Host.

2. **Tópicos do repositório** `Remix-Linux` no GitHub (`music-player`, `cpp`, `linux`, `raylib`).

3. **Versão Windows.** Enquanto a casca Windows não receber as mudanças, ela continua com o código
   antigo — hoje só no histórico do repositório antigo. Guia pronto em
   [MUDANCAS-PARA-O-WINDOWS.md](MUDANCAS-PARA-O-WINDOWS.md). Quando o repositório existir, o link
   entra no README do repositório antigo (a tabela já tem a linha).

---

## Testes feitos (19/09/2026)

Com uma CLI de mentira que cumpre o contrato (imprime JSON e, com `-o -`, despeja um MP3):

| Teste | Resultado |
|---|---|
| Trava da fronteira (`--after N:fonte`) | `/bin/echo` recusado, comando vazio recusado, caminho configurado roda e devolve a versão |
| Busca online no PC | 3 resultados lidos do JSON da CLI, com título, artista e duração |
| Tocar um resultado | áudio chegou ao player (posição 6,8 s de streaming, nada gravado no disco) |
| Celular (Host) | parear por QR → `/api/online/buscar` devolveu as faixas → `/api/online/ouvir/<id>` entregou 2,9 MB de MP3 192 kbps |
| Sem CLI configurada | busca mostra "Fontes externas desligadas" + botão CONFIGURAR FONTE EXTERNA; biblioteca, playlists e novidades seguem normais |
| Player local | arquivo da biblioteca tocou com título e artista |

---

## Como conferir de novo, a qualquer momento

```bash
# troque NOME pelo nome do programa que você usa: tem que sair vazio
grep -rli "NOME" --exclude-dir=.git --exclude-dir=third_party .
grep -rn "system(\|popen(" linux/src/core/                                  # vazio
git ls-files | grep -iE "\.exe$|\.dll$|\.o$|instalar"                        # vazio
grep -rln "fonte::Cmd()\|fonte::CmdTocar(" linux/src/ | grep -v fonte_externa  # 5 do núcleo + app_input (teste)
```
