#include "dock.hpp"
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
int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setStyle("Fusion");
    try {
        workerTests(); ruleTests();
        QTemporaryDir temp;
        require(temp.isValid(), "temporary config directory");
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
