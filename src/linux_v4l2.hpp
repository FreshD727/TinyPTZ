#pragma once
#include "core.hpp"
#include <linux/videodev2.h>
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <functional>
#include <sys/ioctl.h>
namespace ptz::linuxuvc {
// UVC GUID byte order: first three GUID fields are little endian.
inline unsigned char extensionUnit(const std::vector<unsigned char> &descriptors) {
    static const unsigned char guid[]{0x91,0x72,0x1e,0x9a,0x43,0x68,0x83,0x46,0x6d,0x92,0x39,0xbc,0x79,0x06,0xee,0x49};
    unsigned char unit = 0;
    for (size_t i = 0; i < descriptors.size();) {
        auto length = descriptors[i];
        if (length < 2 || i + length > descriptors.size()) throw std::runtime_error("Truncated USB descriptor layout.");
        if (length >= 24 && descriptors[i+1] == 0x24 && descriptors[i+2] == 6 &&
            std::equal(std::begin(guid), std::end(guid), descriptors.begin()+i+4)) {
            if (!descriptors[i+3] || (unit && unit != descriptors[i+3])) throw std::runtime_error("Ambiguous OBSBOT extension-unit descriptors.");
            unit = descriptors[i+3];
        }
        i += length;
    }
    if (!unit) throw std::runtime_error("OBSBOT extension GUID was not found in the USB descriptors.");
    return unit;
}
struct Mapping { unsigned id = 0, automatic = 0; bool angle = false, exposure = false; };
inline Mapping mapping(bool image, int property) {
    if (!image) switch (property) {
    case CameraControl_Pan: return {V4L2_CID_PAN_ABSOLUTE, 0, true};
    case CameraControl_Tilt: return {V4L2_CID_TILT_ABSOLUTE, 0, true};
    case CameraControl_Zoom: return {V4L2_CID_ZOOM_ABSOLUTE};
    case CameraControl_Focus: return {V4L2_CID_FOCUS_ABSOLUTE, V4L2_CID_FOCUS_AUTO};
    case CameraControl_Exposure: return {V4L2_CID_EXPOSURE_ABSOLUTE, V4L2_CID_EXPOSURE_AUTO, false, true};
    default: break;
    }
    if (image) switch (property) {
    case VideoProcAmp_Brightness: return {V4L2_CID_BRIGHTNESS};
    case VideoProcAmp_Contrast: return {V4L2_CID_CONTRAST};
    case VideoProcAmp_Hue: return {V4L2_CID_HUE};
    case VideoProcAmp_Saturation: return {V4L2_CID_SATURATION};
    case VideoProcAmp_Sharpness: return {V4L2_CID_SHARPNESS};
    case VideoProcAmp_Gamma: return {V4L2_CID_GAMMA};
    case VideoProcAmp_WhiteBalance: return {V4L2_CID_WHITE_BALANCE_TEMPERATURE, V4L2_CID_AUTO_WHITE_BALANCE};
    case VideoProcAmp_BacklightCompensation: return {V4L2_CID_BACKLIGHT_COMPENSATION};
    case VideoProcAmp_Gain: return {V4L2_CID_GAIN};
    default: break;
    }
    throw std::runtime_error("This standard control has no Linux V4L2 mapping.");
}
class Controls {
    int fd_;
    std::function<int(int,unsigned long,void*)> ioctl_;
    void call(unsigned long request, void *argument, const char *what) const {
        int result; do { result = ioctl_(fd_, request, argument); } while (result < 0 && errno == EINTR);
        if (result < 0) throw std::runtime_error(std::string(what) + ": " + std::strerror(errno));
    }
    bool autoAvailable(const Mapping &m) const {
        if (!m.automatic) return false;
        try {
            auto q = query(m.automatic);
            if (q.flags & V4L2_CTRL_FLAG_READ_ONLY) return false;
            if (m.exposure) {
                for (auto mode : {V4L2_EXPOSURE_AUTO,V4L2_EXPOSURE_APERTURE_PRIORITY}) {
                    v4l2_querymenu menu{}; menu.id=m.automatic; menu.index=mode;
                    try { call(VIDIOC_QUERYMENU,&menu,"Query automatic exposure mode"); return true; } catch (...) {}
                }
                return false;
            }
            return true;
        } catch (...) { return false; }
    }
  public:
    explicit Controls(int fd, std::function<int(int,unsigned long,void*)> transport =
        [](int fd,unsigned long request,void *p) { return ::ioctl(fd, request, p); }) : fd_(fd), ioctl_(std::move(transport)) {}
    v4l2_queryctrl query(unsigned id) const {
        v4l2_queryctrl q{}; q.id = id; call(VIDIOC_QUERYCTRL, &q, "Query V4L2 control");
        if (q.flags & V4L2_CTRL_FLAG_DISABLED) throw std::runtime_error("V4L2 control is disabled on this camera.");
        if (q.type != V4L2_CTRL_TYPE_INTEGER && q.type != V4L2_CTRL_TYPE_BOOLEAN && q.type != V4L2_CTRL_TYPE_MENU)
            throw std::runtime_error("Unsupported V4L2 control type.");
        return q;
    }
    long get(unsigned id) const { v4l2_control c{}; c.id=id; call(VIDIOC_G_CTRL,&c,"Read V4L2 control"); return c.value; }
    void set(unsigned id, long value) const {
        auto q = query(id);
        if (q.flags & V4L2_CTRL_FLAG_READ_ONLY || value < q.minimum || value > q.maximum ||
            (value-q.minimum) % std::max(1, q.step)) throw std::runtime_error("V4L2 control is read-only or outside its range/step.");
        if (q.type == V4L2_CTRL_TYPE_MENU) {
            v4l2_querymenu menu{}; menu.id=id; menu.index=value; call(VIDIOC_QUERYMENU,&menu,"Unsupported V4L2 menu choice");
        }
        v4l2_control c{}; c.id=id; c.value=value; call(VIDIOC_S_CTRL,&c,"Write V4L2 control");
    }
    Range range(bool image, int property, long &support) const {
        auto m=mapping(image,property); auto q=query(m.id);
        Range r{q.minimum,q.maximum,std::max(1,q.step),q.default_value,true};
        support=(q.flags & V4L2_CTRL_FLAG_READ_ONLY) ? 0 : 2;
        if (autoAvailable(m)) support |= 1;
        if (m.angle) { r.min=std::ceil(q.minimum/3600.); r.max=std::floor(q.maximum/3600.); r.step=std::max(1L,(long)std::ceil(q.step/3600.)); r.normal=std::lround(q.default_value/3600.); }
        if (m.exposure) {
            if (q.minimum <= 0 || q.default_value <= 0) throw std::runtime_error("Invalid V4L2 exposure range.");
            r.min=std::ceil(std::log2(q.minimum/10000.)); r.max=std::floor(std::log2(q.maximum/10000.));
            if (r.min > r.max) throw std::runtime_error("No whole-stop exposure values supported by this driver.");
            r.step=1; r.normal=std::clamp((long)std::lround(std::log2(q.default_value/10000.)),r.min,r.max);
        }
        if (r.min > r.max) throw std::runtime_error("No compatible values in the V4L2 control range.");
        return r;
    }
    long read(bool image, int property, long &flags) const {
        auto m=mapping(image,property); auto value=get(m.id); flags=2;
        if (autoAvailable(m)) { auto mode=get(m.automatic); if (m.exposure ? mode != V4L2_EXPOSURE_MANUAL : mode != 0) flags=1; }
        if (m.angle) return std::lround(value/3600.);
        if (m.exposure) { if(value<=0) throw std::runtime_error("Invalid V4L2 exposure readback."); return std::lround(std::log2(value/10000.)); }
        return value;
    }
    void write(bool image, int property, long value, bool automatic) const {
        auto m=mapping(image,property);
        if (automatic && !autoAvailable(m)) throw std::runtime_error("Automatic mode is unavailable in V4L2.");
        if (autoAvailable(m)) {
            if (m.exposure && automatic) {
                bool applied=false;
                for (auto mode : {V4L2_EXPOSURE_AUTO,V4L2_EXPOSURE_APERTURE_PRIORITY}) {
                    try { set(m.automatic,mode); applied=true; break; } catch (...) {}
                }
                if (!applied) throw std::runtime_error("No supported automatic exposure mode.");
            } else set(m.automatic, m.exposure ? V4L2_EXPOSURE_MANUAL : automatic ? 1 : 0);
        }
        if (automatic) return;
        long native=value;
        if (m.angle) native=value*3600;
        if (m.exposure) {
            auto q=query(m.id); double raw=std::exp2((double)value)*10000.;
            if (!std::isfinite(raw) || raw < q.minimum || raw > q.maximum) throw std::runtime_error("Exposure outside V4L2 range.");
            native=q.minimum+std::lround((raw-q.minimum)/std::max(1,q.step))*std::max(1,q.step);
            native=std::clamp(native,(long)q.minimum,(long)q.maximum);
        }
        set(m.id,native);
    }
};
} // namespace ptz::linuxuvc
