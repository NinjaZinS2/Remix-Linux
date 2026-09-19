#pragma once
// ============================================================================
// CAMADA DE FONTE EXTERNA
// O unico lugar do Remix que pode executar um programa de midia de terceiros.
//
// Regras desta camada (valem para qualquer casca: Linux, Windows, ...):
//
//  1. O Remix NAO instala, NAO baixa, NAO atualiza e NAO embute programa
//     nenhum. Ele so executa o caminho que a PESSOA escolheu em
//     Configuracoes > PROGRAMA DE LINHA DE COMANDO (chave MediaCli do
//     config.ini). Sem esse caminho, as fontes externas ficam desligadas.
//
//  2. Integracao por PROCESSO, nunca por biblioteca: argv em vetor, sem shell,
//     e o Remix so le o JSON que o programa imprime na saida padrao. Nenhum
//     codigo de terceiros e ligado ao binario.
//
//  3. Falha de um jeito so: Configurada() == false e MsgFalta() explica o que
//     fazer. Nenhuma outra parte do app pode inventar caminho alternativo,
//     procurar o programa sozinha nem cair para outro executavel.
//
//  4. O app nao conhece nenhum programa especifico: ele descreve o CONTRATO
//     (Contrato()) das opcoes que o programa precisa aceitar. Qualquer CLI que
//     cumpra o contrato serve.
//
//  5. Tocar e a acao principal. Salvar uma copia e secundario e sempre uma
//     escolha explicita da pessoa.
//
//  Quem toca no assunto: online_resolve.h (busca e links), online_play.h
//  (tocar/salvar), host_server.h (celular), discord_host.h (bot), stems.h.
//  Todos passam por aqui.
// ============================================================================
#include "platform.h"
#include "config.h"
#include "app_proc.h"
#include <mutex>
#include <atomic>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <cstdio>
#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#endif

std::wstring g_cfgMediaCli();   // caminho configurado pela pessoa (app_core.h)

namespace fonte {

// O que o programa de linha de comando precisa aceitar para servir ao Remix.
// Fica visivel nas Configuracoes e na documentacao: e o contrato, nao um nome.
inline const wchar_t* Contrato() {
    return L"Precisa aceitar -j/-J, -f, -o, --flat-playlist, --playlist-items, --no-playlist, "
           L"--ignore-config, --ffmpeg-location e --js-runtimes, e imprimir JSON na saida padrao.";
}

struct Estado {
    std::wstring cli, ffmpeg, jsName, jsPath;    // caminhos achados
    std::wstring vCli, vFf, vJs, jsOld;          // versoes (jsOld: achado, mas antigo demais)
};

struct Interno {
    std::mutex m;
    std::atomic<bool> probed{ false }, probing{ false };
    Estado e;
};
inline Interno& I() { static Interno* t = new Interno(); return *t; }

// ---- arquivos ------------------------------------------------------------
inline bool ExecutavelOk(const std::wstring& caminho) {
    if (caminho.empty()) return false;
    std::string u = WideToUtf8(caminho);
#ifdef _WIN32
    struct _stat64i32 sb;
    return _waccess(caminho.c_str(), 0) == 0 && _wstat(caminho.c_str(), &sb) == 0 && (sb.st_mode & _S_IFREG);
#else
    struct stat sb;
    return access(u.c_str(), X_OK) == 0 && stat(u.c_str(), &sb) == 0 && S_ISREG(sb.st_mode);
#endif
}
// Pastas do "procurar no sistema". So valem para ffmpeg e para o runtime
// JavaScript (programas genericos); o programa de midia vem SO do caminho
// configurado -- o app nunca sai catando um baixador pelo disco.
inline std::vector<std::wstring> PastasDeBusca() {
    std::vector<std::wstring> d;
    auto split = [&](const std::wstring& list, wchar_t sep) {
        size_t st = 0;
        while (st <= list.size()) {
            size_t e = list.find(sep, st);
            std::wstring x = list.substr(st, e == std::wstring::npos ? std::wstring::npos : e - st);
            if (!x.empty()) d.push_back(x);
            if (e == std::wstring::npos) break;
            st = e + 1;
        }
    };
#ifdef _WIN32
    const char* p = std::getenv("PATH"); if (p) split(Utf8ToWide(p), L';');
#else
    const char* p = std::getenv("PATH"); if (p) split(Utf8ToWide(p), L':');
    const char* h = std::getenv("HOME");
    if (h) { std::wstring hw = Utf8ToWide(h); d.push_back(hw + L"/.local/bin"); d.push_back(hw + L"/bin"); }
    for (const wchar_t* x : { L"/usr/local/bin", L"/usr/bin", L"/bin", L"/snap/bin" }) d.push_back(x);
#endif
    return d;
}
inline std::wstring AcharNoSistema(const std::wstring& nome) {
    if (nome.find(L'/') != std::wstring::npos || nome.find(L'\\') != std::wstring::npos)
        return ExecutavelOk(nome) ? nome : L"";
    for (auto& dir : PastasDeBusca()) {
        std::wstring p = Config::Join(dir, nome);
        if (ExecutavelOk(p)) return p;
#ifdef _WIN32
        std::wstring pe = p + L".exe"; if (ExecutavelOk(pe)) return pe;
#endif
    }
    return L"";
}

// ---- conferir o que existe ------------------------------------------------
inline std::wstring CaminhoConfigurado() { return g_cfgMediaCli(); }

inline void ConferirImpl() {
    std::wstring cli = CaminhoConfigurado();
    if (!ExecutavelOk(cli)) cli.clear();            // configurada mas sumiu: fica desligada
    std::wstring ff = AcharNoSistema(L"ffmpeg"), js, jsn;
    auto ver = [](const std::vector<std::wstring>& a) -> std::wstring {
        CapResult r = RunCapture(a, 20000);
        std::string o = r.out.empty() ? r.err : r.out;
        size_t e = o.find_first_of("\r\n"); if (e != std::string::npos) o = o.substr(0, e);
        return Utf8ToWide(o.size() > 60 ? o.substr(0, 60) : o);
    };
    std::wstring vCli = cli.empty() ? L"" : ver({ cli, L"--version" });
    std::wstring vFf = !ff.empty() ? ver({ ff, L"-version" }) : L"";
    { size_t k = vFf.find(L"version "); if (k != std::wstring::npos) { vFf = vFf.substr(k + 8); size_t e = vFf.find(L' '); if (e != std::wstring::npos) vFf = vFf.substr(0, e); } }
    std::wstring vJs, jsOld;   // algumas CLIs precisam de um runtime JavaScript recente
    for (const wchar_t* n : { L"deno", L"node", L"bun" }) {
        std::wstring p = AcharNoSistema(n); if (p.empty()) continue;
        std::wstring v = ver({ p, L"--version" });
        int a = 0, b = 0, c = 0;
        { std::string s2 = WideToUtf8(v); size_t k = s2.find_first_of("0123456789"); if (k != std::string::npos) sscanf(s2.c_str() + k, "%d.%d.%d", &a, &b, &c); }
        wchar_t vb[48]; swprintf(vb, 48, L"%d.%d.%d", a, b, c);
        std::wstring nn(n);
        bool okv = nn == L"deno" ? (a > 2 || (a == 2 && b >= 3)) : nn == L"node" ? a >= 22 : (a > 1 || (a == 1 && (b > 2 || (b == 2 && c >= 11))));
        if (okv) { js = p; jsn = nn; vJs = vb; break; }
        if (jsOld.empty()) jsOld = nn + L" " + vb;
    }
    {
        std::lock_guard<std::mutex> lk(I().m);
        I().e.cli = cli; I().e.ffmpeg = ff; I().e.jsName = jsn; I().e.jsPath = js;
        I().e.vCli = vCli; I().e.vFf = vFf; I().e.vJs = vJs; I().e.jsOld = jsOld;
    }
}
inline void Conferir() {   // nunca deixa "probing" preso: quem espera ficaria parado para sempre
    if (I().probing.exchange(true)) { while (I().probing.load()) std::this_thread::sleep_for(std::chrono::milliseconds(50)); return; }
    RemixSafe("conferir a fonte externa", [] { ConferirImpl(); });
    I().probed = true;
    I().probing = false;
}
inline void Garantir() { if (!I().probed.load()) Conferir(); }
inline void Reconferir() { I().probed = false; Conferir(); }        // depois de mudar o caminho
inline bool JaConferiu() { return I().probed.load(); }
inline bool Conferindo() { return I().probing.load(); }
inline Estado Snapshot() { std::lock_guard<std::mutex> lk(I().m); return I().e; }

// ---- o que o resto do app usa --------------------------------------------
inline bool Configurada() { std::lock_guard<std::mutex> lk(I().m); return !I().e.cli.empty(); }
inline std::wstring Caminho() { std::lock_guard<std::mutex> lk(I().m); return I().e.cli; }
inline std::wstring Versao() { std::lock_guard<std::mutex> lk(I().m); return I().e.vCli; }
inline bool FfmpegOk() { std::lock_guard<std::mutex> lk(I().m); return !I().e.ffmpeg.empty(); }
inline std::wstring Ffmpeg() { std::lock_guard<std::mutex> lk(I().m); return I().e.ffmpeg; }
inline bool Pronta() { return Configurada() && FfmpegOk(); }

// Mensagem unica de "fonte externa desligada". ctx: 0 = geral, 1 = buscar,
// 2 = abrir link, 3 = salvar copia.
inline std::wstring MsgFalta(int ctx = 0) {
    const wchar_t* fim = ctx == 1 ? L"para buscar em fontes externas."
                       : ctx == 2 ? L"para abrir esse link."
                       : ctx == 3 ? L"para salvar uma cópia."
                                  : L"para ouvir de fontes externas.";
    return std::wstring(L"Configure uma CLI compatível em Configurações > FONTES EXTERNAS ") + fim;
}

// Comando base: SEMPRE comeca pelo caminho configurado. Vazio = desligada.
inline std::vector<std::wstring> Cmd() {
    std::lock_guard<std::mutex> lk(I().m);
    std::vector<std::wstring> a;
    if (I().e.cli.empty()) return a;
    a.push_back(I().e.cli);
    for (const wchar_t* x : { L"--ignore-config", L"--no-warnings", L"--encoding", L"utf-8", L"--socket-timeout", L"20" }) a.push_back(x);
    if (!I().e.jsPath.empty()) { a.push_back(L"--js-runtimes"); a.push_back(I().e.jsName + L":" + I().e.jsPath); }
    if (!I().e.ffmpeg.empty()) { a.push_back(L"--ffmpeg-location"); a.push_back(I().e.ffmpeg); }
    return a;
}

// Executa o comando da fonte externa. Trava de seguranca: so roda se o
// primeiro argumento for exatamente o caminho configurado -- assim nenhum
// caminho de codigo consegue executar outro programa por aqui.
inline CapResult Rodar(const std::vector<std::wstring>& args, int timeoutMs,
                       const std::atomic<bool>* cancel = nullptr,
                       std::function<void(const std::string&)> onLine = nullptr,
                       size_t keepMax = (size_t)96 * 1024 * 1024) {
    CapResult r;
    if (args.empty() || args[0] != Caminho() || args[0].empty()) { r.started = false; r.code = -1; return r; }
    return RunCapture(args, timeoutMs, cancel, onLine, keepMax);
}

// Mesma trava para o modo pipe (tocar sem passar pelo disco).
inline bool Abrir(Proc& p, const std::vector<std::wstring>& args, bool pipeOut, bool pipeErr, bool pipeIn = false) {
    if (args.empty() || args[0].empty() || args[0] != Caminho()) return false;
    return p.Start(args, pipeOut, pipeErr, pipeIn);
}

// Argumentos de "tocar sem gravar nada": manda o audio para a saida padrao.
inline std::vector<std::wstring> CmdTocar(const std::wstring& link) {
    auto a = Cmd();
    if (a.empty()) return a;
    for (const wchar_t* x : { L"--no-playlist", L"-q", L"-f", L"bestaudio/best", L"-o", L"-" }) a.push_back(x);
    a.push_back(L"--"); a.push_back(link);     // "--": o link nunca vira opcao
    return a;
}

}   // namespace fonte
