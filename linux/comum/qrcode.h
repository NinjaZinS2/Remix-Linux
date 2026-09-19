#pragma once
// Gerador de QR Code (ISO/IEC 18004) sem dependencias: so STL, header-only, C++17.
// Usado para desenhar na tela do PC o QR com a URL de pareamento do Remix Host.
//
// - Modo byte (os bytes da string vao crus, entao UTF-8 funciona como esta).
// - Versoes 1 a 40 nas tabelas; Encode escolhe a menor versao em
//   [minVersion, maxVersion] que cabe no nivel de correcao pedido.
// - Reed-Solomon em GF(256) com polinomio 0x11D, blocos intercalados.
// - Mascara: a de menor penalidade (N1-N4 do padrao) ou a forcada.
//
// Credito: a logica segue a "QR Code generator library" do Project Nayuki
// (https://www.nayuki.io/page/qr-code-generator-library, licenca MIT),
// reescrita aqui de forma enxuta so com o que o Remix precisa. Por ser obra
// derivada, a licenca MIT pede que o aviso abaixo acompanhe o codigo:
//
//   Copyright (c) Project Nayuki. (MIT License)
//   https://www.nayuki.io/page/qr-code-generator-library
//
//   Permission is hereby granted, free of charge, to any person obtaining a copy of
//   this software and associated documentation files (the "Software"), to deal in
//   the Software without restriction, including without limitation the rights to
//   use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
//   the Software, and to permit persons to whom the Software is furnished to do so,
//   subject to the following conditions:
//   - The above copyright notice and this permission notice shall be included in
//     all copies or substantial portions of the Software.
//   - The Software is provided "as is", without warranty of any kind, express or
//     implied, including but not limited to the warranties of merchantability,
//     fitness for a particular purpose and noninfringement. In no event shall the
//     authors or copyright holders be liable for any claim, damages or other
//     liability, whether in an action of contract, tort or otherwise, arising from,
//     out of or in connection with the Software or the use or other dealings in the
//     Software.
//
// Uso:
//   qr::Code c;
//   if (qr::Encode("http://192.168.15.6:49875/#q=...", c)) {
//       // c.size x c.size modulos, SEM zona de silencio: ao desenhar deixe
//       // 4 modulos claros em volta.
//       for (int y = 0; y < c.size; y++)
//           for (int x = 0; x < c.size; x++)
//               if (c.get(x, y)) PintaModulo(x, y);
//   }
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace qr {

struct Code {
    int size = 0;                        // lado em modulos (17 + 4*versao); 0 = vazio
    std::vector<unsigned char> modules;  // modules[y*size+x] = 1 escuro, 0 claro; sem zona de silencio
    bool get(int x, int y) const {       // fora da matriz = claro
        if (x < 0 || y < 0 || x >= size || y >= size) return false;
        return modules[static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)] != 0;
    }
};

namespace detail {

// ---------------------------------------------------------------- tabelas ---
// Indice [ecl][versao], ecl 0=L 1=M 2=Q 3=H; a coluna 0 nao e usada.
// Codewords de correcao por bloco.
inline constexpr unsigned char kEccPorBloco[4][41] = {
    {0, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {0, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {0, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {0, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};
// Numero de blocos de correcao.
inline constexpr unsigned char kNumBlocos[4][41] = {
    {0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {0, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {0, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {0, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};
// Bits do nivel de correcao na informacao de formato (L=01 M=00 Q=11 H=10).
inline constexpr int kEclFormato[4] = {1, 0, 3, 2};

inline constexpr int kPenalN1 = 3;
inline constexpr int kPenalN2 = 3;
inline constexpr int kPenalN3 = 40;
inline constexpr int kPenalN4 = 10;

// Modulos disponiveis para dados + correcao (tudo menos os padroes de funcao,
// formato e versao), incluindo os bits de sobra.
inline int ModulosBrutos(int ver) {
    int r = (16 * ver + 128) * ver + 64;
    if (ver >= 2) {
        const int na = ver / 7 + 2;
        r -= (25 * na - 10) * na - 55;
        if (ver >= 7) r -= 36;
    }
    return r;
}

// Codewords de dados (sem correcao) para a versao/ecl.
inline int CodewordsDados(int ver, int ecl) {
    return ModulosBrutos(ver) / 8 - kEccPorBloco[ecl][ver] * kNumBlocos[ecl][ver];
}

// Tamanho do contador de caracteres no modo byte.
inline int BitsContadorByte(int ver) { return ver < 10 ? 8 : 16; }

// ---------------------------------------------------------- Reed-Solomon ---
inline std::uint8_t GfMul(std::uint8_t x, std::uint8_t y) {
    unsigned z = 0;
    for (int i = 7; i >= 0; i--) {
        z = (z << 1) ^ ((z >> 7) * 0x11Du);
        z ^= ((static_cast<unsigned>(y) >> i) & 1u) * x;
    }
    return static_cast<std::uint8_t>(z);
}

// Gerador de grau `grau` (coeficiente lider 1 omitido), do maior para o menor.
inline std::vector<std::uint8_t> RsDivisor(int grau) {
    std::vector<std::uint8_t> r(static_cast<size_t>(grau), 0);
    r.back() = 1;
    std::uint8_t raiz = 1;
    for (int i = 0; i < grau; i++) {
        for (size_t j = 0; j < r.size(); j++) {
            r[j] = GfMul(r[j], raiz);
            if (j + 1 < r.size()) r[j] = static_cast<std::uint8_t>(r[j] ^ r[j + 1]);
        }
        raiz = GfMul(raiz, 0x02);
    }
    return r;
}

// Resto da divisao polinomial dos dados pelo gerador = codewords de correcao.
inline std::vector<std::uint8_t> RsResto(const std::uint8_t* dados, size_t n, const std::vector<std::uint8_t>& div) {
    std::vector<std::uint8_t> r(div.size(), 0);
    for (size_t k = 0; k < n; k++) {
        const std::uint8_t f = static_cast<std::uint8_t>(dados[k] ^ r[0]);
        r.erase(r.begin());
        r.push_back(0);
        for (size_t i = 0; i < r.size(); i++) r[i] = static_cast<std::uint8_t>(r[i] ^ GfMul(div[i], f));
    }
    return r;
}

// Centros dos padroes de alinhamento (mesma lista para x e y).
inline std::vector<int> PosAlinhamento(int ver) {
    if (ver == 1) return {};
    const int na = ver / 7 + 2;
    const int passo = (ver * 8 + na * 3 + 5) / (na * 4 - 4) * 2;
    std::vector<int> r(static_cast<size_t>(na));
    r[0] = 6;
    int pos = ver * 4 + 10;  // size - 7
    for (int i = na - 1; i >= 1; i--, pos -= passo) r[static_cast<size_t>(i)] = pos;
    return r;
}

// ---------------------------------------------------------------- matriz ---
class Matriz {
public:
    explicit Matriz(int ver)
        : ver_(ver), n_(ver * 4 + 17),
          mod_(static_cast<size_t>(n_) * static_cast<size_t>(n_), 0),
          fun_(static_cast<size_t>(n_) * static_cast<size_t>(n_), 0) {}

    int Lado() const { return n_; }
    bool Get(int x, int y) const { return mod_[Idx(x, y)] != 0; }
    std::vector<unsigned char>& Modulos() { return mod_; }

    void DesenhaFuncoes() {
        // Timing
        for (int i = 0; i < n_; i++) {
            SetFun(6, i, i % 2 == 0);
            SetFun(i, 6, i % 2 == 0);
        }
        // Finders (com separador) nos tres cantos
        Finder(3, 3);
        Finder(n_ - 4, 3);
        Finder(3, n_ - 4);
        // Alinhamento, menos onde colide com os finders
        const std::vector<int> pos = PosAlinhamento(ver_);
        const size_t na = pos.size();
        for (size_t i = 0; i < na; i++) {
            for (size_t j = 0; j < na; j++) {
                const bool canto = (i == 0 && j == 0) || (i == 0 && j == na - 1) || (i == na - 1 && j == 0);
                if (!canto) Alinhamento(pos[i], pos[j]);
            }
        }
        // Reserva as areas de formato (valor provisorio) e versao
        Formato(0, 0);
        Versao();
    }

    // Informacao de formato: 5 bits (ecl + mascara) + BCH(15,5), xor 0x5412.
    void Formato(int ecl, int mascara) {
        const int dados = (kEclFormato[ecl] << 3) | mascara;
        int rem = dados;
        for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
        const int bits = ((dados << 10) | rem) ^ 0x5412;
        // Primeira copia, em volta do finder de cima-esquerda
        for (int i = 0; i <= 5; i++) SetFun(8, i, Bit(bits, i));
        SetFun(8, 7, Bit(bits, 6));
        SetFun(8, 8, Bit(bits, 7));
        SetFun(7, 8, Bit(bits, 8));
        for (int i = 9; i < 15; i++) SetFun(14 - i, 8, Bit(bits, i));
        // Segunda copia, dividida entre os outros dois finders
        for (int i = 0; i < 8; i++) SetFun(n_ - 1 - i, 8, Bit(bits, i));
        for (int i = 8; i < 15; i++) SetFun(8, n_ - 15 + i, Bit(bits, i));
        SetFun(8, n_ - 8, true);  // modulo escuro
    }

    // Informacao de versao (versao >= 7): 6 bits + BCH(18,6), duas copias.
    void Versao() {
        if (ver_ < 7) return;
        int rem = ver_;
        for (int i = 0; i < 12; i++) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
        const long bits = (static_cast<long>(ver_) << 12) | rem;
        for (int i = 0; i < 18; i++) {
            const bool b = ((bits >> i) & 1) != 0;
            const int a = n_ - 11 + i % 3;
            const int c = i / 3;
            SetFun(a, c, b);
            SetFun(c, a, b);
        }
    }

    // Coloca os codewords em zigue-zague, de baixo para cima, colunas duplas
    // da direita para a esquerda, pulando a coluna 6 (timing).
    void DesenhaCodewords(const std::vector<std::uint8_t>& dados) {
        const size_t totalBits = dados.size() * 8;
        size_t i = 0;
        for (int dir = n_ - 1; dir >= 1; dir -= 2) {
            if (dir == 6) dir = 5;
            const bool subindo = ((dir + 1) & 2) == 0;
            for (int v = 0; v < n_; v++) {
                const int y = subindo ? n_ - 1 - v : v;
                for (int j = 0; j < 2; j++) {
                    const int x = dir - j;
                    const size_t k = Idx(x, y);
                    if (!fun_[k] && i < totalBits) {
                        mod_[k] = static_cast<unsigned char>((dados[i >> 3] >> (7 - static_cast<int>(i & 7))) & 1);
                        i++;
                    }
                    // Bits de sobra ficam claros (0), como manda o padrao
                }
            }
        }
    }

    // XOR da mascara nos modulos que nao sao de funcao (chamar 2x desfaz).
    void AplicaMascara(int m) {
        for (int y = 0; y < n_; y++) {
            for (int x = 0; x < n_; x++) {
                bool inv = false;
                switch (m) {
                    case 0: inv = (x + y) % 2 == 0; break;
                    case 1: inv = y % 2 == 0; break;
                    case 2: inv = x % 3 == 0; break;
                    case 3: inv = (x + y) % 3 == 0; break;
                    case 4: inv = (x / 3 + y / 2) % 2 == 0; break;
                    case 5: inv = x * y % 2 + x * y % 3 == 0; break;
                    case 6: inv = (x * y % 2 + x * y % 3) % 2 == 0; break;
                    default: inv = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
                }
                const size_t k = Idx(x, y);
                if (inv && !fun_[k]) mod_[k] = static_cast<unsigned char>(mod_[k] ^ 1);
            }
        }
    }

    // Penalidade da ISO 18004 (N1 corridas, N2 blocos 2x2, N3 padrao
    // parecido com finder, N4 equilibrio claro/escuro).
    long Penalidade() const {
        long r = 0;
        // N1 e N3 em linhas (passo 0) e colunas (passo 1)
        for (int passo = 0; passo < 2; passo++) {
            for (int a = 0; a < n_; a++) {
                bool corCorrida = false;
                int lenCorrida = 0;
                std::array<int, 7> hist{};
                for (int b = 0; b < n_; b++) {
                    const bool cor = passo == 0 ? Get(b, a) : Get(a, b);
                    if (cor == corCorrida) {
                        lenCorrida++;
                        if (lenCorrida == 5) r += kPenalN1;
                        else if (lenCorrida > 5) r++;
                    } else {
                        HistAdd(lenCorrida, hist);
                        if (!corCorrida) r += ContaFinders(hist) * kPenalN3;
                        corCorrida = cor;
                        lenCorrida = 1;
                    }
                }
                r += HistFecha(corCorrida, lenCorrida, hist) * kPenalN3;
            }
        }
        // N2
        for (int y = 0; y < n_ - 1; y++) {
            for (int x = 0; x < n_ - 1; x++) {
                const bool c = Get(x, y);
                if (c == Get(x + 1, y) && c == Get(x, y + 1) && c == Get(x + 1, y + 1)) r += kPenalN2;
            }
        }
        // N4
        long escuros = 0;
        for (unsigned char m : mod_) escuros += m ? 1 : 0;
        const long total = static_cast<long>(n_) * n_;
        const long k = (std::labs(escuros * 20 - total * 10) + total - 1) / total - 1;
        r += k * kPenalN4;
        return r;
    }

private:
    size_t Idx(int x, int y) const {
        return static_cast<size_t>(y) * static_cast<size_t>(n_) + static_cast<size_t>(x);
    }
    static bool Bit(long v, int i) { return ((v >> i) & 1) != 0; }

    void SetFun(int x, int y, bool escuro) {
        const size_t k = Idx(x, y);
        mod_[k] = escuro ? 1 : 0;
        fun_[k] = 1;
    }

    void Finder(int cx, int cy) {
        for (int dy = -4; dy <= 4; dy++) {
            for (int dx = -4; dx <= 4; dx++) {
                const int d = std::max(std::abs(dx), std::abs(dy));
                const int x = cx + dx, y = cy + dy;
                if (x >= 0 && x < n_ && y >= 0 && y < n_) SetFun(x, y, d != 2 && d != 4);
            }
        }
    }

    void Alinhamento(int cx, int cy) {
        for (int dy = -2; dy <= 2; dy++)
            for (int dx = -2; dx <= 2; dx++)
                SetFun(cx + dx, cy + dy, std::max(std::abs(dx), std::abs(dy)) != 1);
    }

    // Historico das ultimas 7 corridas (a borda clara conta como corrida longa).
    void HistAdd(int len, std::array<int, 7>& hist) const {
        if (hist[0] == 0) len += n_;  // borda clara antes da primeira corrida
        for (size_t i = hist.size() - 1; i > 0; i--) hist[i] = hist[i - 1];
        hist[0] = len;
    }
    // Conta padroes escuro:claro:escuro:claro:escuro = 1:1:3:1:1 com 4 claros de um lado.
    static int ContaFinders(const std::array<int, 7>& h) {
        const int n = h[1];
        const bool nucleo = n > 0 && h[2] == n && h[3] == n * 3 && h[4] == n && h[5] == n;
        return (nucleo && h[0] >= n * 4 && h[6] >= n ? 1 : 0) + (nucleo && h[6] >= n * 4 && h[0] >= n ? 1 : 0);
    }
    int HistFecha(bool corCorrida, int lenCorrida, std::array<int, 7>& hist) const {
        if (corCorrida) {  // fecha a corrida escura
            HistAdd(lenCorrida, hist);
            lenCorrida = 0;
        }
        lenCorrida += n_;  // borda clara depois da ultima corrida
        HistAdd(lenCorrida, hist);
        return ContaFinders(hist);
    }

    int ver_;
    int n_;
    std::vector<unsigned char> mod_;
    std::vector<unsigned char> fun_;
};

}  // namespace detail

// ecl: 0=L 1=M 2=Q 3=H. Modo byte (UTF-8 cru). Escolhe a menor versao em
// [minVersion, maxVersion] (limitado a 1..40) que cabe e a melhor mascara
// (penalidade padrao ISO 18004) quando forceMask < 0; forceMask 0..7 usa essa.
// Devolve false (e out vazio) se os parametros forem invalidos ou o texto nao
// couber.
inline bool Encode(const std::string& text, Code& out, int ecl = 1, int minVersion = 1, int maxVersion = 20, int forceMask = -1) {
    out.size = 0;
    out.modules.clear();
    if (ecl < 0 || ecl > 3 || forceMask < -1 || forceMask > 7) return false;
    minVersion = std::max(minVersion, 1);
    maxVersion = std::min(maxVersion, 40);
    if (minVersion > maxVersion) return false;

    // Menor versao que cabe
    const size_t len = text.size();
    int ver = 0;
    for (int v = minVersion; v <= maxVersion; v++) {
        const int cc = detail::BitsContadorByte(v);
        if (len >= (static_cast<size_t>(1) << cc)) continue;
        const size_t precisa = 4 + static_cast<size_t>(cc) + 8 * len;
        if (precisa <= static_cast<size_t>(detail::CodewordsDados(v, ecl)) * 8) {
            ver = v;
            break;
        }
    }
    if (ver == 0) return false;

    // Fluxo de bits: modo 0100, contador, bytes, terminador, alinhamento e enchimento
    const size_t capBits = static_cast<size_t>(detail::CodewordsDados(ver, ecl)) * 8;
    std::vector<unsigned char> bits;
    bits.reserve(capBits);
    auto poe = [&bits](unsigned v, int n) {
        for (int i = n - 1; i >= 0; i--) bits.push_back(static_cast<unsigned char>((v >> i) & 1u));
    };
    poe(0x4, 4);
    poe(static_cast<unsigned>(len), detail::BitsContadorByte(ver));
    for (char ch : text) poe(static_cast<unsigned char>(ch), 8);
    poe(0, static_cast<int>(std::min<size_t>(4, capBits - bits.size())));
    poe(0, static_cast<int>((8 - bits.size() % 8) % 8));
    for (unsigned pad = 0xEC; bits.size() < capBits; pad ^= 0xEC ^ 0x11) poe(pad, 8);

    std::vector<std::uint8_t> dados(bits.size() / 8, 0);
    for (size_t i = 0; i < bits.size(); i++)
        dados[i >> 3] = static_cast<std::uint8_t>(dados[i >> 3] | (bits[i] << (7 - static_cast<int>(i & 7))));

    // Divide em blocos, calcula a correcao de cada um e intercala
    const int nBlocos = detail::kNumBlocos[ecl][ver];
    const int eccLen = detail::kEccPorBloco[ecl][ver];
    const int brutos = detail::ModulosBrutos(ver) / 8;
    const int nCurtos = nBlocos - brutos % nBlocos;
    const int lenCurto = brutos / nBlocos;  // tamanho (dados + correcao) do bloco curto
    const std::vector<std::uint8_t> div = detail::RsDivisor(eccLen);
    std::vector<std::vector<std::uint8_t>> blocos;
    blocos.reserve(static_cast<size_t>(nBlocos));
    size_t k = 0;
    for (int i = 0; i < nBlocos; i++) {
        const size_t dl = static_cast<size_t>(lenCurto - eccLen + (i < nCurtos ? 0 : 1));
        std::vector<std::uint8_t> b(dados.begin() + static_cast<std::ptrdiff_t>(k),
                                    dados.begin() + static_cast<std::ptrdiff_t>(k + dl));
        k += dl;
        const std::vector<std::uint8_t> ecc = detail::RsResto(b.data(), b.size(), div);
        if (i < nCurtos) b.push_back(0);  // enchimento so para alinhar as colunas; pulado abaixo
        b.insert(b.end(), ecc.begin(), ecc.end());
        blocos.push_back(std::move(b));
    }
    std::vector<std::uint8_t> finais;
    finais.reserve(static_cast<size_t>(brutos));
    const size_t colPulo = static_cast<size_t>(lenCurto - eccLen);
    for (size_t i = 0; i < blocos[0].size(); i++)
        for (size_t j = 0; j < blocos.size(); j++)
            if (i != colPulo || j >= static_cast<size_t>(nCurtos)) finais.push_back(blocos[j][i]);
    if (k != dados.size() || finais.size() != static_cast<size_t>(brutos)) return false;  // nao deve acontecer

    // Monta a matriz
    detail::Matriz m(ver);
    m.DesenhaFuncoes();
    m.DesenhaCodewords(finais);

    int mascara = forceMask;
    if (mascara < 0) {
        long melhor = 0;
        for (int i = 0; i < 8; i++) {
            m.AplicaMascara(i);
            m.Formato(ecl, i);
            const long p = m.Penalidade();
            if (mascara < 0 || p < melhor) {
                mascara = i;
                melhor = p;
            }
            m.AplicaMascara(i);  // desfaz
        }
    }
    m.AplicaMascara(mascara);
    m.Formato(ecl, mascara);

    out.size = m.Lado();
    out.modules = std::move(m.Modulos());
    return true;
}

}  // namespace qr
