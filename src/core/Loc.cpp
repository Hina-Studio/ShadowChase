#include "core/Loc.hpp"

#include <set>
#include <unordered_map>

namespace core {
namespace {
const std::unordered_map<std::string, std::string>& zhTable() {
    static const std::unordered_map<std::string, std::string> table = {
        {"menu.subtitle", "PvE 合作恐怖原型"},
        {"menu.controls", "WASD 移动 / SHIFT 冲刺 / E 交互"},
        {"menu.goal", "修理发电机、打开大门、逃离杀手"},
        {"menu.killers", "杀手配置: "},
        {"menu.map", "地图: "},
        {"menu.mode", "模式: "},
        {"menu.rotation", "轮换: "},
        {"menu.seed", "种子: "},
        {"menu.session", "本次会话: 局数 "},
        {"menu.network", "网络: "},
        {"menu.candidates", "连接候选: "},
        {"menu.platform", "平台: "},
        {"menu.start", "按 ENTER 或 空格 开始"},
        {"menu.tutorial", "按 T 进入教程"},
        {"menu.lobby", "按 J 加入局域网游戏"},
        {"menu.replay", "按 L 观看上一局回放"},
        {"menu.settings", "按 O 进入设置"},
        {"menu.quit", "ESC 退出"},
        {"settings.title", "设置"},
        {"settings.hint", "W/S 选择   A/D 或 ENTER 调整   ESC 返回"},
        {"settings.resolution", "分辨率"},
        {"settings.volume", "音量"},
        {"settings.vhs", "VHS 滤镜"},
        {"settings.grid", "网格"},
        {"settings.msaa", "MSAA（需改配置并重启）"},
        {"settings.language", "语言"},
        {"settings.back", "返回"},
        {"balance.title", "平衡参数面板"},
        {"balance.hint", "W/S 选择   A/D 调整   ENTER 重置/保存   ESC 关闭"},
        {"balance.save", "保存到配置"},
        {"balance.close", "关闭"},
        {"tutorial.title", "教程"},
        {"tutorial.complete", "教程完成"},
        {"tutorial.step1", "第 1/3 步：走到青色标记处（WASD，SHIFT 冲刺）"},
        {"tutorial.step2", "第 2/3 步：靠近发电机并按住 E 直到激活"},
        {"tutorial.step3", "第 3/3 步：大门已开，到 EXIT 处按住 E 撤离"},
        {"tutorial.skip", "ESC 跳过教程"},
        {"tutorial.return", "按 ENTER 返回主菜单"},
        {"replay.title", "回放"},
        {"replay.end", "回放结束"},
        {"replay.stop", "ESC 停止"},
        {"lobby.title", "局域网大厅"},
        {"lobby.hint", "W/S 选择   ENTER 加入   ESC 返回"},
        {"lobby.empty", "搜索中…未发现主机"},
        {"gameover.finished", "对局结束"},
        {"gameover.time", "时间"},
        {"gameover.escaped", "撤离"},
        {"gameover.eliminated", "淘汰"},
        {"gameover.gens", "发电机"},
        {"gameover.rescues", "救援"},
        {"gameover.items", "道具"},
        {"gameover.charms", "保命符"},
        {"gameover.damage", "总承伤"},
        {"gameover.topkiller", "主要凶手"},
        {"gameover.restart", "按 R 重开一局"},
        {"gameover.quit", "ESC 退出"},
        {"hud.hp", "生命"},
        {"hud.stamina", "体力"},
        {"hud.anger", "愤怒"},
        {"hud.coins", "金币"},
        {"hud.inventory", "背包"},
        {"hud.downed", "倒地"},
        {"hud.rescueIn", "救援倒计时"},
        {"hud.buff", "状态"},
    };
    return table;
}
}

Loc& Loc::instance() {
    static Loc inst;
    return inst;
}

void Loc::setLocale(const std::string& code) {
    loc = (code == "zh") ? "zh" : "en";
}

std::string Loc::t(const std::string& key) const {
    if (loc == "zh") {
        auto it = zhTable().find(key);
        if (it != zhTable().end()) return it->second;
    }
    return key;
}

std::vector<int> Loc::zhCodepoints() const {
    std::set<int> cps;
    for (int c = 32; c < 127; ++c) cps.insert(c);
    for (const auto& kv : zhTable()) {
        const std::string& s = kv.second;
        size_t i = 0;
        while (i < s.size()) {
            unsigned char c = static_cast<unsigned char>(s[i]);
            int cp = 0;
            int extra = 0;
            if (c < 0x80) {
                cp = c;
                extra = 0;
            } else if ((c >> 5) == 0x6) {
                cp = c & 0x1F;
                extra = 1;
            } else if ((c >> 4) == 0xE) {
                cp = c & 0x0F;
                extra = 2;
            } else {
                cp = c & 0x07;
                extra = 3;
            }
            ++i;
            for (int k = 0; k < extra && i < s.size(); ++k, ++i) {
                cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
            }
            cps.insert(cp);
        }
    }
    return std::vector<int>(cps.begin(), cps.end());
}
}
