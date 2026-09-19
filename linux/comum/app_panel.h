#pragma once
// Paineis montados uma vez (codigo comum) e desenhados igual no Linux e no Windows: o construtor enche
// uma lista de pecas (retangulo + tipo + texto + zona de clique) e cada plataforma so sabe desenhar os
// tipos (DrawPanel). O clique usa a mesma lista (PanelHit), entao desenho e clique nunca se desencontram.
// Usado pelo SOUNDPAD e pelo DISCORD.
#include "app_core.h"

enum PanelKind : int { PK_DIM, PK_BOX, PK_TEXT, PK_PILL, PK_ROW, PK_CLIP, PK_UNCLIP, PK_BAR, PK_TILE };
enum PanelColor : int { PCL_WHITE, PCL_GRAY, PCL_ACCENT, PCL_TITLE, PCL_DANGER, PCL_OK };
struct PItem {
    int kind = PK_TEXT; RECT r{ 0, 0, 0, 0 };
    std::wstring text, sub;          // PK_TILE: text = nome, sub = linha de baixo
    bool on = false, bold = false, center = false;
    float px = 11, v = -1;           // v: PK_BAR/PK_TILE = progresso 0..1 (negativo = sem barra)
    int color = PCL_WHITE, zone = 0;
};
struct Panel {
    bool open = false;
    RECT box{ 0, 0, 0, 0 }, list{ 0, 0, 0, 0 };
    int scroll = 0, contentH = 0;
    std::vector<PItem> items;
    PItem& Add(int kind, RECT r) { items.emplace_back(); PItem& p = items.back(); p.kind = kind; p.r = r; return p; }
    void Pill(RECT r, const std::wstring& t, bool on, int zone, float px = 10) { PItem& p = Add(PK_PILL, r); p.text = t; p.on = on; p.zone = zone; p.px = px; }
    void Text(RECT r, const std::wstring& t, float px, int color = PCL_WHITE, bool bold = false, bool center = false, int zone = 0) {
        PItem& p = Add(PK_TEXT, r); p.text = t; p.px = px; p.color = color; p.bold = bold; p.center = center; p.zone = zone;
    }
    void Row(RECT r, bool on = false) { Add(PK_ROW, r).on = on; }
    void Bar(RECT r, float v) { Add(PK_BAR, r).v = v; }
    void Clip(RECT r) { Add(PK_CLIP, r); }
    void Unclip() { Add(PK_UNCLIP, RECT{ 0, 0, 0, 0 }); }
    // Caixa padrao centralizada com titulo e X; devolve a area util (abaixo do titulo).
    RECT Frame(int w, int h, int maxW, int maxH, const std::wstring& title, int closeZone) {
        items.clear();
        int bw = std::min(maxW, w - 24), bh = std::min(maxH, h - 24), bx = (w - bw) / 2, by = (h - bh) / 2;
        box = { bx, by, bx + bw, by + bh };
        Add(PK_DIM, RECT{ 0, 0, w, h });
        Add(PK_BOX, box);
        Text({ bx + SI(18), by + SI(12), bx + bw - SI(60), by + SI(40) }, title, S(14), PCL_TITLE, true);
        PItem& x = Add(PK_TEXT, RECT{ bx + bw - SI(46), by + SI(8), bx + bw - SI(10), by + SI(44) }); x.text = L"✕"; x.px = S(14); x.center = true; x.zone = closeZone;
        return { bx + SI(18), by + SI(44), bx + bw - SI(18), by + bh - SI(12) };
    }
    void ClampScroll() { int vis = list.bottom - list.top; scroll = std::max(0, std::min(scroll, std::max(0, contentH - vis))); }
};
// Zona clicada (0 = nenhuma). A ultima peca que cobre o ponto ganha (botao desenhado por cima de um bloco);
// pecas dentro de um recorte so valem se o ponto tambem estiver no recorte (lista rolada).
inline int PanelHit(const Panel& p, int x, int y) {
    RECT clip{ 0, 0, 0, 0 }; bool clipped = false; int hit = 0;
    for (auto& it : p.items) {
        if (it.kind == PK_CLIP) { clip = it.r; clipped = true; continue; }
        if (it.kind == PK_UNCLIP) { clipped = false; continue; }
        if (it.zone <= 0 || !PtIn(it.r, x, y)) continue;
        if (clipped && !PtIn(clip, x, y)) continue;
        hit = it.zone;
    }
    return hit;
}
