#pragma once
#include <filesystem>
#include <string>
#ifdef _WIN32
#include <dshow.h>
#include <windows.h>
#else
#include <unistd.h>
// Public control identifiers retain their Windows values so the shared core's
// definitions and presets stay identical; the Linux backend maps them to V4L2.
using HWND = void *;
using ULONG = unsigned long;
enum { CameraControl_Pan, CameraControl_Tilt, CameraControl_Roll, CameraControl_Zoom,
       CameraControl_Exposure, CameraControl_Iris, CameraControl_Focus };
enum { VideoProcAmp_Brightness, VideoProcAmp_Contrast, VideoProcAmp_Hue,
       VideoProcAmp_Saturation, VideoProcAmp_Sharpness, VideoProcAmp_Gamma,
       VideoProcAmp_ColorEnable, VideoProcAmp_WhiteBalance,
       VideoProcAmp_BacklightCompensation, VideoProcAmp_Gain };
#endif
namespace ptz {
inline unsigned long processId() {
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return static_cast<unsigned long>(::getpid());
#endif
}
std::string utf8(const std::wstring &);
std::wstring wide(const std::string &);
inline std::filesystem::path filePath(const std::wstring &value = {}) {
#ifdef _WIN32
    return std::filesystem::path(value);
#else
    return std::filesystem::u8path(utf8(value));
#endif
}
inline std::wstring pathText(const std::filesystem::path &value) {
#ifdef _WIN32
    return value.wstring();
#else
    return wide(value.u8string());
#endif
}
} // namespace ptz
