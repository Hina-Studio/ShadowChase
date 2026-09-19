#include "core/Network.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

#include "core/Logger.hpp"

#ifdef SLASHCO_ENET
#include <cstring>
#include <enet/enet.h>
#endif

bool Network::start(const NetConfig& cfg) {
    mode = cfg.mode;
    for (auto& c : mode) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (mode.empty() || mode == "off") {
        hostMode = false;
        clientMode = false;
        connected = false;
        return true;
    }

#ifdef SLASHCO_ENET
    if (enet_initialize() != 0) {
        core::Logger::error("ENet initialization failed");
        return false;
    }

    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = static_cast<enet_uint16>(cfg.port);

    if (mode == "host") {
        hostMode = true;
        clientMode = false;
        ENetHost* server = enet_host_create(&address, 8, 2, 0, 0);
        if (!server) {
            core::Logger::error("ENet host creation failed");
            return false;
        }
        hostHandle = server;
        core::Logger::info("ENet host listening on port " + std::to_string(cfg.port));
        return true;
    }

    if (mode == "client") {
        hostMode = false;
        clientMode = true;
        peers.clear();

        std::vector<std::string> eps = cfg.endpoints;
        if (eps.empty()) {
            eps.push_back(cfg.address + ":" + std::to_string(cfg.port));
        }

        ENetHost* client = enet_host_create(nullptr, eps.size(), 2, 0, 0);
        if (!client) {
            core::Logger::error("ENet client host creation failed");
            return false;
        }
        hostHandle = client;

        for (const auto& ep : eps) {
            std::string hostPart = ep;
            int port = cfg.port;
            auto colon = ep.rfind(':');
            if (colon != std::string::npos) {
                port = std::atoi(ep.substr(colon + 1).c_str());
                hostPart = ep.substr(0, colon);
            }
            ENetAddress addr;
            addr.host = ENET_HOST_ANY;
            addr.port = static_cast<enet_uint16>(port);
            enet_address_set_host(&addr, hostPart.c_str());
            ENetPeer* p = enet_host_connect(client, &addr, 2, 0);
            if (p) peers.push_back(p);
        }
        if (peers.empty()) {
            core::Logger::error("ENet connect failed");
            return false;
        }
        peerHandle = peers.front();
        core::Logger::info("ENet client attempting " + std::to_string(peers.size()) +
                           " candidate endpoint(s)");
        return true;
    }

    core::Logger::warn("Unknown net mode: " + mode);
    return false;
#else
    core::Logger::warn("Built without ENet (SLASHCO_ENABLE_ENET=OFF), network disabled");
    hostMode = false;
    clientMode = false;
    connected = false;
    return true;
#endif
}

void Network::shutdown() {
#ifdef SLASHCO_ENET
    if (hostHandle) {
        enet_host_destroy(static_cast<ENetHost*>(hostHandle));
        hostHandle = nullptr;
        peerHandle = nullptr;
        enet_deinitialize();
    }
#endif
    hostMode = false;
    clientMode = false;
    connected = false;
    peers.clear();
}

void Network::poll() {
#ifdef SLASHCO_ENET
    if (!hostHandle) return;

    ENetEvent event;
    while (enet_host_service(static_cast<ENetHost*>(hostHandle), &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                connected = true;
                peerHandle = event.peer;
                if (clientMode) {
                    for (void* p : peers) {
                        if (p != event.peer) {
                            enet_peer_disconnect(static_cast<ENetPeer*>(p), 0);
                        }
                    }
                    core::Logger::info("Connected to host");
                } else {
                    core::Logger::info("Peer connected");
                }
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                connected = false;
                peerHandle = nullptr;
                core::Logger::info("Peer disconnected");
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                std::string payload(reinterpret_cast<char*>(event.packet->data), event.packet->dataLength);
                received.push_back(std::move(payload));
                enet_packet_destroy(event.packet);
                break;
            }
            default:
                break;
        }
    }
#endif
}

std::string Network::status() const {
    if (mode.empty() || mode == "off") return "off";
    std::string s = mode;
    s += connected ? " connected" : " waiting";
    return s;
}

void Network::send(const std::string& payload) {
#ifdef SLASHCO_ENET
    if (!hostHandle || !peerHandle) return;
    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(static_cast<ENetPeer*>(peerHandle), 0, packet);
    enet_host_flush(static_cast<ENetHost*>(hostHandle));
#else
    (void)payload;
#endif
}

std::vector<std::string> Network::takeReceived() {
    std::vector<std::string> out;
    out.swap(received);
    return out;
}
