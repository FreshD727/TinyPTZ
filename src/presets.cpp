#include "core.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <limits>
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
    if (!std::getline(f, line) || line != "TINY_PTZ_PRESETS 1")
        throw std::runtime_error("Unsupported preset file version");
    while (std::getline(f, line)) {
        if (line.empty())
            continue;
        std::istringstream s(line);
        std::string name, device;
        Preset p;
        if (!(s >> std::quoted(name) >> std::quoted(device) >> p.position.pan >> p.position.tilt >>
              p.position.zoom))
            throw std::runtime_error("Invalid preset record; original file preserved");
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
        f << "TINY_PTZ_PRESETS 1\n";
        for (auto &p : items) {
            if (p.name.empty() || p.name.find_first_of(L"\r\n") != std::wstring::npos ||
                p.device.find_first_of(L"\r\n") != std::wstring::npos)
                throw std::runtime_error("Preset names cannot be empty or contain newlines");
            f << std::quoted(utf8(p.name)) << ' ' << std::quoted(utf8(p.device)) << ' '
              << p.position.pan << ' ' << p.position.tilt << ' ' << p.position.zoom << '\n';
        }
        f.flush();
        if (!f)
            throw std::runtime_error("Could not write presets; original file preserved");
    }
    if (!MoveFileExW(temp.c_str(), path_.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        throw std::runtime_error("Could not replace preset file; original file preserved");
}
} // namespace ptz
