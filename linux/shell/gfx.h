#pragma once
// Desenho 2D em cima do raylib com a "cara" das chamadas GDI+ que o main.cpp
// do Windows usa (RectF, FillEllipse, DrawRoundRect, DrawArc...), texto com
// fallback por glifo (DejaVu Sans -> Droid Sans Japanese -> fontes do sistema)
// e clip em pilha. Tudo roda na thread principal (contexto OpenGL).
//
// Inclua em UM unico .cpp (traz a implementacao static do stb_truetype para
// consultar quais glifos cada fonte tem).
#include "raylib.h"
#include "rlgl.h"
#include "platform.h"
#include "config.h"
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

namespace gfx {

struct RectF {
    float X = 0, Y = 0, Width = 0, Height = 0;
    RectF() = default;
    RectF(float x, float y, float w, float h) : X(x), Y(y), Width(w), Height(h) {}
    float Right() const { return X + Width; }
    float Bottom() const { return Y + Height; }
};
inline RectF FromRECT(const RECT& r) { return RectF((float)r.left, (float)r.top, (float)(r.right - r.left), (float)(r.bottom - r.top)); }
inline Rectangle ToRay(const RectF& r) { return Rectangle{ r.X, r.Y, r.Width, r.Height }; }
inline unsigned char ClampB(int v) { return (unsigned char)std::max(0, std::min(255, v)); }
inline Color Col(COLORREF c, int a = 255) { return Color{ GetRValue(c), GetGValue(c), GetBValue(c), ClampB(a) }; }
// Mesma ordem do GDI+ Color(a, r, g, b) para a traducao ficar 1:1.
inline Color ARGB(int a, int r, int g, int b) { return Color{ ClampB(r), ClampB(g), ClampB(b), ClampB(a) }; }
inline Color WithA(Color c, int a) { c.a = ClampB(a); return c; }

static const float PI_F = 3.14159265f;

// --------------------------------------------------------------- primitivas
// Triangulo com orientacao corrigida (raylib descarta faces no sentido
// horario; aqui aceitamos qualquer ordem).
inline void Tri(Vector2 a, Vector2 b, Vector2 c, Color col) {
    float cross = (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x);
    if (cross > 0) std::swap(b, c);
    rlBegin(RL_TRIANGLES);
    rlColor4ub(col.r, col.g, col.b, col.a);
    rlVertex2f(a.x, a.y); rlVertex2f(b.x, b.y); rlVertex2f(c.x, c.y);
    rlEnd();
}
inline void FillRect(float x, float y, float w, float h, Color c) { if (w <= 0 || h <= 0) return; DrawRectangleRec(Rectangle{ x, y, w, h }, c); }
inline void FillRect(const RectF& r, Color c) { FillRect(r.X, r.Y, r.Width, r.Height, c); }
inline void StrokeRect(float x, float y, float w, float h, float thick, Color c) { if (w <= 0 || h <= 0) return; DrawRectangleLinesEx(Rectangle{ x, y, w, h }, thick, c); }
inline void Line(float x1, float y1, float x2, float y2, float thick, Color c) { DrawLineEx(Vector2{ x1, y1 }, Vector2{ x2, y2 }, thick, c); }

inline int SegsFor(float radius, float sweepDeg = 360.f) {
    int s = (int)(std::fabs(sweepDeg) / 360.f * std::max(24.f, std::min(160.f, radius * 0.9f)));
    return std::max(6, s);
}
// Elipse preenchida (leque de triangulos, segmentos proporcionais ao raio).
inline void FillEllipse(float x, float y, float w, float h, Color c) {
    if (w <= 0 || h <= 0) return;
    float rx = w * 0.5f, ry = h * 0.5f, cx = x + rx, cy = y + ry;
    int n = SegsFor(std::max(rx, ry));
    float step = 2.f * PI_F / n;
    Vector2 prev{ cx + rx, cy };
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    for (int i = 1; i <= n; ++i) {
        float a = step * i;
        Vector2 cur{ cx + cosf(a) * rx, cy + sinf(a) * ry };
        // centro -> cur -> prev = anti-horario na tela (y para baixo)
        rlVertex2f(cx, cy); rlVertex2f(cur.x, cur.y); rlVertex2f(prev.x, prev.y);
        prev = cur;
    }
    rlEnd();
}
inline void FillEllipse(const RectF& r, Color c) { FillEllipse(r.X, r.Y, r.Width, r.Height, c); }
inline void FillCircle(float cx, float cy, float r, Color c) { FillEllipse(cx - r, cy - r, r * 2, r * 2, c); }

// Anel eliptico (arco com espessura). Angulos em graus, sentido horario na
// tela a partir do eixo +X, igual ao GDI+ DrawArc/DrawEllipse.
inline void RingEllipse(float cx, float cy, float rx, float ry, float thick, float startDeg, float sweepDeg, Color c) {
    if (rx <= 0 || ry <= 0 || thick <= 0 || std::fabs(sweepDeg) < 0.01f) return;
    if (sweepDeg < 0) { startDeg += sweepDeg; sweepDeg = -sweepDeg; }
    if (sweepDeg > 360.f) sweepDeg = 360.f;
    int n = std::max(3, SegsFor(std::max(rx, ry), sweepDeg));
    float a0 = startDeg * PI_F / 180.f, da = (sweepDeg * PI_F / 180.f) / n;
    float ht = thick * 0.5f;
    float irx = std::max(0.f, rx - ht), iry = std::max(0.f, ry - ht), orx = rx + ht, ory = ry + ht;
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    for (int i = 0; i < n; ++i) {
        float a = a0 + da * i, b = a + da;
        Vector2 i0{ cx + cosf(a) * irx, cy + sinf(a) * iry }, o0{ cx + cosf(a) * orx, cy + sinf(a) * ory };
        Vector2 i1{ cx + cosf(b) * irx, cy + sinf(b) * iry }, o1{ cx + cosf(b) * orx, cy + sinf(b) * ory };
        // dois triangulos por segmento, ambos anti-horarios na tela
        rlVertex2f(o0.x, o0.y); rlVertex2f(i0.x, i0.y); rlVertex2f(i1.x, i1.y);
        rlVertex2f(o0.x, o0.y); rlVertex2f(i1.x, i1.y); rlVertex2f(o1.x, o1.y);
    }
    rlEnd();
}
inline void StrokeEllipse(float x, float y, float w, float h, float thick, Color c) { RingEllipse(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, thick, 0, 360, c); }
inline void StrokeEllipse(const RectF& r, float thick, Color c) { StrokeEllipse(r.X, r.Y, r.Width, r.Height, thick, c); }
inline void Arc(float x, float y, float w, float h, float startDeg, float sweepDeg, float thick, Color c) { RingEllipse(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, thick, startDeg, sweepDeg, c); }

// Retangulo arredondado: tracado com segmentos explicitos (o do raylib usa
// roundness relativa e desenha a borda para fora; aqui a borda e centrada,
// igual ao GDI+).
inline void RoundRectPath(const RectF& r, float rad, std::vector<Vector2>& pts) {
    pts.clear();
    rad = std::max(0.f, std::min(rad, std::min(r.Width, r.Height) * 0.5f));
    int n = std::max(3, (int)(rad * 0.5f)); if (n > 24) n = 24;
    auto arc = [&](float cx, float cy, float a0) {
        for (int i = 0; i <= n; ++i) { float a = a0 + (PI_F * 0.5f) * i / n; pts.push_back(Vector2{ cx + cosf(a) * rad, cy + sinf(a) * rad }); }
    };
    if (rad <= 0.5f) {
        pts = { {r.X, r.Y}, {r.Right(), r.Y}, {r.Right(), r.Bottom()}, {r.X, r.Bottom()} };
        return;
    }
    arc(r.X + rad, r.Y + rad, PI_F);                 // canto sup. esq.
    arc(r.Right() - rad, r.Y + rad, PI_F * 1.5f);    // sup. dir.
    arc(r.Right() - rad, r.Bottom() - rad, 0.f);     // inf. dir.
    arc(r.X + rad, r.Bottom() - rad, PI_F * 0.5f);   // inf. esq.
}
inline void FillPolygonConvex(const std::vector<Vector2>& pts, Color c) {
    if (pts.size() < 3) return;
    // leque a partir do centroide (poligonos convexos: cantos arredondados)
    float cx = 0, cy = 0; for (auto& p : pts) { cx += p.x; cy += p.y; } cx /= pts.size(); cy /= pts.size();
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    for (size_t i = 0; i < pts.size(); ++i) {
        const Vector2& a = pts[i]; const Vector2& b = pts[(i + 1) % pts.size()];
        float cross = (a.x - cx) * (b.y - cy) - (a.y - cy) * (b.x - cx);
        if (cross > 0) { rlVertex2f(cx, cy); rlVertex2f(b.x, b.y); rlVertex2f(a.x, a.y); }
        else { rlVertex2f(cx, cy); rlVertex2f(a.x, a.y); rlVertex2f(b.x, b.y); }
    }
    rlEnd();
}
inline void StrokePolyline(const std::vector<Vector2>& pts, float thick, Color c, bool closed) {
    if (pts.size() < 2) return;
    size_t n = pts.size();
    // faixa de quads ao longo do caminho (juntas simples)
    float ht = thick * 0.5f;
    std::vector<Vector2> L(n), R(n);
    for (size_t i = 0; i < n; ++i) {
        Vector2 p = pts[i];
        Vector2 prev = pts[(i + n - 1) % n], next = pts[(i + 1) % n];
        if (!closed) { if (i == 0) prev = p; if (i == n - 1) next = p; }
        float dx = next.x - prev.x, dy = next.y - prev.y; float len = sqrtf(dx * dx + dy * dy); if (len < 1e-4f) { dx = 1; dy = 0; len = 1; }
        float nx = -dy / len, ny = dx / len;
        L[i] = Vector2{ p.x + nx * ht, p.y + ny * ht }; R[i] = Vector2{ p.x - nx * ht, p.y - ny * ht };
    }
    rlBegin(RL_TRIANGLES);
    rlColor4ub(c.r, c.g, c.b, c.a);
    size_t segs = closed ? n : n - 1;
    for (size_t i = 0; i < segs; ++i) {
        size_t j = (i + 1) % n;
        Vector2 a = L[i], b = R[i], d = R[j], e = L[j];
        float cr = (b.x - a.x) * (d.y - a.y) - (b.y - a.y) * (d.x - a.x);
        if (cr > 0) { rlVertex2f(a.x, a.y); rlVertex2f(d.x, d.y); rlVertex2f(b.x, b.y); rlVertex2f(a.x, a.y); rlVertex2f(e.x, e.y); rlVertex2f(d.x, d.y); }
        else { rlVertex2f(a.x, a.y); rlVertex2f(b.x, b.y); rlVertex2f(d.x, d.y); rlVertex2f(a.x, a.y); rlVertex2f(d.x, d.y); rlVertex2f(e.x, e.y); }
    }
    rlEnd();
}
inline void FillRoundRect(const RectF& r, float radius, Color c) {
    if (r.Width <= 0 || r.Height <= 0) return;
    if (radius <= 0.5f) { FillRect(r, c); return; }
    static std::vector<Vector2> pts; RoundRectPath(r, radius, pts); FillPolygonConvex(pts, c);
}
inline void StrokeRoundRect(const RectF& r, float radius, float thick, Color c) {
    if (r.Width <= 0 || r.Height <= 0) return;
    static std::vector<Vector2> pts; RoundRectPath(r, radius, pts); StrokePolyline(pts, thick, c, true);
}
// Equivalente a DrawRoundRect(g, rect, radius, fill, pen) do main.cpp.
inline void RoundRect(const RectF& r, float radius, const Color* fill, const Color* pen, float penW = 1.f) {
    if (fill) FillRoundRect(r, radius, *fill);
    if (pen) StrokeRoundRect(r, radius, penW, *pen);
}
inline void FillPolygon(const Vector2* p, int n, Color c) {
    if (n < 3) return;
    for (int i = 1; i + 1 < n; ++i) Tri(p[0], p[i], p[i + 1], c);
}

// ------------------------------------------------------------------ clip
inline std::vector<Rectangle>& ClipStack() { static std::vector<Rectangle> s; return s; }
inline void ApplyScissor(const Rectangle& r) {
    int x = (int)floorf(r.x), y = (int)floorf(r.y), w = (int)ceilf(r.width), h = (int)ceilf(r.height);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    BeginScissorMode(x, y, w, h);
}
inline void PushClip(const RectF& rc) {
    Rectangle r{ rc.X, rc.Y, std::max(0.f, rc.Width), std::max(0.f, rc.Height) };
    if (!ClipStack().empty()) {
        const Rectangle& p = ClipStack().back();
        float x1 = std::max(r.x, p.x), y1 = std::max(r.y, p.y);
        float x2 = std::min(r.x + r.width, p.x + p.width), y2 = std::min(r.y + r.height, p.y + p.height);
        r = Rectangle{ x1, y1, std::max(0.f, x2 - x1), std::max(0.f, y2 - y1) };
    }
    ClipStack().push_back(r);
    ApplyScissor(r);
}
inline void PopClip() {
    if (ClipStack().empty()) return;
    ClipStack().pop_back();
    if (ClipStack().empty()) EndScissorMode(); else ApplyScissor(ClipStack().back());
}

// ---------------------------------------------------------------- imagens
struct Img {
    Texture2D tex{};    // original
    Texture2D circ{};   // com mascara circular (modo CD)
    Texture2D tiny{};   // 40x40 embaçada (fundo)
    Image cpu{};        // copia em RAM (RGBA) para derivar as outras
    int w = 0, h = 0;
    bool ok = false;
};
inline Img* ImgFromImage(Image im) {
    Img* g = new Img();
    if (!im.data || im.width <= 0 || im.height <= 0) { if (im.data) UnloadImage(im); return g; }
    if (im.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) ImageFormat(&im, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    g->cpu = im; g->w = im.width; g->h = im.height;
    g->tex = LoadTextureFromImage(im);
    if (g->tex.id) { GenTextureMipmaps(&g->tex); SetTextureFilter(g->tex, TEXTURE_FILTER_TRILINEAR); g->ok = true; }
    return g;
}
inline Img* LoadImg(const std::wstring& path) {
    if (path.empty()) return new Img();
    Image im = LoadImage(WideToUtf8(path).c_str());
    return ImgFromImage(im);
}
inline void FreeImg(Img* g) {
    if (!g) return;
    if (g->tex.id) UnloadTexture(g->tex);
    if (g->circ.id) UnloadTexture(g->circ);
    if (g->tiny.id) UnloadTexture(g->tiny);
    if (g->cpu.data) UnloadImage(g->cpu);
    delete g;
}
inline void DrawImg(Img* g, const RectF& dst, Color tint = WHITE) {
    if (!g || !g->ok) return;
    DrawTexturePro(g->tex, Rectangle{ 0, 0, (float)g->w, (float)g->h }, ToRay(dst), Vector2{ 0, 0 }, 0, tint);
}
// src em pixels da imagem, dst na tela (fatias do efeito glitch).
inline void DrawImgPart(Img* g, const RectF& src, const RectF& dst, Color tint = WHITE) {
    if (!g || !g->ok) return;
    DrawTexturePro(g->tex, ToRay(src), ToRay(dst), Vector2{ 0, 0 }, 0, tint);
}
// Capa dentro de um quadro: corta o excesso no centro em vez de achatar a imagem
// (capa 16:9 do YouTube num quadrado ficava "amassada").
inline void DrawImgCover(Img* g, const RectF& dst, Color tint = WHITE) {
    if (!g || !g->ok || dst.Width <= 0 || dst.Height <= 0) return;
    float iw = (float)g->w, ih = (float)g->h;
    if (iw <= 0 || ih <= 0) return;
    float sa = iw / ih, da = dst.Width / dst.Height;
    RectF src(0, 0, iw, ih);
    if (sa > da) { float nw = ih * da; src = RectF((iw - nw) * 0.5f, 0, nw, ih); }
    else if (sa < da) { float nh = iw / da; src = RectF(0, (ih - nh) * 0.5f, iw, nh); }
    DrawTexturePro(g->tex, ToRay(src), ToRay(dst), Vector2{ 0, 0 }, 0, tint);
}
inline Texture2D& CircleTex(Img* g) {
    if (!g->circ.id && g->cpu.data) {
        Image m = ImageCopy(g->cpu);
        if (m.width != m.height) {   // quadrado primeiro: senao o disco sai oval e a capa achatada
            int lado = std::min(m.width, m.height);
            ImageCrop(&m, Rectangle{ (float)((m.width - lado) / 2), (float)((m.height - lado) / 2), (float)lado, (float)lado });
        }
        Color* px = (Color*)m.data;
        int W = m.width, H = m.height;
        float cx = W * 0.5f, cy = H * 0.5f, rx = W * 0.5f, ry = H * 0.5f, mn = std::min(rx, ry);
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            float d = sqrtf(dx * dx + dy * dy);
            float edge = (1.f - d) * mn + 0.5f;           // distancia ate a borda em pixels
            float a = std::max(0.f, std::min(1.f, edge)); // anti-alias de 1 px
            Color& p = px[y * W + x];
            p.a = (unsigned char)(p.a * a);
        }
        g->circ = LoadTextureFromImage(m);
        if (g->circ.id) { GenTextureMipmaps(&g->circ); SetTextureFilter(g->circ, TEXTURE_FILTER_TRILINEAR); }
        UnloadImage(m);
    }
    return g->circ;
}
inline void DrawImgCircle(Img* g, const RectF& dst, float rotDeg, Color tint = WHITE) {
    if (!g || !g->ok) return;
    Texture2D& t = CircleTex(g);
    if (!t.id) return;
    Rectangle d{ dst.X + dst.Width * 0.5f, dst.Y + dst.Height * 0.5f, dst.Width, dst.Height };
    DrawTexturePro(t, Rectangle{ 0, 0, (float)t.width, (float)t.height }, d, Vector2{ dst.Width * 0.5f, dst.Height * 0.5f }, rotDeg, tint);
}
inline Texture2D& TinyTex(Img* g) {
    if (!g->tiny.id && g->cpu.data) {
        Image t = ImageCopy(g->cpu);
        ImageResize(&t, 40, 40);
        ImageBlurGaussian(&t, 2);
        g->tiny = LoadTextureFromImage(t);
        if (g->tiny.id) SetTextureFilter(g->tiny, TEXTURE_FILTER_BILINEAR);
        UnloadImage(t);
    }
    return g->tiny;
}

// ------------------------------------------------------------------ texto
struct Atlas {
    Font font{};
    bool loaded = false;
    std::vector<int> cps;
    std::unordered_set<int> set;
    float ascentPx = 0;
};
struct Face {
    std::vector<unsigned char> data;
    stbtt_fontinfo info{};
    bool ok = false;
    int ascent = 0, descent = 0, lineGap = 0;
    float spanEm = 1.f;   // (ascent-descent)/unitsPerEm
    float emScale = 1.f;  // fator p/ o glifo ter o mesmo tamanho de em que a fonte primaria
    std::map<int, Atlas> atlases; // por tamanho em px (px pedido, nao o efetivo)
    std::string name;
};
inline std::vector<Face>& Faces() { static std::vector<Face> f; return f; }      // 0=regular 1=bold 2..=fallbacks
inline std::unordered_map<int, int>& CpFaceCache(bool bold) { static std::unordered_map<int, int> c[2]; return c[bold ? 1 : 0]; }

inline bool LoadFace(const std::string& path, const char* name) {
    std::ifstream f(path, std::ios::binary);
    if (!f.good()) return false;
    Face face;
    face.data.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (face.data.size() < 12) return false;
    // TTC: usa a primeira fonte da colecao. Fontes CFF2 (Noto CJK VF) nao sao
    // suportadas pelo stb_truetype: rejeita para nao virar quadradinhos.
    int off = stbtt_GetFontOffsetForIndex(face.data.data(), 0);
    if (off < 0) return false;
    if (!stbtt_InitFont(&face.info, face.data.data(), off)) return false;
    // checa tabela CFF2
    {
        const unsigned char* d = face.data.data() + off;
        if (face.data.size() > (size_t)off + 12) {
            int n = (d[4] << 8) | d[5];
            for (int i = 0; i < n && (size_t)off + 12 + 16 * i + 4 <= face.data.size(); ++i)
                if (memcmp(d + 12 + 16 * i, "CFF2", 4) == 0) return false;
        }
    }
    stbtt_GetFontVMetrics(&face.info, &face.ascent, &face.descent, &face.lineGap);
    {
        float upm = 1.f / std::max(1e-6f, stbtt_ScaleForMappingEmToPixels(&face.info, 1.f));
        face.spanEm = std::max(0.5f, (float)(face.ascent - face.descent) / upm);
        // raylib escala a fonte para (ascent-descent) = px; fontes com "span" maior
        // (CJK) sairiam menores. Compensa em relacao a primaria.
        face.emScale = Faces().empty() || !Faces()[0].ok ? 1.f : face.spanEm / Faces()[0].spanEm;
    }
    face.ok = true; face.name = name;
    Faces().push_back(std::move(face));
    return true;
}
inline void SeedCodepoints(std::vector<int>& cps) {
    for (int c = 32; c < 127; ++c) cps.push_back(c);
    for (int c = 0xA0; c <= 0x17F; ++c) cps.push_back(c);      // Latin-1 + Latin Extended-A (acentos pt-BR etc.)
    const int extras[] = { 0x2013, 0x2014, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2026, 0x20AC,
                           0x2699, 0x270E, 0x21C4, 0x27F3, 0x2715, 0x25BE, 0x25B6, 0x266A, 0x266B, 0x2665, 0x2661, 0x00D7 };
    for (int c : extras) cps.push_back(c);
}
inline bool FaceHas(Face& f, int cp) { return f.ok && stbtt_FindGlyphIndex(&f.info, cp) != 0; }
inline int FaceFor(int cp, bool bold) {
    auto& cache = CpFaceCache(bold);
    auto it = cache.find(cp);
    if (it != cache.end()) return it->second;
    auto& F = Faces();
    int primary = (bold && F.size() > 1 && F[1].ok) ? 1 : 0;
    int chosen = primary;
    if (!FaceHas(F[primary], cp)) {
        int other = primary == 1 ? 0 : 1;
        if (!bold || other == 1) other = -1;        // regular nao cai pro bold; bold pode cair pro regular
        if (other >= 0 && (int)F.size() > other && FaceHas(F[other], cp)) chosen = other;
        else for (size_t i = 2; i < F.size(); ++i) if (FaceHas(F[i], cp)) { chosen = (int)i; break; }
    }
    cache[cp] = chosen;
    return chosen;
}
// Mesmo caminho do LoadFontFromMemory do raylib, mas empacotando com o
// stb_rect_pack (packMethod 1): o empacotador simples em linhas do raylib
// falha com glifos mais altos que o tamanho da fonte (acentos, CJK) e deixa
// caracteres de fora ("Failed to package character").
inline void RebuildAtlas(Face& f, Atlas& a, int px) {
    if (a.loaded) UnloadFont(a.font);
    a.font = Font{};
    a.loaded = false;
    if (a.cps.empty()) return;
    int pxEff = std::max(6, (int)lroundf(px * f.emScale));
    Font font{};
    font.baseSize = pxEff;
    font.glyphCount = (int)a.cps.size();
    font.glyphPadding = 4;
    font.glyphs = LoadFontData(f.data.data(), (int)f.data.size(), pxEff, a.cps.data(), font.glyphCount, FONT_DEFAULT);
    if (!font.glyphs) return;
    Image atlas = GenImageFontAtlas(font.glyphs, &font.recs, font.glyphCount, pxEff, font.glyphPadding, 1);
    if (!atlas.data) { UnloadFontData(font.glyphs, font.glyphCount); return; }
    font.texture = LoadTextureFromImage(atlas);
    for (int i = 0; i < font.glyphCount; i++) {
        UnloadImage(font.glyphs[i].image);
        font.glyphs[i].image = ImageFromImage(atlas, font.recs[i]);
    }
    UnloadImage(atlas);
    a.font = font;
    a.loaded = font.texture.id != 0;
    if (a.loaded) SetTextureFilter(a.font.texture, TEXTURE_FILTER_BILINEAR);
    float sc = stbtt_ScaleForPixelHeight(&f.info, (float)pxEff);
    a.ascentPx = f.ascent * sc;
}
inline Atlas& GetAtlas(int faceIdx, int px) {
    Face& f = Faces()[faceIdx];
    auto it = f.atlases.find(px);
    if (it == f.atlases.end()) {
        Atlas a; SeedCodepoints(a.cps);
        std::vector<int> keep; keep.reserve(a.cps.size());
        for (int c : a.cps) if (FaceHas(f, c)) { keep.push_back(c); a.set.insert(c); }
        if (keep.empty()) { keep.push_back(32); a.set.insert(32); } // raylib precisa de >= 1 glifo
        a.cps = keep;
        it = f.atlases.emplace(px, std::move(a)).first;
        RebuildAtlas(f, it->second, px);
    }
    return it->second;
}
inline int PxOf(float px) { int p = (int)lroundf(px); return std::max(6, std::min(200, p)); }

// Garante que todos os glifos de 's' estao nos atlas (reconstroi no maximo um
// atlas por fonte por chamada). Retorna false se a fonte primaria nao carregou.
inline bool EnsureGlyphs(const std::wstring& s, int px, bool bold) {
    if (Faces().empty() || !Faces()[0].ok) return false;
    std::map<int, std::vector<int>> missing; // faceIdx -> cps novos
    for (wchar_t wc : s) {
        int cp = (int)wc; if (cp < 32) continue;
        int fi = FaceFor(cp, bold);
        Atlas& a = GetAtlas(fi, px);
        if (!a.set.count(cp)) { a.set.insert(cp); missing[fi].push_back(cp); }
    }
    for (auto& kv : missing) {
        Atlas& a = GetAtlas(kv.first, px);
        for (int c : kv.second) a.cps.push_back(c);
        RebuildAtlas(Faces()[kv.first], a, px);
    }
    return true;
}
inline float GlyphAdvance(Atlas& a, int cp) {
    if (!a.loaded) return 0;
    int gi = GetGlyphIndex(a.font, cp);
    if (gi < 0 || gi >= a.font.glyphCount) return 0;
    float adv = (float)a.font.glyphs[gi].advanceX;
    if (adv == 0) adv = a.font.recs[gi].width + (float)a.font.glyphs[gi].offsetX;
    return adv; // atlas gerado no tamanho exato: escala 1
}
inline float TextWidth(const std::wstring& s, float pxf, bool bold = false) {
    int px = PxOf(pxf);
    if (!EnsureGlyphs(s, px, bold)) return (float)s.size() * px * 0.55f;
    float w = 0;
    for (wchar_t wc : s) { int cp = (int)wc; if (cp < 32) continue; w += GlyphAdvance(GetAtlas(FaceFor(cp, bold), px), cp); }
    return w;
}
// Altura da "linha" para centralizar verticalmente (proporcao tipica da DejaVu).
inline float LineHeight(float pxf) { return PxOf(pxf) * 1.17f; }

// Desenha com o canto superior esquerdo em (x,y), igual ao DrawString(PointF).
inline void Text(const std::wstring& s, float x, float y, float pxf, Color c, bool bold = false) {
    if (s.empty()) return;
    int px = PxOf(pxf);
    if (!EnsureGlyphs(s, px, bold)) return;
    float cx = floorf(x + 0.5f), cy = floorf(y + 0.5f);
    int primary = (bold && Faces().size() > 1 && Faces()[1].ok) ? 1 : 0;
    float baseAsc = GetAtlas(primary, px).ascentPx;
    for (wchar_t wc : s) {
        int cp = (int)wc; if (cp < 32) continue;
        int fi = FaceFor(cp, bold);
        Atlas& a = GetAtlas(fi, px);
        if (!a.loaded) continue;
        float dy = (fi == primary) ? 0.f : (baseAsc - a.ascentPx); // alinha a linha de base entre fontes
        DrawTextCodepoint(a.font, cp, Vector2{ cx, cy + dy }, (float)a.font.baseSize, c);
        cx += GlyphAdvance(a, cp);
    }
}

enum HAlign { Near = 0, Center = 1, Far = 2 };
enum Trim { None = 0, EllipsisChar = 1, EllipsisWord = 2, EllipsisPath = 3 };

// Encaixa 's' em maxW segundo o modo de corte (StringTrimming do GDI+).
inline std::wstring FitText(const std::wstring& s, float maxW, float px, bool bold, Trim trim) {
    if (trim == None || TextWidth(s, px, bold) <= maxW + 0.5f) return s;
    const std::wstring ell = L"…";
    float ellW = TextWidth(ell, px, bold);
    if (trim == EllipsisPath) {
        // mantem inicio e fim
        size_t keepL = s.size() / 2, keepR = s.size() - keepL;
        while (keepL + keepR > 0) {
            std::wstring t = s.substr(0, keepL) + ell + s.substr(s.size() - keepR);
            if (TextWidth(t, px, bold) <= maxW) return t;
            if (keepL >= keepR) --keepL; else --keepR;
        }
        return ell;
    }
    // quantos caracteres cabem antes das reticencias
    float acc = 0; size_t n = 0;
    for (; n < s.size(); ++n) {
        float w = TextWidth(std::wstring(1, s[n]), px, bold);
        if (acc + w + ellW > maxW) break;
        acc += w;
    }
    if (trim == EllipsisWord) {
        size_t sp = s.rfind(L' ', n);
        if (sp != std::wstring::npos && sp > 0 && sp > n / 2) n = sp;
    }
    while (n > 0 && s[n - 1] == L' ') --n;
    return s.substr(0, n) + ell;
}

// Texto dentro de um retangulo: alinhamento horizontal, opcional centralizacao
// vertical e modo de corte. Sem corte, o texto e recortado ao retangulo
// (comportamento padrao do DrawString com layout rect).
inline void TextRect(const std::wstring& s, const RectF& rc, float pxf, Color c, bool bold = false,
                     HAlign ha = Near, bool vcenter = false, Trim trim = None) {
    if (s.empty() || rc.Width <= 0) return;
    std::wstring t = FitText(s, rc.Width, pxf, bold, trim);
    float w = TextWidth(t, pxf, bold);
    float x = rc.X;
    if (ha == Center) x = rc.X + (rc.Width - w) * 0.5f; else if (ha == Far) x = rc.Right() - w;
    float y = rc.Y;
    if (vcenter) y = rc.Y + (rc.Height - LineHeight(pxf)) * 0.5f;
    bool clip = (trim == None && (w > rc.Width + 0.5f || rc.Height < LineHeight(pxf)));
    if (clip) PushClip(RectF(rc.X, rc.Y - 1, rc.Width, rc.Height + 2));
    Text(t, x, y, pxf, c, bold);
    if (clip) PopClip();
}

inline void UnloadFonts() {
    for (auto& f : Faces()) for (auto& kv : f.atlases) if (kv.second.loaded) UnloadFont(kv.second.font);
    Faces().clear();
    CpFaceCache(false).clear(); CpFaceCache(true).clear();
}

// Carrega fontes empacotadas + fallbacks CJK do sistema (via fontconfig, se
// houver). Chame depois do InitWindow.
inline void InitFonts(const std::wstring& assetDir) {
    UnloadFonts();
    std::string dir = WideToUtf8(Config::Join(assetDir, L"fonts"));
    if (!LoadFace(dir + "/DejaVuSans.ttf", "DejaVu Sans")) {
        // ultimo recurso: fontes do sistema
        const char* sys[] = { "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
                              "/usr/share/fonts/TTF/DejaVuSans.ttf", "/usr/share/fonts/liberation-sans-fonts/LiberationSans-Regular.ttf" };
        for (auto p : sys) if (LoadFace(p, "sistema")) break;
        if (Faces().empty()) { Face dummy; Faces().push_back(std::move(dummy)); }
    }
    if (!LoadFace(dir + "/DejaVuSans-Bold.ttf", "DejaVu Sans Bold")) {
        const char* sys[] = { "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans-Bold.ttf", "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf" };
        bool ok = false; for (auto p : sys) if (LoadFace(p, "sistema bold")) { ok = true; break; }
        if (!ok) { Face dummy; Faces().push_back(std::move(dummy)); } // indice 1 reservado
    }
    LoadFace(dir + "/DroidSansJapanese.ttf", "Droid Sans Japanese");
    // fallbacks extras via fontconfig (coreano, chines...). Fontes CFF2 sao rejeitadas no LoadFace.
    const char* langs[] = { "ja", "ko", "zh-cn", "zh-tw" };
    for (auto lg : langs) {
        std::string cmd = std::string("fc-match -f '%{file}' ':lang=") + lg + "' 2>/dev/null";
        FILE* p = popen(cmd.c_str(), "r");
        if (!p) continue;
        char buf[1024] = { 0 }; size_t n = fread(buf, 1, sizeof(buf) - 1, p); pclose(p);
        std::string file(buf, n);
        while (!file.empty() && (file.back() == '\n' || file.back() == '\r')) file.pop_back();
        if (file.empty()) continue;
        bool dup = false; for (auto& f : Faces()) if (f.name == file) dup = true;
        if (!dup) LoadFace(file, file.c_str());
    }
}

} // namespace gfx
