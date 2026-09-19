#pragma once
#include <string>
#include <vector>

struct NetConfig {
    std::string mode = "off";
    std::string address = "127.0.0.1";
    int port = 7777;
    std::vector<std::string> endpoints;
};

class Network {
public:
    bool start(const NetConfig& cfg);
    void shutdown();
    void poll();

    bool isHost() const { return hostMode; }
    bool isClient() const { return clientMode; }
    bool isConnected() const { return connected; }
    std::string status() const;

    void send(const std::string& payload);
    std::vector<std::string> takeReceived();

private:
    std::string mode = "off";
    bool hostMode = false;
    bool clientMode = false;
    bool connected = false;
    void* hostHandle = nullptr;
    void* peerHandle = nullptr;
    std::vector<void*> peers;
    std::vector<std::string> received;
};
