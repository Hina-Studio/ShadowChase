#pragma once
#include <string>
#include <vector>

struct LanServer {
    std::string name;
    std::string address;
    int port = 0;
    double lastSeen = 0.0;
};

class LanDiscovery {
public:
    void startHostBeacon(const std::string& name, int gamePort, int beaconPort);
    void updateBeacon(const std::string& name, int gamePort);
    void startClient(int beaconPort);
    void stop();
    void poll();
    std::vector<LanServer> servers() const { return found; }
    bool hosting() const { return hostSocket != nullptr; }

private:
    void* hostSocket = nullptr;
    void* clientSocket = nullptr;
    int beaconPort_ = 7778;
    std::string hostName_;
    int gamePort_ = 7777;
    double lastBeacon_ = 0.0;
    std::vector<LanServer> found;
};
