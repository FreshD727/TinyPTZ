#include "core.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>
using namespace ptz;
namespace {
void settle() {
    std::this_thread::sleep_for(std::chrono::milliseconds(1600));
}
void print(Position p) {
    std::wcout << p.pan << L" " << p.tilt << L" " << p.zoom << L"\n";
}
void expect(Camera &c, Position target, const char *label) {
    settle();
    auto actual = c.read();
    std::cout << label << ": target=" << target.pan << "," << target.tilt << "," << target.zoom
              << " actual=" << actual.pan << "," << actual.tilt << "," << actual.zoom << "\n";
    if (actual.pan != target.pan || actual.tilt != target.tilt || actual.zoom != target.zoom)
        throw std::runtime_error(std::string(label) +
                                 ": camera did not reach the requested position");
}
void hardwareTest(Camera &c) {
    bool wasSleeping = c.sleeping();
    c.wake(true);
    settle();
    auto original = c.read();
    std::cout << "Starting position: ";
    print(original);
    try {
        auto target = original;
        auto pan = c.range(CameraControl_Pan), tilt = c.range(CameraControl_Tilt),
             zoom = c.range(CameraControl_Zoom);
        target.pan = quantize(original.pan + (original.pan + 5 <= pan.max ? 5 : -5), pan);
        c.setAxis(CameraControl_Pan, target.pan);
        expect(c, target, "Pan");
        target.tilt = quantize(original.tilt + (original.tilt + 5 <= tilt.max ? 5 : -5), tilt);
        c.setAxis(CameraControl_Tilt, target.tilt);
        expect(c, target, "Tilt");
        target.zoom = quantize(
            original.zoom + (original.zoom + zoom.step <= zoom.max ? zoom.step : -zoom.step), zoom);
        c.setAxis(CameraControl_Zoom, target.zoom);
        expect(c, target, "Zoom");
        auto path = portableData().parent_path() / L"hardware-test.ptz";
        if (std::filesystem::exists(path))
            throw std::runtime_error("Test data already exists; use a clean build folder.");
        PresetStore store(path);
        store.items = {{L"Test 1", c.id(), original},
                       {L"Test 2", c.id(), target},
                       {L"Test 3", c.id(), {target.pan, original.tilt, original.zoom}},
                       {L"Test 4", c.id(), {original.pan, target.tilt, target.zoom}}};
        store.save();
        PresetStore loaded(path);
        loaded.load();
        for (auto &p : loaded.items) {
            c.move(p.position);
            expect(c, p.position, utf8(p.name).c_str());
        }
        std::filesystem::remove(path);
        c.move(original);
        expect(c, original, "Restore");
        if (wasSleeping)
            c.wake(false);
        std::cout << "PASS: pan, tilt, zoom, four disk-backed presets, recall, original position "
                     "and power state restored\n";
    } catch (...) {
        try {
            c.move(original);
            settle();
            if (wasSleeping)
                c.wake(false);
        } catch (const std::exception &e) {
            std::cerr << "Restore failed: " << e.what() << "\n";
        }
        throw;
    }
}
void smallStepTest(Camera &c) {
    bool wasSleeping = c.sleeping();
    c.wake(true);
    settle();
    auto original = c.read();
    auto restore = [&] {
        c.move(original);
        c.waitForPosition(original);
        if (wasSleeping)
            c.wake(false);
    };
    try {
        for (auto axis : {CameraControl_Pan, CameraControl_Tilt}) {
            auto r = c.range(axis);
            auto start = axis == CameraControl_Pan ? original.pan : original.tilt;
            auto step = start + r.step <= r.max ? r.step : -r.step;
            for (int i = 0; i < 16; ++i) {
                c.setAxis(axis, start + (i % 2 == 0 ? step : 0));
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                auto actual = c.read();
                auto held = axis == CameraControl_Pan ? actual.tilt : actual.pan;
                auto expected = axis == CameraControl_Pan ? original.tilt : original.pan;
                if (held != expected)
                    throw std::runtime_error("Small-step movement changed the untouched axis");
            }
            c.move(original);
            c.waitForPosition(original);
            std::cout << (axis == CameraControl_Pan ? "Pan" : "Tilt")
                      << ": 16 small changes preserved the other axis\n";
            for (int offset : {1, 2, 1, -1, -2, -1}) {
                auto value = start + offset * r.step;
                if (value < r.min || value > r.max)
                    continue;
                auto target = original;
                if (axis == CameraControl_Pan)
                    target.pan = value;
                else
                    target.tilt = value;
                c.setAxis(axis, value);
                c.waitForPosition(target);
                c.setAxis(axis, value); // A slow slider's duplicate release value.
                settle();
                c.waitForPosition(target);
            }
            c.move(original);
            c.waitForPosition(original);
            std::cout << "Slow changes and repeated release targets held position\n";
            c.setAxis(axis, start + step);
            c.setAxis(axis, start);
            c.waitForPosition(original);
            std::cout << "Immediate reversal returned to the starting target\n";
        }
        restore();
        std::cout << "PASS: small-step PTZ isolation; original position and power restored\n";
    } catch (...) {
        restore();
        throw;
    }
}
void liveSettingsTest(Camera &c) {
    bool asleep = c.sleeping();
    c.wake(true);
    auto all = c.settings();
    std::vector<SettingValue> originals;
    auto restore = [&] {
        for (auto i = originals.rbegin(); i != originals.rend(); ++i)
            c.applySetting(*i);
        if (asleep)
            c.wake(false);
    };
    try {
        for (auto key : {"brightness", "composition_x", "auto_sleep", "zone_home_pan"}) {
            auto i =
                std::find_if(all.begin(), all.end(), [&](auto &s) { return s.current.key == key; });
            if (i == all.end() || !i->readable || !i->writable)
                throw std::runtime_error("Live test control unavailable");
            originals.push_back(i->current);
            auto value = i->current;
            double direction = value.value + 3 * i->step <= i->maximum ? 1 : -1;
            for (int tick = 1; tick <= 3; ++tick) {
                value.value = i->current.value + direction * tick * i->step;
                auto start = GetTickCount64();
                c.applySetting(value, false);
                std::cout << "Live " << key << "=" << value.value
                          << " write_ms=" << (GetTickCount64() - start) << "\n";
                Sleep(90);
            }
            Sleep(160);
            c.verifySetting(value);
            std::cout << "PASS final readback " << key << "=" << value.value << "\n";
        }
        restore();
        std::cout << "PASS: immediate standard, raw, scalar and read-modify-write settings; final "
                     "readback; original settings restored\n";
    } catch (...) {
        try {
            restore();
        } catch (const std::exception &e) {
            std::cerr << "Restore error: " << e.what() << "\n";
        }
        throw;
    }
}
void positionTest(Camera &c) {
    bool asleep = c.sleeping();
    c.wake(true);
    auto original = c.read();
    auto all = c.settings();
    auto ai =
        std::find_if(all.begin(), all.end(), [](auto &s) { return s.current.key == "ai_mode"; });
    if (ai == all.end() || !ai->readable)
        throw std::runtime_error("Tracking status unavailable");
    auto originalMode = ai->current;
    auto path = portableData().parent_path() / L"precision-test.ptz";
    if (std::filesystem::exists(path))
        throw std::runtime_error("Precision test file already exists");
    auto restore = [&] {
        c.applySetting({"ai_mode", 0, false});
        c.move(original);
        c.waitForPosition(original);
        c.applySetting(originalMode);
        if (asleep)
            c.wake(false);
    };
    try {
        PresetStore disk(path);
        disk.items = {{L"17 minus 4", c.id(), {17, -4, 0}, {{"ai_mode", 0, false}}}};
        disk.save();
        PresetStore loaded(path);
        loaded.load();
        c.applySetting({"ai_mode", 0, false});
        for (auto start : {Position{12, -8, 0}, Position{20, 3, 0}, Position{17, -3, 0}}) {
            c.move(start);
            c.waitForPosition(start);
            c.recall(loaded.items.at(0));
            std::cout << c.positionDiagnostics();
            expect(c, {17, -4, 0}, "Disk preset recall");
        }
        c.move({17, 4, 0});
        c.waitForPosition({17, 4, 0});
        std::cout << c.positionDiagnostics();
        expect(c, {17, 4, 0}, "Positive tilt");
        std::filesystem::remove(path);
        restore();
        std::cout << "PASS: exact 17/-4/0 disk-backed recall from three directions, positive tilt, "
                     "measured native angles and original state restored\n";
    } catch (...) {
        try {
            restore();
        } catch (const std::exception &e) {
            std::cerr << "Restore error: " << e.what() << "\n";
        }
        throw;
    }
}
void imageControlsTest(Camera &c) {
    bool asleep = c.sleeping();
    c.wake(true);
    auto all = c.settings();
    auto s = std::find_if(all.begin(), all.end(),
                          [](auto &s) { return s.current.key == "exposure_compensation"; });
    if (s == all.end() || !s->readable || !s->defaultAvailable || s->minimum != -9 ||
        s->maximum != 9 || s->defaultValue.value != 0)
        throw std::runtime_error("Exposure compensation metadata is incorrect");
    auto original = s->current;
    auto restore = [&] {
        c.applySetting(original);
        if (asleep)
            c.wake(false);
    };
    try {
        for (double value : {-9., 9., 0.}) {
            c.applySetting({"exposure_compensation", value, false});
            std::cout << "PASS exposure_compensation=" << value << " (verified readback)\n";
        }
        c.applySetting(s->defaultValue);
        restore();
        std::cout << "PASS: image compensation range, neutral default, repeated writes, original "
                     "value restored\n";
    } catch (...) {
        try {
            restore();
        } catch (...) {
        }
        throw;
    }
}
void bootTest(Camera &c) {
    bool asleep = c.sleeping();
    c.wake(true);
    auto id = c.id();
    auto position = c.read();
    auto all = c.settings();
    auto ai =
        std::find_if(all.begin(), all.end(), [](auto &s) { return s.current.key == "ai_mode"; });
    if (ai == all.end() || !ai->readable)
        throw std::runtime_error("Tracking state unavailable");
    auto mode = ai->current;
    auto original = c.bootRecord();
    auto restore = [&] {
        c.writeBootRecord(original);
        c.move(position);
        c.applySetting(mode);
        if (asleep)
            c.wake(false);
    };
    try {
        c.applySetting({"ai_mode", 0, false});
        Preset target{L"Boot test", id, {5, 5, 1}, {}};
        c.setBootPreset(target);
        auto saved = c.bootRecord();
        c.disconnect();
        c.connect(id);
        if (c.bootRecord() != saved)
            throw std::runtime_error("Boot record did not survive reconnect");
        c.activateBootPosition();
        settle();
        print(c.read());
        expect(c, target.position, "Firmware boot-position recall");
        restore();
        std::cout << "PASS: device boot position write, reconnect persistence, firmware recall, "
                     "original boot and camera state restored\n";
    } catch (...) {
        try {
            restore();
        } catch (const std::exception &e) {
            std::cerr << "Restore error: " << e.what() << "\n";
        }
        throw;
    }
}
void settingsTest(Camera &c) {
    bool asleep = c.sleeping();
    c.wake(true);
    settle();
    auto position = c.read();
    auto initial = c.settings();
    std::vector<SettingValue> restore;
    int failures = 0;
    auto lookup = [&](const std::string &key) {
        auto i = std::find_if(initial.begin(), initial.end(),
                              [&](auto &s) { return s.current.key == key; });
        if (i == initial.end() || !i->readable || !i->writable)
            throw std::runtime_error("Test setting unavailable: " + key);
        return *i;
    };
    auto verify = [&](SettingValue v) {
        auto all = c.settings();
        auto i =
            std::find_if(all.begin(), all.end(), [&](auto &s) { return s.current.key == v.key; });
        if (i == all.end() || !i->readable || i->current.automatic != v.automatic ||
            !v.automatic && std::abs(i->current.value - v.value) > .02)
            throw std::runtime_error("Setting readback mismatch: " + v.key);
        std::cout << "PASS " << v.key << "=" << v.value << " auto=" << v.automatic << "\n";
    };
    auto cleanup = [&] {
        bool failed = false;
        for (auto i = restore.rbegin(); i != restore.rend(); ++i)
            try {
                c.applySetting(*i);
            } catch (const std::exception &e) {
                failed = true;
                std::cerr << "RESTORE ERROR " << i->key << ": " << e.what() << "\n";
            }
        auto ai = lookup("ai_mode");
        c.applySetting(ai.current);
        c.move(position);
        settle();
        if (asleep)
            c.wake(false);
        if (failed)
            throw std::runtime_error("Some original settings could not be restored");
    };
    try {
        for (auto key : {"gimbal_reverse", "zone_enabled", "hand"}) {
            auto s = lookup(key);
            auto target = s.current;
            restore.push_back(s.current);
            if (target.automatic) {
                target.automatic = false;
                target.value = s.minimum + std::floor((target.value - s.minimum) / s.step) * s.step;
            } else
                target.value = target.value + s.step <= s.maximum ? target.value + s.step
                                                                  : target.value - s.step;
            try {
                c.applySetting(target);
                verify(target);
            } catch (const std::exception &e) {
                ++failures;
                std::cout << "UNVERIFIED " << key << ": " << e.what() << "\n";
            }
            try {
                c.applySetting(s.current);
                verify(s.current);
            } catch (const std::exception &e) {
                ++failures;
                std::cout << "RESTORE UNVERIFIED " << key << ": " << e.what() << "\n";
            }
        }
        restore.push_back(lookup("brightness").current);
        auto brightness = lookup("brightness").current;
        brightness.value = brightness.value < 99 ? brightness.value + 1 : brightness.value - 1;
        Preset p{L"Settings recall test", c.id(), position, c.snapshotSettings()};
        for (auto &v : p.settings)
            if (v.key == "brightness")
                v = brightness;
        std::cout << "Testing full preset snapshot: " << p.settings.size() << " settings\n";
        auto path = portableData().parent_path() / L"settings-test.ptz";
        if (std::filesystem::exists(path))
            throw std::runtime_error("Test file already exists");
        PresetStore disk(path);
        disk.items = {p};
        disk.save();
        PresetStore loaded(path);
        loaded.load();
        c.recall(loaded.items.at(0));
        for (auto &v : p.settings)
            verify(v);
        std::filesystem::remove(path);
        restore.push_back(lookup("exposure").current);
        restore.push_back(lookup("gain").current);
        auto manualExposure = lookup("exposure").current;
        manualExposure.automatic = false;
        auto manualGain = lookup("gain").current;
        manualGain.value = manualGain.value < 128 ? manualGain.value + 1 : manualGain.value - 1;
        Preset manual{L"Manual exposure/gain recall",
                      c.id(),
                      position,
                      {manualGain, manualExposure}}; // Deliberately gain first in the file.
        c.recall(manual);
        verify(manualExposure);
        verify(manualGain);
        cleanup();
        if (failures)
            throw std::runtime_error(std::to_string(failures) +
                                     " setting checks need investigation");
        std::cout << "PASS: settings read/write/restore and disk-backed settings preset recall\n";
    } catch (...) {
        try {
            cleanup();
        } catch (const std::exception &e) {
            std::cerr << "Cleanup error: " << e.what() << "\n";
        }
        throw;
    }
}
} // namespace
int wmain(int argc, wchar_t **argv) {
    std::cout << std::unitbuf;
    HRESULT init = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(init)) {
        std::cerr << "COM initialization failed\n";
        return 1;
    }
    int result = 0;
    try {
        std::wstring cmd = argc > 1 ? argv[1] : L"help";
        if (cmd == L"help" || cmd == L"--help") {
            std::wcout
                << L"Tiny PTZ CLI\n  ptzctl devices\n  ptzctl read [--device ID]\n  ptzctl wake | "
                   L"sleep [--device ID]\n  ptzctl save NAME [--device ID]\n  ptzctl recall NAME "
                   L"[--device ID]\n  ptzctl diagnostics [--device ID]\n  ptzctl hardware-test "
                   L"[--device ID] (moves PTZ, tests four presets, restores)\n  ptzctl settings\n  "
                   L"ptzctl set KEY VALUE [--auto]\n  ptzctl settings-test (changes settings "
                   L"temporarily, then restores)\n  ptzctl image-defaults\n  ptzctl "
                   L"image-controls-test\n  ptzctl boot-read\n  ptzctl set-boot NAME\n  ptzctl "
                   L"boot-test (temporarily changes boot configuration and PTZ, then restores)\n  "
                   L"ptzctl small-step-test (temporarily moves each axis one step, then restores)\n"
                   L"ptzctl power-session (stdin: wake / sleep / diagnostics / quit)\n";
        } else if (cmd == L"devices") {
            for (auto &d : devices())
                std::wcout << d.name << L"\n" << d.id << L"\n";
        } else {
            if (cmd != L"read" && cmd != L"wake" && cmd != L"sleep" && cmd != L"save" &&
                cmd != L"recall" && cmd != L"diagnostics" && cmd != L"hardware-test" &&
                cmd != L"settings" && cmd != L"set" && cmd != L"settings-test" &&
                cmd != L"boot-read" && cmd != L"image-defaults" && cmd != L"boot-test" &&
                cmd != L"set-boot" && cmd != L"image-controls-test" && cmd != L"power-session" &&
                cmd != L"position-details" && cmd != L"position-test" &&
                cmd != L"live-settings-test" && cmd != L"small-step-test")
                throw std::runtime_error("Unknown command; use --help");
            std::wstring id, name;
            SettingValue setting;
            int option = 2;
            if (cmd == L"save" || cmd == L"recall" || cmd == L"set-boot") {
                if (argc < 3)
                    throw std::runtime_error("Supply a preset name");
                name = argv[2];
                option = 3;
            }
            if (cmd == L"set") {
                if (argc < 4)
                    throw std::runtime_error("Use: set KEY VALUE [--auto] [--device ID]");
                setting.key = utf8(argv[2]);
                size_t end = 0;
                setting.value = std::stod(argv[3], &end);
                if (end != std::wstring(argv[3]).size())
                    throw std::runtime_error("Invalid numeric setting");
                option = 4;
            }
            for (int i = option; i < argc; ++i) {
                if (std::wstring(argv[i]) == L"--device" && i + 1 < argc)
                    id = argv[++i];
                else if (cmd == L"set" && std::wstring(argv[i]) == L"--auto")
                    setting.automatic = true;
                else
                    throw std::runtime_error("Invalid option; use --help");
            }
            if (id.empty())
                for (auto &d : devices())
                    if (d.name.find(L"OBSBOT Tiny SE") != std::wstring::npos) {
                        if (!id.empty())
                            throw std::runtime_error("Multiple Tiny SE cameras: specify --device");
                        id = d.id;
                    }
            if (id.empty())
                throw std::runtime_error(
                    "Tiny SE not found. Use devices, then --device with its exact ID.");
            Camera c;
            c.connect(id);
            if (cmd == L"small-step-test") {
                smallStepTest(c);
            } else if (cmd == L"live-settings-test") {
                liveSettingsTest(c);
            } else if (cmd == L"position-test") {
                positionTest(c);
            } else if (cmd == L"position-details") {
                std::cout << c.positionDiagnostics();
            } else if (cmd == L"power-session") {
                std::string action;
                std::cout << "READY\n";
                while (std::getline(std::cin, action) && action != "quit") {
                    try {
                        if (action == "wake" || action == "sleep")
                            c.wake(action == "wake", true);
                        else if (action == "diagnostics")
                            std::cout << c.diagnostics();
                        else
                            throw std::runtime_error("Unknown power-session command");
                        std::cout << "OK\n";
                    } catch (const std::exception &e) {
                        std::cout << "ERROR " << e.what() << "\n";
                    }
                }
            } else if (cmd == L"image-controls-test") {
                imageControlsTest(c);
            } else if (cmd == L"boot-test") {
                bootTest(c);
            } else if (cmd == L"boot-read") {
                c.wake(true);
                print(c.read());
                std::cout << c.bootDiagnostics();
            } else if (cmd == L"image-defaults") {
                c.restoreImageDefaults();
                std::cout << "Image defaults restored and verified\n";
            } else if (cmd == L"settings") {
                c.wake(true);
                for (auto &s : c.settings())
                    std::cout << s.current.key << "="
                              << (s.readable ? std::to_string(s.current.value) : "unreadable")
                              << " auto=" << s.current.automatic << " writable=" << s.writable
                              << " range=" << s.minimum << ".." << s.maximum << " default="
                              << (s.defaultAvailable ? std::to_string(s.defaultValue.value)
                                                     : "unavailable")
                              << " defaultAuto=" << s.defaultValue.automatic << " | "
                              << utf8(s.details) << "\n";
            } else if (cmd == L"set") {
                c.applySetting(setting);
                std::cout << "Setting command completed; read settings to verify\n";
            } else if (cmd == L"read")
                print(c.read());
            else if (cmd == L"diagnostics")
                std::cout << c.diagnostics();
            else if (cmd == L"hardware-test")
                hardwareTest(c);
            else if (cmd == L"settings-test")
                settingsTest(c);
            else if (cmd == L"wake" || cmd == L"sleep") {
                c.wake(cmd == L"wake", true);
                std::cout << "Power state verified\n";
            } else {
                PresetStore s(portableData());
                s.load();
                auto p = std::find_if(s.items.begin(), s.items.end(),
                                      [&](auto &p) { return p.name == name && p.device == id; });
                if (cmd == L"set-boot") {
                    if (p == s.items.end())
                        throw std::runtime_error("Preset not found for this camera");
                    c.setBootPreset(*p);
                    std::cout << "PTZ boot position stored and read back from camera\n";
                } else if (cmd == L"recall") {
                    if (p == s.items.end())
                        throw std::runtime_error("Preset not found for this camera");
                    c.recall(*p);
                    if (std::none_of(p->settings.begin(), p->settings.end(), [](auto &v) {
                            return (v.key == "ai_mode" || v.key == "zone_enabled") && v.value != 0;
                        }))
                        expect(c, p->position, "Recall");
                    else
                        std::cout << "Preset applied; active tracking may move the camera.\n";
                } else {
                    if (p != s.items.end())
                        throw std::runtime_error("Preset name already exists");
                    if (c.sleeping())
                        throw std::runtime_error(
                            "Wake and position the camera before saving a preset.");
                    s.items.push_back({name, id, c.read(), c.snapshotSettings()});
                    s.save();
                    std::wcout << L"Saved " << name << L"\n";
                }
            }
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        result = 1;
    }
    CoUninitialize();
    return result;
}
