#include "dock.hpp"
#include "preset_transfer.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <future>
#include <fstream>
#include <iostream>

static void require(bool value, const char *message) {
    if (!value) throw std::runtime_error(message);
}
static void workerTests() {
    std::mutex mutex;
    std::condition_variable cv;
    bool entered = false, release = false;
    std::vector<int> values;
    std::promise<void> done;
    std::atomic<int> errors{0};
    DWORD caller = GetCurrentThreadId(), workerThread = 0;
    tinyobs::CameraWorker worker([&](const std::string &) { ++errors; });
    worker.submit("barrier", [&](ptz::Camera &) {
        workerThread = GetCurrentThreadId();
        APTTYPE type; APTTYPEQUALIFIER qualifier;
        require(SUCCEEDED(CoGetApartmentType(&type, &qualifier)) && (type == APTTYPE_STA || type == APTTYPE_MAINSTA),
                "camera worker must own an STA");
        std::unique_lock<std::mutex> lock(mutex); entered = true; cv.notify_all();
        cv.wait(lock, [&] { return release; });
    });
    {
        std::unique_lock<std::mutex> lock(mutex);
        require(cv.wait_for(lock, std::chrono::seconds(5), [&] { return entered; }), "worker started");
    }
    for (int i = 0; i < 100; ++i) worker.submit("slider", [&, i](ptz::Camera &) { values.push_back(i); });
    worker.submit("recall", [&](ptz::Camera &) { values.push_back(-1); });
    worker.cancel("recall");
    worker.submit("failure", [](ptz::Camera &) { throw std::runtime_error("expected test failure"); });
    worker.submit("done", [&](ptz::Camera &) { done.set_value(); });
    { std::lock_guard<std::mutex> lock(mutex); release = true; } cv.notify_all();
    require(done.get_future().wait_for(std::chrono::seconds(5)) == std::future_status::ready, "worker completes after error");
    worker.stop(); worker.stop();
    require(workerThread != caller, "camera jobs run off the UI thread");
    require(values == std::vector<int>{99}, "queued drag values coalesce and stale scene recalls cancel");
    require(errors == 1, "worker reports error and continues");
}
static void ruleTests() {
    tinyobs::SceneRules rules;
    tinyobs::PresetId p{L"camera-a", L"Wide"}, other{L"camera-b", L"Wide"};
    rules.collections["Show A"]["scene-uuid"] = p;
    rules.collections["Show B"]["scene-uuid"] = other;
    require(!rules.lookup("Show A", "scene-uuid"), "automation defaults off");
    rules.enabled = true;
    require(rules.lookup("Show A", "scene-uuid") == p, "scene UUID resolves preset");
    require(rules.lookup("Show B", "scene-uuid") == other, "collection and camera identities isolated");
    require(!rules.lookup("Show A", "unmapped"), "unmapped scene does nothing");
    rules.rename(p, {p.device, L"Close"});
    require(rules.lookup("Show A", "scene-uuid")->name == L"Close", "preset rename updates assignments");
    rules.remove(other);
    require(!rules.lookup("Show B", "scene-uuid"), "deleting preset removes assignment");
}
static void transferTests(const std::filesystem::path &directory) {
    ptz::PresetStore active(directory / L"active.ptz"), incoming(directory / L"incoming.ptz");
    active.items = {{L"Wide", L"camera-a", {17, -4, 0}, {{"brightness", 50, false}}},
                    {L"Wide (2)", L"camera-a", {0, 0, 0}, {}}};
    active.save();
    incoming.items = {{L"Wide", L"camera-a", {-17, -4, 3}, {{"composition_x", .15, false}, {"exposure", -5, true}}},
                      {L"Wide", L"camera-b", {0, 1, 2}, {}},
                      {L"Küche", L"camera-a", {1, 2, 3}, {}}};
    incoming.save();
    auto read = [](const std::filesystem::path &path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    auto sourceBytes = read(incoming.path());
    require(tinyobs::importPresetFile(active, incoming.path()) == 3, "imports all presets");
    require(active.items.size() == 5 && active.items[0].position.pan == 17 &&
            active.items[2].name == L"Wide (3)" && active.items[3].name == L"Wide",
            "import preserves existing presets and resolves same-camera name collisions");
    require(active.items[2].position.tilt == -4 && active.items[2].settings[0].value == .15 &&
            active.items[2].settings[1].automatic && active.items[4].name == L"Küche",
            "import preserves negative coordinates, fractional settings, auto mode and Unicode");
    require(read(incoming.path()) == sourceBytes, "import never edits source library");
    auto original = read(active.path());
    auto reject = [&](auto operation) {
        bool rejected = false; try { operation(); } catch (const std::exception &) { rejected = true; }
        require(rejected, "unsafe/invalid transfer rejected");
        require(read(active.path()) == original && active.items.size() == 5, "failed transfer leaves active library intact");
    };
    reject([&] { tinyobs::importPresetFile(active, active.path()); });
    reject([&] { tinyobs::exportPresetFile(active, active.path()); });
    std::ofstream(directory / L"invalid.ptz") << "TINY_PTZ_PRESETS 2\nnot a preset\n";
    reject([&] { tinyobs::importPresetFile(active, directory / L"invalid.ptz"); });
    reject([&] { tinyobs::importPresetFile(active, directory / L"missing.ptz"); });
    // An external edit must be included even if the dock's cached list is older.
    ptz::PresetStore fresh(active.path()); fresh.load();
    fresh.items.push_back({L"External edit", L"camera-a", {0, 0, 0}, {}}); fresh.save();
    auto exportedPath = directory / L"exported.ptz";
    require(tinyobs::exportPresetFile(active, exportedPath) == 6, "export reads latest disk library");
    ptz::PresetStore exported(exportedPath); exported.load();
    require(exported.items.size() == 6 && exported.items[2].settings[1].automatic &&
            exported.items.back().name == L"External edit", "export roundtrip preserves full library and settings");
    std::cout << "PASS: preset import/export, collisions, Unicode/settings roundtrip, current disk state, source preservation and invalid/self-file rejection\n";
}
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    try {
        workerTests(); ruleTests();
        QTemporaryDir temp;
        require(temp.isValid(), "temporary config directory");
        transferTests(std::filesystem::path(temp.path().toStdWString()) / L"transfer");
        auto config = std::filesystem::path(temp.path().toStdWString()) / L"dock.json";
        auto library = config.parent_path() / L"saved.ptz";
        ptz::PresetStore presets(library);
        presets.items = {{L"Wide", L"offline-test-camera", {17, -4, 0}, {}}}; presets.save();
        std::string collection = "Test collection";
        tinyobs::Scene active{"scene-a", "Camera scene"};
        std::vector<tinyobs::Scene> scenes{{"scene-a", "Camera scene"}, {"scene-b", "Other scene"}};
        tinyobs::Host host{[&] { return scenes; }, [&] { return active; }, [&] { return collection; }};
        {
            tinyobs::Dock dock(config, library, host, false);
            auto list = dock.findChild<QListWidget *>("presets");
            require(list && list->count() == 1, "saved portable presets load in dock");
            require(dock.findChild<QPushButton *>("importPresets") && dock.findChild<QPushButton *>("exportPresets"),
                    "preset import and export buttons available in dock");
            auto automation = dock.findChild<QCheckBox *>("sceneAutomation");
            require(!automation->isChecked(), "scene movement opt in");
            auto target = dock.findChild<QComboBox *>("scenePreset");
            target->setCurrentIndex(1);
            dock.findChild<QPushButton *>("saveSceneAssignment")->click();
            QFile file(QString::fromStdWString(config.wstring()));
            require(file.open(QIODevice::ReadOnly), "dock config saved");
            auto json = QJsonDocument::fromJson(file.readAll()).object();
            require(json["assignments"].toArray().size() == 1, "scene assignment persisted");
            scenes[0].name = "Renamed camera scene"; dock.refreshScenes();
            require(target->currentIndex() == 1, "scene rename retains UUID assignment");
            collection = "Another collection"; dock.refreshScenes();
            require(target->currentIndex() == 0, "separate collection has no accidental assignment");
            collection = "Test collection"; dock.refreshScenes();
            ptz::Setting exposure;
            exposure.current = {"exposure_compensation", 0, false};
            exposure.defaultValue = exposure.current; exposure.defaultAvailable = true;
            exposure.name = L"Exposure Compensation"; exposure.group = L"Image";
            exposure.minimum = -9; exposure.maximum = 9; exposure.step = 1;
            exposure.readable = exposure.writable = true;
            ptz::Setting composition = exposure;
            composition.current = {"composition_x", .15, false};
            composition.name = L"Composition X"; composition.group = L"Tracking";
            composition.minimum = -1; composition.maximum = 1; composition.step = .01;
            ptz::Setting unavailable = exposure;
            unavailable.current.key = "unsupported"; unavailable.name = L"Unsupported";
            unavailable.writable = unavailable.readable = false;
            dock.presentSettings({exposure, composition, unavailable});
            auto slider = dock.findChild<QSlider *>("settingSlider");
            auto number = dock.findChild<QDoubleSpinBox *>("settingNumber");
            require(slider->maximum() == 18 && slider->value() == 9 && number->value() == 0,
                    "negative exposure range with neutral default");
            auto tree = dock.findChild<QTreeWidget *>("cameraSettings");
            tree->setCurrentItem(tree->topLevelItem(1)->child(0));
            require(slider->maximum() == 200 && number->decimals() == 2 && number->value() == .15,
                    "fractional setting values preserved");
            tree->setCurrentItem(tree->topLevelItem(0)->child(1));
            require(!slider->isEnabled() && !number->isEnabled(), "unsupported setting is disabled");
            tree->setCurrentItem(tree->topLevelItem(0)->child(0));
            std::array<ptz::Range, 3> ranges{{{-90, 90, 1, 0, true}, {-30, 30, 1, 0, true}, {0, 100, 1, 0, true}}};
            dock.presentPosition({17, -4, 0}, ranges);
            require(dock.findChild<QDoubleSpinBox *>("tiltNumber")->value() == -4, "negative tilt precision");
            dock.resize(420, 720);
            dock.grab().save("build/obs/dock-preview.png");
            dock.findChild<QTabWidget *>("controlTabs")->setCurrentIndex(1);
            dock.grab().save("build/obs/settings-preview.png");
            dock.findChild<QTabWidget *>("controlTabs")->setCurrentIndex(2);
            dock.grab().save("build/obs/scenes-preview.png");
            dock.shutdown(); dock.shutdown();
        }
        {
            tinyobs::Dock dock(config, library, host, false);
            require(dock.findChild<QComboBox *>("scenePreset")->currentIndex() == 1, "assignment survives dock recreation");
            require(!dock.findChild<QCheckBox *>("sceneAutomation")->isChecked(), "automation state survives restart");
        }
        std::cout << "PASS: COM worker ownership, live-job coalescing, stale recall cancellation, error recovery, scene UUID/collection isolation, rename/delete rules, preset import, configuration roundtrip, negative/fractional controls, unsupported settings and shutdown\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
