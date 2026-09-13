#pragma once
#include <functional>
#include <unordered_map>
#include <string>
#include <vector>

namespace core {
class EventManager {
public:
    using Callback = std::function<void(const std::string&)>;

    static EventManager& instance();

    void subscribe(const std::string& eventName, Callback cb);
    void publish(const std::string& eventName, const std::string& payload = "");
private:
    std::unordered_map<std::string, std::vector<Callback>> listeners;
};
}
