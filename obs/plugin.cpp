#include "dock.hpp"
#include <QApplication>
#include <QPointer>
#include <obs-module.h>
#include <obs-frontend-api.h>

OBS_DECLARE_MODULE()
OBS_MODULE_AUTHOR("Tiny PTZ contributors")
MODULE_EXPORT const char *obs_module_name(void) { return "Tiny PTZ Dock"; }
MODULE_EXPORT const char *obs_module_description(void) {
    return "OBSBOT Tiny SE controls, settings, presets and Program scene automation";
}
static QPointer<tinyobs::Dock> dock;
static bool callbackInstalled = false, exiting = false, frontendLoaded = false;
static constexpr const char *dockId = "tiny-ptz.controls";

static tinyobs::Host host() {
    return {
        [] {
            std::vector<tinyobs::Scene> result;
            obs_frontend_source_list list{};
            obs_frontend_get_scenes(&list);
            for (size_t i = 0; i < list.sources.num; ++i) {
                auto s = list.sources.array[i];
                result.push_back({obs_source_get_uuid(s), QString::fromUtf8(obs_source_get_name(s))});
            }
            obs_frontend_source_list_free(&list);
            return result;
        },
        [] {
            tinyobs::Scene result;
            auto s = obs_frontend_get_current_scene();
            if (s) {
                result = {obs_source_get_uuid(s), QString::fromUtf8(obs_source_get_name(s))};
                obs_source_release(s);
            }
            return result;
        },
        [] {
            auto name = obs_frontend_get_current_scene_collection();
            std::string result = name ? name : "";
            bfree(name); return result;
        }
    };
}
static void frontendEvent(enum obs_frontend_event event, void *) {
    if (!dock) return;
    switch (event) {
    case OBS_FRONTEND_EVENT_FINISHED_LOADING:
        frontendLoaded = true;
        dock->frontendReady(); break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        if (frontendLoaded) dock->frontendReady(); else dock->refreshScenes();
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
        dock->sceneChanged(); break;
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
        dock->refreshScenes(); break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_RENAMED:
        dock->refreshScenes(); dock->sceneChanged(); break;
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
        dock->collectionChanging(); break;
    case OBS_FRONTEND_EVENT_EXIT:
        exiting = true;
        dock->shutdown();
        obs_frontend_remove_event_callback(frontendEvent, nullptr);
        callbackInstalled = false;
        // Destroy plugin-owned C++ widgets while the module is still loaded.
        // Removing the dock is allowed during EXIT, but not after it returns.
        obs_frontend_remove_dock(dockId);
        break;
    default: break;
    }
}
bool obs_module_load(void) {
    if (!qobject_cast<QApplication *>(QCoreApplication::instance()) || !obs_frontend_get_main_window()) {
        blog(LOG_ERROR, "[tiny-ptz] This plugin needs the OBS Studio Qt frontend.");
        return false;
    }
    return true;
}
void obs_module_post_load(void) {
    char *config = obs_module_config_path("dock.json");
    char *presets = obs_module_file("presets.ptz");
    try {
        if (!config) throw std::runtime_error("OBS did not provide a plugin configuration directory.");
        blog(LOG_DEBUG, "[tiny-ptz] Config: %s; bundled presets: %s", config, presets ? presets : "(none)");
        dock = new tinyobs::Dock(ptz::wide(config), presets ? std::filesystem::path(ptz::wide(presets)) : std::filesystem::path(), host());
        if (!obs_frontend_add_dock_by_id(dockId, "Tiny PTZ", dock)) {
            delete dock.data(); dock = nullptr;
            throw std::runtime_error("Dock identifier is already registered.");
        }
        obs_frontend_add_event_callback(frontendEvent, nullptr);
        callbackInstalled = true;
        blog(LOG_INFO, "[tiny-ptz] 0.4.3 dock loaded; camera controls use a dedicated COM thread.");
    } catch (const std::exception &e) {
        blog(LOG_ERROR, "[tiny-ptz] %s", e.what());
    }
    bfree(config); bfree(presets);
}
void obs_module_unload(void) {
    if (callbackInstalled && !exiting) obs_frontend_remove_event_callback(frontendEvent, nullptr);
    callbackInstalled = false;
    if (dock) {
        dock->shutdown();
        if (!exiting) obs_frontend_remove_dock(dockId);
        // Defensive: an unexpected host unload order must not leave our vtable
        // in a Qt child that outlives this DLL.
        if (dock) delete dock.data();
    }
}
