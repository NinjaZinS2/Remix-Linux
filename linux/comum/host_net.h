#pragma once
// Rede do Host (servidor HTTP embutido, docs/HOST.md): sockets bloqueantes com timeout,
// Windows (Winsock2) e POSIX. IPv4 sempre; IPv6 so quando a pessoa liga nas configuracoes.
//
// Filtro de origem (PeerAllowed): o servidor so conversa com loopback (o tunel Cloudflare
// chega por 127.0.0.1) e com a rede local (IPv4 privado; IPv6 link-local, ULA ou o mesmo
// /64 do PC). Se o PC tiver IP publico, ninguem da internet fala direto com a porta.
#include "platform.h"
#include <string>
#include <vector>
#include <array>
#include <cstring>
#include <cstdint>
#ifdef _WIN32
    // winsock2.h/ws2tcpip.h vem por platform.h (antes de windows.h)
    #include <iphlpapi.h>
    typedef SOCKET hsock_t;
    #define HSOCK_BAD INVALID_SOCKET
#else
    #include <sys/socket.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <ifaddrs.h>
    #include <net/if.h>
    #include <netdb.h>
    #include <poll.h>
    #include <errno.h>
    typedef int hsock_t;
    #define HSOCK_BAD (-1)
#endif

namespace hostnet {

inline bool Init() {
#ifdef _WIN32
    static bool done = false, ok = false;
    if (!done) { done = true; WSADATA w; ok = WSAStartup(MAKEWORD(2, 2), &w) == 0; }
    return ok;
#else
    return true;
#endif
}
inline void Close(hsock_t s) {
    if (s == HSOCK_BAD) return;
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}
// Acorda um accept()/poll() parado em outra thread (Linux precisa do shutdown antes do close).
inline void Wake(hsock_t s) {
    if (s == HSOCK_BAD) return;
#ifdef _WIN32
    shutdown(s, SD_BOTH);
#else
    shutdown(s, SHUT_RDWR);
#endif
}
// Fecha "na marra" (RST): o que ainda esta no buffer do sistema e descartado. Usado quando o PC tira a
// permissao ou desliga o Host, para o celular nao continuar recebendo megabytes ja enfileirados.
inline void Abort(hsock_t s) {
    if (s == HSOCK_BAD) return;
    struct linger l; l.l_onoff = 1; l.l_linger = 0;
    setsockopt(s, SOL_SOCKET, SO_LINGER, (const char*)&l, sizeof l);
}
inline void SetTimeout(hsock_t s, int ms) {
#ifdef _WIN32
    DWORD t = (DWORD)ms;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, (const char*)&t, sizeof t);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, (const char*)&t, sizeof t);
#else
    timeval tv; tv.tv_sec = ms / 1000; tv.tv_usec = (ms % 1000) * 1000;
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
#endif
}
// Socket sem heranca: processos filhos (cloudflared, yt-dlp, ffmpeg) NAO podem levar o socket
// junto, senao ao fechar o Remix eles seguram a porta e a proxima abertura falha.
inline hsock_t NewSocket(int family) {
#ifdef _WIN32
    return WSASocketW(family, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_NO_HANDLE_INHERIT | WSA_FLAG_OVERLAPPED);
#else
    return socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
#endif
}
// Escuta na porta (IPv4). lanOk=false -> so 127.0.0.1 (o tunel ainda funciona).
inline hsock_t Listen(int port, bool lanOk, std::string& err) {
    err.clear();
    if (!Init()) { err = "rede indisponivel"; return HSOCK_BAD; }
    hsock_t s = NewSocket(AF_INET);
    if (s == HSOCK_BAD) { err = "socket"; return HSOCK_BAD; }
#ifndef _WIN32
    int one = 1; setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof one);   // no Windows SO_REUSEADDR deixaria outro programa roubar a porta
#else
    int excl = 1; setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&excl, sizeof excl);
#endif
    sockaddr_in a; std::memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port);
    a.sin_addr.s_addr = lanOk ? htonl(INADDR_ANY) : htonl(INADDR_LOOPBACK);
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) { err = "porta " + std::to_string(port) + " em uso (ou sem permissao)"; Close(s); return HSOCK_BAD; }
    if (listen(s, 32) != 0) { err = "listen"; Close(s); return HSOCK_BAD; }
    return s;
}
// Escuta em IPv6 (so IPv6: o IPv4 fica no outro socket). Opcional.
inline hsock_t Listen6(int port, std::string& err) {
    err.clear();
    if (!Init()) { err = "rede indisponivel"; return HSOCK_BAD; }
    hsock_t s = NewSocket(AF_INET6);
    if (s == HSOCK_BAD) { err = "IPv6 indisponivel neste PC"; return HSOCK_BAD; }
    int one = 1;
    setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&one, sizeof one);
#ifndef _WIN32
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof one);
#else
    setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (const char*)&one, sizeof one);
#endif
    sockaddr_in6 a; std::memset(&a, 0, sizeof a);
    a.sin6_family = AF_INET6; a.sin6_port = htons((uint16_t)port); a.sin6_addr = in6addr_any;
    if (bind(s, (sockaddr*)&a, sizeof a) != 0) { err = "IPv6: porta " + std::to_string(port) + " em uso"; Close(s); return HSOCK_BAD; }
    if (listen(s, 32) != 0) { err = "IPv6: listen"; Close(s); return HSOCK_BAD; }
    return s;
}

// ---- endereco do outro lado ----------------------------------------------------------
struct Peer { int family = 0; std::array<uint8_t, 16> addr{}; std::string ip; };
inline bool IsPrivate4(const uint8_t* a) {
    return a[0] == 10 || (a[0] == 172 && (a[1] & 0xF0) == 16) || (a[0] == 192 && a[1] == 168) || (a[0] == 169 && a[1] == 254);
}
inline bool IsLoopback4(const uint8_t* a) { return a[0] == 127; }
// Prefixos /64 dos IPv6 globais deste PC (para aceitar so quem esta no mesmo segmento).
typedef std::array<uint8_t, 8> Prefix64;
inline bool PeerAllowed(const Peer& p, bool lanOk, bool lan6, const std::vector<Prefix64>& own) {
    if (p.family == AF_INET) {
        const uint8_t* a = p.addr.data();
        if (IsLoopback4(a)) return true;
        return lanOk && IsPrivate4(a);
    }
    if (p.family == AF_INET6) {
        const uint8_t* a = p.addr.data();
        static const uint8_t loop[16] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1 };
        if (std::memcmp(a, loop, 16) == 0) return true;
        if (!lan6) return false;
        if (a[0] == 0xFE && (a[1] & 0xC0) == 0x80) return true;   // fe80::/10 link-local
        if ((a[0] & 0xFE) == 0xFC) return true;                    // fc00::/7 ULA
        for (auto& pre : own) if (std::memcmp(a, pre.data(), 8) == 0) return true;   // mesmo /64 do PC
        return false;
    }
    return false;
}
// Chave de limite de taxa: IPv4 inteiro; IPv6 pelo /64 (quem troca de endereco dentro da rede nao escapa).
inline std::string RateKey(const Peer& p) {
    if (p.family != AF_INET6) return p.ip;
    char b[40]; snprintf(b, sizeof b, "%02x%02x:%02x%02x:%02x%02x:%02x%02x::/64", p.addr[0], p.addr[1], p.addr[2], p.addr[3], p.addr[4], p.addr[5], p.addr[6], p.addr[7]);
    return b;
}
inline hsock_t Accept(hsock_t ls, Peer& peer) {
    sockaddr_storage a; socklen_t n = sizeof a; std::memset(&a, 0, sizeof a);
#ifdef _WIN32
    hsock_t c = accept(ls, (sockaddr*)&a, &n);
    if (c == HSOCK_BAD) return c;
    SetHandleInformation((HANDLE)c, HANDLE_FLAG_INHERIT, 0);
#else
    hsock_t c = accept4(ls, (sockaddr*)&a, &n, SOCK_CLOEXEC);
    if (c == HSOCK_BAD) return c;
#endif
    char buf[64] = { 0 };
    peer.family = a.ss_family; peer.addr.fill(0);
    if (a.ss_family == AF_INET) {
        sockaddr_in* s4 = (sockaddr_in*)&a; std::memcpy(peer.addr.data(), &s4->sin_addr, 4);
        peer.ip = inet_ntop(AF_INET, &s4->sin_addr, buf, sizeof buf) ? buf : "?";
    } else if (a.ss_family == AF_INET6) {
        sockaddr_in6* s6 = (sockaddr_in6*)&a; std::memcpy(peer.addr.data(), &s6->sin6_addr, 16);
        peer.ip = inet_ntop(AF_INET6, &s6->sin6_addr, buf, sizeof buf) ? buf : "?";
    } else peer.ip = "?";
    int one = 1; setsockopt(c, IPPROTO_TCP, TCP_NODELAY, (const char*)&one, sizeof one);
    SetTimeout(c, 10000);
    return c;
}
// Espera conexao em um dos dois sockets. Devolve o socket pronto ou HSOCK_BAD (tempo esgotado / erro).
inline hsock_t WaitAccept(hsock_t a, hsock_t b, int timeoutMs) {
#ifdef _WIN32
    WSAPOLLFD f[2]; int n = 0;
    if (a != HSOCK_BAD) { f[n].fd = a; f[n].events = POLLRDNORM; f[n].revents = 0; n++; }
    if (b != HSOCK_BAD) { f[n].fd = b; f[n].events = POLLRDNORM; f[n].revents = 0; n++; }
    if (!n) return HSOCK_BAD;
    int r = WSAPoll(f, (ULONG)n, timeoutMs);
#else
    pollfd f[2]; int n = 0;
    if (a != HSOCK_BAD) { f[n].fd = a; f[n].events = POLLIN; f[n].revents = 0; n++; }
    if (b != HSOCK_BAD) { f[n].fd = b; f[n].events = POLLIN; f[n].revents = 0; n++; }
    if (!n) return HSOCK_BAD;
    int r = poll(f, (nfds_t)n, timeoutMs);
#endif
    if (r <= 0) return HSOCK_BAD;
    for (int i = 0; i < n; i++) if (f[i].revents) return (hsock_t)f[i].fd;
    return HSOCK_BAD;
}
inline long Recv(hsock_t s, char* b, size_t n) {
#ifdef _WIN32
    return (long)recv(s, b, (int)n, 0);
#else
    return (long)recv(s, b, n, 0);
#endif
}
inline bool SendAll(hsock_t s, const char* b, size_t n) {
    while (n) {
#ifdef _WIN32
        int r = send(s, b, (int)n, 0);
#else
        long r = send(s, b, n, MSG_NOSIGNAL);
#endif
        if (r <= 0) return false;
        b += r; n -= (size_t)r;
    }
    return true;
}
inline bool SendAll(hsock_t s, const std::string& d) { return SendAll(s, d.data(), d.size()); }
// A outra ponta desistiu? (sem bloquear; usado para cancelar a busca online quando o celular sai)
inline bool PeerGone(hsock_t s) {
#ifdef _WIN32
    WSAPOLLFD f; f.fd = s; f.events = POLLRDNORM; f.revents = 0;
    int r = WSAPoll(&f, 1, 0);
    if (r < 0) return true;
    if (r == 0) return false;
    if (f.revents & (POLLERR | POLLHUP | POLLNVAL)) return true;
    char b; int n = recv(s, &b, 1, MSG_PEEK); return n <= 0;
#else
    pollfd f; f.fd = s; f.events = POLLIN; f.revents = 0;
    int r = poll(&f, 1, 0);
    if (r < 0) return errno != EINTR;
    if (r == 0) return false;
    if (f.revents & (POLLERR | POLLHUP | POLLNVAL)) return true;
    char b; long n = (long)recv(s, &b, 1, MSG_PEEK | MSG_DONTWAIT);
    return n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR);
#endif
}

// ---- enderecos deste PC na rede local (so cabo e Wi-Fi; docker/VM/VPN ficam de fora) ----
inline bool VirtualName(std::string nm) {
    for (auto& c : nm) c = (char)tolower((unsigned char)c);
    static const char* skip[] = { "docker", "br-", "veth", "virbr", "tun", "tap", "vmnet", "vbox", "tailscale", "wg", "zt", "virtual", "vethernet", "vmware", "hyper-v", "wsl", "loopback" };
    for (const char* k : skip) if (nm.find(k) != std::string::npos) return true;
    return false;
}
struct LocalAddrs { std::vector<std::string> v4, v6; std::vector<Prefix64> pre6; };
inline LocalAddrs LocalAddresses() {
    LocalAddrs out;
    Init();
    auto add6 = [&](const uint8_t* a) {
        if (a[0] == 0xFE && (a[1] & 0xC0) == 0x80) return;       // link-local: navegador nao abre (precisa de %zona)
        if (a[0] == 0 && a[1] == 0) return;                       // ::, ::1, mapeados
        char b[64]; if (!inet_ntop(AF_INET6, a, b, sizeof b)) return;
        out.v6.push_back(b);
        if ((a[0] & 0xE0) == 0x20) { Prefix64 p; std::memcpy(p.data(), a, 8); out.pre6.push_back(p); }   // global: guarda o /64
    };
#ifdef _WIN32
    ULONG sz = 32 * 1024; std::vector<char> buf(sz);
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    ULONG rc = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, (IP_ADAPTER_ADDRESSES*)buf.data(), &sz);
    if (rc == ERROR_BUFFER_OVERFLOW) { buf.resize(sz); rc = GetAdaptersAddresses(AF_UNSPEC, flags, NULL, (IP_ADAPTER_ADDRESSES*)buf.data(), &sz); }
    if (rc != NO_ERROR) return out;
    for (IP_ADAPTER_ADDRESSES* ad = (IP_ADAPTER_ADDRESSES*)buf.data(); ad; ad = ad->Next) {
        if (ad->OperStatus != IfOperStatusUp) continue;
        if (ad->IfType != IF_TYPE_ETHERNET_CSMACD && ad->IfType != IF_TYPE_IEEE80211) continue;   // so cabo e Wi-Fi
        { std::wstring fn = ad->FriendlyName ? ad->FriendlyName : L""; std::string n8; for (wchar_t c : fn) n8.push_back(c < 128 ? (char)c : '_'); if (VirtualName(n8)) continue; }
        for (IP_ADAPTER_UNICAST_ADDRESS* u = ad->FirstUnicastAddress; u; u = u->Next) {
            if (u->Address.lpSockaddr->sa_family == AF_INET) {
                char b[64]; sockaddr_in* si = (sockaddr_in*)u->Address.lpSockaddr;
                const uint8_t* a = (const uint8_t*)&si->sin_addr;
                if (IsLoopback4(a) || (a[0] == 169 && a[1] == 254)) continue;
                if (inet_ntop(AF_INET, &si->sin_addr, b, sizeof b)) out.v4.push_back(b);
            } else if (u->Address.lpSockaddr->sa_family == AF_INET6) {
                if (u->SuffixOrigin == IpSuffixOriginRandom && u->PrefixOrigin == IpPrefixOriginRouterAdvertisement) { /* temporario (privacidade): ainda serve, mas muda */ }
                add6((const uint8_t*)&((sockaddr_in6*)u->Address.lpSockaddr)->sin6_addr);
            }
        }
    }
#else
    ifaddrs* ifa = nullptr;
    if (getifaddrs(&ifa) != 0) return out;
    for (ifaddrs* p = ifa; p; p = p->ifa_next) {
        if (!p->ifa_addr || !(p->ifa_flags & IFF_UP) || (p->ifa_flags & IFF_LOOPBACK)) continue;
        if (VirtualName(p->ifa_name ? p->ifa_name : "")) continue;
        if (p->ifa_addr->sa_family == AF_INET) {
            char b[64]; sockaddr_in* si = (sockaddr_in*)p->ifa_addr; const uint8_t* a = (const uint8_t*)&si->sin_addr;
            if (IsLoopback4(a) || (a[0] == 169 && a[1] == 254)) continue;
            if (inet_ntop(AF_INET, &si->sin_addr, b, sizeof b)) out.v4.push_back(b);
        } else if (p->ifa_addr->sa_family == AF_INET6) {
            add6((const uint8_t*)&((sockaddr_in6*)p->ifa_addr)->sin6_addr);
        }
    }
    freeifaddrs(ifa);
#endif
    return out;
}
inline std::vector<std::string> LanIPv4() { return LocalAddresses().v4; }
inline std::string HostName() {
    Init();
    char b[256] = { 0 };
    if (gethostname(b, sizeof b - 1) != 0 || !b[0]) return "Remix";
    return b;
}

} // namespace hostnet
