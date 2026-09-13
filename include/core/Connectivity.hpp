#pragma once
#include <string>
#include <vector>

struct NetCandidate {
    std::string address;
    int port = 0;
    std::string tier;
};

class Connectivity {
public:
    void probe(const std::string& stunServer, int localPort);
    const std::vector<NetCandidate>& candidates() const { return cands; }
    std::string exportString() const;
    void importString(const std::string& text);
    std::string summary() const;

private:
    std::vector<NetCandidate> cands;
    std::string publicEndpoint;
};
