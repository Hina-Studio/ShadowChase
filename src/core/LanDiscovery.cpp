#include "core/LanDiscovery.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <sstream>

#include "core/Logger.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
using Sock = SOCKET;
static const Sock kInvalidSock = INVALID_SOCKET;
#else
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using Sock = int;
static const Sock kInvalidSock = -1;
#endif

namespace {
Sock toSock(void* p) {
    return static_cast<Sock>(reinterpret_cast<size_t>(p));
}

void* fromSock(Sock s) {
    return reinterpret_cast<void*>(static_cast<size_t>(s));
}

void closeSock(Sock s) {
#ifdef _WIN32
    closesocket(s);
#else
    close(s);
#endif
}

double nowSeconds() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
}

void setNonBlocking(Sock s) {
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(s, FIONBIO, &mode);
#else
    int flags = fcntl(s, F_GETFL, 0);
    fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif
}
}

void LanDiscovery::startHostBeacon(const std::string& name, int gamePort, int beaconPort) {
    stop();
    hostName_ = name;
    gamePort_ = gamePort;
    beaconPort_ = beaconPort;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    Sock s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kInvalidSock) {
        core::Logger::warn("LAN beacon socket failed");
        return;
    }
    int broadcast = 1;
    setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast),
               sizeof(broadcast));
    hostSocket = fromSock(s);
    lastBeacon_ = 0.0;
    core::Logger::info("LAN beacon active on port " + std::to_string(beaconPort_));
}

void LanDiscovery::updateBeacon(const std::string& name, int gamePort) {
    hostName_ = name;
    gamePort_ = gamePort;
}

void LanDiscovery::startClient(int beaconPort) {
    stop();
    beaconPort_ = beaconPort;

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    Sock s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == kInvalidSock) {
        core::Logger::warn("LAN discovery socket failed");
        return;
    }
    int reuse = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(static_cast<unsigned short>(beaconPort_));
    if (bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        core::Logger::warn("LAN discovery bind failed on port " + std::to_string(beaconPort_));
        closeSock(s);
        return;
    }
    setNonBlocking(s);
    clientSocket = fromSock(s);
    found.clear();
    core::Logger::info("LAN discovery listening on port " + std::to_string(beaconPort_));
}

void LanDiscovery::stop() {
    if (hostSocket) {
        closeSock(toSock(hostSocket));
        hostSocket = nullptr;
    }
    if (clientSocket) {
        closeSock(toSock(clientSocket));
        clientSocket = nullptr;
    }
}

void LanDiscovery::poll() {
    if (hostSocket) {
        double now = nowSeconds();
        if (now - lastBeacon_ >= 1.0) {
            lastBeacon_ = now;
            std::ostringstream os;
            os << "SLASHCO|" << hostName_ << "|" << gamePort_;
            std::string msg = os.str();

            Sock s = toSock(hostSocket);
            sockaddr_in dst{};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(static_cast<unsigned short>(beaconPort_));
            dst.sin_addr.s_addr = INADDR_BROADCAST;
            sendto(s, msg.c_str(), static_cast<int>(msg.size()), 0,
                   reinterpret_cast<sockaddr*>(&dst), sizeof(dst));
        }
    }

    if (clientSocket) {
        Sock s = toSock(clientSocket);
        char buf[512];

        while (true) {
            sockaddr_in from{};
#ifdef _WIN32
            int fromLen = sizeof(from);
#else
            socklen_t fromLen = sizeof(from);
#endif
            int n = static_cast<int>(recvfrom(s, buf, sizeof(buf) - 1, 0,
                                              reinterpret_cast<sockaddr*>(&from), &fromLen));
            if (n <= 0) break;
            buf[n] = '\0';

            std::stringstream ss(buf);
            std::string tag;
            std::string name;
            std::string portStr;
            if (!std::getline(ss, tag, '|') || tag != "SLASHCO") continue;
            if (!std::getline(ss, name, '|')) continue;
            if (!std::getline(ss, portStr)) continue;

            char ip[64] = {};
            inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));

            bool updated = false;
            for (auto& srv : found) {
                if (srv.address == ip && srv.port == std::atoi(portStr.c_str())) {
                    srv.name = name;
                    srv.lastSeen = nowSeconds();
                    updated = true;
                    break;
                }
            }
            if (!updated) {
                LanServer srv;
                srv.name = name;
                srv.address = ip;
                srv.port = std::atoi(portStr.c_str());
                srv.lastSeen = nowSeconds();
                found.push_back(srv);
                core::Logger::info("LAN server found: " + name + " @ " + ip + ":" + portStr);
            }
        }

        double now = nowSeconds();
        found.erase(std::remove_if(found.begin(), found.end(),
                                   [now](const LanServer& srv) { return now - srv.lastSeen > 5.0; }),
                    found.end());
    }
}
