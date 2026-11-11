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
    if (std::abs(actual.pan - target.pan) > 1 || std::abs(actual.tilt - target.tilt) > 1 ||
        actual.zoom != target.zoom)
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
} // namespace
int wmain(int argc, wchar_t **argv) {
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
                   L"[--device ID] (moves PTZ, tests four presets, restores)\n";
        } else if (cmd == L"devices") {
            for (auto &d : devices())
                std::wcout << d.name << L"\n" << d.id << L"\n";
        } else {
            if (cmd != L"read" && cmd != L"wake" && cmd != L"sleep" && cmd != L"save" &&
                cmd != L"recall" && cmd != L"diagnostics" && cmd != L"hardware-test")
                throw std::runtime_error("Unknown command; use --help");
            std::wstring id, name;
            int option = 2;
            if (cmd == L"save" || cmd == L"recall") {
                if (argc < 3)
                    throw std::runtime_error("Supply a preset name");
                name = argv[2];
                option = 3;
            }
            for (int i = option; i < argc; ++i) {
                if (std::wstring(argv[i]) == L"--device" && i + 1 < argc)
                    id = argv[++i];
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
            if (cmd == L"read")
                print(c.read());
            else if (cmd == L"diagnostics")
                std::cout << c.diagnostics();
            else if (cmd == L"hardware-test")
                hardwareTest(c);
            else if (cmd == L"wake" || cmd == L"sleep") {
                c.wake(cmd == L"wake");
                std::cout << "Power state verified\n";
            } else {
                PresetStore s(portableData());
                s.load();
                auto p = std::find_if(s.items.begin(), s.items.end(),
                                      [&](auto &p) { return p.name == name && p.device == id; });
                if (cmd == L"recall") {
                    if (p == s.items.end())
                        throw std::runtime_error("Preset not found for this camera");
                    c.move(p->position);
                    expect(c, p->position, "Recall");
                } else {
                    if (p != s.items.end())
                        throw std::runtime_error("Preset name already exists");
                    if (c.sleeping())
                        throw std::runtime_error(
                            "Wake and position the camera before saving a preset.");
                    s.items.push_back({name, id, c.read()});
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
