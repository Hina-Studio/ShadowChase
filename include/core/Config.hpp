#pragma once
#include <unordered_map>
#include <string>

namespace core {
class Config {
public:
    static Config& instance();
    void load(const std::string& path);
    std::string get(const std::string& key, const std::string& def = "") const;
    void set(const std::string& key, const std::string& value);
    bool save(const std::string& path) const;
private:
    std::unordered_map<std::string, std::string> data;
};
}
