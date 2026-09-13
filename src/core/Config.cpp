#include "core/Config.hpp"

#include <fstream>
#include <map>
#include <sstream>
namespace core {
Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;
    std::string line;
    while (std::getline(file, line)) {
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);
        data[key] = value;
    }
}

std::string Config::get(const std::string& key, const std::string& def) const {
    auto it = data.find(key);
    if (it != data.end()) return it->second;
    return def;
}

void Config::set(const std::string& key, const std::string& value) {
    data[key] = value;
}

bool Config::save(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    std::map<std::string, std::string> sorted(data.begin(), data.end());
    for (const auto& kv : sorted) {
        file << kv.first << "=" << kv.second << "\n";
    }
    return true;
}
}
