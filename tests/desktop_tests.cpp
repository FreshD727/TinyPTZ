#include "qt_window.hpp"
#include "configuration_backup.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QElapsedTimer>
#include <QDoubleSpinBox>
#include <QSlider>
#include <QSignalBlocker>
#include <algorithm>
#include <iostream>
static void require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
static void uiTests(QApplication &app) {
    QTemporaryDir temp; require(temp.isValid(), "temporary portable data directory");
    auto data = std::filesystem::path(temp.path().toStdWString());
    ptz::PresetStore presets(data / L"presets.ptz");
    presets.items = {{L"Wide view", L"camera-a", {17, -4, 0}, {}}, {L"Desk", L"camera-a", {-15, 8, 2}, {}}}; presets.save();
    tinyobs::SceneRules rules;
    auto initial = tinyobs::dockConfiguration(presets.path(), rules, true);
    initial["autoConnect"] = false; initial["lastDevice"] = "remembered-camera"; initial["theme"] = "dark";
    tinyobs::writeConfigurationJson(data / L"app.json", initial);
    {
        tinyapp::Window window(data, false);
        auto theme = window.findChild<QComboBox *>("appTheme");
        require(theme && theme->currentData() == "dark", "dark theme restored");
        require(app.palette().color(QPalette::Window).lightness() < 60, "dark palette applied");
        auto autoConnect = window.findChild<QCheckBox *>("autoConnectCamera");
        require(autoConnect && !autoConnect->isChecked(), "auto-connect preference restored");
        auto tabs = window.findChild<QTabWidget *>("controlTabs");
        require(tabs && !tabs->isTabVisible(2), "standalone app hides OBS scene controls");
        bool readHidden = false;
        for (auto button : window.findChildren<QPushButton *>())
            if (button->text() == "Read position") readHidden = button->isHidden();
        require(readHidden && window.findChild<QLabel *>("livePositionLabel"), "manual read replaced by live position");
        // PreviewCanvas has no custom Qt metaobject; obtain through its QWidget name.
        auto canvasWidget = window.findChild<QWidget *>("previewCanvas");
        auto preview = static_cast<tinyapp::PreviewCanvas *>(canvasWidget);
        require(preview && preview->owner() && (app.platformName() != "windows" || IsWindow(preview->owner())), "native preview owner retained");
        window.resize(1180, 790);
        QApplication::processEvents();
        window.grab().save("build/desktop/dark-preview.png");
        auto size = preview->videoSize();
        require(std::abs(size.width() * 9 - size.height() * 16) <= 16, "preview retains 16:9 aspect ratio");
        theme->setCurrentIndex(theme->findData("light"));
        QApplication::processEvents();
        require(app.palette().color(QPalette::Window).lightness() > 220, "light palette applied");
        window.grab().save("build/desktop/light-preview.png");
        auto search = window.findChild<QLineEdit *>("presetSearch");
        search->setText("Desk");
        auto list = window.findChild<QListWidget *>("presets");
        require(list->item(0)->isHidden() && !list->item(1)->isHidden(), "preset search remains available");
        auto devices = window.findChild<QComboBox *>("cameraDevice");
        devices->addItem("First camera", "camera-one"); devices->addItem("Second camera", "camera-two");
        devices->setCurrentIndex(1);
        QFile saved(QString::fromStdWString((data / L"app.json").wstring()));
        require(saved.open(QIODevice::ReadOnly), "portable preferences written");
        auto json = QJsonDocument::fromJson(saved.readAll()).object(); saved.close();
        require(json["lastDevice"].toString() == "camera-two" && json["theme"].toString() == "light" &&
                !json["autoConnect"].toBool(), "selected camera/theme/auto-connect saved together");
        require(json["presetFile"].toString() == "presets.ptz", "portable preset path saved relative to app data");
        window.close();
    }
    {
        tinyapp::Window window(data, false);
        require(window.findChild<QComboBox *>("appTheme")->currentData() == "light", "theme survives restart");
        auto saved = window.controls()->themePreference(); require(saved == "light", "controller preference matches theme");
    }
    auto moved = data / L"moved"; std::filesystem::create_directory(moved);
    std::filesystem::copy_file(data / L"presets.ptz", moved / L"presets.ptz");
    std::filesystem::copy_file(data / L"app.json", moved / L"app.json");
    std::filesystem::rename(data / L"presets.ptz", data / L"original-presets-kept.ptz");
    {
        tinyapp::Window window(moved, false);
        require(window.findChild<QListWidget *>("presets")->count() == 2 &&
                window.findChild<QComboBox *>("appTheme")->currentData() == "light", "moving the portable data folder preserves presets and preferences");
    }
    std::cout << "PASS: Qt standalone themes, portable preference/restart persistence, remembered camera, automatic-position UI, native preview host/aspect ratio, preset search and shutdown\n";
}
static bool waitFor(const std::function<bool()> &condition, int timeout = 12000) {
    QElapsedTimer timer; timer.start();
    do { QApplication::processEvents(); if (condition()) return true; QThread::msleep(15); }
    while (timer.elapsed() < timeout);
    return false;
}
static void hardwareTests() {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    require(SUCCEEDED(hr), "initialize hardware-test COM");
    struct Restore {
        ptz::Preset preset; bool sleeping = false, armed = false;
        void restore() {
            if (!armed) return;
            for (int attempt = 0;; ++attempt) {
                try {
                    ptz::Camera c; c.connect(preset.device); c.recall(preset);
                    if (sleeping) c.wake(false, true);
                    armed = false; std::cout << "PASS: original camera position, settings and power state restored\n"; return;
                } catch (const std::exception &e) {
                    if (attempt || std::string(e.what()).find("requested sleep/wake state") == std::string::npos) throw;
                    QThread::msleep(1000);
                }
            }
        }
        ~Restore() { try { restore(); } catch (const std::exception &e) { std::cerr << "RESTORE ERROR: " << e.what() << '\n'; } }
    } restore;
    try {
        auto list = ptz::devices();
        auto found = std::find_if(list.begin(), list.end(), [](auto &d) { return d.name.find(L"Tiny SE") != std::wstring::npos; });
        require(found != list.end(), "Tiny SE available for hardware test");
        ptz::Range range;
        {
            ptz::Camera c; c.connect(found->id); restore.sleeping = c.sleeping(); c.wake(true);
            restore.preset = {L"Desktop hardware test", found->id, c.read(), c.snapshotSettings()}; restore.armed = true;
            c.applySetting({"ai_mode", 0, false}); range = c.range(CameraControl_Pan);
        }
        QTemporaryDir temp; auto data = std::filesystem::path(temp.path().toStdWString());
        ptz::PresetStore store(data / L"presets.ptz"); store.items = {restore.preset}; store.save();
        auto config = tinyobs::dockConfiguration(store.path(), {}, true);
        config["lastDevice"] = QString::fromStdWString(found->id); config["autoConnect"] = true; config["theme"] = "dark";
        tinyobs::writeConfigurationJson(data / L"app.json", config);
        long target = restore.preset.position.pan + range.step;
        if (target > range.max) target = restore.preset.position.pan - range.step;
        {
            tinyapp::Window window(data);
            auto badge = window.findChild<QLabel *>("connectionBadge");
            require(waitFor([&] { return badge->text() == "Connected"; }), "auto-connect to remembered real camera");
            auto pan = window.findChild<QSlider *>("panSlider");
            auto number = window.findChild<QDoubleSpinBox *>("panNumber");
            auto status = window.findChild<QLabel *>("cameraStatus");
            pan->setValue((int)((target - range.min) / range.step));
            QMetaObject::invokeMethod(pan, "sliderReleased", Qt::DirectConnection);
            require(waitFor([&] { return status->text().contains("Position sent"); }), "live PTZ command completes");
            { QSignalBlocker block(number); number->setValue(target == range.min ? range.max : range.min); }
            require(waitFor([&] { return number->value() == target; }), "automatic position readback replaces stale displayed value without Read position");
            window.close();
        }
        {
            tinyapp::Window window(data);
            auto badge = window.findChild<QLabel *>("connectionBadge");
            require(waitFor([&] { return badge->text() == "Connected"; }), "auto-connect survives application restart");
            auto preview = window.findChild<QPushButton *>("previewToggle");
            auto status = window.findChild<QLabel *>("cameraStatus");
            preview->click();
            if (!waitFor([&] { return preview->text() == "Stop preview" || status->text().startsWith("Error:"); }, 20000))
                throw std::runtime_error("Preview startup timed out.");
            if (preview->text() != "Stop preview") throw std::runtime_error("Preview test: " + status->text().toStdString());
            window.resize(1020, 700); QApplication::processEvents();
            preview->click();
            require(waitFor([&] { return preview->text() == "Start preview" && preview->isEnabled(); }), "preview stops cleanly");
            preview->click();
            require(waitFor([&] { return preview->text() == "Stop preview"; }, 20000), "preview restarts");
            window.close();
        }
        restore.restore();
        std::cout << "PASS: remembered-camera auto-connect/restart, live PTZ readback, native DirectShow preview running/resize/stop/restart and close while previewing\n";
        CoUninitialize();
    } catch (...) { restore.restore(); CoUninitialize(); throw; }
}
int main(int argc, char **argv) {
    QApplication app(argc, argv); app.setStyle("Fusion"); app.setFont(QFont("Segoe UI", 10));
    app.setQuitOnLastWindowClosed(false);
    try { uiTests(app); if (argc > 1 && std::string(argv[1]) == "--hardware") hardwareTests(); return 0; }
    catch (const std::exception &e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
