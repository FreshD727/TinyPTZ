#pragma once
#include <string>
#include <map>
#include <optional>
#include <set>
namespace tinyobs {
struct PresetId {
    std::wstring device, name;
    bool operator==(const PresetId &other) const {
        return device == other.device && name == other.name;
    }
};
// Source UUIDs survive scene renames; collections have separate assignments.
class SceneRules {
  public:
    bool enabled = false;
    std::map<std::string, std::map<std::string, PresetId>> collections;
    std::map<std::string, std::map<std::string, std::string>> sceneNames;
    std::set<std::pair<std::string, std::string>> restorePending;
    std::optional<PresetId> lookup(const std::string &collection, const std::string &scene) const {
        if (!enabled || scene.empty()) return {};
        if (restorePending.count({collection, scene})) return {};
        auto c = collections.find(collection);
        if (c == collections.end()) return {};
        auto s = c->second.find(scene);
        if (s == c->second.end()) return {};
        return s->second;
    }
    void rename(const PresetId &before, const PresetId &after) {
        for (auto &c : collections) for (auto &s : c.second)
            if (s.second == before) s.second = after;
    }
    void remove(const PresetId &id) {
        for (auto &c : collections)
            for (auto s = c.second.begin(); s != c.second.end();)
                if (s->second == id) {
                    sceneNames[c.first].erase(s->first);
                    restorePending.erase({c.first, s->first});
                    s = c.second.erase(s);
                } else ++s;
    }
};
} // namespace tinyobs
