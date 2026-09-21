#pragma once
#include "platform.h"
#include "app_keys.h"
#include <string>
#include <algorithm>
#include <fstream>
#include <vector>
#include <cwctype>
#include <cstdlib>
#include <filesystem>

// Leitura/gravacao em UTF-8 explicito. O wofstream padrao corrompe caminhos
// com acentos (ex.: "Musica", "Area de Trabalho"), o que fazia covers.ini,
// artists.ini e config.ini perderem as associacoes salvas.
#ifdef _WIN32
inline std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring w(n, L'\0');
    if (n) MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &w[0], n);
    return w;
}
inline std::string WideToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), NULL, 0, NULL, NULL);
    std::string s(n, '\0');
    if (n) WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), &s[0], n, NULL, NULL);
    return s;
}
#else
// Linux: wchar_t e UTF-32. Codec manual UTF-8 <-> UTF-32, independente de
// locale (bytes invalidos viram U+FFFD em vez de derrubar a leitura do .ini).
inline std::wstring Utf8ToWide(const std::string& s) {
    std::wstring w; w.reserve(s.size());
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        uint32_t cp; int len;
        if (c < 0x80) { cp = c; len = 1; }
        else if ((c & 0xE0) == 0xC0) { cp = c & 0x1F; len = 2; }
        else if ((c & 0xF0) == 0xE0) { cp = c & 0x0F; len = 3; }
        else if ((c & 0xF8) == 0xF0) { cp = c & 0x07; len = 4; }
        else { w.push_back((wchar_t)0xFFFD); ++i; continue; }
        if (i + (size_t)len > n) { w.push_back((wchar_t)0xFFFD); break; }
        bool ok = true;
        for (int k = 1; k < len; ++k) {
            unsigned char cc = (unsigned char)s[i + k];
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { w.push_back((wchar_t)0xFFFD); ++i; continue; }
        w.push_back((wchar_t)cp); i += len;
    }
    return w;
}
inline std::string WideToUtf8(const std::wstring& w) {
    std::string s; s.reserve(w.size() * 2);
    for (size_t i = 0; i < w.size(); ++i) {
        uint32_t cp = (uint32_t)w[i];
        if (cp >= 0xD800 && cp <= 0xDBFF && i + 1 < w.size()) { // par substituto (dados vindos de ID3 UTF-16)
            uint32_t lo = (uint32_t)w[i + 1];
            if (lo >= 0xDC00 && lo <= 0xDFFF) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); ++i; }
        }
        if (cp < 0x80) s.push_back((char)cp);
        else if (cp < 0x800) { s.push_back((char)(0xC0 | (cp >> 6))); s.push_back((char)(0x80 | (cp & 0x3F))); }
        else if (cp < 0x10000) { s.push_back((char)(0xE0 | (cp >> 12))); s.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); s.push_back((char)(0x80 | (cp & 0x3F))); }
        else { s.push_back((char)(0xF0 | (cp >> 18))); s.push_back((char)(0x80 | ((cp >> 12) & 0x3F))); s.push_back((char)(0x80 | ((cp >> 6) & 0x3F))); s.push_back((char)(0x80 | (cp & 0x3F))); }
    }
    return s;
}
#endif
inline bool ReadAllUtf8Lines(const std::wstring& path, std::vector<std::wstring>& lines) {
    std::ifstream f(std::filesystem::path(path), std::ios::binary);
    if (!f.good()) return false;
    std::string all((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (!all.empty() && (unsigned char)all[0] == 0xEF && all.size() >= 3 &&
        (unsigned char)all[1] == 0xBB && (unsigned char)all[2] == 0xBF) all.erase(0, 3); // BOM
    lines.clear();
    size_t start = 0;
    while (start <= all.size()) {
        size_t end = all.find('\n', start);
        if (end == std::string::npos) end = all.size();
        std::string ln = all.substr(start, end - start);
        if (!ln.empty() && ln.back() == '\r') ln.pop_back();
        lines.push_back(Utf8ToWide(ln));
        if (end == all.size()) break;
        start = end + 1;
    }
    return true;
}
inline bool WriteAllUtf8Lines(const std::wstring& path, const std::vector<std::wstring>& lines) {
    std::ofstream f(std::filesystem::path(path), std::ios::binary | std::ios::trunc);
    if (!f.good()) return false;
    for (auto& l : lines) { f << WideToUtf8(l) << "\n"; }
    return true;
}

struct Config {
    std::wstring musicFolder;
    std::wstring theme = L"azul";
    int uiStyle = 1;                      // estilo da interface: 0 classico, 1 remix (app_ui.h)
    // Host (docs/HOST.md): o Remix vira um servidor para o celular.
    bool hostOn = false;                  // liga o servidor ao abrir
    int hostPort = 49875;                 // porta TCP (1024..65535); 49875 = fora do que os servicos comuns usam
    std::wstring hostPin;                 // 4 a 12 digitos, obrigatorio para ligar
    bool hostTunnel = true;               // sobe o tunel Cloudflare (cloudflared) junto
    bool hostLan = true;                  // aceita a rede local (false = so 127.0.0.1, so pelo tunel)
    std::wstring hostName;                // nome mostrado no celular (vazio = nome do PC)
    bool hostOnline = true;               // o celular pode buscar/ouvir online (o PC usa a CLI configurada + ffmpeg)
    bool hostQrConfirm = false;           // vinculo pelo QR tambem pede ACEITAR no PC
    bool hostIPv6 = false;                // aceita IPv6 da rede local (link-local, ULA ou o mesmo /64)
    std::wstring rpcAppId;                // Rich Presence do Discord: Application ID (vazio = desligado)
    int rxSideW = 250;                    // largura da barra lateral do estilo REMIX (arrastavel)
    std::wstring outDevice;               // onde tocar (vazio = o que o sistema estiver usando)
    // Fonte de audio externa: caminho do programa de linha de comando que a
    // pessoa instalou e configurou. Vazio = as fontes externas ficam desligadas
    // (o player local e os arquivos continuam funcionando normalmente).
    std::wstring mediaCli;
    std::wstring displayMode = L"normal"; // normal | vertical  (layout)
    std::wstring artShape = L"square";    // square | cd       (aparencia da capa)
    int cdSpeed = 33;                     // velocidade de giro do CD, % da antiga (100 = original; padrao ~3x mais lento)
    int listMode = 0;                     // 0 = grade de cards, 1 = lista simples
    int ledBrightness = 100;
    int ledSpeed = 75;
    std::wstring ledEffect = L"respiracao";
    int volume = 80;
    // Efeitos de audio (0 = desligado, 1..3 = intensidade) e modo dos stems ("" = musica completa;
    // vocal, instrumental, bateria, baixo, outros). Slow e speed nao valem juntos.
    int fxSlow = 0, fxSpeed = 0, fxReverb = 0, fxBass = 0, fx8d = 0;
    std::wstring stemMode;
    // Quanta CPU a separacao em stems pode usar: 1 = leve (~1/4 dos nucleos),
    // 2 = equilibrado (~metade), 3 = rapido (~3/4). Leve por padrao: separar nunca
    // pode atrapalhar o resto do computador (jogo, chamada, trabalho).
    int stemsCpu = 1;
    // Separador externo: linha de comando que VOCE configura, com {entrada} e {saida}.
    // Vazio = usa o Python isolado que a pessoa tenha instalado antes, se existir.
    std::wstring sepCmd;
    bool shuffle = false;
    bool repeat = false;

    // Escalas independentes. 100 = tamanho original.
    int uiScale = 100;
    int titleScale = 115;
    int artistScale = 125;
    int verticalScale = 100;
    int playerScale = 100;

    // Efeitos visuais.
    bool particlesOn = false;
    bool glitchOn = false;
    bool autoColor = true;             // tema geral controla TODAS as cores
    int particlesSpeed = 100;
    std::wstring particlesColor = L""; // "" = segue o tema; senao id de tema
    std::wstring ledColor = L"";       // "" = segue o tema; senao id de tema
    bool runnerOn = true;
    int runnerSpeed = 60;
    std::wstring runnerColor = L"";   // "" = segue o tema; senao id de tema
    std::wstring btnPlayColor = L"";  // "" = segue o tema; senao id de tema
    std::wstring btnNavColor = L"";   // "" = segue o tema; senao id de tema

    // Fundo da janela.
    std::wstring bgWallpaper = L"";   // caminho de imagem usado de fundo (vazio = desativado)
    bool coverBlurBg = false;         // usa a capa da musica atual embaçada como fundo

    // Reproducao.
    bool autoplay = true;             // ao terminar uma faixa segue para a proxima (ordem/aleatorio)
    std::wstring sortMode = L"title"; // title | artist | file | date | manual (ordem salva em order.ini)
    bool sortDesc = false;
    bool eqOn = false;
    int eq[8] = {0,0,0,0,0,0,0,0};    // ganho em dB (-12..12) por banda: 60 150 400 1k 2.5k 6k 10k 15k Hz

    // Janela (modo normal): ultimo tamanho usado. 0 = padrao proporcional a tela.
    int winW = 0, winH = 0;

    // Modo leve (PCs fracos): sem particulas/glitch/corredor/blur, menos barras e menos FPS.
    bool perfMode = false;
    bool bgOnClose = true;     // fechar a janela com musica tocando = segue em 2o plano
    bool sysMedia = true;      // controles do sistema: MPRIS (Linux), teclas de midia + bandeja (Windows)
    Hotkey hk[HK_COUNT];       // atalhos configuraveis (app_keys.h)
    std::wstring openPlaylist; // slug da playlist aberta (vazio = biblioteca)
    std::wstring onlineMode = L"stream";   // musicas online: "stream" (so na memoria) ou "download"
    std::wstring onlineFormat = L"mp3";    // downloads: mp3 | m4a | original
    int onlineSource = 1;                  // busca online: 0 YouTube Music, 1 YouTube, 2 SoundCloud
    std::wstring downloadFolder;           // vazio = <pasta Musicas>/Remix Online
    bool askDlFolder = true;               // perguntar a pasta a cada download (comeca na ultima escolhida)
    void ResetHotkeys() { for (int i = 0; i < HK_COUNT; ++i) hk[i] = HkDefault(i); }
    // O padrao dos atalhos virou combinacoes de duas teclas: quem estava no padrao antigo recebe o
    // novo; os que a pessoa trocou ficam como estao. Um atalho novo que ja esteja em uso fica vazio.
    void MigrateHotkeys() {
        bool mig[HK_COUNT] = {};
        for (int i = 0; i < HK_COUNT; ++i) { Hotkey o = HkDefaultAntigo(i); mig[i] = hk[i].key == o.key && hk[i].mods == o.mods; }
        for (int i = 0; i < HK_COUNT; ++i) if (mig[i]) { hk[i].key = 0; hk[i].mods = 0; }
        for (int i = 0; i < HK_COUNT; ++i) {
            if (!mig[i]) continue;
            Hotkey d = HkDefault(i); bool usado = false;
            for (int j = 0; j < HK_COUNT && d.key; ++j) if (j != i && hk[j].key == d.key && hk[j].mods == d.mods) usado = true;
            if (!usado) { hk[i].key = d.key; hk[i].mods = d.mods; }
        }
    }
    Config() { ResetHotkeys(); }
    // Pastas de musica usadas recentemente (menu PASTA), mais recente primeiro.
    std::vector<std::wstring> recentFolders;

    // ---- pastas -------------------------------------------------------------
    // ExeDir  : pasta do executavel.
    // BaseDir : pasta "portatil" onde vivem config.ini, covers.ini, artists.ini
    //           e assets/covers. No Windows e sempre a ExeDir. No Linux e a
    //           ExeDir quando existe config.ini ao lado do binario (modo
    //           portatil), senao ~/.config/remix (instalado via .deb/.rpm).
    //           A variavel REMIX_HOME forca uma pasta especifica.
    // AssetDir: assets empacotados (branding, fontes, temas padrao).
    static std::wstring Join(const std::wstring& a, const std::wstring& b) {
        if (a.empty()) return b;
        if (a.back() == REMIX_SEP) return a + b;
        return a + REMIX_SEP_STR + b;
    }
    static std::wstring ExeDir() {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        GetModuleFileNameW(NULL, buf, MAX_PATH);
        std::wstring p(buf);
        size_t pos = p.find_last_of(L"\\/");
        return (pos == std::wstring::npos) ? L"." : p.substr(0, pos);
#else
        static std::wstring cached;
        if (!cached.empty()) return cached;
        std::error_code ec;
        auto p = std::filesystem::read_symlink("/proc/self/exe", ec);
        cached = ec ? L"." : p.parent_path().wstring();
        if (cached.empty()) cached = L".";
        return cached;
#endif
    }
    // Pasta de cima com config.ini + assets/ = layout do projeto (windows\Remix.exe ou
    // linux/remix rodando de dentro da arvore de codigo): a "casa" (config, capas,
    // Musica/) e a raiz do projeto, compartilhada pelas duas versoes.
    static bool ParentIsHome(const std::wstring& exeDir, std::wstring& out) {
        std::error_code ec;
        std::filesystem::path parent = std::filesystem::path(exeDir).parent_path();
        if (parent.empty()) return false;
        // raiz do projeto: tem assets/ e config.ini (ou, num clone do git ainda sem config, o codigo em comum/)
        if (!std::filesystem::exists(parent / "assets", ec)) return false;
        if (!std::filesystem::exists(parent / "config.ini", ec) && !std::filesystem::exists(parent / "comum" / "config.h", ec)) return false;
        out = parent.wstring();
        while (out.size() > 1 && (out.back() == L'\\' || out.back() == L'/')) out.pop_back();
        return true;
    }
    static std::wstring BaseDir() {
#ifdef _WIN32
        static std::wstring cached;
        if (!cached.empty()) return cached;
        // REMIX_HOME (ou --home) forca uma pasta, igual ao Linux; senao config.ini ao lado do exe
        // (zip portatil); senao a pasta de cima se for a raiz do projeto; senao a pasta do exe.
        if (const wchar_t* env = _wgetenv(L"REMIX_HOME"); env && *env) {
            std::wstring c(env);
            while (c.size() > 1 && (c.back() == L'\\' || c.back() == L'/')) c.pop_back();
            cached = c; return cached;
        }
        std::error_code ec; std::wstring exe = ExeDir(), parent;
        if (std::filesystem::exists(std::filesystem::path(exe) / "config.ini", ec)) cached = exe;
        else if (ParentIsHome(exe, parent)) cached = parent;
        else cached = exe;
        return cached;
#else
        static std::wstring cached;
        if (!cached.empty()) return cached;
        std::error_code ec; std::wstring parent;
        const char* env = std::getenv("REMIX_HOME");
        if (env && *env) cached = Utf8ToWide(env);
        else if (std::filesystem::exists(std::filesystem::path(ExeDir()) / "config.ini", ec)) cached = ExeDir();
        else if (ParentIsHome(ExeDir(), parent)) cached = parent;
        else {
            const char* xdg = std::getenv("XDG_CONFIG_HOME");
            const char* home = std::getenv("HOME");
            std::string base = (xdg && *xdg) ? std::string(xdg) : (std::string(home ? home : ".") + "/.config");
            cached = Utf8ToWide(base + "/remix");
            std::filesystem::create_directories(std::filesystem::path(cached), ec);
        }
        while (cached.size() > 1 && cached.back() == REMIX_SEP) cached.pop_back();
        return cached;
#endif
    }
    static std::wstring AssetDir() {
#ifdef _WIN32
        return BaseDir() + L"\\assets";   // zip portatil: ao lado do exe; arvore do projeto: raiz\assets
#else
        static std::wstring cached;
        if (!cached.empty()) return cached;
        std::error_code ec;
        const char* env = std::getenv("REMIX_ASSETS");
        if (env && *env) { cached = Utf8ToWide(env); return cached; }
        namespace fs = std::filesystem;
        fs::path exe(ExeDir());
        fs::path cands[] = {
            fs::path(BaseDir()) / "assets",          // portatil
            exe / "assets",                          // binario solto ao lado dos assets
            exe / ".." / "assets",                   // build de desenvolvimento (build/remix)
            exe / ".." / "share" / "remix" / "assets", // instalado (/usr/bin -> /usr/share)
            fs::path("/usr/share/remix/assets"),
            fs::path("/usr/local/share/remix/assets"),
        };
        for (auto& c : cands)
            if (fs::exists(c / "branding" / "splash.png", ec)) { cached = fs::weakly_canonical(c, ec).wstring(); if (cached.empty()) cached = c.wstring(); return cached; }
        cached = (fs::path(BaseDir()) / "assets").wstring();
        return cached;
#endif
    }
    static std::wstring CoversDir() { return Join(Join(BaseDir(), L"assets"), L"covers"); }
    static std::wstring ConfigPath() { return Join(BaseDir(), L"config.ini"); }
    static std::wstring CoversPath() { return Join(BaseDir(), L"covers.ini"); }
    static std::wstring ArtistsPath() { return Join(BaseDir(), L"artists.ini"); }
    static std::wstring OrderPath() { return Join(BaseDir(), L"order.ini"); }
    // Cache (arquivos temporarios grandes, ex.: conversoes de formato).
    static std::wstring CacheDir() {
#ifdef _WIN32
        wchar_t buf[MAX_PATH]; DWORD n = GetTempPathW(MAX_PATH, buf);
        std::wstring t = n ? std::wstring(buf, n) : L".";
        while (!t.empty() && t.back() == L'\\') t.pop_back();
        return t + L"\\remix-cache";
#else
        const char* xc = std::getenv("XDG_CACHE_HOME");
        const char* home = std::getenv("HOME");
        std::string base = (xc && *xc) ? std::string(xc) : (std::string(home ? home : "/tmp") + "/.cache");
        return Utf8ToWide(base + "/remix");
#endif
    }

    // ---- caminhos portateis ------------------------------------------------
    // Os .inis guardam caminhos RELATIVOS a BaseDir sempre que o arquivo
    // estiver dentro dela (musicas, capas internas, wallpaper local). Assim as
    // associacoes continuam validas em qualquer maquina/pasta, basta mover o
    // app inteiro. Caminhos fora da BaseDir continuam absolutos.
    static bool IsAbsPath(const std::wstring& p) {
#ifdef _WIN32
        return p.size() >= 2 && (p[1] == L':' || (p[0] == L'\\' && p[1] == L'\\'));
#else
        return !p.empty() && p[0] == L'/';
#endif
    }
    static std::wstring NormSep(std::wstring p) {
#ifdef _WIN32
        for (auto& c : p) if (c == L'/') c = L'\\';
#else
        for (auto& c : p) if (c == L'\\') c = L'/'; // .inis vindos do Windows
#endif
        while (p.size() >= 2 && p[0] == L'.' && p[1] == REMIX_SEP) p.erase(0, 2);
        return p;
    }
    static bool PathCharEq(wchar_t a, wchar_t b) {
#ifdef _WIN32
        return towlower(a) == towlower(b);
#else
        return a == b;
#endif
    }
    static bool StartsI(const std::wstring& full, const std::wstring& base) {
        if (full.size() <= base.size()) return false;
        for (size_t i = 0; i < base.size(); ++i)
            if (!PathCharEq(full[i], base[i])) return false;
        return full[base.size()] == REMIX_SEP;
    }
    static bool IsInsideExeDir(const std::wstring& p) { // "dentro da BaseDir"
        std::wstring q = NormSep(p), base = ExeDirNorm();
        if (q.size() < base.size()) return false;
        for (size_t i = 0; i < base.size(); ++i)
            if (!PathCharEq(q[i], base[i])) return false;
        return q.size() == base.size() || q[base.size()] == REMIX_SEP;
    }
    static std::wstring ExeDirNorm() { // BaseDir normalizada, sem separador final
        std::wstring b = NormSep(BaseDir());
        while (b.size() > 1 && b.back() == REMIX_SEP) b.pop_back();
        return b;
    }
    // absoluto -> relativo a BaseDir, se possivel
    static std::wstring ToPortable(const std::wstring& path) {
        std::wstring p = NormSep(path);
        if (p.empty() || !IsAbsPath(p)) return p;
        std::wstring base = ExeDirNorm();
        if (StartsI(p, base)) return p.substr(base.size() + 1);
        return p;
    }
    // relativo/absoluto salvo no ini -> caminho utilizavel agora
    static std::wstring FromPortable(const std::wstring& stored) {
        std::wstring p = NormSep(stored);
        if (p.empty() || IsAbsPath(p)) return p;
        return ExeDirNorm() + REMIX_SEP_STR + p;
    }

    static bool DirExists(const std::wstring& p) {
        if (p.empty()) return false;
#ifdef _WIN32
        DWORD a = GetFileAttributesW(p.c_str());
        return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
#else
        std::error_code ec;
        return std::filesystem::is_directory(std::filesystem::path(p), ec);
#endif
    }
    // Se a pasta de musica salva nao existe mais (app foi movido), adota a
    // pasta "Musica" ao lado do exe quando existir.
    void HealMusicFolder() {
        if (!musicFolder.empty() && DirExists(musicFolder)) return;
        std::wstring cand = Join(BaseDir(), L"Musica");
        if (DirExists(cand)) { musicFolder = cand; Save(); }
    }

    static std::wstring Trim(const std::wstring& s) {
        size_t a = s.find_first_not_of(L" \t\r\n");
        if (a == std::wstring::npos) return L"";
        size_t b = s.find_last_not_of(L" \t\r\n");
        return s.substr(a, b - a + 1);
    }

    void Load() {

        ResetHotkeys();
        std::vector<std::wstring> ls;
        if (!ReadAllUtf8Lines(ConfigPath(), ls)) { Save(); return; }
        bool hkNovo = false;   // HotkeysVersion=2: atalhos ja no padrao de duas teclas
        for (auto raw : ls) {
            std::wstring line = Trim(raw);
            if (line.empty() || line[0] == L'[' || line[0] == L';') continue;
            size_t eq = line.find(L'=');
            if (eq == std::wstring::npos) continue;
            std::wstring k = Trim(line.substr(0, eq));
            std::wstring v = Trim(line.substr(eq + 1));
            if (k == L"MusicFolder") musicFolder = FromPortable(v);
            else if (k == L"Theme") theme = v;
            else if (k == L"DisplayMode") displayMode = v;
            else if (k == L"Style") uiStyle = _wtoi(v.c_str());
            else if (k == L"HostOn") hostOn = (v == L"1");
            else if (k == L"HostPort") hostPort = _wtoi(v.c_str());
            else if (k == L"HostPin") hostPin = v;
            else if (k == L"HostTunnel") hostTunnel = (v != L"0");
            else if (k == L"HostLan") hostLan = (v != L"0");
            else if (k == L"HostName") hostName = v;
            else if (k == L"HostOnline") hostOnline = (v != L"0");
            else if (k == L"HostQrConfirm") hostQrConfirm = (v == L"1");
            else if (k == L"RpcAppId") rpcAppId = v;
            else if (k == L"SideW") rxSideW = _wtoi(v.c_str());
            else if (k == L"OutDevice") outDevice = v;
            else if (k == L"MediaCli") mediaCli = v;
            else if (k == L"HostIPv6") hostIPv6 = (v == L"1");
            else if (k == L"ArtShape") artShape = v;
            else if (k == L"CdSpeed") cdSpeed = _wtoi(v.c_str());
            else if (k == L"ListMode") listMode = _wtoi(v.c_str());
            else if (k == L"Brightness") ledBrightness = _wtoi(v.c_str());
            else if (k == L"Speed") ledSpeed = _wtoi(v.c_str());
            else if (k == L"Effect") ledEffect = v;
            else if (k == L"Volume") volume = _wtoi(v.c_str());
            else if (k == L"FxSlow") fxSlow = _wtoi(v.c_str());
            else if (k == L"FxSpeed") fxSpeed = _wtoi(v.c_str());
            else if (k == L"FxReverb") fxReverb = _wtoi(v.c_str());
            else if (k == L"FxBass") fxBass = _wtoi(v.c_str());
            else if (k == L"Fx8D") fx8d = _wtoi(v.c_str());
            else if (k == L"StemsCpu") { stemsCpu = _wtoi(v.c_str()); if (stemsCpu < 1 || stemsCpu > 3) stemsCpu = 1; }
            else if (k == L"SepCmd") sepCmd = v;
            else if (k == L"StemMode") stemMode = (v == L"vocal" || v == L"instrumental" || v == L"bateria" || v == L"baixo" || v == L"outros") ? v : L"";
            else if (k == L"Shuffle") shuffle = (v == L"1");
            else if (k == L"Repeat") repeat = (v == L"1");
            else if (k == L"UIScale") uiScale = _wtoi(v.c_str());
            else if (k == L"TitleScale") titleScale = _wtoi(v.c_str());
            else if (k == L"ArtistScale") artistScale = _wtoi(v.c_str());
            else if (k == L"VerticalScale") verticalScale = _wtoi(v.c_str());
            else if (k == L"PlayerScale") playerScale = _wtoi(v.c_str());
            else if (k == L"Particles") particlesOn = (v == L"1");
            else if (k == L"AutoColor") autoColor = (v == L"1");
            else if (k == L"ParticlesSpeed") particlesSpeed = _wtoi(v.c_str());
            else if (k == L"ParticlesColor") particlesColor = v;
            else if (k == L"LedColor") ledColor = v;
            else if (k == L"Glitch") glitchOn = (v == L"1");
            else if (k == L"RunnerOn") runnerOn = (v == L"1");
            else if (k == L"RunnerSpeed") runnerSpeed = _wtoi(v.c_str());
            else if (k == L"RunnerColor") runnerColor = v;
            else if (k == L"BtnPlayColor") btnPlayColor = v;
            else if (k == L"BtnNavColor") btnNavColor = v;
            else if (k == L"Wallpaper") bgWallpaper = FromPortable(v);
            else if (k == L"CoverBlurBg") coverBlurBg = (v == L"1");
            else if (k == L"Autoplay") autoplay = (v == L"1");
            else if (k == L"SortMode") sortMode = v;
            else if (k == L"SortDesc") sortDesc = (v == L"1");
            else if (k == L"EqOn") eqOn = (v == L"1");
            else if (k == L"Eq") {
                size_t pos = 0;
                for (int i = 0; i < 8 && pos <= v.size(); ++i) {
                    size_t c = v.find(L',', pos);
                    this->eq[i] = remix_parse_int(v.substr(pos, c == std::wstring::npos ? std::wstring::npos : c - pos)); // "eq" local = posicao do '='
                    if (c == std::wstring::npos) break;
                    pos = c + 1;
                }
            }
            else if (k == L"WinW") winW = _wtoi(v.c_str());
            else if (k == L"WinH") winH = _wtoi(v.c_str());
            else if (k == L"PerfMode") perfMode = (v == L"1");
            else if (k == L"BackgroundOnClose") bgOnClose = (v == L"1");
            else if (k == L"SystemMediaControls") sysMedia = (v == L"1");
            else if (k == L"OpenPlaylist") openPlaylist = v;
            else if (k == L"OnlineMode") onlineMode = (v == L"download") ? L"download" : L"stream";
            else if (k == L"OnlineFormat") onlineFormat = (v == L"m4a" || v == L"original") ? v : L"mp3";
            else if (k == L"OnlineSource") onlineSource = std::max(0, std::min(2, _wtoi(v.c_str())));
            else if (k == L"DownloadFolder") downloadFolder = v.empty() ? L"" : FromPortable(v);
            else if (k == L"AskDownloadFolder") askDlFolder = (v != L"0");
            else if (k == L"HotkeysVersion") hkNovo = _wtoi(v.c_str()) >= 2;
            else if (k.rfind(L"Hk.", 0) == 0) { int a = HkIndexById(k.substr(3)); if (a >= 0) ParseHotkey(v, hk[a]); }
            else if (k == L"RecentFolders") {
                recentFolders.clear();
                size_t pos = 0;
                while (pos <= v.size() && recentFolders.size() < 8) {
                    size_t c = v.find(L'|', pos);
                    std::wstring item = Trim(v.substr(pos, c == std::wstring::npos ? std::wstring::npos : c - pos));
                    if (!item.empty()) recentFolders.push_back(FromPortable(item));
                    if (c == std::wstring::npos) break;
                    pos = c + 1;
                }
            }
        }
        if (!hkNovo) MigrateHotkeys();
        // Migracao do formato antigo: "square"/"cd"/"vertical" viviam num campo so.
        uiStyle = std::max(0, std::min(1, uiStyle));   // 1.6: o antigo 2 (Spotify + LED) virou o estilo REMIX
        if (hostPort < 1024 || hostPort > 65535) hostPort = 49875;
        { bool okPin = hostPin.size() >= 4 && hostPin.size() <= 12; for (wchar_t c : hostPin) if (c < L'0' || c > L'9') okPin = false; if (!okPin) hostPin.clear(); }
        if (displayMode == L"square") { displayMode = L"normal"; artShape = L"square"; }
        else if (displayMode == L"cd") { displayMode = L"normal"; artShape = L"cd"; }
        else if (displayMode == L"vertical") { if (artShape != L"square") artShape = L"cd"; }
        if (artShape != L"square" && artShape != L"cd") artShape = L"square";
        cdSpeed = std::max(0, std::min(200, cdSpeed));
        if (sortMode != L"title" && sortMode != L"artist" && sortMode != L"file" && sortMode != L"date" && sortMode != L"manual") sortMode = L"title";
        Clamp();
    }

    void Clamp() {
        uiScale = std::max(70, std::min(150, uiScale));
        rxSideW = std::max(170, std::min(460, rxSideW));
        titleScale = std::max(80, std::min(180, titleScale));
        artistScale = std::max(80, std::min(200, artistScale));
        verticalScale = std::max(70, std::min(150, verticalScale));
        playerScale = std::max(60, std::min(170, playerScale));
        runnerSpeed = std::max(0, std::min(200, runnerSpeed));
        particlesSpeed = std::max(10, std::min(300, particlesSpeed));
        ledBrightness = std::max(0, std::min(100, ledBrightness));
        ledSpeed = std::max(0, std::min(100, ledSpeed));
        volume = std::max(0, std::min(100, volume));
        for (int* f : { &fxSlow, &fxSpeed, &fxReverb, &fxBass, &fx8d }) *f = std::max(0, std::min(3, *f));
        if (fxSlow && fxSpeed) fxSpeed = 0;
        for (int& g : eq) g = std::max(-12, std::min(12, g));
        if (winW < 0) winW = 0;
        if (winH < 0) winH = 0;
    }

    void Save() {
        Clamp();
        std::vector<std::wstring> ls;
        wchar_t b[64];
        ls.push_back(L"[General]");
        ls.push_back(L"MusicFolder=" + ToPortable(musicFolder));
        ls.push_back(L"Theme=" + theme);
        ls.push_back(L"DisplayMode=" + displayMode);
        swprintf(b, 64, L"Style=%d", uiStyle); ls.push_back(b);
        ls.push_back(L"[Host]");
        swprintf(b, 64, L"HostOn=%d", hostOn ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"HostPort=%d", hostPort); ls.push_back(b);
        ls.push_back(L"HostPin=" + hostPin);
        swprintf(b, 64, L"HostTunnel=%d", hostTunnel ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"HostLan=%d", hostLan ? 1 : 0); ls.push_back(b);
        ls.push_back(L"HostName=" + hostName);
        swprintf(b, 64, L"HostOnline=%d", hostOnline ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"HostQrConfirm=%d", hostQrConfirm ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"HostIPv6=%d", hostIPv6 ? 1 : 0); ls.push_back(b);
        ls.push_back(L"RpcAppId=" + rpcAppId);
        swprintf(b, 64, L"SideW=%d", rxSideW); ls.push_back(b);
        ls.push_back(L"OutDevice=" + outDevice);
        ls.push_back(L"MediaCli=" + mediaCli);
        ls.push_back(L"ArtShape=" + artShape);
        swprintf(b, 64, L"CdSpeed=%d", cdSpeed); ls.push_back(b);
        swprintf(b, 64, L"ListMode=%d", listMode ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"Volume=%d", volume); ls.push_back(b);
        swprintf(b, 64, L"FxSlow=%d", fxSlow); ls.push_back(b);
        swprintf(b, 64, L"FxSpeed=%d", fxSpeed); ls.push_back(b);
        swprintf(b, 64, L"FxReverb=%d", fxReverb); ls.push_back(b);
        swprintf(b, 64, L"FxBass=%d", fxBass); ls.push_back(b);
        swprintf(b, 64, L"Fx8D=%d", fx8d); ls.push_back(b);
        ls.push_back(L"StemMode=" + stemMode);
        swprintf(b, 64, L"StemsCpu=%d", stemsCpu); ls.push_back(b);
        ls.push_back(L"SepCmd=" + sepCmd);
        ls.push_back(L"Shuffle="); ls.back() += shuffle ? L"1" : L"0";
        ls.push_back(L"Repeat=");  ls.back() += repeat ? L"1" : L"0";
        swprintf(b, 64, L"UIScale=%d", uiScale); ls.push_back(b);
        swprintf(b, 64, L"TitleScale=%d", titleScale); ls.push_back(b);
        swprintf(b, 64, L"ArtistScale=%d", artistScale); ls.push_back(b);
        swprintf(b, 64, L"VerticalScale=%d", verticalScale); ls.push_back(b);
        swprintf(b, 64, L"PlayerScale=%d", playerScale); ls.push_back(b);
        ls.push_back(L"[Efeitos]");
        swprintf(b, 64, L"Particles=%d", particlesOn ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"AutoColor=%d", autoColor ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"Glitch=%d", glitchOn ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"ParticlesSpeed=%d", particlesSpeed); ls.push_back(b);
        ls.push_back(L"ParticlesColor=" + particlesColor);
        ls.push_back(L"LedColor=" + ledColor);
        ls.push_back(L"[LED]");
        swprintf(b, 64, L"Brightness=%d", ledBrightness); ls.push_back(b);
        swprintf(b, 64, L"Speed=%d", ledSpeed); ls.push_back(b);
        ls.push_back(L"Effect=" + ledEffect);
        swprintf(b, 64, L"RunnerOn=%d", runnerOn ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"RunnerSpeed=%d", runnerSpeed); ls.push_back(b);
        ls.push_back(L"RunnerColor=" + runnerColor);
        ls.push_back(L"[Botoes]");
        ls.push_back(L"PlayColor=" + btnPlayColor);
        ls.push_back(L"NavColor=" + btnNavColor);
        ls.push_back(L"[Fundo]");
        ls.push_back(L"Wallpaper=" + ToPortable(bgWallpaper));
        swprintf(b, 64, L"CoverBlurBg=%d", coverBlurBg ? 1 : 0); ls.push_back(b);
        ls.push_back(L"[Reproducao]");
        swprintf(b, 64, L"Autoplay=%d", autoplay ? 1 : 0); ls.push_back(b);
        ls.push_back(L"SortMode=" + sortMode);
        swprintf(b, 64, L"SortDesc=%d", sortDesc ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"EqOn=%d", eqOn ? 1 : 0); ls.push_back(b);
        {
            std::wstring e = L"Eq=";
            for (int i = 0; i < 8; ++i) { swprintf(b, 64, L"%d", eq[i]); e += b; if (i < 7) e += L","; }
            ls.push_back(e);
        }
        swprintf(b, 64, L"PerfMode=%d", perfMode ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"BackgroundOnClose=%d", bgOnClose ? 1 : 0); ls.push_back(b);
        swprintf(b, 64, L"SystemMediaControls=%d", sysMedia ? 1 : 0); ls.push_back(b);
        ls.push_back(L"OpenPlaylist=" + openPlaylist);
        ls.push_back(L"OnlineMode=" + onlineMode);
        ls.push_back(L"OnlineFormat=" + onlineFormat);
        swprintf(b, 64, L"OnlineSource=%d", onlineSource); ls.push_back(b);
        ls.push_back(L"DownloadFolder=" + (downloadFolder.empty() ? std::wstring() : ToPortable(downloadFolder)));
        ls.push_back(std::wstring(L"AskDownloadFolder=") + (askDlFolder ? L"1" : L"0"));
        ls.push_back(L"[Atalhos]");
        ls.push_back(L"HotkeysVersion=2");
        for (int i = 0; i < HK_COUNT; ++i) ls.push_back(std::wstring(L"Hk.") + HkId(i) + L"=" + HotkeyToString(hk[i]));
        {
            std::wstring r = L"RecentFolders=";
            for (size_t i = 0; i < recentFolders.size() && i < 8; ++i) { if (i) r += L"|"; r += ToPortable(recentFolders[i]); }
            ls.push_back(r);
        }
        ls.push_back(L"[Janela]");
        swprintf(b, 64, L"WinW=%d", winW); ls.push_back(b);
        swprintf(b, 64, L"WinH=%d", winH); ls.push_back(b);
        WriteAllUtf8Lines(ConfigPath(), ls);
    }
};
