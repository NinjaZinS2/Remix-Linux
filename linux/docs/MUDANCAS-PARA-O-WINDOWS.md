# O que mudar na versão Windows

Escrito para quem cuida da casca Windows. A versão Linux passou a seguir a arquitetura descrita em
[ARQUITETURA.md](ARQUITETURA.md): **o app não instala, não baixa e não embute programa nenhum** — ele
só executa um programa de linha de comando que a pessoa instalou e apontou nas configurações, e
funciona normalmente sem ele.

O núcleo (`comum/` no repositório antigo, `linux/src/core/` aqui) é o mesmo dos dois lados, então a
maior parte do trabalho é **trazer os arquivos do núcleo** e depois fazer três coisas na casca
Windows. No fim tem a limpeza do repositório, que é o que realmente importa para o portfólio.

---

## 1. Núcleo: um arquivo novo e seis mexidos

### 1.1 Novo: `fonte_externa.h`

Copie `linux/src/core/fonte_externa.h` inteiro. É a fronteira: o único lugar que executa o programa
externo. Ele já vem com `#ifdef _WIN32` no que precisa (`_waccess`/`_wstat`, `PATH` separado por
`;`, sufixo `.exe` na busca por `ffmpeg`/`node`).

Depende de `app_proc.h` (`RunCapture`, `Proc`) e de uma função que a casca/`app_core.h` fornece:

```cpp
std::wstring g_cfgMediaCli();   // devolve g_cfg.mediaCli
```

Detalhe importante: `--ffmpeg-location` e `--js-runtimes` são **extras**. Ao configurar o caminho,
`ConferirImpl()` roda o programa uma vez com essas duas opções; se ele responder "no such option"
(ou sair com erro), `Estado.extras` vira `false` e o `fonte::Cmd()` para de mandá-las. Assim uma CLI
que só cumpre o básico do contrato continua funcionando inteira.

### 1.2 `config.h` — a chave nova

```cpp
    // Fonte de audio externa: caminho do programa de linha de comando que a
    // pessoa instalou e configurou. Vazio = as fontes externas ficam desligadas.
    std::wstring mediaCli;
    ...
    else if (k == L"MediaCli") mediaCli = v;        // no Load
    ...
    ls.push_back(L"MediaCli=" + mediaCli);          // no Save
```

### 1.3 `online_resolve.h` — some com a descoberta de ferramenta

Tudo que era "procurar o programa no sistema / conferir versão / montar argv" saiu deste arquivo e
virou `fonte_externa.h`. No lugar do bloco antigo de ferramentas fica só:

```cpp
#include "fonte_externa.h"
// ---- ferramentas ----------------------------------------------------------
// A camada que fala com o programa externo mora em fonte_externa.h: regras,
// contrato, deteccao e as duas unicas funcoes que executam o processo
// (fonte::Rodar e fonte::Abrir). Daqui para baixo e so montar comando e ler JSON.
```

E, ao longo do arquivo:

| Antes | Agora |
|---|---|
| `EnsureTools()` | `fonte::Garantir()` |
| `<Programa>Ok()` / `MediaCliOk()` | `fonte::Configurada()` |
| `FfmpegOk()` / `FfmpegTool()` | `fonte::FfmpegOk()` / `fonte::Ffmpeg()` |
| `<Programa>Args()` / `MediaCliArgs()` | `fonte::Cmd()` |
| `RunCapture(a, …)` com argv da CLI | `fonte::Rodar(a, …)` |
| mensagem de erro escrita na mão | `fonte::MsgFalta(0..3)` |

**Apague** (não devem existir em lugar nenhum):

- a função de auto-atualização da ferramenta (a que chamava `-U`) e qualquer chamada dela;
- o resolvedor alternativo de Spotify (o que chamava um segundo programa de mídia) e qualquer caminho que caia nele;
- qualquer lista de nomes de programa procurados no `PATH` ou em `~/.local/bin`, `AppData`, etc.
  (o `ffmpeg` e o runtime JavaScript continuam sendo procurados no `PATH` — são utilitários
  genéricos; **a CLI de mídia, não**).

### 1.4 `online_play.h`, `host_server.h`, `discord_host.h` — tocar e salvar

Onde abria o processo em pipe:

```cpp
        auto ya = fonte::CmdTocar(play);
        if (!fonte::Abrir(src, ya, true, false, false)) { dec.Kill(); dec.Wait(); /* erro */ return; }
```

Onde salvava uma cópia (`RunDownload`), o argv sai de `fonte::Cmd()` e roda por `fonte::Rodar(...)`.
As checagens de entrada viram:

```cpp
    if (!fonte::Configurada() || !fonte::FfmpegOk()) { fail(fonte::MsgFalta());  return; }   // tocar
    if (!fonte::Configurada() || !fonte::FfmpegOk()) { finish(3, fonte::MsgFalta(3)); return; }  // salvar
```

### 1.5 `stems.h`

Mesma troca: `fonte::Cmd()` + `fonte::Rodar(a, 600000, &j.cancel, nullptr, 64 * 1024)`.

### 1.6 `app_core.h`, `app_layout.h`, `app_input.h`, `app_panels.h`

- `app_core.h`: `std::wstring g_cfgMediaCli(){ return g_cfg.mediaCli; }`; modo de edição **12**
  (digitar o caminho) e o evento `EV_PICK_CLI` (resultado do seletor de arquivo); zonas
  `Z_SET_CLI`, `Z_SET_CLI_BUSCAR`, `Z_ON_CFG`. Os dois pontos que gravam o caminho fazem
  `g_cfg.mediaCli = v; g_cfg.Save(); fonte::Reconferir();` e avisam pelo `SetStatus`.
- `app_layout.h`: seção **FONTES EXTERNAS** (era "ONLINE") e a seção nova **PROGRAMA DE LINHA DE
  COMANDO (OPCIONAL)** com `R_setCli` (o caminho) e `R_setCliBuscar` (escolher). Na tela de busca
  online, quando não há resultado, não está carregando e `!fonte::Configurada()`, o botão
  `u.btnCfg` aparece no meio da área da lista.
- `app_input.h`: `Z_SET_CLI` abre o modo 12; `Z_SET_CLI_BUSCAR` chama `PlatformPickProgramAsync`;
  `Z_ON_CFG` fecha a busca e abre as Configurações.
- `app_panels.h`: o botão do bot passou a se chamar **BIBLIOTECAS DO BOT (npm)** e a linha de
  estado diz que ele roda o `npm` **da pessoa** (`npm i discord.js @discordjs/voice`) na pasta do
  Remix. Nada de baixar Node.js sozinho.

---

## 2. Casca Windows: três coisas

### 2.1 Seletor do programa

O núcleo chama:

```cpp
void PlatformPickProgramAsync(int evType);   // devolve o caminho pelo evento EV_PICK_CLI
```

No `win_sys.h` já existe a infraestrutura (`RunPicker` numa thread STA). Falta uma função de
escolha sem filtro de tipo:

```cpp
static std::wstring PickAnyFile(HWND owner){
    wchar_t file[MAX_PATH]={0}; OPENFILENAMEW ofn={0}; ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=owner;
    ofn.lpstrFilter=L"Programas\0*.exe;*.cmd;*.bat\0Todos\0*.*\0";
    ofn.lpstrFile=file; ofn.nMaxFile=MAX_PATH;
    ofn.lpstrTitle=L"Escolha o programa de linha de comando";
    ofn.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn)?file:L"";
}
```

e o plugue, junto dos outros `Platform*Async`:

```cpp
void PlatformPickProgramAsync(int evType){ RunPicker(evType,0,[]{ return PickAnyFile(g_hwnd); }); }
```

No Linux isso é um `kdialog`/`zenity` sem filtro (`sys::DLG_ANYFILE`).

### 2.2 Tela de Configurações

Onde hoje aparece o estado das ferramentas, use o snapshot em vez dos campos internos:

```cpp
bool probed = fonte::JaConferiu();
fonte::Estado e = fonte::Snapshot();
std::wstring s = e.cli.empty() ? L"Fonte externa: NÃO CONFIGURADA" : L"Fonte externa: " + e.vCli;
s += e.ffmpeg.empty() ? L"   ·   ffmpeg: NÃO ENCONTRADO" : L"   ·   ffmpeg " + e.vFf;
```

E desenhe a seção nova: uma linha de ajuda ("O Remix não instala nem distribui nada: informe o
caminho de um programa de linha de comando compatível que você já tenha."), o contrato
(`fonte::Contrato()`) quando nada está configurado, o botão **CAMINHO: …** (`Z_SET_CLI`) e o botão
**ESCOLHER O PROGRAMA...** (`Z_SET_CLI_BUSCAR`).

### 2.3 Estado vazio da busca online

Quando `u.btnCfg` tem tamanho, desenhe no meio da lista:

```
Fontes externas desligadas
Para ouvir de fontes externas, o Remix executa um programa de linha de comando que você instala.
Nada é baixado nem instalado pelo app.
          [ CONFIGURAR FONTE EXTERNA ]
```

Botões dos resultados na ordem **▶ (tocar) · + (playlist) · ↓ (salvar cópia)**, e o rodapé
"▶ toca · + playlist · ↓ salva uma cópia".

---

## 3. Limpeza do repositório (o que mais pesa)

- [ ] **Apagar `windows/INSTALAR-DEPENDENCIAS.bat`** e qualquer script que baixe programa de mídia.
      O app não instala nada; quem instala é a pessoa.
- [ ] **Nenhum binário versionado**: `.exe`, `.dll`, `.o`, `.res` fora do git (hoje há
      `windows/app_res.o` e `windows/default-manifest.o` rastreados). Pacote pronto só nas Releases.
- [ ] **Zero menção a nome de programa de terceiros** no código, README, LEIA-ME, docs, `.spec`,
      `control`, `copyright` e scripts. Onde precisar citar, diga "uma CLI de mídia compatível" e
      descreva o contrato.
- [ ] **Nenhum `requirements.txt` / `package.json`** com programa de mídia. (O `discord.js` do bot é
      instalado na pasta do Remix pelo `npm` da pessoa, quando ela pede.)
- [ ] **README**: título "Remix — Player de música multiplataforma"; descrição "Busca e reproduz
      música do YouTube Music, YouTube e SoundCloud. Suporta arquivos locais. Streaming e download
      de conteúdo adicional via CLI externa configurável pelo usuário."; licença Apache 2.0. **Sem**
      "fins de pesquisa", "não incentiva pirataria" ou "projeto de estudo" — é um player, e ponto.
- [ ] **Instalador/zip portátil** não podem levar instalador de dependências nem binário de
      terceiros. O `LEIA-ME` do portátil explica o que a pessoa instala por conta.
- [ ] **Estrutura**: `src/`, `dist/` (fora do git), `config/` (com um `config.exemplo.ini`
      comentado), `README.md`, `LICENSE`.

Conferência rápida (deve sair tudo vazio):

```bash
grep -rli "NOME-DO-PROGRAMA" --exclude-dir=.git .          # vazio
git ls-files | grep -iE "\.exe$|\.dll$|\.o$|instalar"     # vazio
grep -rn "system(\|ShellExecute" comum/                    # vazio (o núcleo nunca usa shell)
```

---

## 4. Ordem sugerida

1. Traga `fonte_externa.h` e o `config.h`.
2. Troque as chamadas em `online_resolve.h`, `online_play.h`, `host_server.h`, `discord_host.h`,
   `stems.h` (é quase tudo renomeação; o compilador acha o que faltou).
3. Plugue o `PlatformPickProgramAsync` e desenhe as duas telas.
4. Faça a limpeza do repositório e reescreva o README.
5. Teste com o campo vazio: o app tem que abrir, tocar arquivo local, mostrar a biblioteca e as
   novidades, e avisar direitinho nas telas online.

> O núcleo desta versão também traz a interface 1.6 (barra lateral, início com fileiras, painel
> Tocando agora, letra sincronizada, escolha de saída de áudio). Se a sua versão ainda estiver na
> 1.5.x, dá para trazer só os arquivos citados aqui — ou o `src/core/` inteiro, se quiser a
> interface nova junto.
