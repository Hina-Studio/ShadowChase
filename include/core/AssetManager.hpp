#pragma once
#include <memory>
#include <string>
#include <unordered_map>

namespace core {
class Asset {
public:
    virtual ~Asset() = default;
};

class AssetManager {
public:
    static AssetManager& instance();

    template <typename T>
    std::shared_ptr<T> load(const std::string& id) {
        auto it = assets.find(id);
        if (it != assets.end()) {
            return std::dynamic_pointer_cast<T>(it->second);
        }
        auto asset = std::make_shared<T>();
        assets[id] = asset;
        return asset;
    }

    void unload(const std::string& id);
    bool exists(const std::string& id) const;
private:
    std::unordered_map<std::string, std::shared_ptr<Asset>> assets;
};
}
