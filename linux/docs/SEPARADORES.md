# Separadores de partes (stems) recomendados

O Remix não instala nem embute separador nenhum: você aponta uma linha de comando em
*Configurações > SEPARAR EM PARTES (STEMS)* (chave `SepCmd` do `config.ini`), com `{entrada}` (o
arquivo) e `{saida}` (a pasta). O Remix executa, lê a pasta e reconhece as partes pelo nome dos
arquivos. Qualquer separador que siga isso serve; abaixo estão os que testei e recomendo.

## Medido nesta máquina

Ryzen 5 5600G (12 threads), música de 3 min, separação **presa a 3 núcleos** (perfil LEVE — o
padrão, que não atrapalha jogo nem chamada):

| Motor | Tempo (3 núcleos) | RAM | Partes | Qualidade (SDR)* |
|---|---|---|---|---|
| **audio-separator** (MDX-Net Inst HQ) | ~2 min | ~3,5 GB | vocal + instrumental | instrumental 15,5 · vocal 8,8 |
| **Demucs** (htdemucs) | ~1 min 53 s | ~1,6 GB | vocal, bateria, baixo, outros | vocal 9,9 · bateria 9,4 · baixo 11,6 |
| audio-separator (BS-Roformer, o "melhor") | **> 11 min** | ~2,5 GB | vocal + instrumental | instrumental 16,5 · vocal 11,8 |

\* SDR = quanto mais alto, mais limpo o corte. Números do catálogo do audio-separator / paper do Demucs.

**O que isso quer dizer, sem enrolação:**

- Em CPU, o audio-separator (com um modelo MDX-Net) e o Demucs empatam em velocidade — **nenhum é
  "rápido"**; ficam perto do tempo real. Quem lembra de separação instantânea provavelmente usou
  GPU ou o Spleeter (mais antigo e de qualidade bem menor).
- O modelo "melhor de todos" (BS-Roformer) é **inviável em CPU** (11 min por música). Não use em CPU.
- **Para o corte vocal / instrumental** (karaokê, destaque de trechos, "só a voz" ou "só a música"),
  o **audio-separator com MDX-Net dá o melhor resultado** entre os que rodam em CPU — é a escolha
  recomendada.
- **Para as 4 partes** (bateria e baixo separados), o **Demucs** é igual em velocidade e usa menos
  memória. O audio-separator também roda modelos Demucs, então dá para ter os dois no mesmo programa.

## Recomendado: audio-separator

CLI aberto (MIT), mantido, que roda os melhores modelos do Ultimate Vocal Remover (MDX-Net,
Roformer) e também o Demucs. Você instala; o Remix só executa.

**Instalar (só CPU, ~1,3 GB — sem os pacotes de GPU):**

```bash
# com uv (recomendado): o --torch-backend cpu evita baixar ~4 GB de CUDA à toa
uv tool install "audio-separator[cpu]" audioread --torch-backend cpu

# ou com pipx:
pipx install "audio-separator[cpu]" --pip-args="--extra-index-url https://download.pytorch.org/whl/cpu"
pipx inject audio-separator audioread
```

Também precisa do `ffmpeg` (a sua distro já deve ter).

**No Remix** (*Configurações > SEPARAR EM PARTES > ESCOLHER O PROGRAMA...*, ou `SepCmd` no
`config.ini`) — o modelo importa:

```ini
# Vocal + instrumental, melhor qualidade que roda em CPU (recomendado):
SepCmd=audio-separator {entrada} --output_dir {saida} --output_format FLAC -m UVR-MDX-NET-Inst_HQ_4.onnx

# As 4 partes (vocal, bateria, baixo, outros), via Demucs no mesmo programa:
SepCmd=audio-separator {entrada} --output_dir {saida} --output_format FLAC -m htdemucs.yaml
```

> **Importante:** sem `-m`, o audio-separator usa o BS-Roformer por padrão — o modelo mais lento, que
> leva mais de 11 min por música em CPU. Sempre passe `-m` com um dos modelos acima.

Os modelos são baixados por ele na primeira vez (o Remix não baixa nada). Veja a lista completa com
`audio-separator --list_models`.

## Alternativa: Demucs

Se você quer as 4 partes e menos uso de memória, o Demucs sozinho serve. É o que o Remix usava antes
e continua funcionando:

```ini
SepCmd=demucs -d cpu -o {saida} --filename {stem}.{ext} {entrada}
```

## Como o Remix reconhece as partes

Pelo **nome** dos arquivos que aparecerem na pasta de saída (em qualquer subpasta), sem depender de
um formato fixo:

| Parte no Remix | Nomes reconhecidos |
|---|---|
| Só vocal | `vocals`, `vocal`, `voz`, `voice` |
| Só música (instrumental) | `instrumental`, `no_vocals`, `accompaniment`, `karaoke`, `backing` |
| Bateria | `drums`, `bateria` |
| Baixo | `bass`, `baixo` |
| Outros | `other`, `outros` |

Se o separador só entrega vocal + instrumental, os modos Bateria/Baixo/Outros aparecem marcados com
"—" (indisponíveis) e o Remix não tenta separar de novo. Quando vêm bateria + baixo + outros mas
não o instrumental, o Remix monta o "só a música" somando as três.
