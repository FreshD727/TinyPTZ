// Optional libobs module: registers a recall hotkey for each stored preset.
// Configure preset_file in module settings via OBS global config or adapt to a dock.
#include "core.hpp"
#include <memory>
#include <obs-module.h>
OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("tiny-ptz", "en-US")
MODULE_EXPORT const char *obs_module_description(void) {
    return "Tiny PTZ preset hotkeys using Windows UVC controls";
}
struct Binding {
    ptz::Preset preset;
    obs_hotkey_id hotkey = OBS_INVALID_HOTKEY_ID;
};
static std::vector<std::unique_ptr<Binding>> bindings;
static void recall(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed) {
    if (!pressed)
        return;
    auto b = static_cast<Binding *>(data);
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    try {
        ptz::Camera camera;
        camera.connect(b->preset.device);
        camera.recall(b->preset);
    } catch (const std::exception &e) {
        blog(LOG_WARNING, "[tiny-ptz] %s", e.what());
    }
    if (SUCCEEDED(hr))
        CoUninitialize();
}
bool obs_module_load(void) {
    try {
        char *path = obs_module_config_path("presets.ptz");
        ptz::PresetStore store(ptz::wide(path));
        bfree(path);
        store.load();
        for (auto &p : store.items) {
            auto b = std::make_unique<Binding>();
            b->preset = p;
            auto name = "tiny-ptz.recall." + ptz::utf8(p.device) + "." + ptz::utf8(p.name);
            auto description = "Tiny PTZ: " + ptz::utf8(p.name);
            b->hotkey =
                obs_hotkey_register_frontend(name.c_str(), description.c_str(), recall, b.get());
            bindings.push_back(std::move(b));
        }
        return true;
    } catch (const std::exception &e) {
        blog(LOG_ERROR, "[tiny-ptz] %s", e.what());
        return false;
    }
}
void obs_module_unload(void) {
    for (auto &b : bindings)
        obs_hotkey_unregister(b->hotkey);
    bindings.clear();
}
