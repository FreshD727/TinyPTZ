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
        // SDK-observed protocol-3 prefix plus the verified fixed-report byte 32.
        auto packet = wakePacket(true, 1);
        unsigned char golden[] = {0xaa, 0x25, 1, 0, 12,   0, 0x18, 0xb8, 0x0a, 2,
                                  0xc2, 0xa0, 4, 0, 0xbe, 7, 0,    0,    0,    0};
        require(std::equal(std::begin(golden), std::end(golden), packet.begin()),
                "protocol-3 run packet fixture");
        require(packet[32] == 1, "video wake report suffix must be present");
        for (size_t i = 20; i < packet.size(); ++i)
            require(packet[i] == (i == 32 ? 1 : 0),
                    "power report must not contain a hardcoded device ID");
        auto sleep = wakePacket(false, 1);
        require(sleep[14] == 0xbf && sleep[15] == 0xfb && sleep[16] == 1 && sleep[17] == 0 &&
                    sleep[18] == 0 && sleep[19] == 0 && sleep[32] == 1,
                "protocol-3 sleep payload and checksum");
        auto sequence = wakePacket(false, 0x1234);
        require(sequence[2] == 0x34 && sequence[3] == 0x12, "protocol-3 sequence encoding");
        require(needsPowerCommand(true, 1, true, true),
                "explicit wake must repair video even when gimbal reports running");
        require(needsPowerCommand(true, 1, false, false),
                "first wake in a control session must send a power command");
        require(needsPowerCommand(true, 3, true, false), "sleeping camera needs full wake");
        require(!needsPowerCommand(true, 1, true, false),
                "ordinary PTZ checks should not resend power commands");
        require(needsPowerCommand(false, 4, false, false), "privacy is not confirmed sleep");
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
        std::cout << "PASS: protocol-3 power packet fixtures and wake recovery, 1000 presets, v1 "
                     "migration/backup, "
                     "setting values/auto modes, Unicode, PTZ values, "
                     "quantization, corrupt-file rejection, atomic-save preservation\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    }
}
