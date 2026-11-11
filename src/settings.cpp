#include "core.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <thread>
#include <vidcap.h>
namespace ptz {
namespace {
enum class Backend { Camera, Image, Raw, Ai, Scalar, Block, ReadOnly, Applied };
struct Definition {
    const char *key;
    const wchar_t *name;
    const wchar_t *group;
    Backend backend;
    int property;
    int offset;
    int bytes;
    unsigned short get = 0, set = 0;
    double min = 0, max = 1, step = 1;
    std::vector<std::pair<double, std::wstring>> choices;
};
using Options = std::vector<std::pair<double, std::wstring>>;
Options onOff{{0, L"Off"}, {1, L"On"}};
const std::vector<Definition> &definitions() {
    static auto defs = [] {
        std::vector<Definition> d;
        auto standard = [&](const char *k, const wchar_t *n, Backend b, int p) {
            d.push_back({k, n, L"Image", b, p, 0, 0});
        };
        standard("brightness", L"Brightness", Backend::Image, VideoProcAmp_Brightness);
        standard("contrast", L"Contrast", Backend::Image, VideoProcAmp_Contrast);
        standard("hue", L"Hue", Backend::Image, VideoProcAmp_Hue);
        standard("saturation", L"Saturation", Backend::Image, VideoProcAmp_Saturation);
        standard("sharpness", L"Sharpness", Backend::Image, VideoProcAmp_Sharpness);
        standard("gamma", L"Gamma", Backend::Image, VideoProcAmp_Gamma);
        standard("white_balance", L"White balance temperature", Backend::Image,
                 VideoProcAmp_WhiteBalance);
        standard("exposure_compensation", L"Exposure Compensation", Backend::Image,
                 VideoProcAmp_BacklightCompensation);
        standard("gain", L"Gain", Backend::Image, VideoProcAmp_Gain);
        standard("focus", L"Focus", Backend::Camera, CameraControl_Focus);
        standard("exposure", L"Exposure (driver units)", Backend::Camera, CameraControl_Exposure);
        auto raw = [&](const char *k, const wchar_t *n, const wchar_t *g, int command, int offset,
                       int bytes, double lo, double hi, Options options = {}) {
            d.push_back({k, n, g, Backend::Raw, command, offset, bytes, 0, 0, lo, hi, 1,
                         std::move(options)});
        };
        raw("hdr", L"HDR", L"Image", 1, 6, 1, 0, 1, onOff);
        raw("anti_flicker", L"Anti-flicker", L"Image", 0, 3, 1, 0, 3,
            {{0, L"Off"}, {1, L"50 Hz"}, {2, L"60 Hz"}, {3, L"Auto"}});
        d.back().set = 0x2902;
        raw("face_exposure", L"Face priority exposure", L"Image", 3, 7, 1, 0, 1, onOff);
        raw("face_focus", L"Face priority focus", L"Image", 0, 13, 1, 0, 1, onOff);
        d.back().set = 0x3602;
        raw("fov", L"Field of view", L"Image", 4, 17, 1, 0, 2,
            {{0, L"Wide"}, {1, L"Medium"}, {2, L"Narrow"}});
        raw("mirror", L"Mirror image", L"Image", 0x14, 19, 1, 0, 1, onOff);
        raw("noise_reduction", L"Microphone noise reduction", L"Device", 0xa, 8, 1, 0, 3,
            {{0, L"Off"}, {1, L"Low"}, {2, L"Medium"}, {3, L"High"}});
        raw("microphone_sleep", L"Microphone during sleep", L"Device", 0x13, 16, 1, 0, 1, onOff);
        raw("audio_auto_gain", L"Microphone automatic gain", L"Device", 0x17, 25, 1, 0, 1, onOff);
        raw("auto_sleep", L"Auto sleep timeout (seconds, 0=off)", L"Device", 0xb, 10, 2, 0, 32767);
        raw("audio_distance", L"Microphone reception distance", L"Device", 0x1b, 34, 1, 0, 2,
            {{0, L"Near"}, {1, L"Standard"}, {2, L"Far"}});
        raw("boot_mode", L"Startup tracking mode", L"Device", 0x19, 32, 1, 0, 5,
            {{0, L"Off"},
             {1, L"Group"},
             {2, L"Human"},
             {3, L"Hand"},
             {4, L"Whiteboard"},
             {5, L"Desk"}});
        raw("boot_framing", L"Startup human framing", L"Device", 0x19, 32, 1, 0, 4,
            {{0, L"Normal"},
             {1, L"Upper body"},
             {2, L"Close-up"},
             {3, L"Headless"},
             {4, L"Lower body"}});
        raw("sleep_background", L"Sleep background slot (already uploaded)", L"Device", 0xe, 27, 2,
            0, 2);
        raw("sleep_background_mirror", L"Mirror sleep background", L"Device", 0xe, 29, 2, 0, 1,
            onOff);
        raw("ai_mode", L"Tracking mode", L"Tracking", 0x16, 24, 2, 0, 5,
            {{0, L"Off"},
             {1, L"Group (firmware dependent)"},
             {2, L"Human"},
             {3, L"Hand"},
             {4, L"Whiteboard (firmware dependent)"},
             {5, L"Desk (firmware dependent)"}});
        raw("human_framing", L"Human body-part framing", L"Tracking", 0x16, 28, 2, 0, 4,
            {{0, L"Normal / full body"},
             {1, L"Upper body"},
             {2, L"Close-up"},
             {3, L"Headless"},
             {4, L"Lower body"}});
        auto ai = [&](const char *k, const wchar_t *n, int offset, unsigned short set, double lo,
                      double hi, Options options = {}) {
            d.push_back({k, n, L"Tracking", Backend::Ai, 0, offset, 1, 0x0104, set, lo, hi, 1,
                         std::move(options)});
        };
        ai("tracking_style", L"Tracking style", 9, 0x0cc4, 0, 2,
           {{0, L"Standard"}, {1, L"Headroom"}, {2, L"Motion"}});
        ai("hand", L"Hand to track", 2, 0x2244, 0, 1, {{0, L"Right"}, {1, L"Left"}});
        ai("gesture_tracking", L"Gesture: select tracking target", 3, 0x30c4, 0, 1, onOff);
        ai("gesture_zoom", L"Gesture: zoom", 4, 0x3144, 0, 1, onOff);
        ai("gesture_dynamic_zoom", L"Gesture: dynamic zoom", 5, 0x3344, 0, 1, onOff);
        ai("gesture_mirror", L"Gesture: reverse zoom direction", 6, 0x33c4, 0, 1, onOff);
        ai("gesture_zoom_factor", L"Gesture zoom factor", 7, 0x3244, 1, 4);
        d.back().bytes = 2;
        d.back().step = .1;
        ai("gimbal_reverse", L"Reverse gimbal pan direction", 11, 0x3b84, 0, 1, onOff);

        auto scalar = [&](const char *k, const wchar_t *n, unsigned short get, unsigned short set,
                          bool real, double lo, double hi, double step = 1) {
            d.push_back({k,
                         n,
                         L"Tracking",
                         Backend::Scalar,
                         real ? 1 : 0,
                         0,
                         real ? 4 : 1,
                         get,
                         set,
                         lo,
                         hi,
                         step,
                         {}});
        };
        ai("zone_enabled", L"Zone tracking", 10, 0x08c4, 0, 1, onOff);
        scalar("zone_auto_select", L"Zone: automatically select new target", 0x4e44, 0x4e04, false,
               0, 1);
        d.back().choices = onOff;
        scalar("zone_pan_min", L"Zone: minimum pan", 0x4c44, 0x4c04, true, -130, 130, .1);
        scalar("zone_pan_max", L"Zone: maximum pan", 0x4cc4, 0x4c84, true, -130, 130, .1);
        scalar("zone_pitch_min", L"Zone: minimum pitch (positive down)", 0x4d44, 0x4d04, true, -90,
               90, .1);
        scalar("zone_pitch_max", L"Zone: maximum pitch (positive down)", 0x4dc4, 0x4d84, true, -90,
               90, .1);
        scalar("composition_x", L"Composition horizontal offset", 0x0c04, 0x0bc4, true, -1, 1, .01);
        scalar("composition_y", L"Composition vertical offset", 0x0c84, 0x0c44, true, -1, 1, .01);
        auto block = [&](const char *k, const wchar_t *n, unsigned short get, unsigned short set,
                         int offset, bool pose, double lo, double hi) {
            d.push_back({k,
                         n,
                         L"Tracking",
                         Backend::Block,
                         pose ? 0 : 1,
                         offset,
                         4,
                         get,
                         set,
                         lo,
                         hi,
                         1,
                         {}});
        };
        block("zone_home_pan", L"Zone home: pan", 0x4ec4, 0x4e84, 4, true, -130, 130);
        block("zone_home_pitch", L"Zone home: pitch (positive down)", 0x4ec4, 0x4e84, 8, true, -90,
              90);
        block("zone_home_zoom", L"Zone home: zoom ratio", 0x4ec4, 0x4e84, 16, true, 1, 4);
        d.back().step = .1;
        block("hand_pan_min", L"Hand range: minimum pan", 0x2004, 0x2044, 0, false, -130, 130);
        block("hand_pan_max", L"Hand range: maximum pan", 0x2004, 0x20c4, 4, false, -130, 130);
        block("hand_pitch_min", L"Hand range: minimum pitch", 0x2004, 0x2144, 8, false, -90, 90);
        block("hand_pitch_max", L"Hand range: maximum pitch", 0x2004, 0x21c4, 12, false, -90, 90);
        d.push_back({"image_style",
                     L"Image style",
                     L"Image",
                     Backend::Applied,
                     2,
                     0,
                     4,
                     0,
                     0x2ec2,
                     0,
                     5,
                     1,
                     {{0, L"Standard"},
                      {1, L"Text"},
                      {2, L"Landscape"},
                      {3, L"Portrait"},
                      {4, L"Nightscape"},
                      {5, L"Film"}}});
        d.push_back({"auto_zoom", L"AI automatic zoom", L"Tracking", Backend::Applied, 0, 0, 1, 0,
                     0x0a44, 0, 1, 1, onOff});
        d.push_back({"led_level",
                     L"LED brightness (read only)",
                     L"Device",
                     Backend::ReadOnly,
                     0,
                     33,
                     1,
                     0,
                     0,
                     0,
                     3,
                     1,
                     {}});
        return d;
    }();
    return defs;
}
const Definition &find(const std::string &key) {
    auto &d = definitions();
    auto i = std::find_if(d.begin(), d.end(), [&](auto &x) { return key == x.key; });
    if (i == d.end())
        throw std::runtime_error("Unknown setting: " + key);
    return *i;
}
unsigned short word(const unsigned char *p) {
    return p[0] | (p[1] << 8);
}
unsigned short crc(const unsigned char *p, size_t n) {
    unsigned short c = 0xffff;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int b = 0; b < 8; ++b)
            c = (c >> 1) ^ ((c & 1) ? 0xa001 : 0);
    }
    return c ^ 0xffff;
}
void put(std::vector<unsigned char> &b, size_t offset, unsigned short value) {
    b[offset] = value & 255;
    b[offset + 1] = value >> 8;
}
std::vector<unsigned char> floatData(double value) {
    float f = (float)value;
    std::vector<unsigned char> b(4);
    std::memcpy(b.data(), &f, 4);
    return b;
}
} // namespace
std::vector<unsigned char> Camera::extension(ULONG selector, bool write,
                                             std::vector<unsigned char> data) {
    discoverExtension();
    if (data.empty())
        data.resize(60);
    if (data.size() != 60)
        throw std::runtime_error("Invalid extension data length");
    KSP_NODE p{};
    p.Property.Set = {0x9a1e7291, 0x6843, 0x4683, {0x6d, 0x92, 0x39, 0xbc, 0x79, 0x06, 0xee, 0x49}};
    p.Property.Id = selector;
    p.Property.Flags =
        (write ? KSPROPERTY_TYPE_SET : KSPROPERTY_TYPE_GET) | KSPROPERTY_TYPE_TOPOLOGY;
    p.NodeId = node_;
    ULONG n = 0;
    HRESULT hr = extension_->KsProperty((PKSPROPERTY)&p, sizeof(p), data.data(), 60, &n);
    if (FAILED(hr) || (!write && n != 60)) {
        std::ostringstream s;
        s << "Camera extension failed (0x" << std::hex << (unsigned long)hr << ")";
        throw std::runtime_error(s.str());
    }
    return data;
}
std::vector<unsigned char> Camera::request(unsigned short command, bool write,
                                           const std::vector<unsigned char> &payload) {
    if (payload.size() > 44)
        throw std::runtime_error("Vendor payload is too large");
    std::vector<unsigned char> b(60);
    b[0] = 0xaa;
    b[1] = write ? (payload.empty() ? 5 : 0x25) : (payload.empty() ? 1 : 0x21);
    put(b, 2, ++sequence_);
    put(b, 4, 12);
    b[8] = 0xa;
    b[9] = command & 0x3f;
    put(b, 10, command);
    put(b, 6, crc(b.data(), 12));
    if (!payload.empty()) {
        put(b, 12, (unsigned short)payload.size());
        std::copy(payload.begin(), payload.end(), b.begin() + 16);
        put(b, 14, crc(b.data() + 12, payload.size() + 4));
    }
    extension(2, true, b);
    for (int attempt = 0; attempt < 12; ++attempt) {
        std::this_thread::sleep_for(std::chrono::milliseconds(35));
        auto r = extension(2, false);
        if (r[0] != 0xaa || word(r.data() + 2) != sequence_ || word(r.data() + 10) != command ||
            r[8] != (command & 0x3f) || r[9] != 0xa)
            continue;
        auto length = word(r.data() + 4);
        if (length != 12 && length != 16)
            continue;
        auto token = word(r.data() + 6);
        r[6] = r[7] = 0;
        if (crc(r.data(), length) != token)
            throw std::runtime_error("Camera reply checksum mismatch");
        if (r[1] == 0x89) {
            std::ostringstream s;
            s << "Camera rejected setting command 0x" << std::hex << command << " (error 0x"
              << word(r.data() + 14) << ")";
            throw std::runtime_error(s.str());
        }
        if (r[1] == 9) {
            return {};
        }
        if (r[1] != 0x29)
            continue;
        auto n = word(r.data() + 12);
        if (n > 44)
            throw std::runtime_error("Invalid reply payload length");
        auto payloadToken = word(r.data() + 14);
        r[14] = r[15] = 0;
        if (crc(r.data() + 12, n + 4) != payloadToken)
            throw std::runtime_error("Setting payload checksum mismatch");
        return {r.begin() + 16, r.begin() + 16 + n};
    }
    if (write)
        return {};
    throw std::runtime_error("Setting command timed out; read settings before retrying");
}
std::vector<Setting> Camera::settings() {
    std::vector<Setting> result;
    std::vector<unsigned char> status, ai;
    std::map<unsigned short, std::vector<unsigned char>> blocks;
    try {
        status = extension(6, false);
    } catch (...) {
    }
    if (!status.empty() && status[9] == 1)
        try {
            ai = request(0x0104, false);
        } catch (...) {
        }
    for (auto &d : definitions()) {
        Setting s;
        s.current.key = d.key;
        s.name = d.name;
        s.group = d.group;
        s.minimum = d.min;
        s.maximum = d.max;
        s.step = d.step;
        s.choices = d.choices;
        try {
            if (d.backend == Backend::Camera || d.backend == Backend::Image) {
                long lo = 0, hi = 0, step = 0, normal = 0, support = 0, value = 0, flags = 0;
                HRESULT hr =
                    d.backend == Backend::Camera
                        ? (control_
                               ? control_->GetRange(d.property, &lo, &hi, &step, &normal, &support)
                               : E_NOINTERFACE)
                        : (procamp_
                               ? procamp_->GetRange(d.property, &lo, &hi, &step, &normal, &support)
                               : E_NOINTERFACE);
                if (FAILED(hr))
                    throw std::runtime_error("Not supported by this camera driver");
                hr = d.backend == Backend::Camera ? control_->Get(d.property, &value, &flags)
                                                  : procamp_->Get(d.property, &value, &flags);
                if (FAILED(hr))
                    throw std::runtime_error("Camera could not read this setting");
                s.defaultValue = {d.key, (double)normal, (support & 1) != 0};
                s.defaultAvailable = true;
                s.current.value = value;
                if (s.current.key == "exposure_compensation") {
                    s.current.value -= 9;
                    lo -= 9;
                    hi -= 9;
                    s.defaultValue.value = 0;
                }
                s.current.automatic = (flags & 1) != 0;
                s.minimum = lo;
                s.maximum = hi;
                s.step = std::max(1L, step);
                s.autoSupported = (support & 1) != 0;
                s.readable = true;
                s.writable = (support & 3) != 0;
            } else if (d.backend == Backend::Raw || d.backend == Backend::ReadOnly) {
                if (status.empty() || status[0] <= d.offset)
                    throw std::runtime_error("Unavailable in this firmware status layout");
                auto value = status[d.offset];
                s.current.value = value;
                if (s.current.key == "auto_sleep")
                    s.current.value = (short)word(status.data() + 10);
                if (s.current.key == "audio_distance")
                    s.current.value = value & 15;
                if (s.current.key == "boot_mode")
                    s.current.value = value >> 5;
                if (s.current.key == "boot_framing")
                    s.current.value = value & 31;
                s.readable = true;
                s.writable = d.backend != Backend::ReadOnly;
            } else if (d.backend == Backend::Ai) {
                if (ai.size() < 12)
                    throw std::runtime_error("Wake camera to read tracking settings");
                s.current.value = d.bytes == 2 ? word(ai.data() + d.offset) / 10.0 : ai[d.offset];
                s.readable = true;
                s.writable = d.set != 0;
            } else if (d.backend == Backend::Block) {
                if (status.empty() || status[9] != 1)
                    throw std::runtime_error("Wake camera to read tracking range");
                if (!blocks.count(d.get))
                    blocks[d.get] = request(d.get, false);
                auto &value = blocks[d.get];
                if (value.size() < (size_t)d.offset + 4)
                    throw std::runtime_error("Tracking range not configured or unavailable");
                float f;
                std::memcpy(&f, value.data() + d.offset, 4);
                if (!std::isfinite(f) || f < d.min || f > d.max)
                    throw std::runtime_error("Tracking range is not configured");
                s.current.value = f;
                s.readable = true;
                s.writable = true;
            } else if (d.backend == Backend::Scalar) {
                if (status.empty() || status[9] != 1)
                    throw std::runtime_error("Wake camera to read advanced settings");
                auto value = request(d.get, false);
                if (value.size() < (size_t)d.bytes)
                    throw std::runtime_error("Setting not returned by this firmware");
                if (d.property) {
                    float f;
                    std::memcpy(&f, value.data(), 4);
                    if (!std::isfinite(f))
                        throw std::runtime_error("Invalid setting value");
                    s.current.value = f;
                } else
                    s.current.value = value[0];
                s.readable = true;
                s.writable = true;
            } else {
                auto i = appliedSettings_.find(d.key);
                s.writable = true;
                if (i != appliedSettings_.end()) {
                    s.current = i->second;
                    s.details = L"Last requested by this app; device readback unavailable";
                } else
                    s.details = L"Set only; last request can be saved after applying";
            }
            if (s.group == L"Image" && s.writable && !s.defaultAvailable) {
                const std::map<std::string, double> defaults{
                    {"hdr", 0}, {"anti_flicker", 1}, {"face_exposure", 1}, {"face_focus", 1},
                    {"fov", 0}, {"mirror", 0},       {"image_style", 0}};
                auto i = defaults.find(s.current.key);
                if (i != defaults.end()) {
                    s.defaultValue = {s.current.key, i->second, false};
                    s.defaultAvailable = true;
                }
            }
            if (s.details.empty())
                s.details = s.writable ? L"Camera readback" : L"Read only";
        } catch (const std::exception &e) {
            s.details = wide(e.what());
            s.readable = false;
            s.writable = false;
        }
        result.push_back(std::move(s));
    }
    return result;
}
void Camera::restoreImageDefaults() {
    wake(true);
    auto all = settings();
    auto get = [&](const std::string &key) -> Setting * {
        auto i =
            std::find_if(all.begin(), all.end(), [&](auto &s) { return s.current.key == key; });
        return i == all.end() ? nullptr : &*i;
    };
    for (auto key : {"image_style", "hdr"}) {
        auto *s = get(key);
        if (s && s->writable && s->defaultAvailable)
            applySetting(s->defaultValue);
    }
    auto *exposure = get("exposure"), *gain = get("gain");
    if (gain && gain->writable && gain->defaultAvailable && exposure && exposure->writable) {
        auto manual = exposure->current;
        manual.automatic = false;
        applySetting(manual);
        applySetting(gain->defaultValue);
    }
    for (auto &s : all)
        if (s.group == L"Image" && s.writable && s.defaultAvailable &&
            s.current.key != "image_style" && s.current.key != "hdr" && s.current.key != "gain" &&
            s.current.key != "exposure")
            applySetting(s.defaultValue);
    if (exposure && exposure->writable && exposure->defaultAvailable)
        applySetting(exposure->defaultValue);
}
std::vector<unsigned char> Camera::bootRecord() {
    wake(true);
    std::vector<unsigned char> b;
    for (int attempt = 0; attempt < 3; ++attempt) {
        try {
            b = request(0x3884, false);
            break;
        } catch (...) {
            if (attempt == 2)
                throw;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        }
    }
    if (b.size() != 24)
        throw std::runtime_error("Unsupported camera boot-position layout");
    return b;
}
void Camera::writeBootRecord(const std::vector<unsigned char> &b) {
    if (b.size() != 24)
        throw std::runtime_error("Invalid boot position");
    wake(true);
    request(0x3844, true, b);
    auto actual = bootRecord();
    for (int offset : {4, 8, 12, 16}) {
        float expected, got;
        std::memcpy(&expected, b.data() + offset, 4);
        std::memcpy(&got, actual.data() + offset, 4);
        if (!std::isfinite(got) || std::abs(got - expected) > .02)
            throw std::runtime_error("Camera did not confirm stored boot position");
    }
}
void Camera::setBootPreset(const Preset &p) {
    if (p.device != id_)
        throw std::runtime_error("Preset belongs to a different camera");
    for (auto entry : {std::pair<int, long>{CameraControl_Pan, p.position.pan},
                       {CameraControl_Tilt, p.position.tilt},
                       {CameraControl_Zoom, p.position.zoom}}) {
        auto r = range(entry.first);
        if (!r.supported || entry.second < r.min || entry.second > r.max)
            throw std::runtime_error("Preset position is outside camera limits");
    }
    auto zoom = range(CameraControl_Zoom);
    if (zoom.min != 0 || zoom.max != 12 || zoom.step != 1)
        throw std::runtime_error(
            "Boot preset conversion requires the validated Tiny SE zoom range (0..12)");
    auto original = bootRecord();
    auto b = original;
    for (auto field : {std::pair<int, double>{4, (double)p.position.pan},
                       {8, -(double)p.position.tilt},
                       {12, 0},
                       {16, 1.0 + p.position.zoom * .25},
                       {20, 0}}) {
        auto bytes = floatData(field.second);
        std::copy(bytes.begin(), bytes.end(), b.begin() + field.first);
    }
    try {
        writeBootRecord(b);
    } catch (...) {
        try {
            writeBootRecord(original);
        } catch (...) {
        }
        throw;
    }
}
void Camera::activateBootPosition() {
    wake(true);
    request(0x3904, true);
}
std::string Camera::bootDiagnostics() {
    auto b = bootRecord();
    std::ostringstream out;
    for (int offset : {4, 8, 12, 16, 20}) {
        float v;
        std::memcpy(&v, b.data() + offset, 4);
        out << v << " ";
    }
    return out.str() + "\n";
}
std::vector<SettingValue> Camera::snapshotSettings() {
    std::vector<SettingValue> out;
    for (auto &s : settings())
        if (s.writable && (s.readable || appliedSettings_.count(s.current.key)))
            out.push_back(s.current);
    return out;
}
void Camera::applySetting(const SettingValue &v) {
    if (v.key == "backlight") {
        applySetting({"exposure_compensation", v.value - 9, v.automatic});
        return;
    }
    auto &d = find(v.key);
    if (v.automatic && d.backend != Backend::Camera && d.backend != Backend::Image)
        throw std::runtime_error("Automatic mode is not available for this setting");
    if (!std::isfinite(v.value))
        throw std::runtime_error("Invalid setting value");
    wake(true);
    if (d.backend == Backend::Camera || d.backend == Backend::Image) {
        long lo = 0, hi = 0, step = 0, normal = 0, flags = 0;
        HRESULT hr =
            d.backend == Backend::Camera
                ? control_->GetRange(d.property, &lo, &hi, &step, &normal, &flags)
                : (procamp_ ? procamp_->GetRange(d.property, &lo, &hi, &step, &normal, &flags)
                            : E_NOINTERFACE);
        if (FAILED(hr) || !(flags & (v.automatic ? 1 : 2)))
            throw std::runtime_error("Requested manual/automatic mode is unsupported");
        double driverValue = v.value + (v.key == "exposure_compensation" ? 9 : 0);
        if (!v.automatic &&
            (driverValue < lo || driverValue > hi || std::floor(driverValue) != driverValue ||
             ((long)driverValue - lo) % std::max(1L, step) != 0))
            throw std::runtime_error("Setting is outside the driver range/step");
        hr = d.backend == Backend::Camera
                 ? control_->Set(d.property, (long)driverValue, v.automatic ? 1 : 2)
                 : procamp_->Set(d.property, (long)driverValue, v.automatic ? 1 : 2);
        if (FAILED(hr))
            throw std::runtime_error("Camera rejected image setting");
    } else {
        if (d.backend == Backend::ReadOnly || v.value < d.min || v.value > d.max ||
            (d.step >= 1 && std::floor(v.value) != v.value))
            throw std::runtime_error("Setting value is unsupported or outside its range");
        if (d.backend == Backend::Raw) {
            if (v.key == "face_focus") {
                unsigned int value = (unsigned int)v.value;
                std::vector<unsigned char> bytes(4);
                std::memcpy(bytes.data(), &value, 4);
                request(d.set, true, bytes);
            } else if (v.key == "anti_flicker") {
                ComPtr<IKsTopologyInfo> topology;
                HRESULT hr = filter_.As(&topology);
                DWORD count = 0;
                bool done = false;
                if (SUCCEEDED(hr))
                    topology->get_NumNodes(&count);
                for (DWORD node = 0; node < count; ++node) {
                    ComPtr<IVideoProcAmp> proc;
                    if (FAILED(topology->CreateNodeInstance(node, __uuidof(IVideoProcAmp),
                                                            (void **)proc.GetAddressOf())))
                        continue;
                    long lo = 0, hi = 0, step = 0, normal = 0, caps = 0;
                    if (FAILED(proc->getRange_PowerlineFrequency(&lo, &hi, &step, &normal, &caps)))
                        continue;
                    hr = proc->put_PowerlineFrequency((long)v.value, 2);
                    if (SUCCEEDED(hr)) {
                        done = true;
                        break;
                    }
                }
                if (!done) {
                    std::ostringstream error;
                    error << "Camera rejected anti-flicker setting (0x" << std::hex
                          << (unsigned long)hr << ")";
                    throw std::runtime_error(error.str());
                }
            } else {
                std::vector<unsigned char> b(60);
                b[0] = (unsigned char)d.property;
                b[1] = (unsigned char)d.bytes;
                b[2] = (unsigned char)(long)v.value;
                if (v.key == "auto_sleep")
                    b[3] = (unsigned char)((long)v.value >> 8);
                if (v.key == "ai_mode") {
                    auto now = extension(6, false);
                    b[3] = now[28];
                }
                if (v.key == "human_framing") {
                    b[2] = 2;
                    b[3] = (unsigned char)v.value;
                }
                if (v.key == "boot_mode" || v.key == "boot_framing") {
                    auto now = extension(6, false);
                    int main = now[32] >> 5, sub = now[32] & 31;
                    if (v.key == "boot_mode")
                        main = (int)v.value;
                    else
                        sub = (int)v.value;
                    b[2] = (unsigned char)((main << 5) | sub);
                }
                if (v.key == "sleep_background") {
                    b[2] = 0;
                    b[3] = (unsigned char)v.value;
                }
                if (v.key == "sleep_background_mirror") {
                    b[2] = 2;
                    b[3] = (unsigned char)v.value;
                }
                extension(6, true, b);
                std::this_thread::sleep_for(std::chrono::milliseconds(v.key == "hdr" ? 3100 : 120));
            }
        } else if (d.backend == Backend::Block) {
            if (d.property)
                request(d.set, true, floatData(v.value));
            else {
                auto value = request(d.get, false);
                if (value.size() != 24)
                    throw std::runtime_error("Unsupported zone-home record layout");
                auto bytes = floatData(v.value);
                std::copy(bytes.begin(), bytes.end(), value.begin() + d.offset);
                request(d.set, true, value);
            }
        } else if (d.backend == Backend::Scalar && d.property || v.key == "gesture_zoom_factor")
            request(d.set, true, floatData(v.value));
        else {
            if (!d.set)
                throw std::runtime_error("This setting is read only");
            if (d.bytes == 4) {
                int value = (int)v.value;
                std::vector<unsigned char> bytes(4);
                std::memcpy(bytes.data(), &value, 4);
                request(d.set, true, bytes);
            } else
                request(d.set, true, {(unsigned char)v.value});
        }
    }
    if (d.backend != Backend::Applied) {
        bool confirmed = false;
        for (int attempt = 0; attempt < 3 && !confirmed; ++attempt) {
            if (attempt)
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            auto values = settings();
            auto current = std::find_if(values.begin(), values.end(),
                                        [&](auto &s) { return s.current.key == v.key; });
            confirmed = current != values.end() && current->readable &&
                        current->current.automatic == v.automatic &&
                        (v.automatic || std::abs(current->current.value - v.value) <= .02);
        }
        if (!confirmed)
            throw std::runtime_error("Camera readback did not confirm setting: " + v.key +
                                     (v.key == "gain" ? " (manual exposure may be required)"
                                      : v.key == "sleep_background"
                                          ? " (select an already uploaded slot)"
                                          : ""));
    }
    appliedSettings_[v.key] = v;
}
void Camera::recall(const Preset &p) {
    if (p.device != id_)
        throw std::runtime_error("Preset belongs to a different camera");
    wake(true);
    auto available = settings();
    auto original = available;
    auto originalPosition = read();
    for (auto &v : p.settings) {
        find(v.key);
        auto i = std::find_if(available.begin(), available.end(),
                              [&](auto &s) { return s.current.key == v.key; });
        if (i == available.end() || !i->writable)
            throw std::runtime_error("Preset setting unavailable: " + v.key);
        if (!std::isfinite(v.value) ||
            !v.automatic && (v.value < i->minimum || v.value > i->maximum))
            throw std::runtime_error("Preset setting outside supported range: " + v.key);
    }
    auto ai = std::find_if(original.begin(), original.end(),
                           [](auto &s) { return s.current.key == "ai_mode"; });
    bool controlsTracking = std::any_of(p.settings.begin(), p.settings.end(),
                                        [](auto &v) { return v.key == "ai_mode"; });
    std::vector<SettingValue> rollback;
    auto apply = [&](const SettingValue &v) {
        auto i = std::find_if(original.begin(), original.end(),
                              [&](auto &s) { return s.current.key == v.key; });
        if (i != original.end() && i->readable &&
            std::none_of(rollback.begin(), rollback.end(), [&](auto &r) { return r.key == v.key; }))
            rollback.push_back(i->current);
        applySetting(v);
    };
    auto same = [&](const SettingValue &v) {
        auto i = std::find_if(available.begin(), available.end(),
                              [&](auto &s) { return s.current.key == v.key; });
        return i != available.end() && i->readable && i->current.automatic == v.automatic &&
               (v.automatic || std::abs(i->current.value - v.value) < .0001);
    };
    try {
        if (controlsTracking)
            apply({"ai_mode", 0, false});
        move(p.position);
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        // A style can reset individual image controls, so apply it before refreshing readback.
        for (auto &v : p.settings)
            if ((v.key == "image_style" || v.key == "hdr") && !same(v)) {
                apply(v);
                available = settings();
            }
        // Gain is only writable with manual exposure on Tiny SE.
        for (auto &v : p.settings)
            if (v.key == "exposure" && !same(v))
                apply(v);
        for (auto &v : p.settings)
            if (v.key != "exposure" && v.key != "hdr" && v.key != "ai_mode" &&
                v.key != "human_framing" && v.key != "image_style" && !same(v))
                apply(v);
        for (auto &v : p.settings)
            if (v.key == "human_framing" && !same(v))
                apply(v);
        for (auto &v : p.settings)
            if (v.key == "ai_mode")
                apply(v);
    } catch (const std::exception &e) {
        bool restored = true;
        for (auto i = rollback.rbegin(); i != rollback.rend(); ++i)
            if (i->key != "ai_mode")
                try {
                    applySetting(*i);
                } catch (...) {
                    restored = false;
                }
        try {
            move(originalPosition);
        } catch (...) {
            restored = false;
        }
        if (controlsTracking && ai != original.end() && ai->readable)
            try {
                applySetting(ai->current);
            } catch (...) {
                restored = false;
            }
        throw std::runtime_error(
            std::string(e.what()) +
            (restored
                 ? ". Readable prior settings and PTZ restored; set-only requests cannot be rolled "
                   "back."
                 : ". Some settings may remain changed; read camera settings before retrying."));
    }
}
} // namespace ptz
