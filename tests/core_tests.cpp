#include "core.hpp"
#include "protocol.hpp"
#include <fstream>
#include <iostream>
using namespace ptz;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
int main() {
    try {
        // Golden packet produced by libdev.dll SHA-256 e23a445b...bc994 at RVA EE320/EDCB0.
        auto packet = wakePacket(true, 1);
        unsigned char golden[] = {0xaa, 0x00, 0x0e, 0x10, 0x00, 0x01, 0xed,
                                  0x97, 0x00, 0xe1, 0x13, 0xc2, 0x01, 0x01};
        require(std::equal(std::begin(golden), std::end(golden), packet.begin()),
                "wake packet differs from original encoder");
        require(std::all_of(packet.begin() + 14, packet.end(), [](auto b) { return b == 0; }),
                "packet padding must be zero");
        auto sleep = wakePacket(false, 0x1234);
        require(sleep[4] == 0x12 && sleep[5] == 0x34 && sleep[13] == 3,
                "sleep and sequence encoding");
        auto path = std::filesystem::current_path() / L"build" /
                    (L"tiny-ptz-test-" + std::to_wstring(GetCurrentProcessId()) + L".ptz");
        PresetStore s(path);
        for (int i = 0; i < 1000; ++i)
            s.items.push_back({L"Scene \"" + std::to_wstring(i) + L"\" – Küche",
                               L"camera-id",
                               {i % 100, -10, 3}});
        s.items[0].settings = {{"brightness", 42, false},
                               {"white_balance", 5040, true},
                               {"composition_x", .125, false}};
        s.save();
        PresetStore other(path);
        other.load();
        require(other.items.size() == 1000, "arbitrary preset count");
        require(other.items[0].settings.size() == 3 && other.items[0].settings[1].automatic &&
                    other.items[0].settings[2].value == .125,
                "preset settings and automatic modes roundtrip");
        auto oldPath = path;
        oldPath += L".v1";
        {
            std::ofstream f(oldPath);
            f << "TINY_PTZ_PRESETS 1\n\"Legacy desk\" \"camera-id\" 12 0 1\n";
        }
        PresetStore old(oldPath);
        old.load();
        require(old.items.size() == 1 && old.items[0].settings.empty(), "v1 compatibility");
        old.save();
        PresetStore upgraded(oldPath);
        upgraded.load();
        require(upgraded.items[0].name == L"Legacy desk", "v1 migration preserves data");
        std::filesystem::remove(oldPath);
        auto backup = oldPath;
        backup += L".v1.backup";
        require(std::filesystem::exists(backup), "v1 migration keeps backup");
        std::filesystem::remove(backup);
        auto legacySettingsPath = path;
        legacySettingsPath += L".v2";
        {
            std::ofstream f(legacySettingsPath);
            f << "TINY_PTZ_PRESETS 2\n\"Old image\" \"camera-id\" 0 0 0 1 \"backlight\" 9 0\n";
        }
        PresetStore legacySettings(legacySettingsPath);
        legacySettings.load();
        require(legacySettings.items[0].settings[0].key == "exposure_compensation" &&
                    legacySettings.items[0].settings[0].value == 0,
                "legacy compensation migration");
        legacySettings.save();
        legacySettings.load();
        require(legacySettings.items[0].settings[0].value == 0,
                "compensation migration is not repeated");
        std::filesystem::remove(legacySettingsPath);
        require(other.items[999].name == s.items[999].name, "Unicode and quoted names roundtrip");
        require(other.items[999].position.zoom == 3, "PTZ roundtrip");
        require(quantize(7, {-10, 10, 3, 0, true}) == 8, "step quantization");
        require(quantize(100, {-10, 10, 1, 0, true}) == 10, "upper clamp");
        require(quantize(-100, {-10, 10, 1, 0, true}) == -10, "lower clamp");
        {
            std::ofstream f(path, std::ios::trunc);
            f << "TINY_PTZ_PRESETS 1\ninvalid\n";
        }
        bool refused = false;
        try {
            other.load();
        } catch (...) {
            refused = true;
        }
        require(refused, "corrupt record rejection");
        require(other.items.size() == 1000, "load failure preserves memory");
        s.items[0].name = L"bad\nname";
        refused = false;
        try {
            s.save();
        } catch (...) {
            refused = true;
        }
        require(refused, "newline rejection");
        {
            std::ifstream f(path);
            std::string a, b;
            std::getline(f, a);
            std::getline(f, b);
            require(b == "invalid", "failed save preserves original");
        }
        std::filesystem::remove(path);
        auto tmp = path;
        tmp += L".tmp";
        std::filesystem::remove(tmp);
        std::cout << "PASS: original-encoder packet fixture, 1000 presets, v1 migration/backup, "
                     "setting values/auto modes, Unicode, PTZ values, "
                     "quantization, corrupt-file rejection, atomic-save preservation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
