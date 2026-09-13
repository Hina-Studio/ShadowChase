#pragma once
#include <chrono>
#include <string>
#include <unordered_map>

namespace core {
class Profiler {
public:
    static Profiler& instance();

    void setEnabled(bool on) { enabled_ = on; }
    bool enabled() const { return enabled_; }
    void addSample(const std::string& name, double ms);
    std::string report() const;
    void reset();

    class Scope {
    public:
        explicit Scope(const char* name);
        ~Scope();

    private:
        const char* name_;
        std::chrono::steady_clock::time_point start_;
        bool active_;
    };

private:
    bool enabled_ = false;
    struct Stat {
        long long count = 0;
        double total = 0.0;
        double max = 0.0;
    };
    std::unordered_map<std::string, Stat> stats_;
};
}
