#include "core/EventManager.hpp"

namespace core {
EventManager& EventManager::instance() {
    static EventManager inst;
    return inst;
}

void EventManager::subscribe(const std::string& eventName, Callback cb) {
    listeners[eventName].push_back(std::move(cb));
}

void EventManager::publish(const std::string& eventName, const std::string& payload) {
    auto it = listeners.find(eventName);
    if (it != listeners.end()) {
        for (auto& cb : it->second) {
            cb(payload);
        }
    }
}
}
