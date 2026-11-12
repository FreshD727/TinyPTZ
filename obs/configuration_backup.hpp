#pragma once
#include "preset_transfer.hpp"
#include "scene_rules.hpp"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>
#include <fstream>
#include <memory>
namespace tinyobs {
struct ConfigurationBackup {
    std::vector<ptz::Preset> presets;
    SceneRules rules;
    bool saveCameraSettings = true;
};
inline QString backupPath(const std::filesystem::path &path) {
    return QString::fromStdWString(ptz::pathText(path));
}
inline QJsonObject dockConfiguration(const std::filesystem::path &presets,
                                     const SceneRules &rules, bool saveCameraSettings) {
    QJsonObject json;
    json["version"] = 1;
    json["presetFile"] = backupPath(presets);
    json["sceneAutomation"] = rules.enabled;
    json["saveCameraSettingsWithPresets"] = saveCameraSettings;
    QJsonArray assignments;
    for (auto &collection : rules.collections) for (auto &scene : collection.second) {
        QJsonObject o;
        o["collection"] = QString::fromUtf8(collection.first.c_str());
        o["scene"] = QString::fromUtf8(scene.first.c_str());
        auto names = rules.sceneNames.find(collection.first);
        if (names != rules.sceneNames.end()) {
            auto name = names->second.find(scene.first);
            if (name != names->second.end()) o["sceneName"] = QString::fromUtf8(name->second.c_str());
        }
        if (rules.restorePending.count({collection.first, scene.first})) o["matchByName"] = true;
        o["device"] = QString::fromStdWString(scene.second.device);
        o["preset"] = QString::fromStdWString(scene.second.name);
        assignments.append(o);
    }
    json["assignments"] = assignments;
    return json;
}
inline void writeConfigurationJson(const std::filesystem::path &path, const QJsonObject &json) {
    auto bytes = QJsonDocument(json).toJson();
    QSaveFile file(backupPath(path));
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        throw std::runtime_error("Cannot save configuration file; the previous file was preserved.");
}
inline void exportConfiguration(const ptz::PresetStore &active, const std::filesystem::path &config,
                                const SceneRules &rules, bool saveCameraSettings,
                                const std::filesystem::path &destination) {
    if (samePresetFile(active.path(), destination) || samePresetFile(config, destination))
        throw std::runtime_error("Choose a separate file for the configuration backup.");
    ptz::PresetStore fresh(active.path()); fresh.load();
    QTemporaryDir temp;
    if (!temp.isValid()) throw std::runtime_error("Cannot create temporary backup folder.");
    ptz::PresetStore normalized(ptz::filePath(temp.path().toStdWString()) / L"presets.ptz");
    normalized.items = std::move(fresh.items); normalized.save();
    QFile presets(backupPath(normalized.path()));
    if (!presets.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read presets for backup.");
    auto content = presets.readAll();
    if (presets.error() != QFile::NoError) throw std::runtime_error("Cannot finish reading presets for backup.");
    QJsonObject backup;
    backup["format"] = "tiny-ptz-configuration";
    backup["version"] = 1;
    backup["configuration"] = dockConfiguration(active.path(), rules, saveCameraSettings);
    backup["presets"] = QString::fromUtf8(content);
    if (QJsonDocument(backup).toJson().size() > 32 * 1024 * 1024)
        throw std::runtime_error("Configuration backup is too large (maximum 32 MiB).");
    writeConfigurationJson(destination, backup);
}
inline ConfigurationBackup readConfigurationBackup(const std::filesystem::path &path) {
    QFile file(backupPath(path));
    if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read configuration backup.");
    constexpr qint64 limit = 32 * 1024 * 1024;
    if (file.size() > limit) throw std::runtime_error("Configuration backup is too large (maximum 32 MiB).");
    auto bytes = file.read(limit + 1);
    if (file.error() != QFile::NoError || bytes.size() > limit) throw std::runtime_error("Cannot read complete configuration backup.");
    QJsonParseError error;
    auto document = QJsonDocument::fromJson(bytes, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) throw std::runtime_error("Invalid configuration backup JSON.");
    auto root = document.object();
    if (root["format"].toString() != "tiny-ptz-configuration" || root["version"].toDouble(-1) != 1 ||
        !root["presets"].isString() || !root["configuration"].isObject())
        throw std::runtime_error("Unsupported configuration backup format or version.");
    auto config = root["configuration"].toObject();
    if (config["version"].toDouble(-1) != 1 || !config["sceneAutomation"].isBool() ||
        !config["saveCameraSettingsWithPresets"].isBool() || !config["assignments"].isArray())
        throw std::runtime_error("Incomplete configuration backup.");
    ConfigurationBackup result;
    result.rules.enabled = config["sceneAutomation"].toBool();
    result.saveCameraSettings = config["saveCameraSettingsWithPresets"].toBool();
    for (auto entry : config["assignments"].toArray()) {
        if (!entry.isObject()) throw std::runtime_error("Invalid scene assignment in backup.");
        auto o = entry.toObject();
        for (const char *key : {"collection", "scene", "device", "preset"})
            if (!o[key].isString() || o[key].toString().isEmpty())
                throw std::runtime_error("Incomplete scene assignment in backup.");
        auto collection = o["collection"].toString().toUtf8().toStdString();
        auto scene = o["scene"].toString().toUtf8().toStdString();
        auto &rules = result.rules.collections[collection];
        if (rules.count(scene)) throw std::runtime_error("Duplicate scene assignment in backup.");
        rules.emplace(scene, PresetId{o["device"].toString().toStdWString(), o["preset"].toString().toStdWString()});
        if (o.contains("sceneName")) {
            if (!o["sceneName"].isString()) throw std::runtime_error("Invalid scene name in backup.");
            result.rules.sceneNames[collection][scene] = o["sceneName"].toString().toUtf8().toStdString();
        }
        if (o.contains("matchByName")) {
            if (!o["matchByName"].isBool()) throw std::runtime_error("Invalid scene matching flag in backup.");
            if (o["matchByName"].toBool()) result.rules.restorePending.insert({collection, scene});
        }
    }
    QTemporaryDir temp;
    if (!temp.isValid()) throw std::runtime_error("Cannot create temporary restore folder.");
    auto presetPath = ptz::filePath(temp.path().toStdWString()) / L"presets.ptz";
    QFile presets(backupPath(presetPath)); auto content = root["presets"].toString().toUtf8();
    if (!presets.open(QIODevice::WriteOnly) || presets.write(content) != content.size())
        throw std::runtime_error("Cannot validate backup presets.");
    presets.close();
    ptz::PresetStore parsed(presetPath); parsed.load();
    result.presets = std::move(parsed.items);
    return result;
}
inline std::unique_ptr<ptz::PresetStore> restoreConfiguration(
    const std::filesystem::path &config, const ConfigurationBackup &backup) {
    // A new managed library plus one atomic config switch avoids half-restores
    // and never overwrites a previously selected external library.
    auto filename = L"restored-presets-" + QUuid::createUuid().toString(QUuid::WithoutBraces).toStdWString() + L".ptz";
    auto restored = std::make_unique<ptz::PresetStore>(config.parent_path() / filename);
    try {
        restored->items = backup.presets; restored->save();
        writeConfigurationJson(config, dockConfiguration(restored->path(), backup.rules, backup.saveCameraSettings));
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(restored->path(), ignored);
        auto temp = restored->path(); temp += L".tmp";
        std::filesystem::remove(temp, ignored);
        throw;
    }
    return restored;
}
} // namespace tinyobs
