#pragma once
#include <array>
#include <dshow.h>
#include <filesystem>
#include <ks.h>
#include <ksproxy.h>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl/client.h>
namespace ptz {
using Microsoft::WRL::ComPtr;
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
    ComPtr<IBaseFilter> filter_;
    ComPtr<IAMCameraControl> control_;
    ComPtr<IAMVideoProcAmp> procamp_;
    ComPtr<IGraphBuilder> graph_;
    ComPtr<IMediaControl> media_;
    ComPtr<IVideoWindow> video_;
    ComPtr<IKsControl> extension_;
    ULONG node_ = 0;
    unsigned short sequence_ = 0;
    bool awakeConfirmed_ = false;
    std::map<std::string, SettingValue> appliedSettings_;
    std::wstring id_;
    void discoverExtension();
    std::vector<unsigned char> extension(ULONG selector, bool write,
                                         std::vector<unsigned char> data = {});
    std::vector<unsigned char> request(unsigned short command, bool write,
                                       const std::vector<unsigned char> &payload = {});

  public:
    void connect(const std::wstring &id);
    void disconnect();
    bool connected() const {
        return control_ != nullptr;
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
    std::vector<Setting> settings();
    std::vector<SettingValue> snapshotSettings();
    void applySetting(const SettingValue &value);
    void recall(const Preset &preset);
    void restoreImageDefaults();
    std::vector<unsigned char> bootRecord();
    void writeBootRecord(const std::vector<unsigned char> &record);
    void setBootPreset(const Preset &preset);
    void activateBootPosition();
    std::string bootDiagnostics();
    void preview(HWND owner, int width, int height);
    void resizePreview(int width, int height);
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
