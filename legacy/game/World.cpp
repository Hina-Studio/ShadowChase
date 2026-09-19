#include "game/World.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <sstream>

#include "core/Random.hpp"

namespace game {
namespace {
constexpr double kRescueRange = 3.5;
constexpr double kRescueTime = 3.0;
constexpr double kRepairRange = 3.5;
constexpr double kAttackRange = 2.2;
constexpr double kHungerThreshold = 50;
constexpr double kWorldBound = 11.0;
}

int findCharmIndex(const std::vector<FoodItem>& items) {
    for (size_t i = 0; i < items.size(); ++i) {
        if (isMapCharm(items[i])) return static_cast<int>(i);
    }
    return -1;
}

namespace {
struct MapDef {
    std::vector<Vec3> gens;
    std::vector<Vec3> exits;
    Vec3 vault;
    Vec3 switchPos;
};

void buildMapBlocks(const std::string& name, game::NavGrid& nav) {
    nav.clear();
    if (name == "Farmyard") {
        nav.addRect(3, -8, 3, -3);
        nav.addRect(-3, 3, -3, 8);
        nav.addRect(-8, 4, -4, 5);
        nav.addRect(5, 4, 8, 5);
    } else if (name == "HighSchool") {
        nav.addRect(-9, -3, -5, -3);
        nav.addRect(1, -3, 6, -3);
        nav.addRect(-6, 3, 0, 3);
        nav.addRect(4, 3, 9, 3);
        nav.addRect(-9, 0, -8, 0);
    } else if (name == "Hospital") {
        nav.addRect(-2, -2, 1, 1);
        nav.addRect(-8, -8, -6, -6);
        nav.addRect(6, 6, 8, 8);
    } else if (name == "Research") {
        nav.addRect(-6, -6, -6, -2);
        nav.addRect(-2, -2, -2, 2);
        nav.addRect(2, 2, 2, 6);
        nav.addRect(6, -6, 6, -2);
        nav.addRect(-6, 2, -2, 2);
        nav.addRect(2, -2, 6, -2);
    } else if (name == "Mansion") {
        nav.addRect(-8, -8, -1, -5);
        nav.addRect(1, -8, 8, -5);
        nav.addRect(-8, 5, -1, 8);
        nav.addRect(1, 5, 8, 8);
        nav.addRect(-1, -4, -1, -1);
        nav.addRect(1, 1, 1, 4);
    } else if (name == "Subway") {
        for (int i = -6; i <= 6; i += 3) {
            nav.addBlock(i, -6);
            nav.addBlock(i, 6);
        }
        nav.addRect(-6, -1, -2, -1);
        nav.addRect(2, 1, 6, 1);
    } else if (name == "Sewer") {
        nav.addRect(-9, -4, -4, -4);
        nav.addRect(-1, -4, 4, -4);
        nav.addRect(-9, 4, -3, 4);
        nav.addRect(1, 4, 9, 4);
        nav.addRect(-6, -8, -6, -6);
        nav.addRect(6, 6, 6, 8);
    } else {
        for (int sx = -1; sx <= 1; sx += 2) {
            for (int sz = -1; sz <= 1; sz += 2) {
                nav.addBlock(sx * 2, sz * 2);
                nav.addBlock(sx * 3, sz * 2);
                nav.addBlock(sx * 2, sz * 3);
                nav.addBlock(sx * 3, sz * 3);
            }
        }
    }
}

MapDef mapDefFor(const std::string& name) {
    MapDef m;
    if (name == "Farmyard") {
        m.gens = {{-6, 0, 0}, {6, 0, 0}, {0, 0, -6}, {0, 0, 6}, {-7, 0, -6}, {7, 0, 6}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {8, 0, -8};
        m.switchPos = {0, 0, 0};
    } else if (name == "HighSchool") {
        m.gens = {{-7, 0, -6}, {-7, 0, 6}, {0, 0, -6}, {0, 0, 6}, {7, 0, -6}, {7, 0, 6}};
        m.exits = {{0, 0, 10.5}, {0, 0, -10.5}};
        m.vault = {-8, 0, -8};
        m.switchPos = {0, 0, 0};
    } else if (name == "Hospital") {
        m.gens = {{6, 0, 0}, {-6, 0, 0}, {0, 0, 6}, {0, 0, -6}, {6, 0, -6}, {-6, 0, 6}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {8, 0, -2};
        m.switchPos = {4, 0, 2};
    } else if (name == "Research") {
        m.gens = {{-4, 0, 4}, {4, 0, 4}, {4, 0, -4}, {-4, 0, -4}, {0, 0, 8}, {0, 0, -8}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {-8, 0, -8};
        m.switchPos = {0, 0, 0};
    } else if (name == "Mansion") {
        m.gens = {{-5, 0, 0}, {5, 0, 0}, {0, 0, -6}, {0, 0, 6}, {-7, 0, 0}, {7, 0, 0}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {0, 0, 0};
        m.switchPos = {0, 0, -3};
    } else if (name == "Subway") {
        m.gens = {{-8, 0, 0}, {8, 0, 0}, {0, 0, -4}, {0, 0, 4}, {-5, 0, 3}, {5, 0, -3}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {8, 0, 8};
        m.switchPos = {0, 0, 0};
    } else if (name == "Sewer") {
        m.gens = {{-8, 0, 0}, {8, 0, 0}, {0, 0, -8}, {0, 0, 8}, {-5, 0, -5}, {5, 0, 5}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {-8, 0, 8};
        m.switchPos = {0, 0, 0};
    } else {
        m.gens = {{5, 0, 3}, {-5, 0, 3}, {-5, 0, -3}, {5, 0, -3}, {0, 0, 6}, {0, 0, -6}};
        m.exits = {{10.5, 0, 0}, {-10.5, 0, 0}};
        m.vault = {-8, 0, 7};
        m.switchPos = {0, 0, -4};
    }
    return m;
}
}

void bumpKill(game::MatchStats& st, const std::string& who) {
    st.killsByKiller[who] += 1;
    int best = -1;
    for (const auto& kv : st.killsByKiller) {
        if (kv.second > best) {
            best = kv.second;
            st.topKiller = kv.first;
        }
    }
}

int World::generatorCountFor(int players) const {
    if (players <= 2) return 1;
    if (players <= 4) return 2;
    if (players <= 8) return 3;
    if (players <= 12) return 4;
    if (players <= 16) return 5;
    return 6;
}

bool World::allGeneratorsDone() const {
    return std::all_of(generatorList.begin(), generatorList.end(),
                       [](const Generator& g) { return g.activated; });
}

bool World::canOpenDoors() const {
    if (mode_ == "Blackout") {
        return allGeneratorsDone() && powerSwitchOn_;
    }
    return allGeneratorsDone();
}

bool World::wardenNear(const Vec3& p) const {
    for (const auto& k : killerList) {
        if (k.kind() != KillerAI::Kind::Warden) continue;
        if (k.position().distance(p) <= 8.0) return true;
    }
    return false;
}

std::vector<std::string> World::takeEvents() {
    std::vector<std::string> out;
    out.swap(pendingEvents);
    return out;
}

std::string World::serializeSnapshot() const {
    std::ostringstream os;
    os << "S|" << matchClock << "|" << killerList.size() << ";";
    for (const auto& k : killerList) {
        os << k.position().x << "," << k.position().z << "," << static_cast<int>(k.state()) << ","
           << k.anger() << ";";
    }
    os << "|" << survivorList.size() << ";";
    for (const auto& s : survivorList) {
        os << s.id << "," << s.position.x << "," << s.position.z << "," << s.health.hp() << ","
           << s.downs << "," << (s.downed ? 1 : 0) << "," << (s.eliminated ? 1 : 0) << ","
           << (s.escaped ? 1 : 0) << "," << s.coins << ";";
    }
    os << "|" << generatorList.size() << ";";
    for (const auto& g : generatorList) {
        os << g.id << "," << g.work << "," << (g.activated ? 1 : 0) << ";";
    }
    os << "|" << (escapeOpen ? 1 : 0) << "|" << resultText;
    return os.str();
}

void World::applySnapshot(const std::string& data) {
    std::istringstream is(data);
    std::string sec;
    if (!std::getline(is, sec, '|') || sec != "S") return;
    if (!std::getline(is, sec, '|')) return;
    matchClock = std::stod(sec);

    if (!std::getline(is, sec, '|')) return;
    {
        std::stringstream ss(sec);
        std::string countStr;
        std::getline(ss, countStr, ';');
        int n = std::atoi(countStr.c_str());
        for (int i = 0; i < n && i < static_cast<int>(killerList.size()); ++i) {
            std::string item;
            if (!std::getline(ss, item, ';') || item.empty()) break;
            std::stringstream fs(item);
            std::string tok;
            double x = 0, z = 0;
            int stateId = 0, ang = 0;
            std::getline(fs, tok, ','); x = std::stod(tok);
            std::getline(fs, tok, ','); z = std::stod(tok);
            std::getline(fs, tok, ','); stateId = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); ang = std::atoi(tok.c_str());
            killerList[static_cast<size_t>(i)].applyNetState(x, z, stateId, ang);
        }
    }

    if (!std::getline(is, sec, '|')) return;
    {
        std::stringstream ss(sec);
        std::string countStr;
        std::getline(ss, countStr, ';');
        int n = std::atoi(countStr.c_str());
        for (int i = 0; i < n; ++i) {
            std::string item;
            if (!std::getline(ss, item, ';') || item.empty()) break;
            std::stringstream fs(item);
            std::string tok;
            int id = 0, hp = 100, downs = 0, downed = 0, elim = 0, esc = 0, coins = 0;
            double x = 0, z = 0;
            std::getline(fs, tok, ','); id = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); x = std::stod(tok);
            std::getline(fs, tok, ','); z = std::stod(tok);
            std::getline(fs, tok, ','); hp = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); downs = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); downed = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); elim = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); esc = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); coins = std::atoi(tok.c_str());
            for (auto& s : survivorList) {
                if (s.id == id) {
                    if (!s.netActive) {
                        s.position = Vec3{x, 0.0, z};
                        s.netActive = true;
                    }
                    s.netPos = Vec3{x, 0.0, z};
                    s.health.setHp(hp);
                    s.downs = downs;
                    s.downed = downed != 0;
                    s.eliminated = elim != 0;
                    s.escaped = esc != 0;
                    s.coins = coins;
                    break;
                }
            }
        }
    }

    if (!std::getline(is, sec, '|')) return;
    {
        std::stringstream ss(sec);
        std::string countStr;
        std::getline(ss, countStr, ';');
        int n = std::atoi(countStr.c_str());
        for (int i = 0; i < n; ++i) {
            std::string item;
            if (!std::getline(ss, item, ';') || item.empty()) break;
            std::stringstream fs(item);
            std::string tok;
            int id = 0, act = 0;
            double work = 0;
            std::getline(fs, tok, ','); id = std::atoi(tok.c_str());
            std::getline(fs, tok, ','); work = std::stod(tok);
            std::getline(fs, tok, ','); act = std::atoi(tok.c_str());
            for (auto& g : generatorList) {
                if (g.id == id) {
                    g.work = work;
                    g.activated = act != 0;
                    break;
                }
            }
        }
    }

    if (!std::getline(is, sec, '|')) return;
    escapeOpen = std::atoi(sec.c_str()) != 0;
    for (auto& e : exitList) e.open = escapeOpen;
    std::getline(is, resultText);
}

const Survivor* World::localSurvivor() const {
    if (localPlayer < 0) return nullptr;
    for (const auto& s : survivorList) {
        if (s.id == localPlayer) return &s;
    }
    return nullptr;
}

void World::useLocalItem(int slot) {
    Survivor* s = nullptr;
    for (auto& x : survivorList) {
        if (x.id == localPlayer) {
            s = &x;
            break;
        }
    }
    if (!s || s->eliminated || s->downed || s->escaped) return;
    if (slot < 0 || slot >= static_cast<int>(s->items.size())) return;

    if (isMapCharm(s->items[slot])) {
        pendingEvents.push_back("MapCharm is auto-saved when you would die (cannot be eaten)");
        return;
    }
    FoodItem f = s->items[slot];
    s->items.erase(s->items.begin() + slot);
    s->applyFood(f);
    stats_.itemsUsed += 1;
    pendingEvents.push_back("Survivor#" + std::to_string(s->id) + " used item " + f.name +
                            " (+" + std::to_string(f.heal) + "hp)");
}

bool World::buyCharm(int survivorId, bool enhanced) {
    Survivor* s = nullptr;
    for (auto& x : survivorList) {
        if (x.id == survivorId) {
            s = &x;
            break;
        }
    }
    if (!s || s->eliminated || s->downed || s->escaped) return false;

    int price = enhanced ? 1000 : 100;
    if (enhanced && enhancedBought) {
        pendingEvents.push_back("GoldCharm already purchased this match (global once)");
        return false;
    }
    if (!enhanced && s->charmBought) {
        pendingEvents.push_back("Survivor#" + std::to_string(s->id) + " already owns a MapCharm");
        return false;
    }
    if (s->coins < price) {
        pendingEvents.push_back("Not enough coins (" + std::to_string(s->coins) + "/" +
                                std::to_string(price) + ")");
        return false;
    }

    s->coins -= price;
    s->items.push_back(enhanced ? goldCharm() : mapCharm());
    if (enhanced) {
        enhancedBought = true;
    } else {
        s->charmBought = true;
    }
    pendingEvents.push_back("Survivor#" + std::to_string(s->id) + " bought " +
                            (enhanced ? std::string("GoldCharm") : std::string("MapCharm")) +
                            " (-" + std::to_string(price) + " coins)");
    return true;
}

void World::setRemoteControl(int survivorId, const Control& c) {
    remoteCmds[survivorId] = c;
}

void World::syncBalance() {
    for (auto& k : killerList) {
        k.setAngerScale(balance_.angerGainScale);
    }
}

void World::smoothNetwork(double dt) {
    double k = 1.0 - std::exp(-12.0 * dt);
    for (auto& s : survivorList) {
        if (!s.netActive) continue;

        if (predictionEnabled && s.id == localPlayer) {
            Vec3 err = s.netPos - s.position;
            double len = err.length();
            if (len > 0.6) {
                s.position = s.netPos;
            } else if (len > 0.02) {
                s.position = s.position + err * 0.15;
            }
            continue;
        }

        s.position = s.position + (s.netPos - s.position) * k;
    }
    for (auto& kl : killerList) {
        kl.smoothNet(dt);
    }
}

void World::predictLocal(double dt, const Control& ctrl) {
    if (localPlayer < 0) return;
    for (auto& s : survivorList) {
        if (s.id != localPlayer) continue;
        if (s.eliminated || s.downed || s.escaped) return;

        double len = ctrl.move.length();
        bool sprinting = ctrl.sprint && s.stamina > 0.5 && len > 0.001;
        bool nearWarden = wardenNear(s.position);
        double press = nearWarden ? 0.85 : 1.0;
        double drainMul = nearWarden ? 2.0 : 1.0;
        if (sprinting) {
            s.stamina = std::max(0.0, s.stamina - balance_.staminaDrain * drainMul * dt);
        } else {
            s.stamina = std::min(100.0, s.stamina + balance_.staminaRegen * dt);
        }

        if (len > 0.001) {
            Vec3 dir = ctrl.move.normalized();
            double speed = (sprinting ? balance_.sprintSpeed : balance_.moveSpeed) * press;
            s.position = navGrid.slideMove(s.position, dir * (speed * dt));
            s.position.x = std::max(-kWorldBound, std::min(kWorldBound, s.position.x));
            s.position.z = std::max(-kWorldBound, std::min(kWorldBound, s.position.z));
        }
        return;
    }
}

void World::bindLocalPlayer(int survivorId) {
    for (const auto& s : survivorList) {
        if (s.id == survivorId) {
            localPlayer = survivorId;
            return;
        }
    }
}

void World::configure(int playerCount, const std::vector<KillerAI::Kind>& kinds,
                      const std::string& mapName, const std::string& mode) {
    playerCount = std::max(1, std::min(20, playerCount));
    mapName_ = mapName.empty() ? "Factory" : mapName;
    mode_ = mode.empty() ? "Classic" : mode;
    survivorList.clear();
    generatorList.clear();
    pendingEvents.clear();
    matchClock = 0.0;
    matchOver = false;
    resultText = "None";
    localPlayer = -1;
    escapeOpen = false;
    mapCharmUsed = false;
    enhancedBought = false;
    remoteCmds.clear();
    stats_ = MatchStats{};

    buildMapBlocks(mapName_, navGrid);
    MapDef def = mapDefFor(mapName_);
    vaultPos_ = def.vault;
    vaultOpened_ = false;
    vaultVariant_ = core::Random::instance().range(0, 3);
    powerSwitchPos_ = def.switchPos;
    powerSwitchOn_ = false;
    powerCharge_ = 0.0;

    int genCount = generatorCountFor(playerCount);
    for (int i = 0; i < genCount; ++i) {
        const Vec3& p = def.gens[static_cast<size_t>(i) % def.gens.size()];
        generatorList.emplace_back(i, p, balance_.generatorFuel);
    }

    for (int i = 0; i < playerCount; ++i) {
        const Generator& g = generatorList[i % generatorList.size()];
        Vec3 radial = g.position.normalized();
        if (radial.length() < 0.1) radial = Vec3{1.0, 0.0, 0.0};
        Vec3 lateral{-radial.z, 0.0, radial.x};
        double side = (i % 2 == 0) ? 1.0 : -1.0;
        Vec3 p = g.position + radial * 1.7 + lateral * (side * 0.9);
        if (navGrid.blockedAt(p)) p = g.position;
        p.x = std::max(-kWorldBound, std::min(kWorldBound, p.x));
        p.z = std::max(-kWorldBound, std::min(kWorldBound, p.z));
        Survivor s(i, p);
        s.items = startingKit();
        s.hurtCooldown = 0.0;
        survivorList.push_back(s);
    }

    if (!survivorList.empty()) {
        survivorList[0].items.push_back(mapCharm());
    }

    exitList.clear();
    for (size_t i = 0; i < def.exits.size(); ++i) {
        exitList.emplace_back(static_cast<int>(i), def.exits[i]);
    }
    escapeOpen = false;

    killerList.clear();
    std::vector<KillerAI::Kind> spawnKinds = kinds;
    if (spawnKinds.empty()) spawnKinds.push_back(KillerAI::Kind::Stalker);
    for (size_t i = 0; i < spawnKinds.size(); ++i) {
        double angle = (2.0 * 3.14159265358979323846 * static_cast<double>(i)) /
                       static_cast<double>(spawnKinds.size());
        double radius = spawnKinds.size() > 1 ? 2.0 : 0.0;
        KillerAI k;
        k.reset(std::cos(angle) * radius, std::sin(angle) * radius, spawnKinds[i]);
        killerList.push_back(k);
    }
    syncBalance();
}

void World::configureTutorial() {
    configure(1, {KillerAI::Kind::Stalker}, "Factory");
    killerList.clear();
    survivorList.clear();
    Survivor s(0, Vec3{0.0, 0.0, -4.0});
    survivorList.push_back(s);
    generatorList.clear();
    generatorList.emplace_back(0, Vec3{0.0, 0.0, 2.0}, balance_.generatorFuel);
    vaultOpened_ = true;
    exitList.clear();
    exitList.emplace_back(0, Vec3{10.5, 0.0, 0.0});
    exitList.emplace_back(1, Vec3{-10.5, 0.0, 0.0});
    localPlayer = 0;
    stats_ = MatchStats{};
}

void World::update(double dt, const Control& ctrl) {
    if (matchOver) return;
    matchClock += dt;
    stats_.matchTime = matchClock;

    for (auto& k : killerList) {
        k.tickLife(dt);
    }

    bool makingNoise = false;
    Vec3 noiseSrc;
    int noiseId = -1;

    for (auto& s : survivorList) {
        const Control* c = nullptr;
        if (s.id == localPlayer) {
            c = &ctrl;
        } else {
            auto it = remoteCmds.find(s.id);
            if (it != remoteCmds.end()) c = &it->second;
        }
        if (!c) continue;
        if (s.eliminated || s.downed || s.escaped) continue;

        bool silenced = false;
        for (const auto& kl : killerList) {
            if (kl.silencedAt(s.position)) {
                silenced = true;
                break;
            }
        }
        if (silenced) {
            s.grantEffect("Silenced", 0.3);
        }
        if (silenced != s.inSilence) {
            s.inSilence = silenced;
            pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                    (silenced ? " entered Whisper silence field"
                                              : " left the silence field"));
        }

        double len = c->move.length();
        bool sprinting = c->sprint && s.stamina > 0.5 && len > 0.001;
        bool nearWarden = wardenNear(s.position);
        double press = nearWarden ? 0.85 : 1.0;
        double drainMul = nearWarden ? 2.0 : 1.0;
        if (sprinting) {
            s.stamina = std::max(0.0, s.stamina - balance_.staminaDrain * drainMul * dt);
        } else {
            double regen = silenced ? balance_.staminaRegen * 0.5 : balance_.staminaRegen;
            s.stamina = std::min(100.0, s.stamina + regen * dt);
        }

        if (len > 0.001) {
            Vec3 dir = c->move.normalized();
            double speed = (sprinting ? balance_.sprintSpeed : balance_.moveSpeed) * press;
            s.position = navGrid.slideMove(s.position, dir * (speed * dt));
            s.position.x = std::max(-kWorldBound, std::min(kWorldBound, s.position.x));
            s.position.z = std::max(-kWorldBound, std::min(kWorldBound, s.position.z));
        }

        if (!silenced && (sprinting || c->interact)) {
            makingNoise = true;
            noiseSrc = s.position;
            noiseId = s.id;
        }
    }

    std::vector<Vec3> positions;
    for (const auto& s : survivorList) {
        if (!s.eliminated && !s.escaped) positions.push_back(s.position);
    }

    if (!vaultOpened_) {
        for (auto& s : survivorList) {
            if (s.eliminated || s.downed || s.escaped) continue;
            if (s.position.distance(vaultPos_) <= 1.6) {
                vaultOpened_ = true;
                Vec3 sp = vaultPos_ + Vec3{1.5, 0.0, 1.5};
                if (navGrid.blockedAt(sp)) sp = vaultPos_ - Vec3{1.5, 0.0, 1.5};

                if (vaultVariant_ == 1) {
                    s.items.push_back(painkillers());
                    s.items.push_back(energyDrink());
                    s.coins += 150;
                    pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                            " looted a supply cache! (+2 items, +150 coins)");
                } else if (vaultVariant_ == 2) {
                    s.items.push_back(painkillers());
                    for (auto& k : killerList) {
                        k.addAnger(30);
                    }
                    for (int e = 0; e < 2; ++e) {
                        double side = (e == 0) ? 1.0 : -1.0;
                        KillerAI echo;
                        echo.reset(vaultPos_.x + side * 1.2, vaultPos_.z + 1.2, KillerAI::Kind::Herd);
                        echo.setLife(12.0);
                        echo.markSplit();
                        killerList.push_back(echo);
                    }
                    pendingEvents.push_back("Trap triggered! The horde is alerted.");
                } else {
                    s.items.push_back(goldCharm());
                    pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                            " looted the vault: GoldCharm!");
                    KillerAI boss;
                    boss.reset(sp.x, sp.z, KillerAI::Kind::Butcher);
                    killerList.push_back(boss);
                    pendingEvents.push_back("Vault alarm! A Butcher has awakened!");
                }
                break;
            }
        }
    }

    if (makingNoise) {
        bool heard = false;
        for (auto& k : killerList) {
            if (k.reportNoise(noiseSrc)) heard = true;
        }
        if (heard) {
            pendingEvents.push_back("A killer heard noise near Survivor#" + std::to_string(noiseId));
        }
    }
    for (auto& k : killerList) {
        k.update(dt, positions, navGrid);
    }

    for (size_t i = 0; i < killerList.size(); ++i) {
        if (killerList[i].kind() == KillerAI::Kind::Herd && !killerList[i].hasSplit() &&
            killerList[i].state() == KillerAI::State::Enrage) {
            killerList[i].markSplit();
            Vec3 base = killerList[i].position();
            Vec3 radial = base.length() > 0.1 ? base.normalized() : Vec3{1.0, 0.0, 0.0};
            Vec3 lateral{-radial.z, 0.0, radial.x};
            for (int e = 0; e < 2; ++e) {
                double side = (e == 0) ? 1.0 : -1.0;
                Vec3 p = base + lateral * (side * 1.2);
                KillerAI echo;
                echo.reset(p.x, p.z, KillerAI::Kind::Herd);
                echo.setLife(15.0);
                echo.markSplit();
                killerList.push_back(echo);
            }
            pendingEvents.push_back("Herd split into 2 echoes! (merge in 15s)");
        }
    }
    for (auto it = killerList.begin(); it != killerList.end();) {
        if (it->isTemporaryKiller() && it->lifeLeft() <= 0.0) {
            pendingEvents.push_back("Herd echo merged back");
            it = killerList.erase(it);
        } else {
            ++it;
        }
    }

    for (auto& s : survivorList) {
        if (s.eliminated) continue;
        s.tickEffects(dt);

        if (s.downed) {
            s.downTimer -= dt;
            if (s.downTimer <= 0.0) {
                int ci = findCharmIndex(s.items);
                if (!mapCharmUsed && ci >= 0) {
                    mapCharmUsed = true;
                    FoodItem ch = s.items[static_cast<size_t>(ci)];
                    s.items.erase(s.items.begin() + ci);
                    stats_.charmsUsed += 1;
                    if (isGoldCharm(ch)) {
                        s.downed = false;
                        s.health.setHp(50);
                        pendingEvents.push_back("GoldCharm revived Survivor#" + std::to_string(s.id) +
                                                " at 50hp!");
                    } else {
                        s.downTimer = balance_.downWindow;
                        pendingEvents.push_back("MapCharm saved Survivor#" + std::to_string(s.id) +
                                                " - one more rescue window!");
                    }
                } else {
                    s.eliminated = true;
                    s.downed = false;
                    stats_.survivorsEliminated += 1;
                    bumpKill(stats_, "downed timer");
                    pendingEvents.push_back("Survivor#" + std::to_string(s.id) + " died while downed");
                }
            }
            continue;
        }

        if (s.hurtCooldown > 0.0) s.hurtCooldown -= dt;

        if (s.health.hp() <= kHungerThreshold) {
            int useIdx = -1;
            for (int j = static_cast<int>(s.items.size()) - 1; j >= 0; --j) {
                if (!isMapCharm(s.items[static_cast<size_t>(j)])) {
                    useIdx = j;
                    break;
                }
            }
            if (useIdx >= 0) {
                FoodItem f = s.items[static_cast<size_t>(useIdx)];
                s.items.erase(s.items.begin() + useIdx);
                s.applyFood(f);
                stats_.itemsUsed += 1;
                std::string extra = f.effect.empty() ? "" : (" effect=" + f.effect);
                pendingEvents.push_back("Survivor#" + std::to_string(s.id) + " ate " + f.name +
                                        " (+" + std::to_string(f.heal) + "hp)" + extra);
            }
        }

        for (auto& k : killerList) {
            bool chaseNear = (k.state() == KillerAI::State::Chase || k.state() == KillerAI::State::Enrage) &&
                             k.position().distance(s.position) < kAttackRange;
            if (!chaseNear || s.hurtCooldown > 0.0) continue;

            if (s.hasEffect("Shield")) {
                s.hurtCooldown = 1.0;
                pendingEvents.push_back("Survivor#" + std::to_string(s.id) + " blocked a hit (Shield)");
                break;
            }

            int dmg = std::max(1, static_cast<int>(k.attackDamage() * balance_.damageScale + 0.5));
            s.health.damage(dmg);
            stats_.damageDealt += dmg;
            s.hurtCooldown = k.attackInterval();
            if (s.health.isDowned()) {
                ++s.downs;
                if (s.downs >= 2) {
                    int ci = findCharmIndex(s.items);
                    if (!mapCharmUsed && ci >= 0) {
                        mapCharmUsed = true;
                        FoodItem ch = s.items[static_cast<size_t>(ci)];
                        s.items.erase(s.items.begin() + ci);
                        stats_.charmsUsed += 1;
                        s.downs = 1;
                        if (isGoldCharm(ch)) {
                            s.downed = false;
                            s.health.setHp(50);
                            pendingEvents.push_back("GoldCharm revived Survivor#" +
                                                    std::to_string(s.id) + " at 50hp!");
                        } else {
                            s.downed = true;
                            stats_.downs += 1;
                            s.downTimer = balance_.downWindow;
                            pendingEvents.push_back("MapCharm saved Survivor#" +
                                                    std::to_string(s.id) +
                                                    " from elimination!");
                        }
                    } else {
                        s.eliminated = true;
                        stats_.survivorsEliminated += 1;
                        bumpKill(stats_, k.typeName());
                        pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                                " eliminated on second down");
                    }
                    } else {
                        s.downed = true;
                        stats_.downs += 1;
                        s.downTimer = balance_.downWindow;
                        pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                                " went down (rescue window " +
                                                std::to_string(static_cast<int>(balance_.downWindow)) + "s)");
                }
            } else {
                pendingEvents.push_back("Survivor#" + std::to_string(s.id) + " hit by " + k.typeName() +
                                        " (-" + std::to_string(dmg) + "hp, now " +
                                        std::to_string(s.health.hp()) + ")");
            }
            break;
        }
    }

    for (auto& s : survivorList) {
        if (!s.downed || s.eliminated) continue;
        bool hasRescuer = false;
        for (const auto& r : survivorList) {
            if (&r != &s && !r.eliminated && !r.downed && !r.escaped &&
                r.position.distance(s.position) <= kRescueRange) {
                hasRescuer = true;
                break;
            }
        }
        if (hasRescuer) {
            s.rescueCharge += dt;
            if (s.rescueCharge >= kRescueTime) {
                s.downed = false;
                s.rescueCharge = 0.0;
                s.health.setHp(20);
                stats_.rescues += 1;
                pendingEvents.push_back("Survivor#" + std::to_string(s.id) + " was rescued (hp=20)");
            }
        } else {
            s.rescueCharge = std::max(0.0, s.rescueCharge - 2.0 * dt);
        }
    }

    for (auto& g : generatorList) {
        if (g.activated) continue;
        bool working = false;
        if (localPlayer >= 0) {
            const Survivor* s = nullptr;
            for (const auto& x : survivorList) {
                if (x.id == localPlayer) {
                    s = &x;
                    break;
                }
            }
            if (s && !s->eliminated && !s->downed && !s->escaped &&
                s->position.distance(g.position) <= kRepairRange && ctrl.interact) {
                working = true;
            }
        } else {
            for (const auto& s : survivorList) {
                if (!s.eliminated && !s.downed && !s.escaped &&
                    s.position.distance(g.position) <= kRepairRange) {
                    working = true;
                    break;
                }
            }
        }
        if (working) {
            g.work += dt;
            if (g.work >= balance_.repairTime) {
                g.activated = true;
                pendingEvents.push_back("Generator#" + std::to_string(g.id) +
                                        " activated! Team +50 coins each");
                for (auto& sv : survivorList) {
                    if (!sv.eliminated) sv.coins += 50;
                }
                stats_.generatorsActivated += 1;
            }
        }
    }

    if (mode_ == "Blackout" && !powerSwitchOn_) {
        bool worker = false;
        for (const auto& s : survivorList) {
            if (s.eliminated || s.downed || s.escaped) continue;
            if (s.position.distance(powerSwitchPos_) > 1.8) continue;
            if (s.id == localPlayer) {
                if (ctrl.interact) worker = true;
            } else {
                worker = true;
            }
        }
        if (worker) {
            powerCharge_ += dt;
            if (powerCharge_ >= 3.0) {
                powerSwitchOn_ = true;
                pendingEvents.push_back("Power restored! Exit doors unlocked.");
            }
        }
    }

    if (!escapeOpen && canOpenDoors()) {
        escapeOpen = true;
        for (auto& e : exitList) e.open = true;
        pendingEvents.push_back("Doors opened - reach an exit to escape!");
    }

    if (escapeOpen) {
        for (auto& s : survivorList) {
            if (s.eliminated || s.downed || s.escaped) continue;

            Exit* target = nullptr;
            double best = 1e18;
            for (auto& e : exitList) {
                if (!e.open) continue;
                double d = s.position.distance(e.position);
                if (d < best) {
                    best = d;
                    target = &e;
                }
            }
            if (!target) continue;

            if (best > 2.4) {
                Vec3 dir = (target->position - s.position).normalized();
                s.position = navGrid.slideMove(s.position, dir * (2.6 * dt));
                s.escapeCharge = 0.0;
            } else {
                bool allowed = (s.id == localPlayer) ? ctrl.interact : true;
                if (allowed) {
                    s.escapeCharge += dt;
                    if (s.escapeCharge >= 1.5) {
                        s.escaped = true;
                        s.coins += 100;
                        stats_.survivorsEscaped += 1;
                        pendingEvents.push_back("Survivor#" + std::to_string(s.id) +
                                                " escaped! (+100 coins)");
                    }
                } else {
                    s.escapeCharge = 0.0;
                }
            }
        }
    }

    bool anyActive = false;
    bool anyEscaped = false;
    for (const auto& s : survivorList) {
        if (s.eliminated) continue;
        if (s.escaped) {
            anyEscaped = true;
        } else {
            anyActive = true;
        }
    }

    if (!anyActive) {
        matchOver = true;
        resultText = anyEscaped ? "SurvivorsWin" : "KillerWins";
        pendingEvents.push_back("Match over: " + resultText);
    }
}
}
