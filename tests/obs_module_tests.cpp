// Loads the real built plugin against installed libobs and an isolated frontend fixture.
#include "core.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QElapsedTimer>
#include <QLabel>
#include <QListWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QTemporaryDir>
#include <QThread>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QSlider>
#include <obs-frontend-api.h>
#ifdef _WIN32
#define TINY_TEST_IMPORT __declspec(dllimport)
#else
#define TINY_TEST_IMPORT
#endif
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <algorithm>
#include <iostream>
extern "C" TINY_TEST_IMPORT void tiny_test_setup(QMainWindow *, obs_source_t *, obs_source_t *);
extern "C" TINY_TEST_IMPORT void tiny_test_scene(obs_source_t *);
extern "C" TINY_TEST_IMPORT void tiny_test_event(obs_frontend_event);
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static bool wait(const std::function<bool()> &condition, int timeout = 8000) {
    QElapsedTimer timer; timer.start();
    do {
        QApplication::processEvents();
        if (condition()) return true;
        QThread::msleep(10);
    } while (timer.elapsed() < timeout);
    return false;
}
struct RestoreCamera {
    ptz::Preset original;
    bool sleeping = false, armed = false;
    void restore() {
        if (!armed) return;
        // The firmware can still be completing a power transition when the
        // worker releases its handle. Retry cleanup once after that settles.
        for (int attempt = 0;; ++attempt) {
            try {
                ptz::Camera camera; camera.connect(original.device); camera.recall(original);
                if (sleeping) camera.wake(false, true);
                break;
            } catch (const std::exception &e) {
                if (attempt || std::string(e.what()).find("requested sleep/wake state") == std::string::npos) throw;
                std::cerr << "Cleanup: waiting for the camera's power transition before retrying restoration.\n";
                QThread::msleep(1000);
            }
        }
        armed = false;
        std::cout << "PASS: original camera position, settings and power state restored\n";
    }
    ~RestoreCamera() {
        try { restore(); } catch (const std::exception &e) { std::cerr << "RESTORE ERROR: " << e.what() << '\n'; }
    }
};
int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setStyle("Fusion");
    try {
        require(argc == 2 || argc == 3, "pass compiled plugin path and optional --hardware");
        bool hardware = argc == 3 && std::string(argv[2]) == "--hardware";
        QTemporaryDir temp;
        auto directory = temp.path().toUtf8().toStdString();
        std::ofstream(ptz::filePath(temp.path().toStdWString()) / L"presets.ptz") <<
            "TINY_PTZ_PRESETS 2\n\"Module test preset\" \"offline-test-camera\" 17 -4 0 0\n";
        require(obs_startup("en-US", directory.c_str(), nullptr), "installed libobs startup");
        RestoreCamera restore;
        if (hardware) {
            auto devices = ptz::devices();
            auto found = std::find_if(devices.begin(), devices.end(), [](auto &d) { return d.name.find(L"Tiny SE") != std::wstring::npos; });
            require(found != devices.end(), "Tiny SE connected for explicit hardware test");
            ptz::Camera camera; camera.connect(found->id);
            restore.sleeping = camera.sleeping();
            restore.original = {L"Module hardware test", found->id, camera.read(), camera.snapshotSettings()};
            restore.armed = true;
            ptz::PresetStore store(ptz::filePath(temp.path().toStdWString()) / L"presets.ptz");
            store.items = {restore.original}; store.save();
        }
        auto a = obs_scene_create("Camera scene"); auto b = obs_scene_create("Other scene");
        require(a && b, "real OBS scene objects");
        auto window = std::make_unique<QMainWindow>();
        tiny_test_setup(window.get(), obs_scene_get_source(a), obs_scene_get_source(b));
        obs_module_t *module = nullptr;
        auto modulePath = QString::fromLocal8Bit(argv[1]).replace('\\', '/').toUtf8();
        require(obs_open_module(&module, modulePath.constData(), directory.c_str()) == MODULE_SUCCESS, "real plugin DLL ABI/module exports");
        require(obs_init_module(module), "real plugin module_load");
        obs_post_load_modules();
        auto container = window->findChild<QDockWidget *>("tiny-ptz.controls");
        require(container && container->widget(), "plugin registers native dock");
        auto dock = container->widget();
        auto status = dock->findChild<QLabel *>("cameraStatus");
        auto presets = dock->findChild<QListWidget *>("presets");
        if (!presets || presets->count() != 1) {
            std::cerr << "Dock status: " << dock->findChild<QLabel *>("cameraStatus")->text().toStdString() << '\n';
            for (auto &entry : std::filesystem::recursive_directory_iterator(temp.path().toStdWString()))
                std::cerr << "Fixture file: " << entry.path().u8string() << '\n';
        }
        require(presets && presets->count() == 1, "module data path imports preset library");
        tiny_test_event(OBS_FRONTEND_EVENT_FINISHED_LOADING);
        if (hardware) {
            auto devices = dock->findChild<QComboBox *>("cameraDevice");
            require(wait([&] { return devices->findData(QString::fromStdWString(restore.original.device)) >= 0; }), "dock discovers Tiny SE");
            devices->setCurrentIndex(devices->findData(QString::fromStdWString(restore.original.device)));
            dock->findChild<QPushButton *>("connectCamera")->click();
            require(wait([&] { return status->text().contains("Camera connected"); }), "dock camera worker reads hardware");
            auto tree = dock->findChild<QTreeWidget *>("cameraSettings");
            QTreeWidgetItem *brightness = nullptr;
            for (QTreeWidgetItemIterator i(tree); *i; ++i)
                if ((*i)->data(1, Qt::UserRole).toString() == "brightness") brightness = *i;
            require(brightness, "brightness exposed in dock"); tree->setCurrentItem(brightness);
            auto slider = dock->findChild<QSlider *>("settingSlider");
            int initial = slider->value(); slider->setValue(initial < slider->maximum() ? initial + 1 : initial - 1);
            require(wait([&] { return status->text().contains("Setting applied and verified"); }), "dock slider applies and verifies hardware setting");
            slider->setValue(initial);
            require(wait([&] { return status->text().contains("Setting applied and verified") && brightness->text(1) == QString::number(dock->findChild<QDoubleSpinBox *>("settingNumber")->value()); }), "dock restores brightness");
            std::cout << "PASS: real Tiny SE dock connection, PTZ/settings read and live brightness slider/readback\n";
        }
        auto target = dock->findChild<QComboBox *>("scenePreset"); target->setCurrentIndex(1);
        dock->findChild<QPushButton *>("saveSceneAssignment")->click();
        auto automation = dock->findChild<QCheckBox *>("sceneAutomation");
        automation->setChecked(true);
        auto recalled = [&] { return hardware ? status->text().contains("Scene preset loaded:") : status->text().contains("Error:"); };
        require(wait(recalled, 20000), "mapped Program scene invokes camera worker");
        QString before = status->text();
        tiny_test_event(OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED);
        QApplication::processEvents();
        require(status->text() == before, "Preview scene changes do not recall");
        tiny_test_scene(obs_scene_get_source(b)); tiny_test_event(OBS_FRONTEND_EVENT_SCENE_CHANGED);
        require(status->text().contains("No preset assigned"), "unassigned Program scene leaves camera unchanged");
        obs_source_set_name(obs_scene_get_source(a), "Renamed scene");
        tiny_test_event(OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED);
        tiny_test_scene(obs_scene_get_source(a)); tiny_test_event(OBS_FRONTEND_EVENT_SCENE_CHANGED);
        require(wait(recalled, 20000), "renamed scene retains UUID preset assignment");
        std::cout << "PASS: mapped Program recall, Preview isolation, unmapped scene and scene rename callbacks\n";
        // Real OBS may unload modules before the main window's child destruction.
        tiny_test_event(OBS_FRONTEND_EVENT_EXIT);
        restore.restore();
        obs_scene_release(a); obs_scene_release(b);
        obs_shutdown();
        window.reset();
        std::cout << "PASS: installed libobs module loader, DLL ABI, native dock registration, real scene UUIDs, scene/preview callbacks, missing-device error, and module unload before Qt window destruction\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
