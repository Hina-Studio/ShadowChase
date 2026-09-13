#include "core/AssetManager.hpp"

namespace core {
AssetManager& AssetManager::instance() {
    static AssetManager inst;
    return inst;
}

void AssetManager::unload(const std::string& id) {
    assets.erase(id);
}

bool AssetManager::exists(const std::string& id) const {
    return assets.find(id) != assets.end();
}
}
