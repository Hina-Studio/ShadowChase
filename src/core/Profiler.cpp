#include "core/Profiler.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace core {
Profiler& Profiler::instance() {
    static Profiler inst;
    return inst;
}

Profiler::Scope::Scope(const char* name) : name_(name), active_(false) {
    if (Profiler::instance().enabled()) {
        start_ = std::chrono::steady_clock::now();
        active_ = true;
    }
}

Profiler::Scope::~Scope() {
    if (!active_) return;
    auto end = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(end - start_).count();
    Profiler::instance().addSample(name_, ms);
}

void Profiler::addSample(const std::string& name, double ms) {
    Stat& s = stats_[name];
    ++s.count;
    s.total += ms;
    if (ms > s.max) s.max = ms;
}

std::string Profiler::report() const {
    std::vector<std::pair<std::string, Stat>> sorted(stats_.begin(), stats_.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second.total > b.second.total; });

    std::string out;
    char line[160];
    for (const auto& kv : sorted) {
        double avg = kv.second.count > 0 ? kv.second.total / static_cast<double>(kv.second.count)
                                         : 0.0;
        std::snprintf(line, sizeof(line), "  %-14s count=%lld total=%.1fms avg=%.3fms max=%.3fms\n",
                      kv.first.c_str(), kv.second.count, kv.second.total, avg, kv.second.max);
        out += line;
    }
    if (out.empty()) out = "  (no samples)\n";
    return out;
}

void Profiler::reset() {
    stats_.clear();
}
}
