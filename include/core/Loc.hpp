#pragma once
#include <string>
#include <vector>

namespace core {
class Loc {
public:
    static Loc& instance();

    void setLocale(const std::string& code);
    const std::string& locale() const { return loc; }
    std::string t(const std::string& key) const;
    std::vector<int> zhCodepoints() const;

private:
    std::string loc = "en";
};
}
