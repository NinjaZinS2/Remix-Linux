#pragma once
// Estilo da interface (Windows e Linux): a pessoa escolhe nas configuracoes.
//   CLASSICO - o visual original: painel com contorno da cor do tema, LED,
//              corredor de luz, cards com transporte e onda.
//   REMIX    - o visual novo (1.6): barra lateral com a biblioteca, tela inicial
//              com fileiras de novidades, player numa barra embaixo ocupando a
//              largura toda. Substituiu os antigos LIMPO e SPOTIFY + LED.
// A paleta abaixo e a unica fonte de cor "de fundo" das duas cascas.
#include "platform.h"

// Versao mostrada nas configuracoes (mude junto com app.rc, win_diag.h e os build*.sh).
static const wchar_t* const REMIX_VERSAO = L"1.6.2";

enum : int { UI_CLASSICO = 0, UI_REMIX = 1, UI_STYLE_COUNT = 2 };

struct UiPal {
    COLORREF bg;         // fundo da janela
    COLORREF bar;        // cabecalho e coluna do player (so nos estilos novos)
    COLORREF surface;    // card / linha / caixa
    COLORREF surfaceHi;  // card selecionado ou sob o mouse
    COLORREF border;     // contorno / divisoria discreta
    COLORREF borderHi;   // contorno em destaque
    COLORREF text;       // texto principal
    COLORREF textDim;    // texto secundario
    COLORREF textFaint;  // rotulos pequenos
};
inline const UiPal& UiPalFor(int style) {
    static const UiPal classico = {
        RGB(5,7,18), RGB(5,7,18), RGB(9,12,25), RGB(18,21,34),
        RGB(40,44,65), RGB(60,64,88), RGB(235,236,242), RGB(145,147,160), RGB(120,124,150) };
    // REMIX: preto quase puro na moldura (lateral e barra de baixo) e cinza
    // escuro na area de conteudo, como os players modernos.
    static const UiPal remix = {
        RGB(18,18,18), RGB(0,0,0), RGB(28,28,28), RGB(44,44,44),
        RGB(38,38,38), RGB(70,70,70), RGB(255,255,255), RGB(179,179,179), RGB(138,138,138) };
    return style == UI_CLASSICO ? classico : remix;
}
inline const wchar_t* UiStyleName(int style) {
    return style == UI_CLASSICO ? L"CLÁSSICO" : L"REMIX";
}
inline const wchar_t* UiStyleHint(int style) {
    return style == UI_CLASSICO ? L"o visual original, com LED e cards"
                                : L"lateral com a biblioteca, início com novidades e player embaixo";
}
// Cantos dos estilos novos: card/campo 6, botao 4 (o classico usa os raios antigos).
static const int UI_R_CARD = 6, UI_R_PILL = 4;
