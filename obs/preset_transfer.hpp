#pragma once
#include "core.hpp"
#include "scene_rules.hpp"
#include <algorithm>
namespace tinyobs {
inline size_t rebindPresets(std::vector<ptz::Preset> &presets, SceneRules &rules,
                            const std::wstring &source, const std::wstring &target) {
    if (source.empty() || target.empty() || source == target) throw std::runtime_error("Choose two different camera identities.");
    size_t count = 0;
    for (auto &p : presets) if (p.device == source) {
        PresetId before{p.device,p.name}; auto base=p.name;
        auto exists = [&] { return std::any_of(presets.begin(),presets.end(),[&](const auto &other) { return &other != &p && other.device==target && other.name==p.name; }); };
        for(size_t suffix=2;exists();++suffix) p.name=base+L" ("+std::to_wstring(suffix)+L")";
        p.device=target; rules.rename(before,{p.device,p.name}); ++count;
    }
    return count;
}
inline bool samePresetFile(const std::filesystem::path &a, const std::filesystem::path &b) {
    std::error_code error;
    if (std::filesystem::equivalent(a, b, error)) return true;
    auto first = ptz::pathText(std::filesystem::weakly_canonical(a));
    auto second = ptz::pathText(std::filesystem::weakly_canonical(b));
#ifdef _WIN32
    return CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
    return first == second;
#endif
}
// Read both files afresh; only publish the merged list after atomic save succeeds.
inline size_t importPresetFile(ptz::PresetStore &active, const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) throw std::runtime_error("The selected preset file does not exist.");
    if (samePresetFile(active.path(), path)) throw std::runtime_error("This is already the active preset file. Choose a different file to import.");
    ptz::PresetStore incoming(path), merged(active.path());
    incoming.load(); merged.load();
    for (auto preset : incoming.items) {
        auto base = preset.name;
        auto exists = [&] {
            return std::any_of(merged.items.begin(), merged.items.end(), [&](const auto &p) {
                return p.device == preset.device && p.name == preset.name;
            });
        };
        for (size_t suffix = 2; exists(); ++suffix)
            preset.name = base + L" (" + std::to_wstring(suffix) + L")";
        merged.items.push_back(std::move(preset));
    }
    if (!incoming.items.empty()) merged.save();
    active.items = std::move(merged.items);
    return incoming.items.size();
}
inline size_t exportPresetFile(const ptz::PresetStore &active, const std::filesystem::path &path) {
    if (samePresetFile(active.path(), path)) throw std::runtime_error("Choose a different file to export a copy of your presets.");
    ptz::PresetStore current(active.path()), exported(path);
    current.load(); exported.items = std::move(current.items);
    exported.save();
    return exported.items.size();
}
} // namespace tinyobs
