# Descobrir: novidades e recomendações

A partir da **1.6** o Remix tem uma **tela inicial** com fileiras de novidades e uma aba
**Descobrir** para navegar por gênero — no PC (estilo REMIX) e no celular.

## De onde vêm as informações

Da **API pública do Deezer** (`api.deezer.com`): sem conta, sem login e sem chave de
desenvolvedor. De lá vêm só **metadados**: nome da música, artista, capa, tipo e o link.

**Nada disso toca música.** Quem toca e baixa continua sendo o motor online de sempre
(a CLI de mídia que o usuário configurou): ao clicar num cartão, o Remix procura aquela música e toca **inteira** — e não o
trecho de 30 segundos que as APIs entregam.

Se o PC estiver sem internet, a tela abre do mesmo jeito com o que já tinha sido buscado
(cache de 6 horas em `descobrir.cache`, na pasta de cache do Remix).

## As fileiras

| Fileira | O que é |
|---|---|
| **Tocados recentemente** | as últimas músicas da sua biblioteca que você ouviu (só no PC) |
| **O melhor de \<artista\>** | as mais tocadas de quem você mais ouve |
| **Parecido com \<artista\>** | artistas que combinam com o que você ouve |
| **Dos artistas que você ouve** | álbuns e singles recentes deles |
| **Bombando agora** | as mais tocadas do país |
| **Playlists da semana** | playlists prontas |
| **Álbuns em alta** / **Artistas do momento** | as paradas de álbuns e artistas |

**VER TUDO** abre a fileira inteira ali mesmo (sem rolagem para o lado, que atrapalha no
mouse e no toque). Clicar de novo fecha.

## Descobrir (por gênero)

A aba **Descobrir** mostra os gêneros do Deezer (Pop, Sertanejo, MPB, Rap/Funk Brasileiro,
Rock, Dance...). Entrando num deles aparecem as paradas daquele estilo: músicas, playlists,
álbuns e artistas. **BUSCAR ONLINE** abre a busca de sempre (nome ou link).

## O que acontece ao clicar

- **Música**: o PC resolve e ela começa a tocar (entra na fila como qualquer música online).
- **Álbum** ou **playlist**: abre a lista na busca online, onde dá para **tocar tudo** ou
  **salvar como playlist** (no celular, os mesmos botões).
- **Artista**: faz uma busca pelo nome dele.

## Como as recomendações são montadas

O Remix guarda em `gostos.ini` (na pasta do Remix, **só no seu PC**) quantas vezes você ouviu
cada artista e cada música, e quando foi a última vez. O peso de um artista é
`vezes × (1 + 2 / (1 + dias desde a última vez))` — ou seja, o que você ouviu hoje pesa cerca
de três vezes mais do que o que você ouviu meses atrás. Os dois primeiros artistas dessa lista
viram as fileiras personalizadas.

Nada sai do PC: o Deezer só recebe "quais são as paradas" e "quem é parecido com o artista X".
Apagar `gostos.ini` zera as recomendações.

## No celular

O Início do celular mostra as mesmas fileiras, abaixo da sua biblioteca e das playlists:

- `GET /api/descobrir` devolve as fileiras já montadas pelo PC (só com o online ligado);
- `GET /api/desccapa/<id>` serve a capa. **O PC baixa e guarda** — o celular nunca fala com a
  internet direto (a página só pode carregar imagem do próprio PC, e assim o telefone não
  aparece para o Deezer).

## Arquivos

- `gostos.ini` (pasta do Remix): o que você ouve. Dá para apagar.
- `descobrir.cache` (pasta de cache): as fileiras já montadas, para a tela abrir na hora.
- `online-thumbs/` (pasta de cache): as capas baixadas.
