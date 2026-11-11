#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
namespace ptz {
long quantize(long v, Range r) {
    if (!r.supported || r.step < 1 || r.min > r.max)
        throw std::runtime_error("Invalid camera range");
    long long clamped = std::clamp(v, r.min, r.max);
    long long steps = (clamped - r.min + r.step / 2) / r.step;
    return (long)std::min<long long>(r.max, (long long)r.min + steps * r.step);
}
// UTF-8 names and device IDs, quoted to preserve whitespace. No firmware slot limit.
void PresetStore::load() {
    std::vector<Preset> loaded;
    if (!std::filesystem::exists(path_)) {
        items.clear();
        return;
    }
    std::ifstream f(path_, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot read preset file");
    std::string line;
    if (!std::getline(f, line))
        throw std::runtime_error("Empty preset file");
    if (!line.empty() && line.back() == '\r')
        line.pop_back();
    if (line != "TINY_PTZ_PRESETS 1" && line != "TINY_PTZ_PRESETS 2")
        throw std::runtime_error("Unsupported preset file version");
    bool version2 = line == "TINY_PTZ_PRESETS 2";
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        std::istringstream s(line);
        std::string name, device;
        Preset p;
        if (!(s >> std::quoted(name) >> std::quoted(device) >> p.position.pan >> p.position.tilt >>
              p.position.zoom))
            throw std::runtime_error("Invalid preset record; original file preserved");
        if (version2) {
            size_t count = 0;
            std::set<std::string> keys;
            if (!(s >> count) || count > 512)
                throw std::runtime_error("Invalid preset settings count");
            for (size_t i = 0; i < count; ++i) {
                SettingValue v;
                int automatic;
                if (!(s >> std::quoted(v.key) >> v.value >> automatic) || !std::isfinite(v.value) ||
                    v.key.empty() || (automatic != 0 && automatic != 1))
                    throw std::runtime_error("Invalid preset settings; original file preserved");
                if (v.key == "backlight") {
                    v.key = "exposure_compensation";
                    v.value -= 9;
                }
                if (!keys.insert(v.key).second)
                    throw std::runtime_error("Duplicate preset setting");
                v.automatic = automatic != 0;
                p.settings.push_back(std::move(v));
            }
        }
        s >> std::ws;
        if (!s.eof() || name.empty())
            throw std::runtime_error("Invalid preset record; original file preserved");
        p.name = wide(name);
        p.device = wide(device);
        loaded.push_back(std::move(p));
    }
    if (!f.eof())
        throw std::runtime_error("Could not finish reading preset file");
    items = std::move(loaded);
}
void PresetStore::save() const {
    if (!path_.parent_path().empty())
        std::filesystem::create_directories(path_.parent_path());
    auto temp = path_;
    temp += L".tmp";
    {
        std::ofstream f(temp, std::ios::binary | std::ios::trunc);
        if (!f)
            throw std::runtime_error(
                "Preset folder is not writable. Move the portable app to a writable folder.");
        f << "TINY_PTZ_PRESETS 2\n" << std::setprecision(17);
        for (auto &p : items) {
            if (p.name.empty() || p.name.find_first_of(L"\r\n") != std::wstring::npos ||
                p.device.find_first_of(L"\r\n") != std::wstring::npos)
                throw std::runtime_error("Preset names cannot be empty or contain newlines");
            f << std::quoted(utf8(p.name)) << ' ' << std::quoted(utf8(p.device)) << ' '
              << p.position.pan << ' ' << p.position.tilt << ' ' << p.position.zoom << ' '
              << p.settings.size();
            if (p.settings.size() > 512)
                throw std::runtime_error("Too many settings in one preset");
            std::set<std::string> keys;
            for (auto &v : p.settings) {
                if (v.key.empty() || v.key.find_first_of("\r\n") != std::string::npos ||
                    !std::isfinite(v.value) || !keys.insert(v.key).second)
                    throw std::runtime_error("Invalid or duplicate preset setting");
                f << ' ' << std::quoted(v.key) << ' ' << v.value << ' ' << (v.automatic ? 1 : 0);
            }
            f << '\n';
        }
        f.flush();
        if (!f)
            throw std::runtime_error("Could not write presets; original file preserved");
    }
    if (std::filesystem::exists(path_)) {
        std::ifstream old(path_, std::ios::binary);
        std::string header;
        std::getline(old, header);
        if (!header.empty() && header.back() == '\r')
            header.pop_back();
        old.close();
        if (header == "TINY_PTZ_PRESETS 1") {
            auto backup = path_;
            backup += L".v1.backup";
            std::filesystem::copy_file(path_, backup, std::filesystem::copy_options::skip_existing);
        }
    }
    if (!MoveFileExW(temp.c_str(), path_.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Could not replace preset file; original file preserved");
}
} // namespace ptz
