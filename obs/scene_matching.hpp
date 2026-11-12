#pragma once
#include "scene_rules.hpp"
#include "platform.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <filesystem>
#include <vector>
namespace tinyobs {
using SceneKey = std::pair<std::string, std::string>;
struct SceneCatalog {
    std::map<std::string, std::map<std::string, std::string>> collections;
    bool complete = false;
};
// Read inactive collections without switching OBS scenes or loading their sources.
// The current collection is replaced by live frontend data by the caller.
inline SceneCatalog readSceneCatalog(const std::filesystem::path &folder) {
    SceneCatalog result;
    QDir directory(QString::fromStdWString(ptz::pathText(folder)));
    if (!directory.exists() || !directory.isReadable()) return result;
    result.complete = true;
    for (auto &entry : directory.entryInfoList({"*.json"}, QDir::Files)) {
        QFile file(entry.absoluteFilePath());
        if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024 * 1024) { result.complete = false; continue; }
        QJsonParseError error;
        auto document = QJsonDocument::fromJson(file.readAll(), &error);
        auto root = document.object();
        if (file.error() != QFile::NoError || error.error != QJsonParseError::NoError ||
            !root["name"].isString() || root["name"].toString().isEmpty() || !root["sources"].isArray()) {
            result.complete = false; continue;
        }
        auto collection = root["name"].toString().toUtf8().toStdString();
        if (result.collections.count(collection)) result.complete = false;
        auto &scenes = result.collections[collection];
        for (auto value : root["sources"].toArray()) {
            auto source = value.toObject();
            if (source["id"].toString() != "scene") continue;
            auto uuid = source["uuid"].toString().toUtf8().toStdString();
            auto name = source["name"].toString().toUtf8().toStdString();
            if (uuid.empty() || name.empty()) { result.complete = false; continue; }
            if (!scenes.emplace(uuid, name).second) result.complete = false;
        }
    }
    return result;
}
inline bool rememberSceneNames(SceneRules &rules, const SceneCatalog &catalog) {
    bool changed = false;
    for (auto &collection : rules.collections) {
        auto found = catalog.collections.find(collection.first);
        if (found == catalog.collections.end()) continue;
        for (auto &scene : collection.second) {
            // Pending UUIDs belong to the origin PC; do not replace their name.
            if (rules.restorePending.count({collection.first, scene.first})) continue;
            auto current = found->second.find(scene.first);
            if (current != found->second.end() && rules.sceneNames[collection.first][scene.first] != current->second) {
                rules.sceneNames[collection.first][scene.first] = current->second; changed = true;
            }
        }
    }
    return changed;
}
struct SceneMatchReport {
    size_t uuid = 0, name = 0, unresolved = 0;
    bool changed = false;
    std::vector<std::string> details;
};
inline SceneMatchReport resolveSceneAssignments(SceneRules &rules, const SceneCatalog &catalog, bool restoring = false) {
    struct Candidate { SceneKey source, target; PresetId preset; int rank; std::string name; };
    std::map<SceneKey, std::vector<Candidate>> candidates;
    SceneMatchReport report;
    auto unresolved = [&](const SceneKey &key, const std::string &name, const std::string &reason) {
        if (rules.restorePending.insert(key).second) report.changed = true;
        ++report.unresolved;
        report.details.push_back(key.first + " / " + (name.empty() ? key.second : name) + ": " + reason);
    };
    std::set<SceneKey> protectedTargets;
    for (auto &collection : rules.collections) for (auto &scene : collection.second) {
        SceneKey source{collection.first, scene.first};
        if (!restoring && !rules.restorePending.count(source)) { protectedTargets.insert(source); continue; }
        std::string name;
        auto names = rules.sceneNames.find(source.first);
        if (names != rules.sceneNames.end()) {
            auto n = names->second.find(source.second); if (n != names->second.end()) name = n->second;
        }
        std::vector<SceneKey> targets;
        int rank = 0;
        auto local = catalog.collections.find(source.first);
        if (local != catalog.collections.end() && local->second.count(source.second)) targets.push_back(source);
        else {
            // If a collection was renamed on the destination PC, only a complete
            // catalog can establish that a UUID/name is unique across collections.
            if (local == catalog.collections.end() && catalog.complete) {
                rank = 1;
                for (auto &c : catalog.collections) if (c.second.count(source.second)) targets.push_back({c.first, source.second});
            }
            if (targets.empty() && !name.empty()) {
                rank = local == catalog.collections.end() ? 3 : 2;
                if (local != catalog.collections.end()) {
                    for (auto &s : local->second) if (s.second == name) targets.push_back({source.first, s.first});
                } else if (catalog.complete) {
                    for (auto &c : catalog.collections) for (auto &s : c.second)
                        if (s.second == name) targets.push_back({c.first, s.first});
                }
            }
        }
        if (targets.size() != 1) {
            unresolved(source, name, targets.size() > 1 ? "ambiguous scene match; not assigned" :
                       name.empty() ? "no saved scene name and UUID not found; re-export an older backup on the origin PC" :
                                      "scene not found; retained for a later collection/scene load");
            continue;
        }
        candidates[targets[0]].push_back({source, targets[0], scene.second, rank, name});
    }
    std::vector<Candidate> accepted;
    for (auto &entry : candidates) {
        auto &list = entry.second;
        int best = 4; size_t count = 0;
        for (auto &candidate : list) {
            if (candidate.rank < best) { best = candidate.rank; count = 0; }
            if (candidate.rank == best) ++count;
        }
        for (auto &candidate : list) {
            if (protectedTargets.count(entry.first) || count != 1 || candidate.rank != best)
                unresolved(candidate.source, candidate.name, "another assignment targets this scene; not overwritten");
            else accepted.push_back(candidate);
        }
    }
    for (auto &candidate : accepted) {
        rules.collections[candidate.source.first].erase(candidate.source.second);
        rules.sceneNames[candidate.source.first].erase(candidate.source.second);
        rules.restorePending.erase(candidate.source);
    }
    for (auto &candidate : accepted) {
        rules.collections[candidate.target.first][candidate.target.second] = candidate.preset;
        auto name = catalog.collections.at(candidate.target.first).at(candidate.target.second);
        rules.sceneNames[candidate.target.first][candidate.target.second] = name;
        rules.restorePending.erase(candidate.target);
        if (candidate.rank < 2) ++report.uuid; else ++report.name;
        if (candidate.source != candidate.target || candidate.name != name) report.changed = true;
        report.details.push_back(candidate.source.first + " / " + (candidate.name.empty() ? candidate.source.second : candidate.name) +
                                 " -> " + candidate.target.first + " / " + name + (candidate.rank < 2 ? " (UUID)" : " (name)"));
    }
    // Clearing a pending flag is itself a persistent change, even if UUID is unchanged.
    if (!accepted.empty()) report.changed = true;
    return report;
}
} // namespace tinyobs
