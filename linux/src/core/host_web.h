#pragma once
// Host: a pagina que o celular abre (app web estilo player de streaming), o manifesto/
// service worker da PWA e a pagina "conectar" que o usuario manda pelo WhatsApp.
// Tudo embutido no executavel: nada e lido do disco, nada e montado a partir de dados
// do usuario sem escape.
// Regras de seguranca da pagina (a CSP do servidor ja bloqueia o resto):
//  - sem script inline, sem atributos de evento no HTML, sem transformar texto em codigo, sem recurso externo;
//  - icones em SVG criados por document.createElementNS; texto do servidor so por textContent;
//  - POST sempre com o cabecalho X-Remix e corpo JSON; URLs montadas com encodeURIComponent;
//  - localStorage so para preferencias locais (ultimo nome digitado, aba, filtros, aleatorio/repetir);
//  - o token do QR code (#q=...) e tirado da barra de endereco assim que a pagina le.
// As capas carregam so quando aparecem na tela e poucas por vez, porque o PC limita os
// pedidos por aparelho (API + capas + audio contam juntos).
#include <string>

namespace hostweb {

// ---------------------------------------------------------------- index --
// @ACCENT@ e trocado pela cor do tema (#rrggbb) e @BUILD@ pela versao + um resumo do app.js/app.css
// (o navegador guarda os dois por um dia; mudou o codigo, muda a URL e o celular baixa de novo).
static const char* INDEX_HTML = R"~~~(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#121212">
<meta name="color-scheme" content="dark">
<meta name="referrer" content="no-referrer">
<meta name="format-detection" content="telephone=no">
<meta name="apple-mobile-web-app-capable" content="yes">
<meta name="mobile-web-app-capable" content="yes">
<meta name="apple-mobile-web-app-status-bar-style" content="black">
<meta name="apple-mobile-web-app-title" content="Remix">
<title>Remix Player</title>
<link rel="manifest" href="/manifest.webmanifest">
<link rel="icon" href="/icon.png">
<link rel="apple-touch-icon" href="/icon.png">
<link rel="stylesheet" href="/app.css?v=@BUILD@">
<style>:root{--accent:@ACCENT@}
.bootHelp{margin:22px 16px 0;color:#b3b3b3;font:15px/1.4 system-ui,-apple-system,sans-serif;text-align:center;opacity:0;animation:bootShow .3s 12s forwards}
.bootHelp.now{opacity:1;animation:none}.bootHelp a{color:#fff;font-weight:700;display:inline-block;padding:12px 6px}
@keyframes bootShow{to{opacity:1}}</style>
</head>
<body>
<div id="boot" class="boot" role="status" aria-label="Carregando"><div class="spin"></div>
  <p class="bootHelp">Está demorando? <a href="/">Toque aqui para recarregar</a></p>
  <noscript><p class="bootHelp now">Ative o JavaScript para usar o Remix.</p></noscript></div>

<section id="pair" class="pair" hidden>
  <div class="pairBox">
    <img class="pairLogo" src="/icon.png" alt="">
    <h1 id="pairTitle">Conectar ao Remix</h1>
    <p id="pairSub" class="pairSub"></p>
    <div id="pairFields">
      <label class="fld"><span>Nome deste aparelho</span>
        <input id="pairName" type="text" maxlength="40" placeholder="Ex.: Celular da Ana" autocomplete="off" autocapitalize="words" autocorrect="off" spellcheck="false" enterkeyhint="next"></label>
      <label class="fld" id="pinWrap"><span>PIN do PC</span>
        <input id="pairPin" type="text" inputmode="numeric" pattern="[0-9]*" maxlength="12" placeholder="4 a 12 números" autocomplete="off" autocorrect="off" spellcheck="false" enterkeyhint="go"></label>
      <label class="chk"><input type="checkbox" id="pairRemember" checked><span>Lembrar este aparelho neste navegador</span></label>
      <p class="fine">Nada do aparelho é coletado: só um código aleatório fica guardado neste navegador.</p>
    </div>
    <button id="pairBtn" class="btnP wide" type="button">Conectar</button>
    <p id="pairMsg" class="pairMsg" role="status" aria-live="polite"></p>
    <p id="pairHint" class="pairHint">ou escaneie o QR code no painel HOST do PC</p>
  </div>
</section>

<div id="app" hidden>
  <main id="views">
    <section id="vHome" class="view" aria-label="Início"></section>
    <section id="vSearch" class="view" aria-label="Buscar" hidden></section>
    <section id="vLib" class="view" aria-label="Sua Biblioteca" hidden></section>
    <section id="vPl" class="view plView" aria-label="Playlist" hidden></section>
  </main>
  <div id="dock">
    <div id="mini" class="mini" hidden>
      <button id="miniOpen" class="miniOpen" type="button" aria-label="Abrir Tocando agora">
        <span id="miniArt" class="miniArt"></span>
        <span class="miniTx"><span id="miniT" class="miniT"></span><span id="miniA" class="miniA"></span></span>
      </button>
      <button id="miniAdd" class="ib" type="button" data-ic="plusC" aria-label="Adicionar a playlist"></button>
      <button id="miniPlay" class="ib miniPlay" type="button" data-ic="play" aria-label="Tocar"></button>
      <div class="miniBar" aria-hidden="true"><div id="miniFill"></div></div>
    </div>
    <nav id="nav" class="nav" aria-label="Navegação principal">
      <button type="button" data-tab="home"><span class="nIc" data-ic="home"></span><span>Início</span></button>
      <button type="button" data-tab="search"><span class="nIc" data-ic="search"></span><span>Buscar</span></button>
      <button type="button" data-tab="lib"><span class="nIc" data-ic="lib"></span><span>Sua Biblioteca</span></button>
    </nav>
  </div>
</div>

<section id="np" class="np" aria-hidden="true" aria-label="Tocando agora">
  <div class="npIn">
    <div id="npDrag" class="npDrag">
      <header class="npTop">
        <button id="npClose" class="ib" type="button" data-ic="chevDown" aria-label="Fechar Tocando agora"></button>
        <button id="npFromBtn" class="npFrom" type="button" aria-label="Abrir a playlist que está tocando"><small>TOCANDO DE</small><b id="npFrom"></b></button>
        <button id="npMore" class="ib" type="button" data-ic="more" aria-label="Mais opções"></button>
      </header>
    </div>
    <div class="npBody">
      <div class="npArtW"><div id="npArt" class="npArt"></div></div>
      <div class="npPanel">
        <div class="npMeta">
          <div class="npTx">
            <div id="npT" class="npT"></div>
            <div class="npAw"><span id="npOn" class="badge" hidden>ONLINE</span><span id="npA" class="npA"></span></div>
          </div>
          <button id="npAdd" class="ib" type="button" data-ic="plusC" aria-label="Adicionar a playlist"></button>
        </div>
        <canvas id="npWave" class="npWave" aria-hidden="true"></canvas>
        <div class="npSeek">
          <input id="npSeek" class="rng" type="range" min="0" max="1000" step="1" value="0" aria-label="Posição da música">
          <div class="npTimes"><span id="npPos">0:00</span><span id="npLen">0:00</span></div>
        </div>
        <div class="npCtl">
          <button id="npShuf" class="ib dot" type="button" data-ic="shuffle" aria-label="Aleatório" aria-pressed="false"></button>
          <button id="npPrev" class="ib big" type="button" data-ic="prev" aria-label="Anterior"></button>
          <button id="npPlay" class="playBig xl" type="button" data-ic="play" aria-label="Tocar"></button>
          <button id="npNext" class="ib big" type="button" data-ic="next" aria-label="Próxima"></button>
          <button id="npRep" class="ib dot" type="button" data-ic="repeat" aria-label="Repetir: desligado"></button>
        </div>
        <div class="npFxRow"><button id="npFx" class="npFxBtn" type="button" aria-label="Efeitos e stems"><span data-ic="fx"></span><span id="npFxTxt">Efeitos e stems</span></button><button id="npLetra" class="npFxBtn" type="button" aria-label="Letra da música"><span data-ic="lib"></span><span>Letra</span></button></div>
        <div id="npMsg" class="npMsg" role="status" aria-live="polite" hidden></div>
        <div class="npDev"><span data-ic="phone"></span><span id="npDev">Tocando neste celular</span></div>
      </div>
    </div>
  </div>
</section>

<div id="sheetWrap" class="sheetWrap" hidden>
  <div id="sheetBg" class="sheetBg"></div>
  <div id="sheet" class="sheet" role="dialog" aria-modal="true" tabindex="-1"></div>
</div>
<div id="toast" class="toast" role="status" aria-live="polite"></div>
<audio id="audio" preload="none" playsinline></audio>
<script src="/app.js?v=@BUILD@"></script>
</body>
</html>
)~~~";

// ------------------------------------------------------------------ css --
// --accent vem do index; o app.js calcula --acc-rgb, --on-acc (texto sobre o destaque)
// e --acc-txt (destaque clareado quando a cor do tema e escura demais para texto).
static const char* APP_CSS = R"~~~(
:root{--accent:#1db954;--acc-rgb:29,185,84;--on-acc:#000;--acc-txt:#1db954;
  --bg:#121212;--el:#1f1f1f;--el2:#2a2a2a;--tx:#fff;--tx2:#b3b3b3;--tx3:#8f8f8f;--danger:#f3727f;
  --sat:env(safe-area-inset-top,0px);--sab:env(safe-area-inset-bottom,0px);--sal:env(safe-area-inset-left,0px);--sar:env(safe-area-inset-right,0px);
  --padL:calc(16px + var(--sal));--padR:calc(16px + var(--sar));--navH:62px;--miniH:0px}
*{box-sizing:border-box}
[hidden]{display:none!important}
html{background:var(--bg);-webkit-text-size-adjust:100%;text-size-adjust:100%}
body{margin:0;min-height:100vh;min-height:100dvh;background:var(--bg);color:var(--tx);font:15px/1.35 system-ui,-apple-system,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
  -webkit-tap-highlight-color:transparent;overscroll-behavior-y:none;-webkit-font-smoothing:antialiased}
body.hasMini{--miniH:64px}
body.lock{position:fixed;left:0;right:0;overflow:hidden}
h1,h2,h3,p{margin:0}
button,input,select,textarea{font:inherit;color:inherit;letter-spacing:inherit}
button{background:none;border:0;padding:0;margin:0;cursor:pointer;-webkit-user-select:none;user-select:none;touch-action:manipulation;text-align:inherit}
button:disabled{cursor:default}
:focus:not(:focus-visible){outline:none}   /* sem :focus-visible (iOS < 15.4) fica o contorno padrao */
:focus-visible{outline:2px solid var(--acc-txt);outline-offset:2px}
img{-webkit-touch-callout:none;-webkit-user-drag:none}
a,button,.trow,.mini,.card,.prow,.scI,.tile{-webkit-touch-callout:none}
.trow,.mini,.npTx,.miniTx,.shHead,.prof{-webkit-user-select:none;user-select:none}
svg{display:block;width:24px;height:24px;flex:none}
.ick{stroke:var(--on-acc)}
.grow{flex:1}

/* botoes */
.ib{position:relative;min-width:44px;min-height:44px;display:inline-flex;align-items:center;justify-content:center;border-radius:50%;color:var(--tx2);flex:none;transition:transform .1s,color .15s}
.ib:active{transform:scale(.9)}
.ib:disabled{opacity:.4}
.ib.on{color:var(--acc-txt)}
.ib.dot.on::after{content:"";position:absolute;bottom:4px;left:50%;width:4px;height:4px;margin-left:-2px;border-radius:50%;background:var(--acc-txt)}
.btnP,.btnS{display:inline-flex;align-items:center;justify-content:center;min-height:48px;padding:0 28px;border-radius:24px;font-weight:700;font-size:15px;letter-spacing:.01em;transition:transform .1s,opacity .15s}
.btnP{background:var(--accent);color:var(--on-acc)}
.btnS{border:1px solid #7c7c7c;color:var(--tx)}
.btnP:active,.btnS:active{transform:scale(.97)}
.btnP:disabled,.btnS:disabled{opacity:.55}
.btnP.danger{background:var(--danger);color:#1a0003}
.btnP.wide{width:100%;min-height:52px;border-radius:26px;font-size:16px;margin-top:22px}
.link{min-height:44px;padding:0 4px;color:var(--tx2);font-size:13px;font-weight:700;transition:opacity .1s}
.link:active,.npFrom:active,.avBtn:active,.chip:active{opacity:.65}
.playBig{position:relative;width:56px;height:56px;border-radius:50%;background:var(--accent);color:var(--on-acc);display:inline-flex;align-items:center;justify-content:center;flex:none;transition:transform .1s;box-shadow:0 8px 20px rgba(0,0,0,.3)}
.playBig svg{width:26px;height:26px}
.playBig:active{transform:scale(.94)}
.playBig.wait::after,.miniPlay.wait::after{content:"";position:absolute;top:-5px;right:-5px;bottom:-5px;left:-5px;border-radius:50%;border:3px solid transparent;border-top-color:var(--acc-txt);animation:rot .9s linear infinite}
.miniPlay.wait::after{top:4px;right:4px;bottom:4px;left:4px;border-width:2px;border-top-color:#fff}

/* carregando */
.boot{min-height:100vh;min-height:100dvh;display:flex;flex-direction:column;align-items:center;justify-content:center}
.spin{width:36px;height:36px;border-radius:50%;border:3px solid rgba(255,255,255,.15);border-top-color:var(--accent);animation:rot .8s linear infinite}
@keyframes rot{to{transform:rotate(360deg)}}

/* pareamento */
.pair{min-height:100vh;min-height:100dvh;display:flex;align-items:center;justify-content:center;
  padding:calc(28px + var(--sat)) var(--padR) calc(28px + var(--sab)) var(--padL);
  background:radial-gradient(130% 55% at 50% 0%,rgba(var(--acc-rgb),.38),rgba(var(--acc-rgb),0) 70%),var(--bg)}
.pairBox{width:100%;max-width:400px;text-align:center}
.pairLogo{width:76px;height:76px;border-radius:18px;margin:0 auto 18px;display:block;box-shadow:0 10px 30px rgba(0,0,0,.45)}
.pair h1{font-size:28px;font-weight:800;letter-spacing:-.02em;line-height:1.15}
.pairSub{color:var(--tx2);margin-top:8px;font-size:15px}
#pairFields{margin-top:14px}
.fld{display:block;text-align:left;margin-top:16px}
.fld span{display:block;font-size:13px;font-weight:700;margin-bottom:7px}
.fld input,.bigIn{display:block;width:100%;height:52px;border-radius:6px;border:1px solid #7c7c7c;background:#121212;color:#fff;padding:0 14px;font-size:17px;-webkit-appearance:none;appearance:none}
.fld input:focus,.bigIn:focus{border-color:#fff;box-shadow:0 0 0 1px #fff}
.fld input:focus-visible,.bigIn:focus-visible{outline:none}
.fld input::placeholder{color:#6f6f6f}
.chk{display:flex;align-items:center;gap:12px;min-height:48px;margin-top:10px;text-align:left;cursor:pointer;font-size:15px}
.chk input{-webkit-appearance:none;appearance:none;position:relative;width:24px;height:24px;margin:0;flex:none;border:2px solid #8a8a8a;border-radius:5px;background:#121212;cursor:pointer;transition:background .12s,border-color .12s}
.chk input:checked{background:var(--accent);border-color:var(--accent)}
.chk input:checked::after{content:"";position:absolute;left:7px;top:2px;width:6px;height:12px;border:solid var(--on-acc);border-width:0 2.5px 2.5px 0;transform:rotate(45deg)}
.fine{font-size:12.5px;color:var(--tx3);text-align:left;margin-top:2px}
.pairMsg{min-height:22px;margin-top:16px;color:var(--tx2);font-size:14.5px}
.pairMsg.err{color:#ff9aa4}
.pairMsg.ok{color:var(--acc-txt)}
.pairMsg.wait::before{content:"";display:inline-block;width:12px;height:12px;margin:0 8px -1px 0;border-radius:50%;border:2px solid rgba(255,255,255,.2);border-top-color:var(--accent);animation:rot .8s linear infinite}
.pairHint{margin-top:10px;color:var(--tx3);font-size:13.5px}

/* telas */
.view{max-width:1000px;margin:0 auto;padding:calc(10px + var(--sat)) var(--padR) calc(var(--navH) + var(--miniH) + var(--sab) + 28px) var(--padL);min-height:100vh}
.top{display:flex;align-items:center;gap:8px;min-height:56px;margin:0 0 10px -6px}
.top h1{flex:1;min-width:0;font-size:24px;font-weight:800;letter-spacing:-.02em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.avBtn{width:44px;height:44px;display:inline-flex;align-items:center;justify-content:center;flex:none;border-radius:50%}
.av{width:34px;height:34px;border-radius:50%;background:var(--accent);color:var(--on-acc);font-weight:800;font-size:15px;display:inline-flex;align-items:center;justify-content:center;flex:none}
.av.big{width:64px;height:64px;font-size:28px}
.secH{display:flex;align-items:center;justify-content:space-between;gap:12px;margin:30px 0 12px}
.secH h2,.secT{font-size:21px;font-weight:800;letter-spacing:-.02em}
.secT{font-size:18px;margin:22px 0 8px}
.resInfo{color:var(--tx2);font-size:13px;margin:16px 0 4px}

/* capas */
.art{position:relative;overflow:hidden;flex:none;display:flex;align-items:center;justify-content:center;background:#282828;color:#7a7a7a;border-radius:4px}
.art>svg{width:42%;height:42%}
.art img{position:absolute;top:0;right:0;bottom:0;left:0;width:100%;height:100%;object-fit:cover;opacity:0;transition:opacity .25s}
.art img.on{opacity:1}
.art.grad{color:#fff;background:linear-gradient(135deg,rgba(var(--acc-rgb),1) 0%,rgba(var(--acc-rgb),.55) 55%,#3a3a3a 100%)}
.art .ini{font-weight:800;line-height:1;text-shadow:0 2px 12px rgba(0,0,0,.25);font-size:22px}
.art.newPl{background:#2a2a2a;color:var(--tx2)}
.art.round,.art.round img{border-radius:50%}
.lyr{padding:4px 2px 10px}
.lyr b,.lyr span{display:block;padding:9px 12px;border-radius:8px;font-size:15px;line-height:1.35}
.lyr span{color:var(--tx2);font-weight:500}
.lyr b{color:#fff;background:rgba(255,255,255,.08);font-weight:700}
.lyrNote{color:var(--tx3);font-size:12px;padding:6px 12px}
.s44{width:44px;height:44px}
.s48{width:48px;height:48px}
.s56{width:56px;height:56px}
.s64{width:64px;height:64px}
.s48 .ini{font-size:20px}.s56 .ini{font-size:22px}.s64 .ini{font-size:26px}
.c140{width:100%;aspect-ratio:1/1;height:auto;border-radius:6px;box-shadow:0 6px 16px rgba(0,0,0,.35)}
@supports not (aspect-ratio:1/1){.card .c140{height:clamp(128px,38vw,168px)}}
.c140 .ini{font-size:56px}
.artBig{width:min(230px,62vw);height:min(230px,62vw);border-radius:6px;box-shadow:0 14px 40px rgba(0,0,0,.5)}
.artBig .ini{font-size:96px}
.fill{position:absolute;top:0;right:0;bottom:0;left:0;width:100%;height:100%;border-radius:inherit}

/* inicio */
.sc{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px}
.scI{position:relative;display:flex;align-items:center;gap:10px;height:56px;padding-right:8px;border-radius:6px;overflow:hidden;background:rgba(255,255,255,.08);font-weight:700;font-size:13px;line-height:1.2;transition:background .15s}
.scI:active{background:rgba(255,255,255,.16)}
.scI .art{border-radius:0}
.scN{flex:1;min-width:0;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;word-break:break-word}
.scI .eq{display:none}
.scI.cur .scN{color:var(--acc-txt)}
.scI.cur .eq{display:flex}
.car{display:flex;gap:14px;overflow-x:auto;scroll-snap-type:x proximity;margin:0 calc(-1 * var(--padR)) 0 calc(-1 * var(--padL));padding:2px var(--padR) 6px var(--padL);scroll-padding-left:var(--padL);scrollbar-width:none;-webkit-overflow-scrolling:touch}
.car::-webkit-scrollbar{display:none}
.card{flex:none;width:clamp(128px,38vw,168px);scroll-snap-align:start;display:flex;flex-direction:column;gap:2px;text-align:left;transition:transform .1s}
.card:active{transform:scale(.97)}
.cT{margin-top:8px;font-size:13.5px;font-weight:700;line-height:1.25;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;word-break:break-word}
.cS{font-size:12.5px;color:var(--tx2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.card.cur .cT{color:var(--acc-txt)}

/* vazio */
.empty{text-align:center;padding:36px 8px 12px;color:var(--tx2)}
.empty>svg{width:52px;height:52px;margin:0 auto 14px;color:#7a7a7a}
.empty h3{color:var(--tx);font-size:20px;font-weight:800;letter-spacing:-.01em;margin-bottom:8px}
.empty p{font-size:14.5px;max-width:420px;margin:0 auto}
.emptyActs{display:flex;flex-wrap:wrap;gap:10px;justify-content:center;margin-top:20px}
.loading{display:flex;flex-direction:column;align-items:center;gap:16px;padding:48px 16px;color:var(--tx2);text-align:center}

/* buscar */
.sbox{display:flex;align-items:center;gap:6px;height:48px;border-radius:8px;background:#fff;color:#121212;padding:0 2px 0 12px}
.sbox>svg{color:#121212}
.sbox input{flex:1;min-width:0;height:100%;border:0;background:transparent;color:#121212;font-size:16px;font-weight:600;padding:0 4px;-webkit-appearance:none;appearance:none}
.sbox input::placeholder{color:#6a6a6a;font-weight:500}
.sbox input::-webkit-search-cancel-button{display:none}
.sbox input:focus-visible{outline:none}
.sbox:focus-within{box-shadow:0 0 0 2px var(--acc-txt)}
.sbox .clr{color:#121212}
.chips{display:flex;gap:8px;overflow-x:auto;margin:11px calc(-1 * var(--padR)) 0 calc(-1 * var(--padL));padding:6px var(--padR) 6px var(--padL);scrollbar-width:none}
.chips::-webkit-scrollbar{display:none}
.chips.sub{margin-top:0}
.chip{position:relative;flex:none;height:34px;padding:0 15px;border-radius:17px;background:#2a2a2a;color:var(--tx);font-size:14px;white-space:nowrap;transition:background .15s}
.chip::before{content:"";position:absolute;left:0;right:0;top:-6px;bottom:-6px}
.chip.on{background:var(--accent);color:var(--on-acc);font-weight:600}
.chips.sub .chip{height:32px;font-size:13px;background:transparent;border:1px solid #535353}
.chips.sub .chip::before{top:-7px;bottom:-7px}
.chips.sub .chip.on{background:#fff;color:#121212;border-color:#fff}
.sGo{display:flex;justify-content:flex-start;margin-top:10px}
.tiles{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:10px}
.tile{position:relative;height:100px;border-radius:8px;overflow:hidden;padding:12px;text-align:left;background:#333;transition:transform .1s}
.tile:active{transform:scale(.97)}
.tileT{position:relative;z-index:1;display:-webkit-box;-webkit-line-clamp:2;-webkit-box-orient:vertical;overflow:hidden;font-weight:800;font-size:16px;letter-spacing:-.01em;max-width:72%;word-break:break-word;text-shadow:0 1px 6px rgba(0,0,0,.25)}
.tileArt{position:absolute;right:-12px;bottom:-4px;width:66px;height:66px;transform:rotate(25deg);box-shadow:0 2px 10px rgba(0,0,0,.45)}
.tileArt .ini{font-size:28px}

/* listas */
.plist{display:flex;flex-direction:column;margin-top:8px}
.prow{display:flex;align-items:center;gap:12px;width:100%;min-height:76px;padding:6px 0;text-align:left;border-radius:6px;transition:background .15s}
.prow:active{background:rgba(255,255,255,.05)}
.pTx{flex:1;min-width:0;display:flex;flex-direction:column;gap:3px}
.pn{font-size:16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ps{font-size:13px;color:var(--tx2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.prow.cur .pn{color:var(--acc-txt)}
.hasIc{color:var(--accent);padding:0 6px}
.tlist{display:flex;flex-direction:column;margin-top:6px}
.trow{display:flex;align-items:center;min-height:64px;border-radius:4px}
.tmain{flex:1;min-width:0;display:flex;align-items:center;gap:12px;min-height:64px;padding:8px 0;border-radius:4px;text-align:left;transition:background .15s}
.tmain:active{background:rgba(255,255,255,.05)}
.tnum{width:24px;flex:none;text-align:center;color:var(--tx2);font-size:14px;font-variant-numeric:tabular-nums;display:flex;justify-content:center}
.ttx{flex:1;min-width:0;display:flex;flex-direction:column;gap:3px}
.tt{font-size:16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.ta{display:flex;align-items:center;gap:6px;min-width:0;font-size:13.5px;color:var(--tx2)}
.tan{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.badge{flex:none;display:inline-block;font-size:9px;line-height:1;font-weight:800;letter-spacing:.06em;padding:3px 4px;border-radius:2px;background:var(--tx2);color:#121212}
.tdur{flex:none;color:var(--tx2);font-size:13px;font-variant-numeric:tabular-nums}
.miniT,.miniA{display:block}
.tmore{margin-right:-8px}
.trow.cur .tt{color:var(--acc-txt)}
.trow .eq{display:none}
.trow.cur .tnum>span:first-child{display:none}
.trow.cur .eq{display:flex}
.eq{height:14px;gap:2px;align-items:flex-end;justify-content:center;flex:none}
.eq i{display:block;width:3px;height:100%;background:var(--acc-txt);border-radius:1px;transform-origin:bottom;animation:eq 1s ease-in-out infinite;transform:scaleY(.35)}
.eq i:nth-child(2){animation-delay:-.4s}
.eq i:nth-child(3){animation-delay:-.7s}
body:not(.isPlaying) .eq i{animation:none}
body.hasBeat.isPlaying .eq i{animation:none;transform:scaleY(calc(.28 + .72*var(--beat,0)))}   /* no ritmo de verdade (dados do PC) */
body.hasBeat.isPlaying .eq i:nth-child(2){transform:scaleY(calc(.4 + .6*var(--beat,0)))}
body.hasBeat.isPlaying .eq i:nth-child(3){transform:scaleY(calc(.22 + .5*var(--beat,0)))}
@keyframes eq{0%,100%{transform:scaleY(.3)}50%{transform:scaleY(1)}}
.scI .eq{margin-right:4px}

/* playlist */
.plView{padding-top:0}
.plBar{position:sticky;top:0;z-index:5;display:flex;align-items:center;gap:6px;height:calc(56px + var(--sat));padding:var(--sat) var(--padR) 0 calc(4px + var(--sal));margin:0 calc(-1 * var(--padR)) 0 calc(-1 * var(--padL));transition:background-color .2s}
.plBar .ib{color:#fff;background:rgba(0,0,0,.25)}
.plBar.solid{background-color:#121212;background-image:linear-gradient(rgba(var(--acc-rgb),.38),rgba(var(--acc-rgb),.38))}
.plBar.solid .ib{background:transparent}
.plBarT{flex:1;min-width:0;font-weight:700;font-size:16px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;opacity:0;transition:opacity .2s}
.plBar.solid .plBarT{opacity:1}
.plHead{display:flex;flex-direction:column;align-items:center;text-align:center;margin:calc(-56px - var(--sat)) calc(-1 * var(--padR)) 0 calc(-1 * var(--padL));padding:calc(64px + var(--sat)) var(--padR) 8px var(--padL);
  background:linear-gradient(180deg,rgba(var(--acc-rgb),.62) 0%,rgba(var(--acc-rgb),.24) 55%,rgba(var(--acc-rgb),0) 100%)}
.plArtW{margin-bottom:18px}
.plHeadTx{align-self:stretch;text-align:left;min-width:0}
.plName{font-size:clamp(22px,7vw,34px);font-weight:800;letter-spacing:-.02em;line-height:1.15;word-break:break-word}
.plSub{margin-top:8px;color:var(--tx2);font-size:14px}
.plMeta{margin-top:4px;color:var(--tx2);font-size:13px}
.plAct{display:flex;align-items:center;gap:8px;margin:8px 0 4px -10px}
.plAct .ib svg{width:28px;height:28px}
.shufB{margin-right:4px}

/* dock: mini player + navegacao */
#dock{position:fixed;left:0;right:0;bottom:0;z-index:20;pointer-events:none}
#dock>*{pointer-events:auto}
body.kbOpen #dock{display:none}
.mini{position:relative;display:flex;align-items:center;height:58px;touch-action:pan-y;will-change:transform;transition:transform .18s ease;max-width:1000px;margin:0 auto 4px;margin-left:max(8px,calc(8px + var(--sal)));margin-right:max(8px,calc(8px + var(--sar)));padding:0 4px 0 7px;border-radius:8px;overflow:hidden;
  background-color:#2b2b2b;background-image:linear-gradient(rgba(var(--acc-rgb),.28),rgba(var(--acc-rgb),.14));box-shadow:0 4px 18px rgba(0,0,0,.45)}
@media (min-width:1016px){.mini{margin-left:auto;margin-right:auto}}
.miniOpen{flex:1;min-width:0;display:flex;align-items:center;gap:10px;height:100%;text-align:left;color:#fff}
.miniArt{position:relative;display:block;width:44px;height:44px;border-radius:4px;overflow:hidden;flex:none;background:#282828}
.miniTx{flex:1;min-width:0}
.miniT{font-size:14px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.miniA{font-size:13px;color:rgba(255,255,255,.72);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.mini .ib{color:#fff}
.mini .ib svg{width:26px;height:26px}
.miniBar{position:absolute;left:8px;right:8px;bottom:0;height:2px;border-radius:1px;background:rgba(255,255,255,.22);overflow:hidden}
#miniFill{height:100%;background:#fff;transform-origin:left;transform:scaleX(0)}
.nav{display:flex;height:calc(var(--navH) + var(--sab));padding:6px var(--sar) var(--sab) var(--sal);background:linear-gradient(to top,rgba(0,0,0,.98) 55%,rgba(0,0,0,.82) 80%,rgba(0,0,0,0))}
.nav button{flex:1;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:3px;min-height:48px;color:var(--tx2);font-size:11px;letter-spacing:.01em;transition:color .15s}
.nav button.on{color:#fff}
.nav button:active .nIc{transform:scale(.9)}
.nIc{display:block;transition:transform .1s}
.nIc svg{width:25px;height:25px}

/* tocando agora */
.np{position:fixed;top:0;right:0;bottom:0;left:0;z-index:40;overflow-y:auto;overscroll-behavior:contain;background-color:#121212;
  background-image:linear-gradient(180deg,rgba(var(--acc-rgb),.58) 0%,rgba(var(--acc-rgb),.2) 50%,rgba(18,18,18,1) 92%);
  transform:translateY(100%);visibility:hidden;transition:transform .34s cubic-bezier(.2,.8,.2,1),visibility 0s linear .34s}
.np.open{transform:none;visibility:visible;transition:transform .34s cubic-bezier(.2,.8,.2,1),visibility 0s}
.npIn{display:flex;flex-direction:column;min-height:100%;max-width:560px;margin:0 auto;padding:var(--sat) calc(20px + var(--sar)) calc(18px + var(--sab)) calc(20px + var(--sal))}
.npTop{display:flex;align-items:center;gap:6px;min-height:60px;margin:0 -10px}
.npTop .ib{color:#fff}
.npTop .ib svg{width:28px;height:28px}
.npFrom{flex:1;min-width:0;min-height:44px;display:flex;flex-direction:column;align-items:center;justify-content:center;text-align:center;border-radius:8px}
.npFrom small{font-size:10.5px;letter-spacing:.12em;color:rgba(255,255,255,.75);font-weight:600}
.npFrom b{font-size:13.5px;max-width:100%;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.npBody{flex:1;display:flex;flex-direction:column}
.npArtW{flex:1;display:flex;align-items:center;justify-content:center;padding:14px 0 22px;min-height:0}
.npArt{position:relative;width:min(100%,50vh);width:min(100%,50svh);aspect-ratio:1/1;border-radius:8px;overflow:hidden;background:#282828;box-shadow:0 18px 48px rgba(0,0,0,.5)}
.npArt .art>svg{width:34%;height:34%}
.npArt .art{color:#6f6f6f;background:linear-gradient(135deg,#333,#1c1c1c)}
@supports not (aspect-ratio:1/1){.npArt{height:min(calc(100vw - 40px),50vh,520px)}}
.npMeta{display:flex;align-items:center;gap:10px}
.npTx{flex:1;min-width:0}
.npT{font-size:22px;font-weight:800;letter-spacing:-.01em;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.npAw{display:flex;align-items:center;gap:6px;min-width:0;margin-top:2px}
.npA{font-size:15.5px;color:rgba(255,255,255,.72);white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.npMeta .ib{color:#fff;margin-right:-10px}
.npMeta .ib svg{width:28px;height:28px}
.npSeek{margin-top:8px;touch-action:none}
.npTimes{display:flex;justify-content:space-between;font-size:12px;color:rgba(255,255,255,.7);font-variant-numeric:tabular-nums;margin-top:-10px;padding-bottom:4px}
.npCtl{display:flex;align-items:center;justify-content:space-between;margin:10px -8px 0}
.npCtl .ib{color:#fff}
.npCtl .ib.on{color:var(--acc-txt)}
.npCtl .ib svg{width:26px;height:26px}
.npCtl .ib.big svg{width:36px;height:36px}
.playBig.xl{width:66px;height:66px;background:#fff;color:#121212}
.playBig.xl svg{width:30px;height:30px}
.npWave{display:block;width:100%;height:46px;margin-top:14px}
.npFxRow{display:flex;justify-content:center;margin-top:10px}
.npFxBtn{display:inline-flex;align-items:center;gap:8px;min-height:40px;max-width:100%;padding:0 16px;border-radius:20px;background:rgba(255,255,255,.1);color:#fff;font-size:13.5px;font-weight:600;transition:transform .1s,background .15s}
.npFxBtn svg{width:18px;height:18px;flex:none}
.npFxBtn span:last-child{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.npFxBtn.on{background:var(--accent);color:var(--on-acc)}
.npFxBtn:active{transform:scale(.96)}
.fxGrid{display:grid;grid-template-columns:repeat(auto-fill,minmax(96px,1fr));gap:10px;margin:6px 0 14px}
.fxB{display:flex;flex-direction:column;align-items:center;justify-content:center;gap:8px;min-height:74px;border-radius:12px;background:#2e2e2e;color:var(--tx);font-weight:700;font-size:15px;transition:transform .1s,background .15s}
.fxB:active{transform:scale(.96)}
.fxB.on{background:rgba(var(--acc-rgb),.28);box-shadow:inset 0 0 0 2px var(--accent)}
.fxD{display:flex;gap:6px}
.fxD i{display:block;width:9px;height:9px;border-radius:50%;background:rgba(255,255,255,.22)}
.fxD i.on{background:var(--acc-txt)}
.stemChips{display:flex;flex-wrap:wrap;gap:8px;margin:4px 0 8px}
.stemChips .chip{height:38px}
.stemSt{font-size:13.5px;color:var(--tx2);min-height:20px;margin:6px 0 2px}
.stemBar{height:4px;border-radius:2px;background:rgba(255,255,255,.12);overflow:hidden;margin-top:6px}
.stemBar i{display:block;height:100%;width:0;background:var(--accent);transition:width .3s}
.npMsg{margin-top:12px;padding:10px 12px;border-radius:8px;background:rgba(0,0,0,.35);color:#fff;font-size:13.5px;text-align:center}
.npDev{display:flex;align-items:center;justify-content:center;gap:6px;margin-top:16px;color:var(--acc-txt);font-size:12.5px;font-weight:600;min-width:0}
.npDev svg{width:16px;height:16px}
.npDev span:last-child{white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
@media (min-width:760px) and (orientation:landscape){
  .npIn{max-width:1040px}
  .npBody{flex-direction:row;align-items:center;gap:48px}
  .npArtW{flex:0 0 auto;padding:12px 0}
  .npArt{width:min(44vw,70vh)}
  .npPanel{flex:1;min-width:0}
}
@supports not (aspect-ratio:1/1){@media (min-width:760px) and (orientation:landscape){.npArt{height:min(44vw,70vh)}}}
@media (max-height:600px) and (orientation:portrait){.npArt{width:min(100%,40vh)}.npT{font-size:19px}}
@supports not (aspect-ratio:1/1){@media (max-height:600px) and (orientation:portrait){.npArt{height:min(calc(100vw - 40px),40vh)}}}

/* barra de posicao */
.rng{-webkit-appearance:none;appearance:none;display:block;width:100%;height:44px;margin:0;padding:0;background:transparent;cursor:pointer;touch-action:none;--p:0%}
.rng:disabled{cursor:default;opacity:.6}
.rng::-webkit-slider-runnable-track{height:4px;border-radius:2px;background:linear-gradient(to right,#fff var(--p),rgba(255,255,255,.28) var(--p))}
.rng::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:30px;height:30px;margin-top:-13px;border:0;border-radius:50%;background:radial-gradient(circle,#fff 0,#fff 6.5px,rgba(0,0,0,.3) 7.5px,rgba(0,0,0,0) 10px)}
.rng:active::-webkit-slider-runnable-track,.rng.drag::-webkit-slider-runnable-track{background:linear-gradient(to right,var(--accent) var(--p),rgba(255,255,255,.28) var(--p))}
.rng::-moz-range-track{height:4px;border-radius:2px;background:rgba(255,255,255,.28)}
.rng::-moz-range-progress{height:4px;border-radius:2px;background:#fff}
.rng::-moz-range-thumb{width:13px;height:13px;border:0;border-radius:50%;background:#fff}

/* folhas */
.sheetWrap{position:fixed;top:0;right:0;bottom:0;left:0;z-index:60}
.sheetWrap:not(.open){pointer-events:none}   /* fechando: o fundo invisivel nao engole o proximo toque */
.sheetBg{position:absolute;top:0;right:0;bottom:0;left:0;background:rgba(0,0,0,.62);opacity:0;transition:opacity .22s}
.sheet{position:absolute;left:0;right:0;bottom:var(--kb,0px);max-width:640px;margin:0 auto;max-height:88vh;max-height:88dvh;overflow-y:auto;overscroll-behavior:contain;
  background:#242424;border-radius:14px 14px 0 0;padding:4px calc(16px + var(--sar)) calc(18px + var(--sab)) calc(16px + var(--sal));
  transform:translateY(105%);transition:transform .26s cubic-bezier(.2,.8,.2,1);box-shadow:0 -8px 30px rgba(0,0,0,.4)}
.sheetWrap.open .sheetBg{opacity:1}
.sheetWrap.open .sheet{transform:none}
body.kbOpen .sheet{max-height:calc(100% - var(--kb,0px) - 12px);padding-bottom:18px}
.sheet:focus{outline:none}
.grab{width:38px;height:4px;border-radius:2px;background:#5e5e5e;margin:6px auto 12px}
.shHead{display:flex;align-items:center;gap:12px;padding-bottom:12px;margin-bottom:6px;border-bottom:1px solid #353535}
.shHeadTx{flex:1;min-width:0}
.shHead h2{font-size:16px;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.shHead p{font-size:13.5px;color:var(--tx2);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin-top:2px}
.shTitle{font-size:18px;font-weight:800;text-align:center;margin:2px 0 16px;letter-spacing:-.01em;word-break:break-word}
.shCenter{display:flex;justify-content:center;margin-bottom:10px}
.shNote{font-size:13px;color:var(--tx2);margin:14px 0 4px}
.shNote.center{text-align:center;font-size:14.5px;margin:0 0 6px}
.shBtns{display:flex;gap:10px;justify-content:center;margin-top:20px}
.linkBtns{margin:6px 0 14px;flex-wrap:wrap}
.shBtns>*{flex:1;max-width:220px}
.bigIn{margin:4px 0 0;text-align:center;font-size:20px;font-weight:700;height:56px;background:#2e2e2e;border-color:#3e3e3e}
.mi{display:flex;align-items:center;gap:16px;width:100%;min-height:54px;padding:6px 4px;border-radius:8px;text-align:left;font-size:16px;color:var(--tx)}
.mi>svg{color:var(--tx2)}
.mi:active{background:rgba(255,255,255,.06)}
.mi:disabled{opacity:.6}
.mi.danger,.mi.danger>svg{color:var(--danger)}
.miTx{flex:1;min-width:0;display:flex;flex-direction:column;gap:2px}
.miTx small{font-size:12.5px;color:var(--tx2)}
.miTx small.wait{color:#f5c26b}
.sw{position:relative;width:46px;height:28px;border-radius:14px;background:#535353;flex:none;transition:background .2s}
.sw::after{content:"";position:absolute;top:3px;left:3px;width:22px;height:22px;border-radius:50%;background:#fff;transition:transform .2s}
.sw.on{background:var(--accent)}
.sw.on::after{transform:translateX(18px)}
.prof{display:flex;align-items:center;gap:14px;padding:4px 0 14px}
.profTx{min-width:0}
.profTx h2{font-size:20px;font-weight:800;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.profTx p{color:var(--tx2);font-size:14px}
.info{display:grid;grid-template-columns:auto 1fr;gap:6px 14px;margin:0 0 10px;padding:12px 14px;border-radius:8px;background:#2c2c2c;font-size:13.5px}
.info dt{color:var(--tx2)}
.info dd{margin:0;text-align:right;min-width:0;word-break:break-word;overflow-wrap:anywhere}
.sheet .prow{min-height:64px}

/* aviso */
.toast{position:fixed;left:50%;bottom:calc(var(--navH) + var(--miniH) + var(--sab) + 14px);z-index:80;max-width:min(92vw,520px);width:max-content;
  padding:12px 16px;border-radius:8px;background:#fff;color:#121212;font-size:14.5px;font-weight:600;box-shadow:0 8px 24px rgba(0,0,0,.45);
  transform:translate(-50%,20px);opacity:0;pointer-events:none;transition:opacity .2s,transform .2s}
.toast.show{opacity:1;transform:translate(-50%,0)}
.toast.err{background:#3b1f23;color:#ffd7db;border:1px solid #6d3139}
body.lock .toast{bottom:calc(24px + var(--sab))}

/* tablet */
@media (min-width:700px){
  .sc{grid-template-columns:repeat(4,minmax(0,1fr))}
  .tiles{grid-template-columns:repeat(4,minmax(0,1fr))}
  .plHead{flex-direction:row;align-items:flex-end;gap:24px;text-align:left;padding-bottom:20px}
  .plArtW{margin:0}
  .plHeadTx{flex:1;align-self:flex-end}
  .plName{font-size:clamp(28px,5vw,56px)}
  .top h1{font-size:28px}
}
@media (max-width:360px){
  .view{--padL:calc(12px + var(--sal));--padR:calc(12px + var(--sar))}
  .top h1{font-size:21px}
  .tdur{display:none}
  .npCtl .ib.big svg{width:32px;height:32px}
  .playBig.xl{width:58px;height:58px}
}
@media (prefers-reduced-motion:reduce){
  *,*::before,*::after{transition-duration:.01ms!important;animation-duration:1.5s!important}
  .eq i{animation:none!important}
}
)~~~";

// ------------------------------------------------------------------- js --
// Telas: Inicio, Buscar, Sua Biblioteca, playlist, Tocando agora (tela cheia) e folhas.
// Cada tela sobreposta empilha uma entrada no historico (o Voltar do Android fecha a de cima).
// Musica local: /api/faixa/<id> (Range). Online: /api/online/ouvir/<id>?t=<s> (stream ao vivo,
// o avanco recria o src com ?t= e soma o deslocamento no tempo mostrado).
static const char* APP_JS = R"~~~('use strict';
// Remix Host - app do celular. Sem bibliotecas, sem script inline (CSP), dados do
// servidor so entram na pagina por textContent ou atributos montados aqui.
// localStorage guarda so preferencias locais (ultimo nome digitado, aba, filtros).
// Erro antes de a tela aparecer: mostra o motivo em vez da rodinha eterna.
window.addEventListener('error',ev=>{
  const b=document.getElementById('boot');
  if(b&&!b.hidden&&!b.querySelector('.bootErr')){const p=document.createElement('p');p.className='bootHelp now bootErr';p.textContent='Erro ao abrir: '+((ev&&ev.message)||'desconhecido')+'. Toque em recarregar.';b.appendChild(p);}
});
window.addEventListener('unhandledrejection',ev=>{const r=ev&&ev.reason;if(!r||r.name==='AbortError'||r.st===401)return;try{toast(r.message||'Algo deu errado.',1);}catch(e){}});
const $=id=>document.getElementById(id);
const enc=encodeURIComponent;
const STANDALONE=window.navigator.standalone===true||!!(window.matchMedia&&window.matchMedia('(display-mode: standalone)').matches);
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const LS={
  get(k,d){try{const v=localStorage.getItem('remix_'+k);return v==null?d:v;}catch(e){return d;}},
  set(k,v){try{localStorage.setItem('remix_'+k,String(v));}catch(e){}}
};
function str(v,max){if(v==null)return '';const s=typeof v==='string'?v:String(v);return s.length>(max||300)?s.slice(0,max||300):s;}

// ---------------------------------------------------------------- DOM --
// h('div',{class:'x',text:'y',onclick:fn,'aria-label':'z'},filhos...)
function h(tag,p,...kids){
  const e=document.createElement(tag);
  if(p)for(const k in p){
    const v=p[k];if(v==null||v===false)continue;
    if(k==='class')e.className=v;
    else if(k==='text')e.textContent=v;
    else if(k.startsWith('on')&&typeof v==='function')e.addEventListener(k.slice(2),v);
    else e.setAttribute(k,v===true?'':String(v));
  }
  for(const c of kids.flat()){if(c==null||c===false)continue;e.appendChild(typeof c==='string'?document.createTextNode(c):c);}
  return e;
}
const SVGNS='http://www.w3.org/2000/svg';
// Icones desenhados aqui (24x24). p=traco, P=traco grosso, f=preenchido, k=traco na cor do texto
// sobre o destaque, c/C=circulo (traco normal/grosso), cf=circulo cheio, r=retangulo cheio.
const IC={
  home:[['p','M3 10.2 12 3l9 7.2V20a1 1 0 0 1-1 1h-5.5v-6h-5v6H4a1 1 0 0 1-1-1z']],
  homeF:[['f','M3 10.2 12 3l9 7.2V20a1 1 0 0 1-1 1h-5.5v-6h-5v6H4a1 1 0 0 1-1-1z']],
  search:[['c',11,11,7],['p','m20.5 20.5-4.6-4.6']],
  searchF:[['C',11,11,7],['P','m20.5 20.5-4.6-4.6']],
  lib:[['p','M4.5 3.5v17M10 3.5v17M14.8 4.3l5 15.9']],
  libF:[['P','M4.5 3.5v17M10 3.5v17M14.8 4.3l5 15.9']],
  play:[['f','M7.5 4.6v14.8a1 1 0 0 0 1.52.85l12-7.4a1 1 0 0 0 0-1.7l-12-7.4A1 1 0 0 0 7.5 4.6z']],
  pause:[['r',6,4.5,4,15],['r',14,4.5,4,15]],
  next:[['f','M4.5 5.3v13.4a.9.9 0 0 0 1.38.76l10.4-6.7a.9.9 0 0 0 0-1.52L5.88 4.54A.9.9 0 0 0 4.5 5.3z'],['r',17,4.5,2.6,15]],
  prev:[['f','M19.5 5.3v13.4a.9.9 0 0 1-1.38.76l-10.4-6.7a.9.9 0 0 1 0-1.52l10.4-6.7a.9.9 0 0 1 1.38.76z'],['r',4.4,4.5,2.6,15]],
  shuffle:[['p','M16 3.5h4.5V8M3.5 19.5l17-16M20.5 16v4.5H16M14.5 14.5l6 6M3.5 4.5l5.5 5.5']],
  repeat:[['p','M17 2.5 20.5 6 17 9.5M3.5 11.5V10A4 4 0 0 1 7.5 6h13M7 21.5 3.5 18 7 14.5M20.5 12.5V14a4 4 0 0 1-4 4h-13']],
  repeat1:[['p','M17 2.5 20.5 6 17 9.5M3.5 11.5V10A4 4 0 0 1 7.5 6h13M7 21.5 3.5 18 7 14.5M20.5 12.5V14a4 4 0 0 1-4 4h-13'],['p','M11.2 10.6l1.3-.9v4.8']],
  chevDown:[['p','m5.5 9 6.5 6.5L18.5 9']],
  back:[['p','M15 5.5 8.5 12l6.5 6.5']],
  more:[['cf',5,12,1.8],['cf',12,12,1.8],['cf',19,12,1.8]],
  plus:[['p','M12 5v14M5 12h14']],
  plusC:[['c',12,12,9.5],['p','M12 7.5v9M7.5 12h9']],
  check:[['p','m5 12.5 4.5 4.5L19 7.5']],
  checkC:[['cf',12,12,10],['k','m7.5 12.3 3 3 6-6.3']],
  note:[['p','M9 18V5.5l11-2V16'],['c',6,18,3],['c',17,16,3]],
  trash:[['p','M4 6.5h16M9.5 6.5V4h5v2.5M6.5 6.5l1 13.5h9l1-13.5M10 10.5v6M14 10.5v6']],
  edit:[['p','M4 20h4.2L19.5 8.7l-4.2-4.2L4 15.8zM13.5 6.3l4.2 4.2']],
  share:[['c',18,5.5,2.5],['c',6,12,2.5],['c',18,18.5,2.5],['p','m8.3 10.8 7.4-4.1M8.3 13.2l7.4 4.1']],
  close:[['p','M6 6l12 12M18 6 6 18']],
  globe:[['c',12,12,9],['p','M3 12h18M12 3c2.5 2.6 3.8 5.6 3.8 9s-1.3 6.4-3.8 9c-2.5-2.6-3.8-5.6-3.8-9S9.5 5.6 12 3z']],
  pc:[['p','M4 4.5h16a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1H4a1 1 0 0 1-1-1v-10a1 1 0 0 1 1-1zM8.5 20.5h7M12 16.5v4']],
  phone:[['p','M8 2.5h8a1.5 1.5 0 0 1 1.5 1.5v16a1.5 1.5 0 0 1-1.5 1.5H8A1.5 1.5 0 0 1 6.5 20V4A1.5 1.5 0 0 1 8 2.5zM11 18h2']],
  logout:[['p','M14.5 4H18a2 2 0 0 1 2 2v12a2 2 0 0 1-2 2h-3.5M9.5 16.5 5 12l4.5-4.5M5 12h11']],
  refresh:[['p','M20 11.5A8 8 0 1 0 17.7 17M20 4.5v7h-7']],
  queue:[['p','M4 6h12M4 11h12M4 16h6M15.5 14v6.5l5-3.25z']],
  goPl:[['p','M4 6h11M4 11h11M4 16h7'],['c',17.5,17.5,2.5],['p','M20 17.5V7.5l2.5-1']],
  fx:[['p','M5 20v-6M5 10V4M12 20v-9M12 7V4M19 20v-4M19 12V4M2.5 14h5M9.5 7h5M16.5 16h5']]
};
function ic(name){
  const s=document.createElementNS(SVGNS,'svg');
  const at=(e,o)=>{for(const k in o)e.setAttribute(k,String(o[k]));return e;};
  at(s,{viewBox:'0 0 24 24','aria-hidden':'true',focusable:'false',fill:'none',stroke:'currentColor','stroke-width':'2','stroke-linecap':'round','stroke-linejoin':'round'});
  for(const it of IC[name]||[]){
    const k=it[0];let e;
    if(k==='p'||k==='P'||k==='f'||k==='k'){
      e=at(document.createElementNS(SVGNS,'path'),{d:it[1]});
      if(k==='f')at(e,{fill:'currentColor',stroke:'none'});
      if(k==='P')at(e,{'stroke-width':'2.8'});
      if(k==='k')e.setAttribute('class','ick');
    }else if(k==='c'||k==='C'||k==='cf'){
      e=at(document.createElementNS(SVGNS,'circle'),{cx:it[1],cy:it[2],r:it[3]});
      if(k==='cf')at(e,{fill:'currentColor',stroke:'none'});
      if(k==='C')at(e,{'stroke-width':'2.8'});
    }else if(k==='r'){
      e=at(document.createElementNS(SVGNS,'rect'),{x:it[1],y:it[2],width:it[3],height:it[4],rx:1,fill:'currentColor',stroke:'none'});
    }
    if(e)s.appendChild(e);
  }
  return s;
}
function setIc(el,name){if(el.dataset.icn===name)return;el.dataset.icn=name;el.textContent='';el.appendChild(ic(name));}

// ----------------------------------------------------------- formatos --
function fmt(s){s=Math.max(0,Math.floor(s||0));const hh=Math.floor(s/3600),m=Math.floor(s%3600/60),ss=String(s%60).padStart(2,'0');return hh?hh+':'+String(m).padStart(2,'0')+':'+ss:m+':'+ss;}
function plural(n,um,varios){return n+' '+(n===1?um:varios);}
function totalDur(list){
  let s=0,unk=0;for(const t of list){if(t.d)s+=t.d;else unk++;}
  if(!s)return '';
  const hh=Math.floor(s/3600),m=Math.floor(s%3600/60);
  const txt=hh?hh+' h '+m+' min':(m?m+' min '+(s%60)+' s':s+' s');
  return (unk?'mais de ':'')+txt;
}
function fold(s){return str(s,500).normalize('NFD').replace(/[\u0300-\u036f]/g,'').toLowerCase();}
function initial(s){const m=str(s).trim().match(/[\p{L}\p{N}]/u);return m?m[0].toUpperCase():'♪';}
function hueOf(s){let x=0;for(const ch of str(s))x=(x*31+ch.codePointAt(0))>>>0;return x%360;}

// ------------------------------------------------------ dados validados --
const HEX16=/^[0-9a-f]{16}$/i;
function nT(x){
  if(!x||typeof x!=='object')return null;
  const id=str(x.id,32);if(!HEX16.test(id))return null;
  return {id,t:str(x.t)||'Sem título',a:str(x.a),d:Math.max(0,Math.floor(+x.d||0))||0,c:!!x.c,o:!!x.o};
}
function nP(x,kind){
  if(!x||typeof x!=='object')return null;
  const slug=str(x.slug,120);if(!slug)return null;
  const p={kind,slug,nome:str(x.nome,120)||'Playlist',itens:Array.isArray(x.itens)?x.itens.map(nT).filter(Boolean):[],
    compartilhar:!!x.compartilhar,liberada:!!x.liberada,dono:str(x.dono,60)};
  p.key=kind+':'+(kind==='sh'?p.dono+'/':'')+slug;
  return p;
}

// ---------------------------------------------------------------- API --
const ERR={pin:'PIN errado.',qr:'Este QR code expirou ou já foi usado. Gere outro no painel HOST do PC.',
  travado:'Muitas tentativas erradas. Espere um minuto e tente de novo.',pedidos:'Já tem pedidos demais esperando no PC.',
  calma:'Calma: muitas ações seguidas. Espere alguns segundos.',sem_ferramentas:'O PC não tem as ferramentas de música online (CLI de midia e ffmpeg).',
  online_desligado:'A busca online está desligada no PC.',limite:'Limite atingido.',playlist:'Playlist não encontrada.',
  faixa:'Essa música não está disponível para este aparelho.',nome:'Digite um nome.',pareie:'Este aparelho não está conectado ao PC.',
  origem:'O PC recusou o pedido.',rota:'O PC não entendeu o pedido (versões diferentes do Remix?).',rede:'Sem conexão com o PC.',
  buscando:'O PC ainda está terminando a busca anterior. Espere alguns segundos.',
  link:'Isso não parece um link.',link_nao_suportado:'Link não suportado. Use YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music ou Bandcamp.',
  link_vazio:'Não achei músicas nesse link.',
  chave:'Esse link de aparelho não vale mais neste PC. Peça um novo no painel HOST.'};
function errTxt(c){return c&&Object.prototype.hasOwnProperty.call(ERR,c)?ERR[c]:'';}
class ApiErr extends Error{constructor(st,code,msg){super(msg||errTxt(code)||('Algo deu errado ('+st+').'));this.st=st;this.code=code||'';this.msg=msg||'';}}
// O PC limita pedidos por aparelho (tudo conta: API, capas e audio). As capas esperam a vez.
const reqLog=[];
function recentReq(){const now=Date.now();while(reqLog.length&&now-reqLog[0]>10000)reqLog.shift();return reqLog.length;}
function noteReq(n){const now=Date.now();for(let i=0;i<(n||1);i++)reqLog.push(now);recentReq();}
async function raw(path,body,signal){
  const o={method:body?'POST':'GET',credentials:'same-origin',cache:'no-store',headers:{}};
  if(body){o.headers['Content-Type']='application/json';o.headers['X-Remix']='1';o.body=JSON.stringify(body);}
  // sem sinal de quem chamou: desiste em 25 s (senao o iOS pode ficar um minuto so com a rodinha)
  let own=null,tm=0;
  if(signal)o.signal=signal;
  else if(window.AbortController){own=new AbortController();o.signal=own.signal;tm=setTimeout(()=>own.abort(),25000);}
  noteReq();
  let r;
  try{r=await fetch(path,o);}catch(e){clearTimeout(tm);if(e&&e.name==='AbortError'&&!own)throw e;throw new ApiErr(0,'rede');}
  let j=null;try{j=await r.json();}catch(e){}
  clearTimeout(tm);
  if(!j||typeof j!=='object')j={};
  return {st:r.status,ok:r.ok,j};
}
async function api(path,body,signal){
  const r=await raw(path,body,signal);
  if(r.st===401){if(S.ready)toPair('Este aparelho foi desconectado do PC.');throw new ApiErr(401,'pareie');}
  if(!r.ok){const c=str(r.j.erro,40);throw new ApiErr(r.st,r.st===429&&!errTxt(c)?'calma':c,str(r.j.msg,200));}   // 429: travado, pedidos, buscando ou calma
  return r.j;
}

// ------------------------------------------------------------- estado --
const S={
  ready:false,host:'',dev:'',ver:'',libOk:false,online:false,nFaixas:0,
  lib:[],pc:[],mine:[],sh:[],lastLoad:0,
  tab:'home',scroll:{home:0,search:0,lib:0},plKey:'',
  libFilter:'all',
  sq:{mode:'pc',fonte:0,q:'',res:[],busy:false,err:'',done:'',doneF:0,ctl:null,link:'',linkNome:'',linkFonte:''},
  // player
  queue:[],order:[],pos:-1,cur:null,off:0,ctx:{name:'',key:''},shuffle:false,repeat:0,errs:0,rate:1,
  // efeitos (0..3, aplicados pelo PC) e stems ('' = completa)
  fx:{slow:0,speed:0,reverb:0,bass:0,d8:0},stem:'',stemsOk:false,stemReady:{}
};

// -------------------------------------------------------------- toast --
let toastT=0;
function toast(msg,err){
  const t=$('toast');t.textContent=msg;t.className='toast show'+(err?' err':'');
  clearTimeout(toastT);toastT=setTimeout(()=>{t.className='toast'+(err?' err':'');},err?4000:2600);
}
function fail(e){if(e&&e.name==='AbortError')return;if(e&&e.st===401)return;toast((e&&e.message)||'Algo deu errado.',1);}

// ------------------------------------------- camadas + botao Voltar --
// Cada tela sobreposta (playlist, Tocando agora, folha) empilha uma entrada no historico;
// o Voltar do Android fecha a de cima antes de sair da pagina.
const L=[];let popWait=[],popQ=Promise.resolve();
const closers={pl:()=>closePlNow(),np:()=>closeNpNow(),sheet:()=>closeSheetNow()};
function pushL(name){L.push(name);try{history.pushState({rl:L.length},'');}catch(e){}}
// Um voltar por vez: dois history.go() seguidos se somam no navegador (o segundo sairia da
// pagina; ex.: 401 com o Tocando agora aberto fechava o np e depois voltava tudo de novo).
function popTo(depth){
  const run=()=>{
    if(L.length<=depth)return;
    return new Promise(res=>{
      let done=false;const fin=()=>{if(!done){done=true;res();}};
      popWait.push(fin);
      try{history.go(depth-L.length);}catch(e){}
      setTimeout(()=>{ // navegador que nao disparou popstate: fecha na mao
        if(done)return;
        while(L.length>depth)closers[L.pop()]();
        fin();
      },700);
    });
  };
  return popQ=popQ.then(run,run);
}
function closeL(name){const i=L.lastIndexOf(name);return i<0?Promise.resolve():popTo(i);}
window.addEventListener('popstate',e=>{
  const d=(e.state&&e.state.rl)|0;
  while(L.length>d)closers[L.pop()]();
  const w=popWait;popWait=[];w.forEach(f=>f());
});
let lockY=0;
function updLock(){
  const on=SH.open||NP.open,b=document.body;
  if(on===b.classList.contains('lock'))return;
  if(on){lockY=window.scrollY||window.pageYOffset||0;b.style.top=(-lockY)+'px';b.classList.add('lock');}
  else{b.classList.remove('lock');b.style.top='';window.scrollTo(0,lockY);}
}

// ------------------------------------------------------------- folhas --
const SH={open:false,at:0};
function sheet(build){
  const wrap=$('sheetWrap'),box=$('sheet');
  clear(box);box.appendChild(h('div',{class:'grab','aria-hidden':'true'}));
  build(box);
  SH.at=Date.now();   // toque duplo: o 2o toque nao cai no que acabou de aparecer
  if(!SH.open){
    SH.open=true;wrap.hidden=false;void wrap.offsetWidth;wrap.classList.add('open');pushL('sheet');updLock();
  }
  box.scrollTop=0;
  const f=box.querySelector('[data-autofocus]');
  // foco direto no toque: no iOS o teclado so abre se o focus() vier do proprio toque (setTimeout nao abre)
  if(f){try{f.focus({preventScroll:true});if(f.setSelectionRange)f.setSelectionRange(0,f.value.length);}catch(e){}}
  else{try{box.focus({preventScroll:true});}catch(e){}}
}
function closeSheetNow(){
  SH.open=false;LYR.aberta=false;const wrap=$('sheetWrap');wrap.classList.remove('open');updLock();
  setTimeout(()=>{if(!SH.open){wrap.hidden=true;clear($('sheet'));}},280);
}
const closeSheet=()=>closeL('sheet');
function shHead(title,sub,artEl){
  return h('div',{class:'shHead'},artEl||null,h('div',{class:'shHeadTx'},h('h2',{text:title}),sub?h('p',{text:sub}):null));
}
function mi(icon,label,fn,cls){
  return h('button',{class:'mi'+(cls?' '+cls:''),type:'button',onclick:fn},ic(icon),h('span',{text:label}));
}
function dragDown(target,area,close,canStart){
  let y0=null,dy=0;
  area.addEventListener('touchstart',e=>{
    if(e.touches.length!==1||(canStart&&!canStart(e)))return;
    y0=e.touches[0].clientY;dy=0;target.style.transition='none';
  },{passive:true});
  area.addEventListener('touchmove',e=>{
    if(y0==null)return;dy=Math.max(0,e.touches[0].clientY-y0);
    if(dy>0&&e.cancelable)e.preventDefault();   // puxando para baixo: a pagina de tras nao rola nem pula
    target.style.transform=dy?'translateY('+dy+'px)':'';
  },{passive:false});
  const end=()=>{
    if(y0==null)return;y0=null;target.style.transition='';
    if(dy>110){target.style.transform='translateY(105%)';close();setTimeout(()=>{target.style.transform='';},450);}   // desce daqui mesmo
    else target.style.transform='';
  };
  area.addEventListener('touchend',end);area.addEventListener('touchcancel',end);
}

// -------------------------------------------------------------- capas --
// Carrega so o que aparece na tela, poucas por vez e respeitando o limite do PC.
const COV={q:[],ok:new Set(),busy:0,t:0,io:null};
function coverUrl(t){return t.dc?('/api/desccapa/'+enc(t.c)):('/api/capa/'+enc(t.id));}
function art(t,cls,opt){
  opt=opt||{};
  const w=h('div',{class:'art '+(cls||'')});
  if(opt.round)w.classList.add('round');
  if(opt.pl){w.classList.add('grad');w.appendChild(h('span',{class:'ini',text:initial(opt.pl)}));}
  else if(opt.lib){w.classList.add('grad');w.appendChild(ic('pc'));}
  else w.appendChild(ic('note'));
  if(t&&t.c){
    const img=h('img',{alt:'',decoding:'async',draggable:'false'});
    img.dataset.src=coverUrl(t);w.appendChild(img);
    if(COV.ok.has(img.dataset.src))setSrc(img);
    else if(COV.io)COV.io.observe(img);
    else{COV.q.push(img);clearTimeout(COV.t);COV.t=setTimeout(pumpCov,0);}
  }
  return w;
}
function setSrc(img){
  img.addEventListener('load',()=>{img.classList.add('on');},{once:true});
  img.src=img.dataset.src;
}
function pumpCov(){
  clearTimeout(COV.t);
  while(COV.q.length&&COV.busy<3){
    if(recentReq()>=38){COV.t=setTimeout(pumpCov,700);return;}
    const img=COV.q.shift();
    if(!img.isConnected)continue;
    const src=img.dataset.src;
    if(COV.ok.has(src)){setSrc(img);continue;}
    COV.busy++;noteReq();
    let done=false;
    const fin=ok=>{
      if(done)return;done=true;COV.busy--;
      if(ok){COV.ok.add(src);img.classList.add('on');}
      else if(!img.dataset.retry&&img.isConnected){ // falhou (ex.: PC pediu calma): tenta de novo uma vez, mais tarde
        const n=h('img',{alt:'',decoding:'async',draggable:'false'});n.dataset.src=src;n.dataset.retry='1';img.replaceWith(n);
        setTimeout(()=>{if(n.isConnected){COV.q.push(n);pumpCov();}},11000);
      }else img.remove();
      pumpCov();
    };
    img.addEventListener('load',()=>fin(true),{once:true});
    img.addEventListener('error',()=>fin(false),{once:true});
    img.src=src;
  }
}
if('IntersectionObserver' in window){
  COV.io=new IntersectionObserver(es=>{
    let add=false;
    for(const e of es){if(!e.isIntersecting)continue;COV.io.unobserve(e.target);COV.q.push(e.target);add=true;}
    if(add)pumpCov();
  },{rootMargin:'300px 300px'});
}
// lista longa: desenha aos poucos conforme a rolagem (first = quantas ja na primeira vez)
function chunked(box,items,mk,first){
  let i=0;const N=60;const end=h('div',{class:'more hasIo'});box.appendChild(end);
  const more=n=>{
    const f=document.createDocumentFragment();const lim=Math.min(items.length,i+(n||N));
    for(;i<lim;i++)f.appendChild(mk(items[i],i));
    box.insertBefore(f,end);
    if(i>=items.length){if(end._io)end._io.disconnect();end.remove();}
  };
  more(Math.max(N,first||0));
  if(i<items.length){
    if('IntersectionObserver' in window){end._io=new IntersectionObserver(es=>{if(es.some(e=>e.isIntersecting))more();},{rootMargin:'900px 0px'});end._io.observe(end);}
    else while(i<items.length)more();
  }
}
// limpa uma tela: solta observadores antes de apagar os elementos
function clear(el){
  if(COV.io)el.querySelectorAll('img[data-src]').forEach(i=>COV.io.unobserve(i));
  el.querySelectorAll('.hasIo').forEach(m=>{if(m._io)m._io.disconnect();});
  el.textContent='';
}

// --------------------------------------------------------- pareamento --
const PAIR={mode:'pin',token:'',chave:'',host:'',busy:false,waiting:false,gen:0};
function setPairMsg(t,kind){const m=$('pairMsg');m.textContent=t||'';m.className='pairMsg'+(kind?' '+kind:'');}
function pairTexts(){
  const q=PAIR.mode==='qr',off=PAIR.mode==='off';
  $('pairTitle').textContent=off?'Não achei o PC':(q?'Vincular a '+(PAIR.host||'este PC'):'Conectar ao Remix');
  $('pairSub').textContent=off?'Confira se o Remix está aberto no PC com o Host ligado e se este aparelho está na mesma rede (ou use o link da internet).'
    :(q?'Você escaneou o QR code do Remix. Falta só dar um nome a este aparelho.'
      :(PAIR.host?'PC “'+PAIR.host+'”. Digite o PIN que aparece no painel HOST.':'Digite o PIN que aparece no painel HOST do PC.'));
  $('pairFields').hidden=off;$('pinWrap').hidden=q;$('pairHint').hidden=q||off;
  $('pairHint').textContent=STANDALONE?'Aberto pela tela de início: use o PIN. O QR code abre no navegador, não neste app.':'ou escaneie o QR code no painel HOST do PC';
  $('pairBtn').textContent=PAIR.waiting?'Cancelar':(off?'Tentar de novo':(q?'Vincular':'Conectar'));
}
function pairMode(mode,msg,kind){
  PAIR.mode=mode;S.ready=false;
  $('boot').hidden=true;$('app').hidden=true;$('pair').hidden=false;
  pairTexts();setPairMsg(msg||'',msg?(kind||'err'):'');
  document.title='Remix Player';
  if(mode!=='off'){const n=$('pairName');if(!n.value)n.value=LS.get('nome','');}
}
function pingHost(){
  raw('/api/ping').then(r=>{if(r.ok&&r.j.app==='remix'){PAIR.host=str(r.j.nome,80);if(!$('pair').hidden)pairTexts();}}).catch(()=>{});
}
let religando=false;
async function toPair(msg){
  S.ready=false;stopAudio();
  // o vínculo está no PC: se o cookie caiu (Safari limpou, endereço novo), a chave religa sem PIN
  if(!religando&&/^[0-9a-f]{32}$/.test(LS.get('chave',''))){
    religando=true;
    try{
      if(await entrarComChave(LS.get('chave',''))){ await startApp(); toast('Conexão com o PC renovada.'); return; }
    }catch(e){}
    finally{religando=false;}
  }
  await popTo(0);
  pairMode(PAIR.token?'qr':'pin',msg,'err');pingHost();
}
function pairDone(){PAIR.waiting=false;PAIR.busy=false;$('pairBtn').disabled=false;pairTexts();}
// O PC aceitou mas o cookie nao ficou (cookies bloqueados no Safari/app): explica em vez de voltar ao PIN.
async function startAfterPair(){
  try{await startApp();}
  catch(e){
    if(e&&e.st===401){setPairMsg('O PC aceitou, mas este navegador não guardou o acesso (cookies bloqueados). No iPhone: Ajustes > Safari > desligue "Bloquear Todos os Cookies" (ou abra o link no Safari) e tente de novo.','err');return;}
    throw e;
  }
}
async function pairGo(){
  if(PAIR.mode==='off'){boot();return;}
  // o token do QR so vale uma vez: depois de usado (pedido pendente cancelado/recusado) volta para o PIN,
  // sem mandar token vazio ao PC (contaria como tentativa errada e travaria o pareamento)
  const qrGasto=()=>PAIR.mode==='qr'&&!PAIR.token;
  if(PAIR.waiting){PAIR.gen++;pairDone();if(qrGasto())pairMode('pin');setPairMsg('Pedido cancelado.');return;}
  if(PAIR.busy)return;
  if(qrGasto()){pairMode('pin',ERR.qr);return;}
  const nome=$('pairName').value.trim().slice(0,40);
  if(!nome){setPairMsg('Dê um nome a este aparelho (ex.: Celular da Ana).','err');$('pairName').focus();return;}
  const lembrar=$('pairRemember').checked;LS.set('lembrar',lembrar?1:0);
  const gen=++PAIR.gen;PAIR.busy=true;$('pairBtn').disabled=true;
  const errOf=r=>errTxt(str(r.j.erro,20))||(r.st===429?ERR.calma:'O PC respondeu com erro ('+r.st+').');
  try{
    let r;
    if(PAIR.mode==='qr'){
      setPairMsg('Vinculando...','wait');
      r=await raw('/api/parear/qr',{token:PAIR.token,nome,lembrar});
      LS.set('nome',nome);
      if(r.st===401){PAIR.token='';pairMode('pin',ERR.qr);return;}
      if(!r.ok){setPairMsg(errOf(r),'err');return;}
      if(r.j.estado==='aceito'){setPairMsg('Pronto! Abrindo...','ok');PAIR.token='';await startAfterPair();guardaChave();return;}
      if(r.j.estado==='pendente'&&HEX16.test(str(r.j.req,20))){PAIR.token='';await waitAccept(r.j.req,gen);return;}
      setPairMsg('Resposta inesperada do PC.','err');return;
    }
    const pin=$('pairPin').value.replace(/\D/g,'');
    if(!/^\d{4,12}$/.test(pin)){setPairMsg('O PIN tem de 4 a 12 números.','err');$('pairPin').focus();return;}
    setPairMsg('Pedindo permissão ao PC...','wait');
    r=await raw('/api/parear',{pin,nome,lembrar});
    LS.set('nome',nome);
    if(r.st===401){setPairMsg(ERR.pin,'err');const pp=$('pairPin');try{pp.focus();pp.setSelectionRange(0,pp.value.length);}catch(e){}return;}
    if(!r.ok){setPairMsg(errOf(r),'err');return;}
    if(!HEX16.test(str(r.j.req,20))){setPairMsg('Resposta inesperada do PC.','err');return;}
    await waitAccept(r.j.req,gen);
  }catch(e){if(gen===PAIR.gen)setPairMsg((e&&e.message)||ERR.rede,'err');}
  finally{if(gen===PAIR.gen)pairDone();}
}
async function waitAccept(req,gen){
  PAIR.waiting=true;$('pairBtn').disabled=false;pairTexts();
  setPairMsg('Confirme no PC: aceite este aparelho na tela do Remix.','wait');
  const t0=Date.now();
  const end=m=>{if(PAIR.mode==='qr'&&!PAIR.token)pairMode('pin',m,'err');else setPairMsg(m,'err');};
  while(Date.now()-t0<180000){
    await sleep(2000);
    if(gen!==PAIR.gen)return;
    let r;try{r=await raw('/api/parear/estado?req='+enc(req));}catch(e){continue;}
    if(gen!==PAIR.gen)return;
    if(!r.ok)continue;
    const st=r.j.estado;
    if(st==='aceito'){PAIR.waiting=false;setPairMsg('Aceito! Abrindo...','ok');await startAfterPair();return;}
    if(st==='recusado'){end('O PC recusou este aparelho.');return;}
    if(st==='expirado'){end('O pedido expirou. Tente de novo.');return;}
  }
  end('O PC não respondeu a tempo. Tente de novo.');
}

// ------------------------------------------------------------ dados --
function arr(v){return Array.isArray(v)?v:[];}
// Novidades da tela inicial: o PC monta as fileiras (API pública do Deezer) e
// serve as capas; tocar continua passando pelo motor online dele (CLI de midia).
const DSC={at:0,busy:false,f:[],err:'',carregando:false};
const TIPO=['Música','Álbum','Playlist','Artista'];
async function loadDesc(force){
  if(DSC.busy||!S.online)return;
  if(!force&&DSC.at&&Date.now()-DSC.at<10*60000)return;
  DSC.busy=true;
  try{
    const j=await api('/api/descobrir');
    DSC.f=arr(j.fileiras).slice(0,12).map(f=>({
      titulo:str(f.titulo,80),nota:str(f.nota,90),
      itens:arr(f.itens).slice(0,30).map(i=>({t:str(i.t,120),s:str(i.s,120),c:str(i.c,32),l:str(i.l,300),k:+i.k||0,d:+i.d||0,dc:1})).filter(i=>i.t&&i.l)
    })).filter(f=>f.itens.length);
    DSC.at=Date.now();DSC.carregando=!!j.carregando;DSC.err='';
  }catch(e){DSC.err=(e&&e.message)||'';}
  DSC.busy=false;
  if(S.tab==='home')renderHome();
}
function novCard(it){
  return h('button',{class:'card',type:'button',onclick:()=>abrirNov(it)},
    art(it,'c140',{round:it.k===3}),h('span',{class:'cT',text:it.t}),h('span',{class:'cS',text:it.s||TIPO[it.k]||''}));
}
// Procurar playlists ou álbuns prontos (o PC consulta o catálogo público).
const LST={tipo:0,busy:false,itens:[],q:'',err:''};
async function buscarListas(){
  const q=(S.sq.q||'').trim();
  if(!q||LST.tipo===0)return;
  LST.busy=true;LST.err='';LST.itens=[];renderResults();
  try{
    const j=await api('/api/online/listas',{q,tipo:LST.tipo});
    LST.itens=arr(j.itens).slice(0,24).map(i=>({t:str(i.t,120),s:str(i.s,120),c:str(i.c,32),l:str(i.l,300),k:+i.k||2,dc:1}));
    LST.q=q;
  }catch(e){LST.err=(e&&e.message)||'Algo deu errado.';}
  LST.busy=false;renderResults();
}
// Letra: o PC busca (LRCLIB) e guarda; aqui só mostramos acompanhando o tempo.
const LYR={id:'',estado:0,sync:false,linhas:[],texto:'',fonte:'',pedindo:false,aberta:false};
async function carregarLetra(id){
  if(!id||LYR.pedindo)return;
  if(LYR.id===id&&LYR.estado!==0)return;
  LYR.pedindo=true;
  try{
    const j=await api('/api/letra',{id});
    LYR.id=id;LYR.estado=+j.estado||0;LYR.sync=!!j.sync;LYR.fonte=str(j.fonte,60);
    LYR.linhas=arr(j.linhas).slice(0,400).map(x=>({ms:+x.ms||0,t:str(x.t,200)}));
    LYR.texto=str(j.texto,20000);
  }catch(e){LYR.estado=2;LYR.id=id;}
  LYR.pedindo=false;
  if(LYR.aberta)pintaLetra();
  if(LYR.estado===0)setTimeout(()=>{LYR.id='';carregarLetra(id);},2500);
}
function pintaLetra(){
  const box=$('lyrBox');if(!box)return;
  clear(box);
  if(LYR.estado===0){box.appendChild(h('p',{class:'lyrNote',text:'O PC está procurando a letra...'}));return;}
  if(LYR.estado===2){box.appendChild(h('p',{class:'lyrNote',text:'Não achei a letra desta música. O PC procura no LRCLIB pelo nome, artista e duração.'}));return;}
  if(!LYR.sync){
    LYR.texto.split('\n').forEach(l=>box.appendChild(h('span',{text:l||' '})));
    box.appendChild(h('p',{class:'lyrNote',text:(LYR.fonte||'LRCLIB')+' · sem marcação de tempo'}));
    return;
  }
  const pos=Math.round(curTime()*1000);
  let at=-1;
  for(let i=0;i<LYR.linhas.length;i++){if(LYR.linhas[i].ms<=pos)at=i;else break;}
  LYR.linhas.forEach((l,i)=>{
    const el=h(i===at?'b':'span',{text:l.t||'♪'});
    el.addEventListener('click',()=>seek(Math.max(0,l.ms/1000)));
    box.appendChild(el);
    if(i===at){const sc=box.parentElement;if(sc)sc.scrollTop=Math.max(0,el.offsetTop-sc.clientHeight/2);}
  });
  box.appendChild(h('p',{class:'lyrNote',text:(LYR.fonte||'LRCLIB')+' · toque numa linha para pular'}));
}
function abrirLetra(){
  if(!S.cur){toast('Toque uma música primeiro.');return;}
  LYR.aberta=true;
  sheet(box=>{
    box.appendChild(shHead('Letra',S.cur.t+' · '+(S.cur.a||''),art(S.cur,'s64')));
    box.appendChild(h('div',{id:'lyrBox',class:'lyr'}));
  });
  carregarLetra(S.cur.id);
  pintaLetra();
}
async function abrirNov(it){
  if(it.k===0){   // música: o PC resolve o link e já toca
    try{
      toast('Preparando "'+it.t+'"...');
      const j=await api('/api/online/link',{url:it.l});
      const l=arr(j.faixas).map(nT).filter(Boolean);l.forEach(t=>{t.o=true;});
      if(!l.length){toast('Não achei essa música.',1);return;}
      playFrom(l,0,{name:it.s||'Novidades',key:'nov:'+it.l});
    }catch(e){fail(e);}
    return;
  }
  // álbum, playlist ou artista: abre a lista na aba Buscar (dá para tocar tudo ou salvar)
  S.sq.mode='online';LS.set('smode','online');
  if(it.k===3){S.sq.q=it.t;renderSearch();await showTab('search');searchOnline();return;}
  S.sq.q=it.l;renderSearch();await showTab('search');openLinkOnline(it.l);
}
function applyEst(est){
  S.host=str(est.host,80)||'PC';S.dev=str(est.dispositivo,60)||'Este aparelho';S.ver=str(est.v,30);
  S.libOk=!!est.biblioteca;S.online=!!est.online;S.nFaixas=Math.max(0,est.faixas|0);S.stemsOk=!!est.stems;
  if(!S.online&&S.sq.mode==='online')S.sq.mode='pc';   // o PC desligou a busca online
}
async function loadData(){
  const [b,p]=await Promise.all([S.libOk?api('/api/biblioteca'):Promise.resolve({faixas:[]}),api('/api/playlists')]);
  S.lib=arr(b.faixas).map(nT).filter(Boolean);
  S.pc=arr(p.pc).map(x=>nP(x,'pc')).filter(Boolean);
  S.mine=arr(p.minhas).map(x=>nP(x,'mine')).filter(Boolean);
  S.sh=arr(p.compartilhadas).map(x=>nP(x,'sh')).filter(Boolean);
  S.idx=null;S.pick=null;S.lastLoad=Date.now();
}
async function startApp(){
  const est=await api('/api/estado');
  applyEst(est);
  await loadData();
  S.ready=true;PAIR.gen++;PAIR.waiting=false;PAIR.busy=false;$('pairBtn').disabled=false;
  if(!S.online)S.sq.mode='pc';
  $('boot').hidden=true;$('pair').hidden=true;$('app').hidden=false;
  document.title='Remix · '+S.host;
  renderAll();showViews();window.scrollTo(0,0);
  restoreSess();
}
async function reload(){await loadData();renderAll();}
async function refresh(manual){
  try{applyEst(await api('/api/estado'));await loadData();renderAll();if(manual)toast('Listas atualizadas');}
  catch(e){if(manual)fail(e);}
}
// O vinculo mora no PC: a "chave" identifica ESTE aparelho em qualquer endereco (tunel novo, rede local,
// app na tela de inicio). O cookie continua sendo o atalho do dia a dia; a chave recupera quando ele some.
async function entrarComChave(chave){
  const k=String(chave||'').toLowerCase();
  if(!/^[0-9a-f]{32}$/.test(k))return false;
  try{
    const r=await raw('/api/entrar',{chave:k});
    if(r.st!==200||!r.j||!r.j.ok)return false;
    LS.set('chave',k);
    return true;
  }catch(e){return false;}
}
async function guardaChave(){   // depois de vincular: guarda a chave para nao perder o vinculo se o cookie sumir
  try{const j=await api('/api/minhachave',{});if(j&&/^[0-9a-f]{32}$/.test(String(j.chave||'')))LS.set('chave',String(j.chave));}catch(e){}
}
function meuLink(){
  const k=LS.get('chave','');
  return /^[0-9a-f]{32}$/.test(k)?(location.origin+location.pathname+'#a='+k):'';
}
async function boot(){
  $('pair').hidden=true;$('app').hidden=true;$('boot').hidden=false;
  pingHost();
  if(PAIR.chave){const k=PAIR.chave;PAIR.chave='';if(await entrarComChave(k)){try{await startApp();toast('Aparelho religado a este PC.');return;}catch(e){}}}
  try{
    await startApp();
    if(PAIR.token){PAIR.token='';toast('Este aparelho já está conectado a '+S.host+'.');}
    guardaChave();
  }catch(e){
    if(e&&e.st===401){
      // cookie perdido (endereço novo, app da tela de início, Safari limpou): a chave guardada religa sozinha
      if(await entrarComChave(LS.get('chave',''))){
        try{await startApp();guardaChave();return;}catch(e2){}
      }
      pairMode(PAIR.token?'qr':'pin');
    }
    else{pairMode('off');if(e&&e.st)setPairMsg(e.message,'err');}   // ex.: 429 (o PC respondeu, mas pediu calma)
  }
}

// --------------------------------------------------------- playlists --
function allPls(){return S.pc.concat(S.mine,S.sh);}
function libPl(){return {kind:'lib',key:'lib',slug:'',nome:'Biblioteca do PC',itens:S.lib};}
function findPl(key){if(key==='lib')return S.libOk?libPl():null;return allPls().find(p=>p.key===key)||null;}
function shareTxt(p){return p.compartilhar?(p.liberada?'compartilhada':'aguardando o PC liberar'):'';}
function listSub(p){
  if(p.kind==='lib')return 'Músicas · '+plural(p.itens.length,'faixa','faixas');
  if(p.kind==='pc')return 'Playlist · do PC';
  if(p.kind==='mine'){const s=shareTxt(p);return 'Playlist · sua'+(s?' · '+s:'');}
  return 'de '+(p.dono||'outro aparelho');
}
function headSub(p){
  if(p.kind==='lib')return 'Tudo o que o PC “'+S.host+'” liberou para este aparelho';
  if(p.kind==='pc')return 'Playlist do PC “'+S.host+'”';
  if(p.kind==='mine'){const s=shareTxt(p);return 'Sua playlist'+(s?' · '+s:'');}
  return 'Compartilhada por '+(p.dono||'outro aparelho');
}
function metaTxt(list){const d=totalDur(list);return plural(list.length,'música','músicas')+(d?' · '+d:'');}
function plArt(p,cls){if(p.kind==='lib')return art(null,cls,{lib:1});return art(p.itens.find(x=>x.c)||null,cls,{pl:p.nome});}

// ------------------------------------------------------ pecas comuns --
function greet(){const hr=new Date().getHours();return hr<5?'Boa noite':hr<12?'Bom dia':hr<18?'Boa tarde':'Boa noite';}
function avatarBtn(){return h('button',{class:'avBtn',type:'button','aria-label':'Perfil e conexão',onclick:openProfile},h('span',{class:'av',text:initial(S.dev)}));}
function topBar(title,...extra){return h('header',{class:'top'},avatarBtn(),h('h1',{text:title}),...extra);}
function emptyBox(icon,title,text,acts){
  return h('div',{class:'empty'},ic(icon),h('h3',{text:title}),text?h('p',{text}):null,
    h('div',{class:'emptyActs'},(acts||[]).filter(Boolean).map((a,i)=>h('button',{class:i?'btnS':'btnP',type:'button',text:a[0],onclick:a[1]}))));
}
function secHead(title,moreFn){return h('div',{class:'secH'},h('h2',{text:title}),moreFn?h('button',{class:'link',type:'button',text:'Ver tudo',onclick:moreFn}):null);}
function eqEl(){return h('span',{class:'eq','aria-hidden':'true'},h('i'),h('i'),h('i'));}
function plCard(p){
  return h('button',{class:'card',type:'button','data-pkey':p.key,onclick:()=>openPl(p.key)},
    plArt(p,'c140'),h('span',{class:'cT',text:p.nome}),h('span',{class:'cS',text:p.kind==='sh'?'de '+(p.dono||'outro aparelho'):plural(p.itens.length,'música','músicas')}));
}
function trackCard(t,list,i,ctx){
  return h('button',{class:'card',type:'button','data-tid':t.id,onclick:()=>playFrom(list,i,ctx)},
    art(t,'c140'),h('span',{class:'cT',text:t.t}),h('span',{class:'cS',text:t.a||'Artista desconhecido'}));
}
function plRow(p,onclick){
  return h('button',{class:'prow',type:'button','data-pkey':p.key,onclick:onclick||(()=>openPl(p.key))},
    plArt(p,'s64'),h('span',{class:'pTx'},h('span',{class:'pn',text:p.nome}),h('span',{class:'ps',text:listSub(p)})));
}
// linha = botao "tocar" + botao "mais opcoes" lado a lado (botao dentro de botao quebra o toque e o VoiceOver)
function trackRow(t,i,list,ctx,opt){
  opt=opt||{};
  const main=h('button',{class:'tmain',type:'button',onclick:()=>playFrom(list,i,ctx)},
    opt.num?h('span',{class:'tnum'},h('span',{text:String(i+1)}),eqEl()):null,
    art(t,'s48'),
    h('span',{class:'ttx'},h('span',{class:'tt',text:t.t}),
      h('span',{class:'ta'},t.o?h('span',{class:'badge',text:'ONLINE'}):null,h('span',{class:'tan',text:t.a||'Artista desconhecido'}))),
    t.d?h('span',{class:'tdur',text:fmt(t.d)}):null);
  const more=h('button',{class:'ib tmore',type:'button','aria-label':'Mais opções: '+t.t,onclick:()=>trackMenu(t,opt.pl||null)},ic('more'));
  return h('div',{class:'trow','data-tid':t.id},main,more);
}

// ------------------------------------------------------------- telas --
function showViews(){
  const pl=!!S.plKey;
  $('vHome').hidden=pl||S.tab!=='home';$('vSearch').hidden=pl||S.tab!=='search';$('vLib').hidden=pl||S.tab!=='lib';$('vPl').hidden=!pl;
  document.querySelectorAll('#nav button').forEach(b=>{
    const on=b.dataset.tab===S.tab;b.classList.toggle('on',on);
    if(on)b.setAttribute('aria-current','page');else b.removeAttribute('aria-current');
    const i=b.querySelector('.nIc');if(i)setIc(i,b.dataset.tab+(on?'F':''));
  });
}
async function showTab(tab){
  if(S.plKey){await closeL('pl');if(S.tab===tab)return;}
  if(S.tab===tab){
    if(window.scrollY>0)window.scrollTo({top:0,behavior:'smooth'});
    else if(tab==='search'){const i=$('sIn');if(i)i.focus();}
    showViews();return;
  }
  S.scroll[S.tab]=window.scrollY;S.tab=tab;LS.set('tab',tab);
  if(tab==='home')renderHome();
  showViews();window.scrollTo(0,S.scroll[tab]||0);
}
function renderAll(){
  renderHome();renderLib();
  if($('sIn')&&document.activeElement===$('sIn')&&S.online===S.sq.onlineShown)renderResults();else renderSearch();
  if(S.plKey)renderPl();
  markAll();
}

// Inicio
function renderHome(){
  const v=$('vHome');clear(v);
  v.appendChild(topBar(greet()));
  const hasLib=S.libOk&&S.lib.length>0;
  const nothing=!S.pc.length&&!S.sh.length&&!hasLib;
  const short=(hasLib?[libPl()]:[]).concat(allPls()).slice(0,8);
  if(short.length){
    v.appendChild(h('div',{class:'sc'},short.map(p=>h('button',{class:'scI',type:'button','data-pkey':p.key,onclick:()=>openPl(p.key)},
      plArt(p,'s56'),h('span',{class:'scN',text:p.nome}),eqEl()))));
  }
  if(nothing){
    v.appendChild(emptyBox('pc','Nada liberado ainda','No PC, abra o painel HOST do Remix e hosteie playlists para este aparelho ou libere a biblioteca inteira. Depois toque em Atualizar.',
      [['Atualizar',()=>refresh(true)],S.online?['Buscar online',()=>{S.sq.mode='online';LS.set('smode','online');renderSearch();showTab('search');}]:null]));
  }
  if(hasLib){
    if(!S.pick){
      const idx=[...S.lib.keys()];
      for(let i=idx.length-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[idx[i],idx[j]]=[idx[j],idx[i]];}
      S.pick=idx.slice(0,16);
    }
    const ctx={name:'Biblioteca do PC',key:'lib'};
    v.appendChild(h('section',{class:'sec'},secHead('Biblioteca do PC',()=>openPl('lib')),
      h('div',{class:'car'},S.pick.map(i=>trackCard(S.lib[i],S.lib,i,ctx)))));
  }
  if(S.pc.length)v.appendChild(h('section',{class:'sec'},secHead('Playlists do PC',S.pc.length>3?()=>libFilter('pc'):null),h('div',{class:'car'},S.pc.map(plCard))));
  v.appendChild(h('section',{class:'sec'},secHead('Suas playlists',S.mine.length>3?()=>libFilter('mine'):null),
    h('div',{class:'car'},S.mine.map(plCard),h('button',{class:'card',type:'button',onclick:()=>nameSheet()},
      h('div',{class:'art c140 newPl'},ic('plus')),h('span',{class:'cT',text:'Criar playlist'}),h('span',{class:'cS',text:S.online?'músicas do PC e online':'músicas do PC'})))));
  if(S.sh.length)v.appendChild(h('section',{class:'sec'},secHead('Compartilhadas com você',S.sh.length>3?()=>libFilter('sh'):null),h('div',{class:'car'},S.sh.map(plCard))));
  for(const f of DSC.f)
    v.appendChild(h('section',{class:'sec'},secHead(f.titulo),h('div',{class:'car'},f.itens.map(novCard))));
  if(S.online&&!DSC.f.length&&(DSC.busy||DSC.carregando))
    v.appendChild(h('section',{class:'sec'},secHead('Novidades'),h('p',{class:'shNote',text:'O PC está buscando as novidades...'})));
  markAll();
  loadDesc(false);
}
function libFilter(f){S.libFilter=f;LS.set('lfil',f);renderLib();showTab('lib');}

// Buscar
const FONTES=['YouTube Music','YouTube','SoundCloud'];
function pcIndex(){
  if(S.idx)return S.idx;
  const seen=new Set(),out=[];
  const add=t=>{if(seen.has(t.id))return;seen.add(t.id);out.push([fold(t.t+' '+t.a),t]);};
  S.lib.forEach(add);S.pc.forEach(p=>p.itens.forEach(add));S.sh.forEach(p=>p.itens.forEach(add));S.mine.forEach(p=>p.itens.forEach(add));
  return S.idx=out;
}
let searchT=0;
// Link de playlist/album de outra plataforma: o PC resolve (CLI de midia, Spotify/Deezer/Apple) e devolve as musicas.
async function openLinkOnline(url){
  if(S.sq.ctl)S.sq.ctl.abort();
  const ctl=new AbortController();S.sq.ctl=ctl;
  S.sq.busy=true;S.sq.err='';S.sq.link='';renderResults();
  const tm=setTimeout(()=>ctl.abort(),300000);   // playlist grande demora (o PC lê tudo)
  try{
    let j;
    for(;;){
      try{j=await api('/api/online/link',{url},ctl.signal);break;}
      catch(e){
        if(!(e&&e.code==='buscando')||S.sq.ctl!==ctl)throw e;
        await sleep(2000);
        if(S.sq.ctl!==ctl)return;
        if(ctl.signal.aborted){const x=new Error('tempo');x.name='AbortError';throw x;}
      }
    }
    if(S.sq.ctl!==ctl)return;
    S.sq.res=arr(j.faixas).map(nT).filter(Boolean);S.sq.res.forEach(t=>{t.o=true;});
    S.sq.link=url;S.sq.linkNome=str(j.nome,80)||'Link';S.sq.linkFonte=str(j.fonte,30);
    S.sq.done=url;S.sq.doneF=S.sq.fonte;
    if(!S.sq.res.length)S.sq.err='Esse link não trouxe músicas.';
  }catch(e){
    if(S.sq.ctl!==ctl)return;
    if(e&&e.name==='AbortError')S.sq.err='O PC demorou demais para abrir o link. Tente de novo.';
    else if(e&&e.code==='link_nao_suportado')S.sq.err='Link não suportado. Use YouTube, YouTube Music, SoundCloud, Spotify, Deezer, Apple Music ou Bandcamp.';
    else if(e&&e.code==='link_vazio')S.sq.err=str(e.msg,200)||'Não achei músicas nesse link.';
    else S.sq.err=(e&&e.message)||'Algo deu errado.';
  }finally{
    clearTimeout(tm);
    if(S.sq.ctl===ctl){S.sq.busy=false;S.sq.ctl=null;renderResults();}
  }
}
function renderSearch(){
  const v=$('vSearch');clear(v);S.sq.onlineShown=S.online;
  const on=S.sq.mode==='online';
  v.appendChild(topBar('Buscar'));
  const inp=h('input',{id:'sIn',type:'search',enterkeyhint:'search',autocomplete:'off',autocapitalize:'off',autocorrect:'off',spellcheck:'false',maxlength:'200',
    'aria-label':on?'Buscar música online ou colar link':'Buscar no PC',placeholder:on?'Música, artista ou link de playlist/álbum':'O que você quer ouvir?'});
  inp.value=S.sq.q;
  const clr=h('button',{class:'ib clr',type:'button','aria-label':'Limpar busca'},ic('close'));clr.hidden=!S.sq.q;
  inp.addEventListener('input',()=>{S.sq.q=inp.value;clr.hidden=!inp.value;if(S.sq.mode==='pc'){clearTimeout(searchT);searchT=setTimeout(renderResults,140);}});
  inp.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();if(S.sq.mode==='online')searchOnline();else{renderResults();inp.blur();}}});
  clr.addEventListener('click',()=>{S.sq.q='';inp.value='';clr.hidden=true;inp.focus();if(S.sq.mode==='pc')renderResults();});
  v.appendChild(h('div',{class:'sbox'},ic('search'),inp,clr));
  const chips=h('div',{class:'chips',role:'tablist','aria-label':'Onde buscar'});
  for(const [m,label] of [['pc','No PC'],['online','Online']]){
    if(m==='online'&&!S.online)continue;
    chips.appendChild(h('button',{class:'chip'+(S.sq.mode===m?' on':''),type:'button',role:'tab','aria-selected':String(S.sq.mode===m),text:label,
      onclick:()=>{if(S.sq.mode===m)return;S.sq.mode=m;LS.set('smode',m);renderSearch();}}));
  }
  v.appendChild(chips);
  if(on){
    const tp=h('div',{class:'chips sub','aria-label':'O que procurar'});
    [['Músicas',0],['Playlists',1],['Álbuns',2]].forEach(([n,i])=>tp.appendChild(h('button',{class:'chip'+(LST.tipo===i?' on':''),type:'button','aria-pressed':String(LST.tipo===i),text:n,
      onclick:()=>{if(LST.tipo===i)return;LST.tipo=i;LST.itens=[];LST.q='';renderSearch();if(i&&(S.sq.q||'').trim())buscarListas();}})));
    v.appendChild(tp);
  }
  if(on&&LST.tipo===0){
    const f=h('div',{class:'chips sub','aria-label':'Fonte'});
    FONTES.forEach((n,i)=>f.appendChild(h('button',{class:'chip'+(S.sq.fonte===i?' on':''),type:'button','aria-pressed':String(S.sq.fonte===i),text:n,
      onclick:()=>{if(S.sq.fonte===i)return;S.sq.fonte=i;LS.set('fonte',i);const again=(!!S.sq.done||S.sq.busy)&&!!S.sq.q.trim();renderSearch();if(again)searchOnline();}})));
    v.appendChild(f);
    v.appendChild(h('div',{class:'sGo'},h('button',{id:'sGoBtn',class:'btnP',type:'button',text:S.sq.busy?(isLink(S.sq.q)?'Abrindo o link...':'Buscando...'):(isLink(S.sq.q)?'Abrir link':'Buscar'),onclick:()=>{if(LST.tipo)buscarListas();else searchOnline();}})));
    if(isLink(S.sq.q))v.appendChild(h('p',{class:'resInfo',text:'Link reconhecido: o PC abre a playlist ou o álbum e você salva no seu aparelho.'}));
  }
  v.appendChild(h('div',{id:'sRes'}));
  renderResults();
}
function renderResults(){
  const box=$('sRes');if(!box)return;
  const prev=box.querySelectorAll('.trow').length;
  clear(box);
  const b=$('sGoBtn');if(b){const lk=isLink(S.sq.q);b.textContent=S.sq.busy?(lk?'Abrindo o link...':'Buscando...'):(lk?'Abrir link':'Buscar');b.disabled=S.sq.busy;}
  const q=S.sq.q.trim();
  if(S.sq.mode==='online'&&LST.tipo!==0){
    if(LST.busy){box.appendChild(h('div',{class:'loading',role:'status'},h('div',{class:'spin'}),h('p',{text:'O PC está procurando...'})));return;}
    if(LST.err){box.appendChild(emptyBox('globe','Não deu para procurar',LST.err,[['Tentar de novo',buscarListas]]));return;}
    if(!LST.q){box.appendChild(emptyBox('globe',LST.tipo===1?'Procurar playlists':'Procurar álbuns','Digite o nome e toque em Buscar: o PC procura no catálogo e você abre a lista aqui mesmo, sem sair do app.'));return;}
    if(!LST.itens.length){box.appendChild(emptyBox('search','Nada encontrado','Nenhum resultado para “'+LST.q+'”.'));return;}
    const lb=h('div',{class:'tlist'});box.appendChild(lb);
    LST.itens.forEach(it=>lb.appendChild(h('button',{class:'prow',type:'button',onclick:()=>{LST.tipo=0;S.sq.q=it.l;renderSearch();openLinkOnline(it.l);}},
      art(it,'s64'),h('span',{class:'pTx'},h('span',{class:'pn',text:it.t}),h('span',{class:'ps',text:it.s||(it.k===1?'Playlist':'Álbum')})))));
    return;
  }
  if(S.sq.mode==='online'){
    if(S.sq.busy){box.appendChild(h('div',{class:'loading',role:'status'},h('div',{class:'spin'}),h('p',{text:isLink(S.sq.q)?'O PC está abrindo o link... playlist grande pode levar um tempo.':'O PC está buscando em '+FONTES[S.sq.fonte]+'... pode levar alguns segundos.'})));return;}
    if(S.sq.err){box.appendChild(emptyBox('globe','Não deu para buscar',S.sq.err,[['Tentar de novo',searchOnline]]));return;}
    if(!S.sq.done){box.appendChild(emptyBox('globe','Buscar online','O PC procura em '+FONTES[S.sq.fonte]+' e toca para você por streaming. Você também pode colar um link de playlist ou álbum (Spotify, YouTube, YouTube Music, Deezer, Apple Music, SoundCloud) e salvar como playlist deste aparelho.'));return;}
    if(!S.sq.res.length){box.appendChild(emptyBox('search','Nada encontrado',S.sq.link?'Esse link não trouxe músicas.':'Nenhum resultado para “'+S.sq.done+'”. Tente outras palavras ou outra fonte.'));return;}
    if(S.sq.link){
      const nome=S.sq.linkNome||'Link';
      box.appendChild(h('p',{class:'resInfo',text:nome+'  ·  '+plural(S.sq.res.length,'música','músicas')+(S.sq.linkFonte?'  ·  '+S.sq.linkFonte:'')}));
      box.appendChild(h('div',{class:'shBtns linkBtns'},
        h('button',{class:'btnS',type:'button',text:'Tocar tudo',onclick:()=>playFrom(S.sq.res,0,{name:nome,key:'link:'+nome})}),
        h('button',{class:'btnP',type:'button',text:'Salvar como playlist',onclick:()=>nameSheet(null,null,S.sq.res)})));
    }else box.appendChild(h('p',{class:'resInfo',text:plural(S.sq.res.length,'resultado','resultados')+' para “'+S.sq.done+'” em '+FONTES[S.sq.doneF]}));
    const list=h('div',{class:'tlist'});box.appendChild(list);
    const ctx={name:S.sq.link?(S.sq.linkNome||'Link'):('Busca: '+S.sq.done),key:S.sq.link?('link:'+(S.sq.linkNome||'')):'search'};
    chunked(list,S.sq.res,(t,i)=>trackRow(t,i,S.sq.res,ctx),prev);
    markAll();return;
  }
  const hasLib=S.libOk&&S.lib.length>0;
  if(!q){
    const pls=(hasLib?[libPl()]:[]).concat(allPls());
    if(!pls.length){box.appendChild(emptyBox('search','Procure no PC','Quando o PC liberar músicas para este aparelho, elas aparecem aqui.'));return;}
    box.appendChild(h('h2',{class:'secT',text:'Navegar'}));
    box.appendChild(h('div',{class:'tiles'},pls.map(p=>{
      const b=h('button',{class:'tile',type:'button','data-pkey':p.key,onclick:()=>openPl(p.key)},h('span',{class:'tileT',text:p.nome}),plArt(p,'tileArt'));
      b.style.backgroundColor='hsl('+hueOf(p.key+p.nome)+',52%,36%)';
      return b;
    })));
    return;
  }
  const words=fold(q).split(/\s+/).filter(Boolean);
  const plHits=(hasLib?[libPl()]:[]).concat(allPls()).filter(p=>{const k=fold(p.nome);return words.every(w=>k.includes(w));}).slice(0,6);
  const res=pcIndex().filter(x=>words.every(w=>x[0].includes(w))).map(x=>x[1]);
  if(!res.length&&!plHits.length){box.appendChild(emptyBox('search','Nada encontrado','Nenhuma música do PC combina com “'+q+'”.'+(S.online?' Tente a busca Online.':'')));return;}
  if(plHits.length){box.appendChild(h('h2',{class:'secT',text:'Playlists'}));box.appendChild(h('div',{class:'plist'},plHits.map(p=>plRow(p))));}
  if(res.length){
    box.appendChild(h('h2',{class:'secT',text:'Músicas'}));
    const list=h('div',{class:'tlist'});box.appendChild(list);
    const ctx={name:'Busca: '+q,key:'search'};
    chunked(list,res,(t,i)=>trackRow(t,i,res,ctx),prev);
  }
  markAll();
}
async function searchOnline(){
  const inp=$('sIn');const q=S.sq.q.trim().slice(0,600);
  if(!q){if(inp)inp.focus();return;}
  if(inp)inp.blur();
  if(isLink(q)){openLinkOnline(q);return;}
  if(S.sq.ctl)S.sq.ctl.abort();
  const ctl=new AbortController();S.sq.ctl=ctl;
  S.sq.busy=true;S.sq.err='';renderResults();
  const tm=setTimeout(()=>ctl.abort(),60000);
  const fonte=S.sq.fonte;
  try{
    let j;
    for(;;){
      try{j=await api('/api/online/buscar?q='+enc(q)+'&fonte='+enc(String(fonte)),null,ctl.signal);break;}
      catch(e){
        // 429 "buscando": a busca anterior (cancelada aqui, ex.: trocou a fonte) continua no PC ate
        // terminar; espera e tenta de novo em vez de mostrar erro
        if(!(e&&e.code==='buscando')||S.sq.ctl!==ctl)throw e;
        await sleep(2000);
        if(S.sq.ctl!==ctl)return;
        if(ctl.signal.aborted){const x=new Error('tempo');x.name='AbortError';throw x;}
      }
    }
    if(S.sq.ctl!==ctl)return;
    S.sq.res=arr(j.faixas).map(nT).filter(Boolean);S.sq.res.forEach(t=>{t.o=true;});
    S.sq.done=q;S.sq.doneF=fonte;S.sq.link='';
  }catch(e){
    if(S.sq.ctl!==ctl)return;
    if(e&&e.name==='AbortError')S.sq.err='O PC demorou demais para responder. Tente de novo.';
    else S.sq.err=(e&&e.message)||'Algo deu errado.';
  }finally{
    clearTimeout(tm);
    if(S.sq.ctl===ctl){S.sq.ctl=null;S.sq.busy=false;renderResults();window.scrollTo(0,0);}
  }
}

// Sua Biblioteca
function renderLib(){
  const v=$('vLib');clear(v);
  v.appendChild(topBar('Sua Biblioteca',h('button',{class:'ib',type:'button','aria-label':'Criar playlist',onclick:()=>nameSheet()},ic('plus'))));
  const chips=h('div',{class:'chips','aria-label':'Filtrar'});
  for(const [f,label] of [['all','Tudo'],['pc','Do PC'],['mine','Minhas'],['sh','Compartilhadas']]){
    chips.appendChild(h('button',{class:'chip'+(S.libFilter===f?' on':''),type:'button','aria-pressed':String(S.libFilter===f),text:label,
      onclick:()=>{S.libFilter=(S.libFilter===f&&f!=='all')?'all':f;LS.set('lfil',S.libFilter);renderLib();}}));
  }
  v.appendChild(chips);
  const f=S.libFilter,items=[];
  if((f==='all'||f==='pc')&&S.libOk&&S.lib.length)items.push(libPl());
  if(f==='all'||f==='pc')items.push(...S.pc);
  if(f==='all'||f==='mine')items.push(...S.mine);
  if(f==='all'||f==='sh')items.push(...S.sh);
  if(!items.length){
    const mk=[['Criar playlist',()=>nameSheet()]];
    if(f==='pc')v.appendChild(emptyBox('pc','Nada do PC por aqui','O PC ainda não hosteou playlists nem liberou a biblioteca para este aparelho.',[['Atualizar',()=>refresh(true)]]));
    else if(f==='sh')v.appendChild(emptyBox('share','Nada compartilhado','Quando outro aparelho compartilhar uma playlist e o PC liberar, ela aparece aqui.',[['Atualizar',()=>refresh(true)]]));
    else v.appendChild(emptyBox('plus','Crie sua primeira playlist','Junte músicas do PC'+(S.online?' e da busca online':'')+'. Ela fica guardada no PC, ligada a este aparelho.',mk));
  }else{
    v.appendChild(h('div',{class:'plist'},items.map(p=>plRow(p))));
    if(f==='all'&&!S.mine.length)v.appendChild(h('button',{class:'prow',type:'button',onclick:()=>nameSheet()},h('div',{class:'art s64 newPl'},ic('plus')),
      h('span',{class:'pTx'},h('span',{class:'pn',text:'Criar playlist'}),h('span',{class:'ps',text:'Junte suas músicas favoritas'}))));
  }
  markAll();
}

// Tela da playlist
function openPl(key){
  const p=findPl(key);if(!p){toast('Essa playlist não está mais disponível.',1);return;}
  if(!S.plKey){S.scroll[S.tab]=window.scrollY;pushL('pl');}
  S.plKey=key;renderPl(true);showViews();window.scrollTo(0,0);
}
function closePlNow(){
  S.plKey='';showViews();clear($('vPl'));
  const y=S.scroll[S.tab]||0;window.scrollTo(0,y);requestAnimationFrame(()=>window.scrollTo(0,y));
}
function renderPl(fresh){
  const v=$('vPl');const prev=fresh?0:v.querySelectorAll('.trow').length;clear(v);
  const p=findPl(S.plKey);
  const bar=h('div',{class:'plBar hasIo'},h('button',{class:'ib',type:'button','aria-label':'Voltar',onclick:()=>closeL('pl')},ic('back')),
    h('span',{class:'plBarT',text:p?p.nome:''}));
  v.appendChild(bar);
  if(!p){v.appendChild(emptyBox('note','Playlist indisponível','Ela foi apagada ou o PC deixou de liberar para este aparelho.',[['Voltar',()=>closeL('pl')]]));return;}
  const list=p.itens,ctx={name:p.nome,key:p.key};
  const name=h('h1',{class:'plName',text:p.nome});
  v.appendChild(h('div',{class:'plHead'},h('div',{class:'plArtW'},plArt(p,'artBig')),h('div',{class:'plHeadTx'},name,h('p',{class:'plSub',text:headSub(p)}),h('p',{class:'plMeta',text:metaTxt(list)}))));
  const act=h('div',{class:'plAct'});
  if(p.kind==='mine')act.appendChild(h('button',{class:'ib',type:'button','aria-label':'Opções da playlist',onclick:()=>plMenu(p)},ic('more')));
  act.appendChild(h('span',{class:'grow'}));
  if(list.length){
    act.appendChild(h('button',{class:'ib shufB',type:'button','data-shuf':'1','aria-label':'Aleatório','aria-pressed':String(S.shuffle),
      onclick:()=>{if(S.ctx.key===p.key&&S.cur){setShuffle(!S.shuffle);}else{setShuffle(true);playFrom(list,-1,ctx);}}},ic('shuffle')));
    act.appendChild(h('button',{class:'playBig',type:'button','data-plplay':p.key,'aria-label':'Tocar',
      onclick:()=>{if(S.ctx.key===p.key&&S.cur)togglePlay();else playFrom(list,S.shuffle?-1:0,ctx);}},ic('play')));
  }
  v.appendChild(act);
  if(!list.length){
    v.appendChild(p.kind==='mine'
      ?emptyBox('search','Playlist vazia','Busque músicas e toque em ··· → Adicionar a playlist.',[['Buscar músicas',()=>closeL('pl').then(()=>showTab('search'))]])
      :emptyBox('note','Playlist vazia',''));
  }else{
    const tl=h('div',{class:'tlist'});v.appendChild(tl);
    chunked(tl,list,(t,i)=>trackRow(t,i,list,ctx,{num:true,pl:p}),prev);
  }
  if('IntersectionObserver' in window){
    bar._io=new IntersectionObserver(es=>{bar.classList.toggle('solid',!es[0].isIntersecting);},{rootMargin:'-64px 0px 0px 0px'});
    bar._io.observe(name);
  }
  markAll();
}

// ------------------------------------------------------------ folhas --
function trackMenu(t,pl){
  sheet(box=>{
    box.appendChild(shHead(t.t,(t.a||'Artista desconhecido')+(t.o?' · online':''),art(t,'s48')));
    box.appendChild(mi('plusC','Adicionar a playlist',()=>addSheet(t)));
    if(S.cur&&S.cur.id!==t.id)box.appendChild(mi('queue','Tocar a seguir',()=>{playNext(t);closeSheet();}));
    if(pl&&pl.kind==='mine')box.appendChild(mi('trash','Remover desta playlist',async()=>{
      await closeSheet();
      try{await api('/api/minhas',{acao:'remover',slug:pl.slug,id:t.id});await reload();toast('Removida de '+pl.nome);}catch(e){fail(e);}
    }));
  });
}
function addSheet(t){
  sheet(box=>{
    box.appendChild(h('h2',{class:'shTitle',text:'Adicionar a playlist'}));
    box.appendChild(h('div',{class:'shCenter'},h('button',{class:'btnP',type:'button',text:'Nova playlist',onclick:()=>nameSheet(null,t)})));
    if(!S.mine.length)box.appendChild(h('p',{class:'shNote center',text:'Você ainda não tem playlists. Crie a primeira acima.'}));
    for(const p of S.mine){
      const has=p.itens.some(x=>x.id===t.id);
      const row=plRow(p,async()=>{
        if(has){toast('Já está em '+p.nome);return;}
        await closeSheet();
        try{await api('/api/minhas',{acao:'add',slug:p.slug,id:t.id});toast('Adicionada a '+p.nome);await reload();}catch(e){fail(e);}
      });
      row.querySelector('.ps').textContent=plural(p.itens.length,'música','músicas');
      if(has)row.appendChild(h('span',{class:'hasIc','aria-label':'já está nesta playlist'},ic('checkC')));
      box.appendChild(row);
    }
  });
}
const isLink=q=>/^https?:\/\//i.test((q||'').trim());
// Cria uma playlist deste aparelho com todas as musicas de um link (album/playlist de outra plataforma).
async function saveLinkPl(nome,itens){
  const r=await api('/api/minhas',{acao:'criar',nome});
  const slug=str(r.slug,120);
  if(!slug)throw new Error('Não consegui criar a playlist.');
  const ids=itens.map(t=>t.id).filter(Boolean).slice(0,500);
  const add=await api('/api/minhas',{acao:'addvarios',slug,ids});
  await reload();
  return {slug,n:Number(add&&add.n)||0};
}
function nameSheet(p,addT,linkItens){
  sheet(box=>{
    box.appendChild(h('h2',{class:'shTitle',text:p?'Renomear playlist':(linkItens?'Salvar como playlist':'Dê um nome à sua playlist')}));
    if(linkItens)box.appendChild(h('p',{class:'shNote center',text:plural(linkItens.length,'música','músicas')+' do link vão para esta playlist do seu aparelho.'}));
    const inp=h('input',{class:'bigIn',type:'text',maxlength:'60',autocomplete:'off',enterkeyhint:'done','aria-label':'Nome da playlist','data-autofocus':'1'});
    inp.value=p?p.nome:(linkItens&&S.sq.linkNome?S.sq.linkNome.slice(0,60):'Minha playlist nº '+(S.mine.length+1));
    const ok=h('button',{class:'btnP',type:'button',text:p?'Salvar':'Criar'});
    const go=async()=>{
      const nome=inp.value.trim().slice(0,60);
      if(!nome){inp.focus();return;}
      ok.disabled=true;
      try{
        if(p){
          await api('/api/minhas',{acao:'renomear',slug:p.slug,nome});
          await closeSheet();await reload();toast('Playlist renomeada');
        }else if(linkItens){
          const {slug,n}=await saveLinkPl(nome,linkItens);
          await closeSheet();
          toast(n?('Playlist criada com '+plural(n,'música','músicas')):'Playlist criada');
          if(findPl('mine:'+slug))openPl('mine:'+slug);
        }else{
          const r=await api('/api/minhas',{acao:'criar',nome});
          const slug=str(r.slug,120);
          if(addT&&slug)await api('/api/minhas',{acao:'add',slug,id:addT.id});
          await closeSheet();await reload();
          if(addT)toast('Adicionada a '+nome);
          else{toast('Playlist criada');if(slug&&findPl('mine:'+slug))openPl('mine:'+slug);}
        }
      }catch(e){ok.disabled=false;fail(e);}
    };
    inp.addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();go();}});
    inp.addEventListener('focus',()=>{try{inp.select();}catch(e){}},{once:true});
    ok.addEventListener('click',go);
    box.appendChild(inp);
    box.appendChild(h('div',{class:'shBtns'},h('button',{class:'btnS',type:'button',text:'Cancelar',onclick:closeSheet}),ok));
  });
}
function plMenu(p){
  sheet(box=>{
    box.appendChild(shHead(p.nome,headSub(p),plArt(p,'s48')));
    box.appendChild(mi('edit','Renomear',()=>nameSheet(p)));
    const stat=p.compartilhar?(p.liberada?'Outros aparelhos do PC já podem ouvir':'Aguardando o PC liberar'):'Só este aparelho vê esta playlist';
    const sw=h('button',{class:'mi',type:'button',role:'switch','aria-checked':String(p.compartilhar)},ic('share'),
      h('span',{class:'miTx'},h('span',{text:'Compartilhar com outros aparelhos'}),h('small',{class:p.compartilhar&&!p.liberada?'wait':'',text:stat})),
      h('span',{class:'sw'+(p.compartilhar?' on':''),'aria-hidden':'true'}));
    sw.addEventListener('click',async()=>{
      sw.disabled=true;
      try{
        await api('/api/minhas',{acao:'compartilhar',slug:p.slug,valor:!p.compartilhar});
        await reload();
        const np=findPl(p.key);
        if(np&&SH.open)plMenu(np);
        toast(!p.compartilhar?(np&&np.liberada?'Playlist compartilhada':'Pedido feito: o PC precisa liberar'):'Compartilhamento desligado');
      }catch(e){sw.disabled=false;fail(e);}
    });
    box.appendChild(sw);
    box.appendChild(mi('trash','Apagar playlist',()=>confirmDel(p),'danger'));
  });
}
function confirmDel(p){
  sheet(box=>{
    box.appendChild(h('h2',{class:'shTitle',text:'Apagar “'+p.nome+'”?'}));
    box.appendChild(h('p',{class:'shNote center',text:'A playlist some deste aparelho e de quem a recebeu por compartilhamento. As músicas continuam no PC.'}));
    const ok=h('button',{class:'btnP danger',type:'button',text:'Apagar'});
    ok.addEventListener('click',async()=>{
      ok.disabled=true;
      try{
        await api('/api/minhas',{acao:'apagar',slug:p.slug});
        const i=L.lastIndexOf('pl');
        if(i>=0&&S.plKey===p.key)await popTo(i);else await closeSheet();
        await reload();toast('Playlist apagada');
      }catch(e){ok.disabled=false;fail(e);}
    });
    box.appendChild(h('div',{class:'shBtns'},h('button',{class:'btnS',type:'button',text:'Cancelar',onclick:closeSheet}),ok));
  });
}
function openProfile(){
  sheet(box=>{
    box.appendChild(h('div',{class:'prof'},h('span',{class:'av big',text:initial(S.dev)}),
      h('div',{class:'profTx'},h('h2',{text:S.dev}),h('p',{text:'Conectado a '+S.host}))));
    const dl=h('dl',{class:'info'});
    const row=(k,v)=>{dl.appendChild(h('dt',{text:k}));dl.appendChild(h('dd',{text:v}));};
    row('Este aparelho',S.dev);row('PC',S.host);row('Versão do Remix',S.ver||'?');
    row('Biblioteca do PC',S.libOk?'liberada ('+plural(S.lib.length,'música','músicas')+')':'não liberada');
    row('Busca online',S.online?'disponível':'indisponível');
    box.appendChild(dl);
    box.appendChild(mi('refresh','Atualizar listas',async()=>{await closeSheet();refresh(true);}));
    box.appendChild(mi('share','Copiar link deste aparelho',async()=>{
      if(!meuLink())await guardaChave();
      const l=meuLink();
      if(!l){toast('Não consegui pegar a chave deste aparelho.',1);return;}
      let ok=false;
      try{if(navigator.clipboard&&navigator.clipboard.writeText){await navigator.clipboard.writeText(l);ok=true;}}catch(e){}
      toast(ok?'Link copiado: abre direto neste aparelho, em qualquer endereço do PC.':'Link deste aparelho: '+l,ok?0:1);
    }));
    box.appendChild(mi('logout','Desconectar este aparelho',confirmSair,'danger'));
    box.appendChild(h('p',{class:'shNote',text:'O vínculo fica no PC. Este link (e o QR "religar" do painel HOST) trazem o aparelho de volta mesmo quando o endereço muda ou o navegador esquece a conexão.'}));
  });
}
function confirmSair(){
  sheet(box=>{
    box.appendChild(h('h2',{class:'shTitle',text:'Desconectar do PC?'}));
    box.appendChild(h('p',{class:'shNote center',text:'Para voltar vai precisar do PIN ou do QR code do painel HOST. As playlists criadas neste aparelho são apagadas do PC.'}));
    const ok=h('button',{class:'btnP danger',type:'button',text:'Desconectar'});
    ok.addEventListener('click',async()=>{
      ok.disabled=true;
      try{await api('/api/sair',{});}catch(e){if(e.st!==401){ok.disabled=false;fail(e);return;}}
      LS.set('chave','');
      S.ready=false;stopAudio();await popTo(0);
      S.lib=[];S.pc=[];S.mine=[];S.sh=[];
      pairMode('pin','Aparelho desconectado.','ok');pingHost();
    });
    box.appendChild(h('div',{class:'shBtns'},h('button',{class:'btnS',type:'button',text:'Cancelar',onclick:closeSheet}),ok));
  });
}
function npMenu(){
  const t=S.cur;if(!t)return;
  sheet(box=>{
    box.appendChild(shHead(t.t,(t.a||'Artista desconhecido')+(t.o?' · online':''),art(t,'s48')));
    box.appendChild(mi('plusC','Adicionar a playlist',()=>addSheet(t)));
    box.appendChild(mi('fx','Efeitos e stems',fxSheet));
    const p=findPl(S.ctx.key);
    if(p)box.appendChild(mi('goPl','Ir para a playlist',async()=>{await closeSheet();await closeL('np');openPl(p.key);}));
  });
}

// --------------------------------------------------- efeitos e stems --
function saveFx(){LS.set('fx',JSON.stringify(S.fx));LS.set('stem',S.stem);}
function fxLabel(){
  const p=[];for(const k of FXK){const v=S.fx[k[0]];if(v)p.push(k[1]+' '+v);}
  const st=STEMS.find(x=>x[0]===S.stem);if(S.stem&&st)p.push(st[1]);
  return p.length?p.join(' · '):'Efeitos e stems';
}
function updFxBtn(){const b=$('npFx');if(!b)return;const on=fxOn()||!!S.stem;b.classList.toggle('on',on);$('npFxTxt').textContent=fxLabel();}
// Troca efeito/stem com a musica tocando: recomeca do mesmo ponto com o audio novo (no mesmo toque, iOS ok).
function reloadCur(){
  if(!S.cur)return;
  const at=Math.floor(curTime()),was=!A.paused&&!A.error;
  clearTimeout(W.fxT);   // varios toques seguidos nos efeitos: recarrega uma vez so, no fim
  W.fxT=setTimeout(()=>{if(S.cur)loadCur(at,was);},350);
}
function fxDots(v){return h('span',{class:'fxD','aria-hidden':'true'},h('i',{class:v>=1?'on':''}),h('i',{class:v>=2?'on':''}),h('i',{class:v>=3?'on':''}));}
const STM={t:0,id:''};
function stemStatusTxt(j){
  if(!S.stemsOk||(j&&j.instalado===false))return 'O PC não tem o separador de stems (Demucs). Instale o Demucs no PC para usar.';
  if(!j)return '';
  const e=str(j.estado,20);
  if(e==='pronto')return 'Stems desta música prontos.';
  if(e==='fila')return 'Na fila para separar...';
  if(e==='baixando')return 'O PC está baixando o áudio para separar...';
  if(e==='separando')return 'Separando: '+(j.pct|0)+'% (na 1ª vez leva ~metade da música; toca a completa enquanto isso)';
  if(e==='falhou')return 'A separação falhou'+(j.erro?': '+str(j.erro,160):'.');
  return '';
}
function stemPaint(j){
  const el=$('stemSt');if(el)el.textContent=stemStatusTxt(j);
  const bar=$('stemBar');if(bar){const e=j?str(j.estado,20):'';bar.parentNode.hidden=!(e==='separando'||e==='fila'||e==='baixando');bar.style.width=(e==='separando'?(j.pct|0):(e==='pronto'?100:4))+'%';}
}
// Pergunta ao PC como esta a separacao da faixa atual; com modo de stem ligado, pede para separar e acompanha.
function stemCheck(t){
  clearTimeout(STM.t);
  if(!t||!S.stem||!S.stemsOk){stemPaint(null);return;}
  const id=t.id;STM.id=id;
  const ask=()=>{
    if(STM.id!==id||!S.stem||!S.cur||S.cur.id!==id)return;
    raw('/api/stems/'+enc(id),S.stemReady[id]?null:{fila:false}).then(r=>{
      if(STM.id!==id||!r.ok)return;
      stemPaint(r.j);
      if(str(r.j.estado,20)==='pronto'){
        if(!S.stemReady[id]){S.stemReady[id]=true;if(S.cur&&S.cur.id===id&&S.stem){reloadCur();toast('Tocando: '+((STEMS.find(x=>x[0]===S.stem)||['',''])[1]));}}
        return;
      }
      if(r.j.instalado===false||str(r.j.estado,20)==='falhou')return;
      STM.t=setTimeout(ask,4000);
    }).catch(()=>{STM.t=setTimeout(ask,8000);});
  };
  ask();
}
function fxSheet(){
  sheet(box=>{
    box.appendChild(h('h2',{class:'shTitle',text:'Efeitos'}));
    box.appendChild(h('p',{class:'shNote center',text:'Cada toque sobe o nível (1, 2, 3); o próximo desliga. O PC aplica no áudio.'}));
    const grid=h('div',{class:'fxGrid'});
    for(const k of FXK){
      const b=h('button',{class:'fxB'+(S.fx[k[0]]?' on':''),type:'button','aria-label':k[1]+': nível '+S.fx[k[0]]},h('span',{text:k[1]}),fxDots(S.fx[k[0]]));
      b.addEventListener('click',()=>{
        const v=(S.fx[k[0]]+1)%4;S.fx[k[0]]=v;
        if(k[0]==='slow'&&v)S.fx.speed=0;
        if(k[0]==='speed'&&v)S.fx.slow=0;
        saveFx();
        grid.querySelectorAll('.fxB').forEach((x,i)=>{const kk=FXK[i][0];x.classList.toggle('on',S.fx[kk]>0);x.replaceChild(fxDots(S.fx[kk]),x.lastChild);x.setAttribute('aria-label',FXK[i][1]+': nível '+S.fx[kk]);});
        updFxBtn();reloadCur();
      });
      grid.appendChild(b);
    }
    box.appendChild(grid);
    box.appendChild(h('div',{class:'shCenter'},h('button',{class:'btnS',type:'button',text:'Desligar efeitos',onclick:()=>{for(const k of FXK)S.fx[k[0]]=0;saveFx();updFxBtn();reloadCur();fxSheet();}})));
    box.appendChild(h('h2',{class:'secT',text:'Stems'}));
    box.appendChild(h('p',{class:'shNote',text:'Separa a música em partes (vocal, bateria, baixo e o resto) no PC, com o Demucs.'}));
    const chips=h('div',{class:'stemChips'});
    for(const m of STEMS){
      chips.appendChild(h('button',{class:'chip'+(S.stem===m[0]?' on':''),type:'button','aria-pressed':String(S.stem===m[0]),text:m[1],disabled:(!S.stemsOk&&m[0])?true:null,onclick:()=>{
        if(S.stem===m[0])return;
        const had=S.cur&&stemFor(S.cur);S.stem=m[0];saveFx();
        chips.querySelectorAll('.chip').forEach((x,i)=>{const on=STEMS[i][0]===S.stem;x.classList.toggle('on',on);x.setAttribute('aria-pressed',String(on));});
        updFxBtn();
        if(S.cur&&(had||stemFor(S.cur)))reloadCur();
        stemCheck(S.cur);
      }}));
    }
    box.appendChild(chips);
    box.appendChild(h('p',{id:'stemSt',class:'stemSt',role:'status','aria-live':'polite'}));
    const bw=h('div',{class:'stemBar'},h('i',{id:'stemBar'}));bw.hidden=true;box.appendChild(bw);
    stemPaint(null);
    if(S.cur&&S.stem)stemCheck(S.cur);else if(!S.stemsOk)stemPaint(null);
  });
}
// ------------------------------------------------------ onda no ritmo --
// Energia e batidas a cada 25 ms calculadas pelo PC (/api/ritmo). Desenha a onda do Tocando agora e faz os
// icones de "tocando" das listas pularem na batida. Sem Web Audio: a tela bloqueada do iPhone segue tocando.
const RIT={key:'',data:null};
function loadRitmo(t,tent){
  const key=t?t.id+'|'+stemFor(t):'';if(RIT.key===key&&!tent)return;
  RIT.key=key;RIT.data=null;document.body.classList.remove('hasBeat');
  if(!t)return;
  raw('/api/ritmo/'+enc(t.id)+(stemFor(t)?'?stem='+enc(stemFor(t)):'')).then(r=>{
    if(RIT.key!==key)return;
    if(r.st===429&&(tent|0)<3){setTimeout(()=>{if(RIT.key===key)loadRitmo(t,(tent|0)+1);},3000);return;}   // o PC ja esta analisando outras
    if(!r.ok||!Array.isArray(r.j.e)||!r.j.e.length)return;
    RIT.data={hop:Math.max(5,r.j.hop|0)||25,e:r.j.e,b:Array.isArray(r.j.b)?r.j.b:[]};
    if(S.cur&&S.cur.id===t.id&&!S.cur.d){S.cur.d=Math.floor(r.j.e.length*RIT.data.hop/1000);updTime();}   // arquivo local: a duracao sai daqui
    document.body.classList.add('hasBeat');kickWave();
  }).catch(()=>{});
}
let waveRaf=0,beat=0,waveLast=0;
function kickWave(){if(!waveRaf)waveRaf=requestAnimationFrame(waveLoop);}
function waveLoop(ts){
  waveRaf=0;
  const playing=!!S.cur&&!A.paused&&!A.error,d=RIT.data;
  const dt=waveLast?Math.min(200,ts-waveLast):16;waveLast=ts;
  const tMs=S.cur?curTime()*1000:0;
  let b=0;if(d&&playing){const i=Math.floor(tMs/d.hop);b=(d.b[i]||0)/255;}
  beat=Math.max(b,beat*Math.pow(.8,dt/30));
  document.body.style.setProperty('--beat',beat.toFixed(3));
  const cv=$('npWave');
  if(NP.open&&cv&&cv.clientWidth){
    const dpr=Math.min(2,window.devicePixelRatio||1),W0=Math.round(cv.clientWidth*dpr),H0=Math.round(cv.clientHeight*dpr);
    if(cv.width!==W0||cv.height!==H0){cv.width=W0;cv.height=H0;}
    const g=cv.getContext('2d');g.clearRect(0,0,W0,H0);
    const N=Math.max(24,Math.min(64,Math.floor(cv.clientWidth/7))),gap=2*dpr,bw=(W0-gap*(N-1))/N;
    const back=2000,span=8000,acc=getComputedStyle(document.documentElement).getPropertyValue('--acc-txt').trim()||'#1db954';
    for(let k=0;k<N;k++){
      const at=tMs-back+span*k/(N-1);
      let v=.06;
      if(d){const i=Math.floor(at/d.hop);if(i>=0&&i<d.e.length){let m=0;for(let q=i;q<i+3&&q<d.e.length;q++)m=Math.max(m,d.e[q]);v=Math.max(.06,Math.sqrt(m/255));}}
      else v=.12+.1*Math.sin(ts/400+k*.6);
      const near=1-Math.min(1,Math.abs(at-tMs)/1400);
      v=Math.min(1,v*(1+.6*beat*near));
      const bh=Math.max(2*dpr,v*H0),x=k*(bw+gap);
      g.fillStyle=at<=tMs?acc:'rgba(255,255,255,.34)';
      g.fillRect(x,(H0-bh)/2,bw,bh);
    }
  }
  if(playing||beat>.01)waveRaf=requestAnimationFrame(waveLoop);
}

// ------------------------------------------------------------ player --
const A=$('audio');
const NP={open:false,drag:false,at:0};
const W={t:0,seekTo:0,retry:0,pos:0,cut:0,cutOff:-1,pausedAt:0,seekable:false,swapAt:0,pre:'',sess:0,fxT:0};
function curIdx(){return S.order[S.pos];}
// "modo stream" (sm): online, com efeito ou com stem. O PC converte e manda sem tamanho: o tempo da musica
// e o deslocamento pedido (?t=) + o que tocou vezes a velocidade (slow/speed mudam o ritmo).
function curTime(){return S.cur&&S.cur.sm?S.off+(A.currentTime||0)*S.rate:(A.currentTime||0);}
function curDur(){
  if(!S.cur)return 0;
  const d=A.duration,ok=isFinite(d)&&d>0;
  if(S.cur.sm)return S.cur.d||(W.seekable&&ok?Math.floor(S.off+d*S.rate):0);
  return ok?d:(S.cur.d||0);
}
function mkOrder(start){
  const n=S.queue.length,idx=[...Array(n).keys()];
  if(S.shuffle){
    for(let i=n-1;i>0;i--){const j=Math.floor(Math.random()*(i+1));[idx[i],idx[j]]=[idx[j],idx[i]];}
    if(start>=0){const k=idx.indexOf(start);[idx[0],idx[k]]=[idx[k],idx[0]];}
    S.order=idx;S.pos=0;
  }else{S.order=idx;S.pos=start>=0?start:0;}
}
function playFrom(list,i,ctx){
  if(!list.length)return;
  if(i>=0&&S.cur&&S.ctx.key===ctx.key&&S.cur.id===list[i].id&&S.queue.length===list.length){if(A.paused||A.error)togglePlay();return;}
  S.queue=list.slice();S.ctx={name:ctx.name,key:ctx.key};
  if(i<0)i=Math.floor(Math.random()*list.length);
  mkOrder(i);S.errs=0;loadCur(0,true);
}
function playNext(t){
  if(!S.cur){playFrom([t],0,{name:'Busca',key:'one'});return;}
  S.queue.push(t);S.order.splice(S.pos+1,0,S.queue.length-1);
  toast('Vai tocar a seguir');
}
const FXK=[['slow','Slow'],['speed','Speed'],['reverb','Reverb'],['bass','Grave'],['d8','8D']];
const STEMS=[['','Completa'],['vocal','Só vocal'],['instrumental','Só música'],['bateria','Bateria'],['baixo','Baixo'],['outros','Outros']];
function fxOn(){return FXK.some(k=>S.fx[k[0]]>0);}
function fxRate(){const sl=[1,.9,.82,.75],sp=[1,1.1,1.2,1.3];return S.fx.slow?sl[S.fx.slow]:sp[S.fx.speed];}
function fxParam(){const p=[];for(const k of FXK){const v=S.fx[k[0]];if(v)p.push((k[0]==='d8'?'8d':k[0])+':'+v);}return p.join(',');}
function stemFor(t){return S.stem&&t&&S.stemReady[t.id]?S.stem:'';}
function streamMode(t){return !!t&&(t.o||fxOn()||!!stemFor(t));}
function srcFor(t,at){
  const q=[];if(fxOn())q.push('fx='+enc(fxParam()));const st=stemFor(t);if(st)q.push('stem='+enc(st));
  if(t.o||q.length){q.unshift('t='+enc(String(at)));return (t.o?'/api/online/ouvir/':'/api/faixa/')+enc(t.id)+'?'+q.join('&');}
  return '/api/faixa/'+enc(t.id);
}
function srcOnline(t,at){return srcFor(t,at);}
function loadCur(at,play){
  const t=S.queue[curIdx()];if(!t)return;
  S.cur=t;S.off=0;W.retry=0;W.seekTo=0;W.cut=0;W.cutOff=-1;W.seekable=false;W.swapAt=Date.now();clearTimeout(W.t);npMsg('');
  t.sm=streamMode(t);S.rate=t.sm?fxRate():1;
  if(t.sm){S.off=Math.max(0,Math.floor(at||0));A.src=srcFor(t,S.off);noteReq(1);}
  else{A.src='/api/faixa/'+enc(t.id);noteReq(2);if(at>0)W.seekTo=at;}
  loadRitmo(t);updFxBtn();stemCheck(t);
  if(play)doPlay();else{setBusy(false);}
  updNow();updMeta();updPlayBtns();updTime();
}
// Avisa o PC quais sao as proximas online: ele adianta a extracao (senao cada troca espera o CLI de midia).
function prefetchNext(){
  if(!S.queue.length||S.pos<0)return;
  const ids=[];
  for(let k=1;k<=2;k++){const p=S.pos+k;if(p>=S.order.length)break;const t=S.queue[S.order[p]];if(t&&t.o)ids.push(t.id);}
  const key=ids.join(',');
  if(!ids.length||key===W.pre)return;
  W.pre=key;raw('/api/preparar',{ids}).catch(()=>{});
}
function doPlay(){
  if(!S.cur)return;
  const p=A.play();
  if(p&&p.catch)p.catch(err=>{
    if(!err||err.name==='AbortError')return;
    setBusy(false);updPlayBtns();
    if(err.name==='NotAllowedError')toast('Toque em play para começar.');
  });
}
function togglePlay(){
  if(!S.cur){
    if(S.libOk&&S.lib.length)playFrom(S.lib,S.shuffle?-1:0,{name:'Biblioteca do PC',key:'lib'});
    return;
  }
  if(A.paused||A.error){
    // Depois de um erro o play() nao recarrega nada (e o WebKit deixa paused=false): recria o src no mesmo
    // toque. Online pausado por mais de 1 min: o PC ja fechou aquele stream, recomeca de onde estava.
    const velho=S.cur.sm&&W.pausedAt>0&&Date.now()-W.pausedAt>60000;
    if(!A.getAttribute('src')||A.error||A.networkState===3||velho)loadCur(Math.floor(curTime()),true);
    else doPlay();
  }else A.pause();
}
// O iOS recarrega a aba quando fica sem memoria (ou o PWA volta do zero): guarda a fila e o ponto.
function saveSess(force){
  try{
    if(!S.cur||!S.queue.length){LS.set('sess','');return;}
    const now=Date.now();if(!force&&now-W.sess<5000)return;W.sess=now;
    LS.set('sess',JSON.stringify({q:S.queue.slice(0,300),o:S.order.slice(0,300),p:S.pos,at:Math.floor(curTime()),c:S.ctx,t:now}));
  }catch(e){}
}
function restoreSess(){
  if(S.cur)return;
  let d=null;try{d=JSON.parse(LS.get('sess','')||'null');}catch(e){}
  if(!d||!Array.isArray(d.q)||!d.q.length||!Array.isArray(d.o)||!(d.p>=0)||d.p>=d.o.length)return;
  if(Date.now()-(d.t||0)>24*3600*1000){LS.set('sess','');return;}
  S.queue=d.q;S.order=d.o;S.pos=d.p;S.ctx=d.c&&d.c.key?d.c:{name:'',key:''};
  loadCur(Math.max(0,d.at||0),false);   // pausada: o iOS so toca depois de um toque
  npMsg('');
}
function stopAudio(){
  try{A.pause();A.removeAttribute('src');A.load();}catch(e){}
  clearTimeout(W.t);S.cur=null;S.queue=[];S.order=[];S.pos=-1;S.ctx={name:'',key:''};
  setBusy(false);updNow();updPlayBtns();LS.set('sess','');
  if('mediaSession' in navigator)try{navigator.mediaSession.metadata=null;}catch(e){}
}
function next(auto){
  if(!S.queue.length)return;
  if(auto&&S.repeat===2){seek(0);doPlay();return;}
  if(S.pos+1<S.order.length)S.pos++;
  else{
    if(S.shuffle&&S.order.length>1)mkOrder(-1);
    S.pos=0;
    if(auto&&S.repeat===0){loadCur(0,false);return;}
  }
  if(!auto)S.errs=0;
  loadCur(0,true);
}
function prev(){
  if(!S.queue.length)return;
  if(curTime()>3){seek(0);if(A.paused)doPlay();return;}
  if(S.pos>0)S.pos--;
  else if(S.repeat===1)S.pos=S.order.length-1;
  else{seek(0);return;}
  S.errs=0;loadCur(0,true);
}
function seek(s){
  const t=S.cur;if(!t)return;
  const d=curDur();s=Math.max(0,d?Math.min(s,Math.max(0,d-1)):s);
  if(t.sm){
    const rel=(s-S.off)/S.rate;   // stream com tamanho (Range): o proprio audio pula, sem pedir tudo de novo
    if(W.seekable&&rel>=0&&isFinite(A.duration)&&rel<=A.duration){try{A.currentTime=rel;updTime();posState(true);return;}catch(e){}}
    const play=!A.paused;
    S.off=Math.floor(s);A.src=srcFor(t,S.off);W.swapAt=Date.now();noteReq(1);
    if(play)doPlay();
  }else if(A.readyState>=1){try{A.currentTime=s;}catch(e){W.seekTo=s;}}
  else W.seekTo=s;
  updTime();posState(true);
}
function setShuffle(on){
  if(S.shuffle===on)return;
  S.shuffle=on;LS.set('shuffle',on?1:0);
  if(S.queue.length&&S.pos>=0){
    const ci=curIdx();
    if(on)mkOrder(ci);else{S.order=[...Array(S.queue.length).keys()];S.pos=ci;}
  }
  updModes();
}
function cycleRepeat(){S.repeat=(S.repeat+1)%3;LS.set('repeat',S.repeat);updModes();toast(['Repetir desligado','Repetindo a fila','Repetindo esta música'][S.repeat]);}
function updModes(){
  const sb=$('npShuf');sb.classList.toggle('on',S.shuffle);sb.setAttribute('aria-pressed',String(S.shuffle));
  const rb=$('npRep');setIc(rb,S.repeat===2?'repeat1':'repeat');rb.classList.toggle('on',S.repeat>0);
  rb.setAttribute('aria-label',['Repetir: desligado','Repetir: tudo','Repetir: uma'][S.repeat]);
  markAll();
}

// --------------------------------------------------- telas do player --
function markAll(){
  const id=S.cur?S.cur.id:'',playing=!!S.cur&&!A.paused&&!A.error;
  document.body.classList.toggle('isPlaying',playing);
  document.querySelectorAll('#views [data-tid]').forEach(e=>e.classList.toggle('cur',e.dataset.tid===id));
  document.querySelectorAll('#views [data-pkey]').forEach(e=>e.classList.toggle('cur',!!id&&e.dataset.pkey===S.ctx.key));
  document.querySelectorAll('[data-plplay]').forEach(b=>{
    const on=playing&&b.dataset.plplay===S.ctx.key;setIc(b,on?'pause':'play');b.setAttribute('aria-label',on?'Pausar':'Tocar');
  });
  document.querySelectorAll('[data-shuf]').forEach(b=>{b.classList.toggle('on',S.shuffle);b.setAttribute('aria-pressed',String(S.shuffle));});
}
function setArt(box,t,cls){
  const k=t?t.id:'';if(box.dataset.k===k)return;box.dataset.k=k;clear(box);if(t)box.appendChild(art(t,cls));
}
function updNow(){
  const t=S.cur;
  $('mini').hidden=!t;document.body.classList.toggle('hasMini',!!t);
  if(!t){if(NP.open)closeL('np');if(S.ready)document.title='Remix · '+S.host;return;}
  document.title=t.t+(t.a?' · '+t.a:'');   // iOS sem Media Session mostra o titulo da pagina na tela bloqueada
  setArt($('miniArt'),t,'fill');setArt($('npArt'),t,'fill');
  $('miniT').textContent=t.t;$('miniA').textContent=t.a||'Artista desconhecido';
  $('npT').textContent=t.t;$('npA').textContent=t.a||'Artista desconhecido';
  $('npOn').hidden=!t.o;
  $('npFrom').textContent=S.ctx.name||'Busca';
  $('npFromBtn').disabled=!findPl(S.ctx.key);
  $('npDev').textContent='Tocando neste celular · via '+S.host;
  markAll();
}
function updPlayBtns(){
  const on=!!S.cur&&!A.paused&&!A.error;
  for(const id of ['miniPlay','npPlay']){const b=$(id);setIc(b,on?'pause':'play');b.setAttribute('aria-label',on?'Pausar':'Tocar');}
  if('mediaSession' in navigator)try{navigator.mediaSession.playbackState=S.cur?(on?'playing':'paused'):'none';}catch(e){}
  markAll();
}
function setBusy(on){$('npPlay').classList.toggle('wait',on);$('miniPlay').classList.toggle('wait',on);}
function npMsg(t){const m=$('npMsg');m.textContent=t||'';m.hidden=!t;}
function updTime(){
  const d=curDur(),c=curTime(),p=d?Math.max(0,Math.min(1,c/d)):0;
  $('miniFill').style.transform='scaleX('+p.toFixed(4)+')';
  const sk=$('npSeek');sk.disabled=!S.cur||!d;
  if(!NP.drag){sk.value=String(Math.round(p*1000));sk.style.setProperty('--p',(p*100).toFixed(2)+'%');$('npPos').textContent=fmt(d?Math.min(c,d):c);}
  $('npLen').textContent=d?fmt(d):'--:--';
  if(LYR.aberta&&LYR.sync)pintaLetra();   // a letra acompanha a música
  posState(false);saveSess();
}
let posT=0;
function posState(force){
  if(!('mediaSession' in navigator)||!navigator.mediaSession.setPositionState)return;
  const now=Date.now();if(!force&&now-posT<1000)return;posT=now;
  const d=curDur();
  try{
    const r=S.cur&&S.cur.sm?S.rate:1;
    if(S.cur&&d>0)navigator.mediaSession.setPositionState({duration:d/r,position:Math.max(0,Math.min(curTime(),d))/r,playbackRate:1});
    else navigator.mediaSession.setPositionState();   // sem duracao: limpa (senao fica a da faixa anterior)
  }catch(e){}
}
function updMeta(){
  if(!('mediaSession' in navigator)||!S.cur||typeof MediaMetadata==='undefined')return;
  const t=S.cur;
  const artwork=t.c?[{src:new URL(coverUrl(t),location.href).href,sizes:'512x512'}]:[{src:new URL('/icon.png',location.href).href,sizes:'256x256',type:'image/png'}];
  try{navigator.mediaSession.metadata=new MediaMetadata({title:t.t,artist:t.a||'',album:S.ctx.name||S.host,artwork});}catch(e){}
}
function openNp(){
  if(!S.cur||NP.open)return;
  NP.open=true;NP.at=Date.now();const np=$('np');np.inert=false;np.classList.add('open');np.setAttribute('aria-hidden','false');
  pushL('np');updLock();updTime();kickWave();
}
function closeNpNow(){
  NP.open=false;const np=$('np');np.classList.remove('open');np.setAttribute('aria-hidden','true');np.inert=true;updLock();
}
// avisos de rede: o stream online demora a comecar (o PC converte ao vivo)
function armStall(){
  clearTimeout(W.t);
  if(!S.cur)return;
  const t=S.cur;
  W.t=setTimeout(()=>{
    if(S.cur!==t||A.paused||A.readyState>=3)return;
    if(W.retry<1){
      W.retry++;toast('Conexão lenta com o PC. Tentando de novo...');
      const at=curTime();
      if(t.sm){S.off=Math.floor(at);A.src=srcFor(t,S.off);noteReq(1);doPlay();}
      else{A.load();W.seekTo=at;noteReq(2);doPlay();}
      armStall();
    }else npMsg(t.o?'A música online não está chegando. O PC pode estar sem internet ou a fonte está lenta.':'A conexão com o PC está lenta.');
  },t.sm?25000:12000);
}
function onAudioError(){
  if(!S.cur||!A.getAttribute('src'))return;
  const t=S.cur;setBusy(false);clearTimeout(W.t);
  // religou perto do fim (ver 'ended') e nao veio nada: a musica tinha acabado mesmo
  if(t.sm&&W.cutOff>=0&&W.cutOff===S.off&&S.off>=t.d-30&&!(A.currentTime>1)){W.cutOff=-1;next(true);return;}
  if(t.sm&&t.d>0&&curTime()>=t.d-3){next(true);return;}   // o iOS as vezes da 'error' em vez de 'ended' no fim do stream
  const at=curTime();
  if(W.retry<1&&at>2&&(t.sm||A.error&&A.error.code===2)){ // caiu no meio: continua de onde estava
    W.retry++;
    if(t.sm){S.off=Math.floor(at);A.src=srcFor(t,S.off);noteReq(1);}
    else{A.load();W.seekTo=at;noteReq(2);}
    doPlay();return;
  }
  if(Date.now()-W.swapAt<6000&&W.retry<3){   // acabei de trocar efeito/stem ou de avancar: o PC pode ter recusado por excesso de pedidos
    W.retry++;const volta=Math.max(0,at);
    setTimeout(()=>{if(S.cur===t)loadCur(volta,true);},900*W.retry);
    npMsg('O PC está ocupado; tentando de novo...');return;
  }
  const msg=t.o?'Não deu para tocar “'+t.t+'” online agora. O PC pode estar sem internet ou a fonte recusou.'
    :'Não consegui tocar “'+t.t+'”. O arquivo pode ter saído do PC ou o formato não toca neste navegador.';
  try{A.pause();}catch(e){}   // o botao volta a mostrar play (e o proximo toque recarrega)
  toast(msg,1);npMsg(msg);updPlayBtns();
  raw('/api/estado').then(r=>{if(r.st===401&&S.ready)toPair('Este aparelho foi desconectado do PC.');}).catch(()=>{});
  S.errs++;
  if(S.errs<3&&S.pos+1<S.order.length)setTimeout(()=>{if(S.cur===t){S.pos++;loadCur(0,true);}},2000);
}
// O PC manda o stream com tamanho quando sabe a duracao: ai o proprio navegador pula no tempo (sem recomecar).
function noteSeekable(){
  const ok=!!(S.cur&&S.cur.sm&&((isFinite(A.duration)&&A.duration>0)||(A.seekable&&A.seekable.length&&A.seekable.end(0)>1)));
  if(ok!==W.seekable){W.seekable=ok;updTime();}
}
A.addEventListener('canplay',noteSeekable);
A.addEventListener('progress',noteSeekable);
A.addEventListener('loadedmetadata',()=>{
  noteSeekable();
  if(S.cur&&!S.cur.sm&&isFinite(A.duration)&&A.duration>0&&!S.cur.d)S.cur.d=Math.floor(A.duration);   // guarda a duracao (para efeito/stem depois)
  if(W.seekTo>0&&S.cur&&!S.cur.sm){try{A.currentTime=W.seekTo;}catch(e){}W.seekTo=0;}updTime();});
A.addEventListener('timeupdate',()=>{updTime();if(A.currentTime>0&&!A.paused){clearTimeout(W.t);}});
A.addEventListener('durationchange',()=>{noteSeekable();updTime();});
A.addEventListener('play',()=>{updPlayBtns();if(A.readyState<3){setBusy(true);armStall();}});
A.addEventListener('pause',()=>{W.pausedAt=Date.now();setBusy(false);clearTimeout(W.t);updPlayBtns();posState(true);saveSess(true);});
A.addEventListener('waiting',()=>{setBusy(true);if(S.cur&&S.cur.sm)npMsg('O PC está preparando a música...');armStall();});
A.addEventListener('stalled',()=>{if(!A.paused)armStall();});
A.addEventListener('playing',()=>{W.pausedAt=0;setBusy(false);npMsg('');clearTimeout(W.t);S.errs=0;updPlayBtns();posState(true);kickWave();prefetchNext();saveSess();});
A.addEventListener('ended',()=>{
  setBusy(false);
  // O stream online nao tem tamanho: se a conexao cai (rede, tunel, pausa longa e o PC desiste)
  // o navegador so ve "acabou". Antes do fim conhecido, religa de onde parou em vez de pular a faixa.
  const t=S.cur;
  if(t&&t.sm&&t.d>0&&W.cut<3&&A.currentTime>1){
    const at=Math.floor(curTime());
    if(at<t.d-8){W.cut++;W.cutOff=at;S.off=at;A.src=srcFor(t,at);noteReq(1);doPlay();updTime();return;}
  }
  next(true);
});
A.addEventListener('error',onAudioError);

// ---------------------------------------------------------- iniciar --
function setupAccent(){
  const root=document.documentElement;
  const v=getComputedStyle(root).getPropertyValue('--accent').trim();
  const m=/^#([0-9a-f]{6})$/i.exec(v);const hex=m?m[1]:'1db954';
  const r=parseInt(hex.slice(0,2),16),g=parseInt(hex.slice(2,4),16),b=parseInt(hex.slice(4,6),16);
  const lin=c=>{c/=255;return c<=0.03928?c/12.92:Math.pow((c+0.055)/1.055,2.4);};
  const lum=0.2126*lin(r)+0.7152*lin(g)+0.0722*lin(b);
  if(!m)root.style.setProperty('--accent','#'+hex);
  root.style.setProperty('--acc-rgb',r+','+g+','+b);
  root.style.setProperty('--on-acc',lum>0.179?'#000':'#fff');
  const mix=c=>Math.round(c+(255-c)*0.45);
  root.style.setProperty('--acc-txt',lum<0.12?'rgb('+mix(r)+','+mix(g)+','+mix(b)+')':'#'+hex);
}
function init(){
  // Sem um listener de toque o Safari do iOS nem aplica :active: os botoes pareciam mortos.
  document.addEventListener('touchstart',()=>{},{passive:true});
  setupAccent();
  // fragmento #q=<token> do QR code: le e tira da barra na hora (nao fica no historico)
  const hs=history.state;
  if((location.hash||'').startsWith('#q=')||(location.hash||'').startsWith('#a=')){
    const m=/^#([qa])=([0-9a-fA-F]{32})$/.exec(location.hash);
    if(m&&m[1]==='q')PAIR.token=m[2].toLowerCase();
    else if(m)PAIR.chave=m[2].toLowerCase();   // link/QR de religar: o vínculo está no PC
    try{history.replaceState(null,'',location.pathname+location.search);}catch(e){}
  }else if(hs&&hs.rl>0){try{history.go(-hs.rl);}catch(e){}}
  document.querySelectorAll('[data-ic]').forEach(e=>setIc(e,e.dataset.ic));
  // QR lido com a pagina ja aberta (so muda o fragmento, sem recarregar)
  window.addEventListener('hashchange',()=>{
    const hsh=location.hash||'';if(!hsh.startsWith('#q=')&&!hsh.startsWith('#a='))return;
    const m=/^#([qa])=([0-9a-fA-F]{32})$/.exec(hsh);
    try{history.replaceState(history.state,'',location.pathname+location.search);}catch(e){}
    if(!m)return;
    if(m[1]==='a'){   // QR/link de religar lido com a página aberta
      if(S.ready){toast('Este aparelho já está conectado a '+S.host+'.');return;}
      entrarComChave(m[2].toLowerCase()).then(ok=>{if(ok)boot();else{PAIR.gen++;pairDone();pairMode('pin','Essa chave não vale mais neste PC.','err');}});
      return;
    }
    if(S.ready){toast('Este aparelho já está conectado a '+S.host+'.');return;}
    PAIR.gen++;pairDone();PAIR.token=m[2].toLowerCase();pairMode('qr');pingHost();
  });
  // preferencias locais
  const tab=LS.get('tab','home');S.tab=['home','search','lib'].includes(tab)?tab:'home';
  const lf=LS.get('lfil','all');S.libFilter=['all','pc','mine','sh'].includes(lf)?lf:'all';
  S.sq.mode=LS.get('smode','pc')==='online'?'online':'pc';
  const fo=+LS.get('fonte','0');S.sq.fonte=fo>=0&&fo<=2?fo:0;
  S.shuffle=LS.get('shuffle','0')==='1';
  const rp=+LS.get('repeat','0');S.repeat=rp>=0&&rp<=2?rp:0;
  try{const f=JSON.parse(LS.get('fx','{}'));for(const k of FXK){const v=+f[k[0]];S.fx[k[0]]=v>=0&&v<=3?v|0:0;}if(S.fx.slow)S.fx.speed=0;}catch(e){}
  {const st=LS.get('stem','');S.stem=STEMS.some(x=>x[0]===st)?st:'';}
  // pareamento
  $('pairName').value=LS.get('nome','');
  $('pairRemember').checked=LS.get('lembrar','1')!=='0';
  $('pairBtn').addEventListener('click',pairGo);
  $('pairPin').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();pairGo();}});
  $('pairName').addEventListener('keydown',e=>{if(e.key==='Enter'){e.preventDefault();if(PAIR.mode==='qr')pairGo();else $('pairPin').focus();}});
  // o teclado numerico do iPhone nao tem Enter: deixa o botao Conectar a vista
  $('pairPin').addEventListener('focus',()=>setTimeout(()=>{try{$('pairBtn').scrollIntoView({block:'nearest'});}catch(e){}},450));
  // teclado aberto (visualViewport): a folha sobe acima dele e o dock some
  if(window.visualViewport){
    const vv=window.visualViewport;
    const kb=()=>{const k=Math.max(0,Math.round(window.innerHeight-vv.height-vv.offsetTop));document.documentElement.style.setProperty('--kb',(k>80?k:0)+'px');document.body.classList.toggle('kbOpen',k>80);};
    vv.addEventListener('resize',kb);vv.addEventListener('scroll',kb);
  }
  // navegacao
  document.querySelectorAll('#nav button').forEach(b=>b.addEventListener('click',()=>showTab(b.dataset.tab)));
  // mini player
  const mini=$('miniOpen'),miniBox=$('mini');let sx=null,sy=0,swiped=false,dragX=false;
  mini.addEventListener('click',()=>{if(swiped){swiped=false;return;}openNp();});
  const miniReset=()=>{miniBox.style.transition='';miniBox.style.transform='';};
  miniBox.addEventListener('touchstart',e=>{if(e.touches.length!==1||!S.cur)return;sx=e.touches[0].clientX;sy=e.touches[0].clientY;dragX=false;},{passive:true});
  miniBox.addEventListener('touchmove',e=>{
    if(sx==null)return;
    const t=e.touches[0],dx=t.clientX-sx,dy=t.clientY-sy;
    if(!dragX&&Math.abs(dx)>12&&Math.abs(dx)>Math.abs(dy)*1.4)dragX=true;
    if(!dragX)return;
    if(e.cancelable)e.preventDefault();   // segura o gesto de voltar do Safari
    miniBox.style.transition='none';miniBox.style.transform='translateX('+(dx*0.4).toFixed(1)+'px)';
  },{passive:false});
  miniBox.addEventListener('touchend',e=>{
    if(sx==null)return;const dx=e.changedTouches[0].clientX-sx;sx=null;
    if(!dragX)return;
    dragX=false;miniReset();
    if(Math.abs(dx)<55)return;
    swiped=true;setTimeout(()=>{swiped=false;},500);
    if(dx<0){next(false);toast('Próxima');}else{prev();toast('Anterior');}
  },{passive:true});
  miniBox.addEventListener('touchcancel',()=>{sx=null;dragX=false;miniReset();});
  $('miniPlay').addEventListener('click',togglePlay);
  $('miniAdd').addEventListener('click',()=>{if(S.cur)addSheet(S.cur);});
  // tocando agora
  const np=$('np');np.inert=true;
  $('npClose').addEventListener('click',()=>closeL('np'));
  $('npMore').addEventListener('click',npMenu);
  $('npAdd').addEventListener('click',()=>{if(S.cur)addSheet(S.cur);});
  $('npFromBtn').addEventListener('click',async()=>{const p=findPl(S.ctx.key);if(!p)return;await closeL('np');openPl(p.key);});
  $('npPlay').addEventListener('click',togglePlay);
  $('npNext').addEventListener('click',()=>next(false));
  $('npPrev').addEventListener('click',prev);
  $('npShuf').addEventListener('click',()=>setShuffle(!S.shuffle));
  $('npRep').addEventListener('click',cycleRepeat);
  $('npFx').addEventListener('click',fxSheet);
  $('npLetra').addEventListener('click',abrirLetra);
  updFxBtn();
  // Barra de posicao: no iOS o range so arrasta se o dedo comecar em cima do polegar e tocar na trilha nao
  // faz nada. A area inteira (barra + tempos) vira a pista: tocar pula para o ponto, arrastar acompanha.
  const sk=$('npSeek'),skBox=sk.parentNode;let skId=null;
  const skShow=p=>{sk.value=String(Math.round(p*1000));sk.style.setProperty('--p',(p*100).toFixed(2)+'%');$('npPos').textContent=fmt(p*curDur());};
  const skAt=x=>{const r=sk.getBoundingClientRect();return r.width>0?Math.max(0,Math.min(1,(x-r.left)/r.width)):0;};
  const skTouch=e=>{for(let i=0;i<e.changedTouches.length;i++)if(e.changedTouches[i].identifier===skId)return e.changedTouches[i];return null;};
  skBox.addEventListener('touchstart',e=>{if(sk.disabled||e.touches.length!==1)return;if(e.cancelable)e.preventDefault();skId=e.touches[0].identifier;NP.drag=true;sk.classList.add('drag');skShow(skAt(e.touches[0].clientX));},{passive:false});
  skBox.addEventListener('touchmove',e=>{const t=skId==null?null:skTouch(e);if(!t)return;if(e.cancelable)e.preventDefault();skShow(skAt(t.clientX));},{passive:false});
  skBox.addEventListener('touchend',e=>{const t=skId==null?null:skTouch(e);if(!t)return;if(e.cancelable)e.preventDefault();skId=null;sk.classList.remove('drag');const p=skAt(t.clientX);skShow(p);NP.drag=false;if(!sk.disabled)seek(p*curDur());},{passive:false});
  skBox.addEventListener('touchcancel',()=>{if(skId==null)return;skId=null;sk.classList.remove('drag');NP.drag=false;updTime();});
  sk.addEventListener('input',()=>{NP.drag=true;skShow(sk.value/1000);});
  sk.addEventListener('change',()=>{NP.drag=false;seek((sk.value/1000)*curDur());});
  ['pointerup','pointercancel','blur'].forEach(n=>sk.addEventListener(n,()=>setTimeout(()=>{if(NP.drag&&skId==null){NP.drag=false;updTime();}},0)));
  dragDown(np,$('npDrag'),()=>closeL('np'));
  // folhas
  $('sheetBg').addEventListener('click',closeSheet);
  // toque duplo: o 2o toque nao aciona nada na folha/Tocando agora que acabou de abrir
  const ghost=(el,o)=>el.addEventListener('click',e=>{if(Date.now()-o.at<350){e.stopPropagation();e.preventDefault();}},true);
  ghost($('sheetWrap'),SH);ghost(np,NP);
  const sh=$('sheet');
  dragDown(sh,sh,closeSheet,e=>sh.scrollTop<=0&&!e.target.closest('input'));
  document.addEventListener('keydown',e=>{
    if(e.key==='Escape'&&L.length){e.preventDefault();popTo(L.length-1);return;}
    if(e.key===' '&&S.ready&&e.target===document.body){e.preventDefault();togglePlay();}
  });
  // Media Session (tela bloqueada, fone bluetooth)
  if('mediaSession' in navigator){
    const ms=navigator.mediaSession;
    const set=(a,f)=>{try{ms.setActionHandler(a,f);}catch(e){}};
    set('play',()=>{if(A.paused||A.error)togglePlay();});
    set('pause',()=>A.pause());
    set('nexttrack',()=>next(false));
    set('previoustrack',prev);
    set('seekto',d=>{if(d&&typeof d.seekTime==='number')seek(d.seekTime*(S.cur&&S.cur.sm?S.rate:1));});
    // sem seekbackward/seekforward: com eles o iOS troca anterior/proxima por "10 s" na tela bloqueada
    set('stop',()=>A.pause());
  }
  updModes();
  // volta para o app: atualiza as listas se ficou muito tempo fora
  document.addEventListener('visibilitychange',()=>{
    if(document.visibilityState==='hidden'){saveSess(true);return;}
    if(S.ready&&Date.now()-S.lastLoad>45000)refresh(false);
  });
  try{if('serviceWorker' in navigator&&window.isSecureContext!==false)navigator.serviceWorker.register('/sw.js').catch(()=>{});}catch(e){}
  boot();
}
try{init();}
catch(e){try{pairMode('off');setPairMsg('Erro ao iniciar: '+((e&&e.message)||e)+'. Recarregue a página.','err');}catch(_){}}
)~~~";

// PWA simples. O service worker nao guarda nada e nao intercepta pedidos (sem "fetch": o audio e as
// capas vao direto ao PC, sem passar por ele).
static const char* MANIFEST_JSON = R"~~~({"id":"/","name":"Remix Player","short_name":"Remix","description":"Ouça as músicas do seu PC no celular","lang":"pt-BR","start_url":"/","scope":"/","display":"standalone","background_color":"#121212","theme_color":"#121212","icons":[{"src":"/icon.png","sizes":"256x256","type":"image/png","purpose":"any"}]})~~~";

static const char* SW_JS = R"~~~(self.addEventListener('install',e=>self.skipWaiting());self.addEventListener('activate',e=>e.waitUntil(self.clients.claim()));)~~~";

// -------------------------------------------------- pagina "conectar" ----
// Arquivo que o usuario manda pelo WhatsApp (aberto como arquivo local, fora da CSP do servidor).
// No iPhone ele abre no visualizador do WhatsApp/Arquivos, que NAO roda JavaScript: por isso os links
// ja vem prontos no HTML (@LINKS@, montados e escapados pelo servidor) e o script so tenta achar o PC
// na rede local sozinho. @NOME@ = nome do PC (ja escapado), @LANS@ = lista JSON das URLs da rede local,
// #ACCENT = cor do tema, @ONACC@ = cor do texto sobre ela. So links: sem PIN, sem IP publico.
static const char* CONNECT_HTML = R"~~~(<!doctype html>
<html lang="pt-BR"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<meta name="theme-color" content="#121212"><meta name="referrer" content="no-referrer"><meta name="format-detection" content="telephone=no">
<title>Conectar ao Remix</title>
<style>
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%;text-size-adjust:100%}
body{margin:0;background:#121212;background-image:radial-gradient(130% 55% at 50% 0%,#ACCENT55,#12121200 70%);color:#fff;font:16px/1.5 system-ui,-apple-system,Segoe UI,Roboto,sans-serif;display:flex;min-height:100vh;align-items:center;justify-content:center;padding:24px 20px;-webkit-tap-highlight-color:rgba(255,255,255,.12)}
.c{width:100%;max-width:420px;text-align:center}
h1{margin:0 0 4px;font-size:28px;font-weight:800;letter-spacing:-.02em}
p{color:#b3b3b3;margin:6px 0 18px}
a.b{display:flex;align-items:center;justify-content:center;min-height:52px;text-align:center;border:1px solid #7c7c7c;color:#fff;text-decoration:none;padding:12px 18px;border-radius:26px;margin:10px 0;font-weight:700;word-break:break-word;-webkit-touch-callout:default}
a.b:active{opacity:.7}
a.b.p{background:#ACCENT;border-color:#ACCENT;color:@ONACC@}
small{color:#8f8f8f;display:block;margin-top:18px;font-size:13px;line-height:1.45}
#st{color:#b3b3b3;font-size:14px;min-height:22px;margin-bottom:6px}
</style></head><body><div class="c">
<h1>Remix Player</h1>
<p>Conectar ao PC <b>@NOME@</b></p>
<div id="st">Toque em um link para abrir o Remix:</div>
<div id="links">@LINKS@</div>
<small>Na primeira vez a página pede o PIN do PC (ou escaneie o QR code do painel HOST) e o PC precisa aceitar o aparelho. Na mesma rede (Wi-Fi ou cabo do mesmo roteador) o link local é mais rápido; fora de casa use o link da internet. No iPhone, se o link abrir dentro do WhatsApp e não carregar, toque em "Abrir no Safari".</small>
</div>
<script>
(function(){
var lans=@LANS@;
var links=document.getElementById('links'), st=document.getElementById('st');
if(!links.getElementsByTagName('a').length){st.textContent='O PC ainda não gerou nenhum link: abra o painel HOST no Remix.';return;}
if(!lans.length||!window.fetch)return;
st.textContent='Procurando o PC na sua rede...';
var done=false;
lans.forEach(function(u){
  var ctl=window.AbortController?new AbortController():null;if(ctl)setTimeout(function(){ctl.abort();},3000);
  fetch(u+'/api/ping',ctl?{mode:'cors',signal:ctl.signal}:{mode:'cors'}).then(function(r){return r.json();}).then(function(j){
    if(!done&&j&&j.app==='remix'){done=true;st.textContent='PC encontrado na rede local. Abrindo...';location.href=u;}
  }).catch(function(){});
});
setTimeout(function(){if(!done)st.textContent='Não achei o PC na rede local sozinho: toque em um dos links.';},4000);
})();
</script></body></html>
)~~~";

} // namespace hostweb
