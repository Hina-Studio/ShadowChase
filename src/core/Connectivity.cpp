#include "core/Connectivity.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {
constexpr unsigned short kStunBindingRequest = 0x0001;
constexpr unsigned int kStunMagicCookie = 0x2112A442u;

std::string buildStunRequest() {
    std::string req(20, '\0');
    req[0] = 0x00;
    req[1] = 0x01;
    for (int i = 2; i < 4; ++i) req[i] = 0x00;
    req[4] = 0x21;
    req[5] = 0x12;
    req[6] = 0xA4;
    req[7] = 0x42;
    for (int i = 8; i < 20; ++i) {
        req[i] = static_cast<char>(std::rand() & 0xFF);
    }
    return req;
}

bool parseStunResponse(const char* data, int len, std::string& out) {
    if (len < 20) return false;
    const unsigned char* p = reinterpret_cast<const unsigned char*>(data);
    unsigned short type = static_cast<unsigned short>((p[0] << 8) | p[1]);
    unsigned short bodyLen = static_cast<unsigned short>((p[2] << 8) | p[3]);
    if (type != 0x0101 || bodyLen + 20 > len) return false;

    int offset = 20;
    while (offset + 4 <= 20 + bodyLen) {
        unsigned short attr = static_cast<unsigned short>((p[offset] << 8) | p[offset + 1]);
        unsigned short alen = static_cast<unsigned short>((p[offset + 2] << 8) | p[offset + 3]);
        const unsigned char* val = p + offset + 4;
        if (offset + 4 + alen > len) break;

        if (attr == 0x0020 || attr == 0x0001) {
            bool xorAddr = (attr == 0x0020);
            if (alen >= 8 && val[1] == 0x01) {
                unsigned short port = static_cast<unsigned short>((val[2] << 8) | val[3]);
                unsigned int ip = (static_cast<unsigned int>(val[4]) << 24) |
                                  (static_cast<unsigned int>(val[5]) << 16) |
                                  (static_cast<unsigned int>(val[6]) << 8) |
                                  static_cast<unsigned int>(val[7]);
                if (xorAddr) {
                    port = static_cast<unsigned short>(port ^ 0x2112);
                    ip ^= kStunMagicCookie;
                }
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u:%u", (ip >> 24) & 0xFF,
                              (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
                out = buf;
                return true;
            }
        }
        offset += 4 + ((alen + 3) & ~3);
    }
    return false;
}
}

void Connectivity::probe(const std::string& stunServer, int localPort) {
    cands.clear();
    publicEndpoint.clear();

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    char host[256] = {};
    if (gethostname(host, sizeof(host) - 1) == 0) {
        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(host, nullptr, &hints, &res) == 0) {
            for (addrinfo* it = res; it != nullptr; it = it->ai_next) {
                char ip[64] = {};
                sockaddr_in* in = reinterpret_cast<sockaddr_in*>(it->ai_addr);
                if (inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip))) {
                    NetCandidate c;
                    c.address = ip;
                    c.port = localPort;
                    c.tier = "tier1-lan";
                    cands.push_back(c);
                }
            }
            freeaddrinfo(res);
        }
    }

    if (!stunServer.empty() && stunServer != "off") {
        std::string server = stunServer;
        std::string portStr = "3478";
        auto colon = server.rfind(':');
        if (colon != std::string::npos) {
            portStr = server.substr(colon + 1);
            server = server.substr(0, colon);
        }

        addrinfo hints{};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* res = nullptr;
        if (getaddrinfo(server.c_str(), portStr.c_str(), &hints, &res) == 0) {
#ifdef _WIN32
            SOCKET sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            bool valid = sock != INVALID_SOCKET;
#else
            int sock = socket(AF_INET, SOCK_DGRAM, 0);
            bool valid = sock >= 0;
#endif
            if (valid) {
#ifdef _WIN32
                DWORD timeout = 700;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout),
                           sizeof(timeout));
#else
                timeval tv{};
                tv.tv_sec = 0;
                tv.tv_usec = 700000;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif
                std::string req = buildStunRequest();
                sendto(sock, req.data(), static_cast<int>(req.size()), 0, res->ai_addr,
                       static_cast<int>(res->ai_addrlen));
                char buf[1024];
                sockaddr_storage from{};
                socklen_t fromLen = sizeof(from);
                int n = static_cast<int>(recvfrom(sock, buf, sizeof(buf), 0,
                                                  reinterpret_cast<sockaddr*>(&from), &fromLen));
                if (n > 0 && parseStunResponse(buf, n, publicEndpoint)) {
                    NetCandidate c;
                    auto cpos = publicEndpoint.rfind(':');
                    c.address = publicEndpoint.substr(0, cpos);
                    c.port = localPort;
                    c.tier = "tier4-stun";
                    cands.push_back(c);
                }
#ifdef _WIN32
                closesocket(sock);
#else
                close(sock);
#endif
            }
            freeaddrinfo(res);
        }
    }

#ifdef _WIN32
    WSACleanup();
#endif
}

std::string Connectivity::exportString() const {
    std::ostringstream os;
    for (size_t i = 0; i < cands.size(); ++i) {
        if (i > 0) os << ";";
        os << cands[i].tier << "|" << cands[i].address << ":" << cands[i].port;
    }
    return os.str();
}

void Connectivity::importString(const std::string& text) {
    cands.clear();
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ';')) {
        if (item.empty()) continue;
        NetCandidate c;
        auto bar = item.find('|');
        std::string endpoint = item;
        if (bar != std::string::npos) {
            c.tier = item.substr(0, bar);
            endpoint = item.substr(bar + 1);
        } else {
            c.tier = "imported";
        }
        auto colon = endpoint.rfind(':');
        if (colon == std::string::npos) continue;
        c.address = endpoint.substr(0, colon);
        c.port = std::atoi(endpoint.substr(colon + 1).c_str());
        if (c.port > 0) cands.push_back(c);
    }
}

std::string Connectivity::summary() const {
    if (cands.empty()) return "no candidates";
    std::ostringstream os;
    for (size_t i = 0; i < cands.size(); ++i) {
        if (i > 0) os << "  ";
        os << cands[i].tier << "=" << cands[i].address << ":" << cands[i].port;
    }
    if (!publicEndpoint.empty()) os << "  (public " << publicEndpoint << ")";
    return os.str();
}
