#pragma once
#include <array>
#include "platform.hpp"
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef _WIN32
#include <ks.h>
#include <ksproxy.h>
#include <wrl/client.h>
#endif
namespace ptz {
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
#endif
struct Device {
    std::wstring name, id;
};
struct Position {
    long pan = 0, tilt = 0, zoom = 0;
};
struct Range {
    long min = 0, max = 0, step = 1, normal = 0;
    bool supported = false;
};
struct SettingValue {
    std::string key;
    double value = 0;
    bool automatic = false;
};
struct Setting {
    SettingValue current, defaultValue;
    bool defaultAvailable = false;
    std::wstring name, group, details;
    double minimum = 0, maximum = 1, step = 1;
    bool readable = false, writable = false, autoSupported = false;
    std::vector<std::pair<double, std::wstring>> choices;
};
struct Preset {
    std::wstring name, device;
    Position position;
    std::vector<SettingValue> settings;
};
std::vector<Device> devices();
std::string utf8(const std::wstring &);
std::wstring wide(const std::string &);
std::filesystem::path portableData();
class Camera {
#ifdef _WIN32
    ComPtr<IBaseFilter> filter_;
    ComPtr<IAMCameraControl> control_;
    ComPtr<IAMVideoProcAmp> procamp_;
    ComPtr<IGraphBuilder> graph_;
    ComPtr<IMediaControl> media_;
    ComPtr<IVideoWindow> video_;
    ComPtr<IKsControl> extension_;
    ULONG node_ = 0;
    void writePanTilt(long pan, long tilt);
#else
    int fd_ = -1;
    unsigned char unit_ = 0;
#endif
    unsigned short sequence_ = 0;
    bool awakeConfirmed_ = false;
    std::map<std::string, SettingValue> appliedSettings_;
    std::wstring id_;
    void discoverExtension();
    Range standardRange(bool image, int property, long &support) const;
    long standardRead(bool image, int property, long &flags) const;
    void standardWrite(bool image, int property, long value, bool automatic);
    void setAntiFlicker(long value);
    std::vector<unsigned char> extension(ULONG selector, bool write,
                                         std::vector<unsigned char> data = {});
    std::vector<unsigned char> request(unsigned short command, bool write,
                                       const std::vector<unsigned char> &payload = {},
                                       bool waitForReply = true);

  public:
    Camera() = default;
    Camera(const Camera &) = delete;
    Camera &operator=(const Camera &) = delete;
    void connect(const std::wstring &id);
    void disconnect();
    bool connected() const {
#ifdef _WIN32
        return control_ != nullptr;
#else
        return fd_ >= 0;
#endif
    }
    const std::wstring &id() const {
        return id_;
    }
    Range range(long property) const;
    Position read();
    void waitForPosition(Position target);
    void setAxis(long property, long value);
    void move(Position position);
    void wake(bool awake, bool explicitRequest = false);
    bool sleeping();
    std::string diagnostics();
    std::string positionDiagnostics();
    std::vector<Setting> settings(const std::string &onlyKey = {});
    std::vector<SettingValue> snapshotSettings();
    // Live intermediate writes skip readback; callers verify the final value.
    void applySetting(const SettingValue &value, bool verify = true);
    void verifySetting(const SettingValue &value);
    void recall(const Preset &preset);
    void restoreImageDefaults();
    std::vector<unsigned char> bootRecord();
    void writeBootRecord(const std::vector<unsigned char> &record);
    void setBootPreset(const Preset &preset);
    void activateBootPosition();
    std::string bootDiagnostics();
    void preview(HWND owner, int width, int height);
    void resizePreview(int width, int height);
    bool previewRunning() const {
#ifdef _WIN32
        OAFilterState state = State_Stopped;
        return media_ && SUCCEEDED(media_->GetState(2000, &state)) && state == State_Running;
#else
        return false;
#endif
    }
    void stopPreview();
    ~Camera() {
        try {
            disconnect();
        } catch (...) {
        }
    }
};
class PresetStore {
    std::filesystem::path path_;

  public:
    std::vector<Preset> items;
    explicit PresetStore(std::filesystem::path path) : path_(std::move(path)) {}
    void load();
    void save() const;
    const std::filesystem::path &path() const {
        return path_;
    }
};
long quantize(long value, Range r);
} // namespace ptz
