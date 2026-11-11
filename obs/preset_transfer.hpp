#pragma once
#include "core.hpp"
#include <algorithm>
namespace tinyobs {
inline bool samePresetFile(const std::filesystem::path &a, const std::filesystem::path &b) {
    std::error_code error;
    if (std::filesystem::equivalent(a, b, error)) return true;
    auto first = std::filesystem::weakly_canonical(a).wstring();
    auto second = std::filesystem::weakly_canonical(b).wstring();
    return CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
}
// Read both files afresh; only publish the merged list after atomic save succeeds.
inline size_t importPresetFile(ptz::PresetStore &active, const std::filesystem::path &path) {
    if (!std::filesystem::exists(path)) throw std::runtime_error("The selected preset file does not exist.");
    if (samePresetFile(active.path(), path)) throw std::runtime_error("This is already the active preset file. Use Reload file instead.");
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
