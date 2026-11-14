#include "core.hpp"
#include <cmath>
#include <cstring>
#include <iostream>
#include <map>

namespace {
void require(bool condition, const char *message) {
    if (!condition) throw std::runtime_error(message);
}
struct DeviceState {
    ptz::Position measured{49, 0, 0};
    ptz::Position cached{49, -84, 0};
    bool awake = false;
    std::map<long, ptz::Range> ranges{{CameraControl_Pan, {-130, 130, 1, 0, true}},
                                    {CameraControl_Tilt, {-90, 90, 1, 0, true}},
                                    {CameraControl_Zoom, {0, 12, 1, 0, true}}};
    std::vector<ptz::Position> targets;
    std::vector<std::pair<long, long>> standardWrites;
    std::vector<std::string> events;
} *device;
template<class F> void rejects(F action) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error &) { rejected = true; }
    require(rejected, "invalid movement must be rejected");
}
}

// Transport/telemetry stand-ins. The real Linux setAxis/move implementation is
// linked into this test, along with the real encoder and quantization function.
namespace ptz {
Range Camera::range(long property) const { return device->ranges[property]; }
void Camera::wake(bool awake, bool) {
    require(awake, "motion should wake the camera");
    device->awake = true;
    device->events.push_back("wake");
}
Position Camera::read() {
    device->events.push_back("read");
    return device->awake ? device->measured : device->cached;
}
void Camera::standardWrite(bool image, int property, long value, bool automatic) {
    require(!image && !automatic, "PTZ writes should be manual camera controls");
    device->standardWrites.push_back({property, value});
    if (property == CameraControl_Pan) {
        device->measured.pan = value;
        device->measured.tilt = device->cached.tilt;
    } else if (property == CameraControl_Tilt) {
        device->measured.pan = device->cached.pan;
        device->measured.tilt = value;
    } else if (property == CameraControl_Zoom) {
        device->measured.zoom = value;
    }
}
std::vector<unsigned char> Camera::request(unsigned short command, bool write,
                                         const std::vector<unsigned char> &payload, bool wait) {
    require(device->awake, "wake must precede native movement");
    require(command == 0x6444 && write && !wait, "use native absolute motor target");
    require(payload.size() == 12, "motor target must contain three float32 values");
    float values[3];
    std::memcpy(values, payload.data(), sizeof(values));
    require(values[0] == -1000.f, "leave unused roll axis unchanged");
    device->measured.pan = std::lround(values[2]);
    device->measured.tilt = std::lround(-values[1]);
    device->targets.push_back(device->measured);
    device->events.push_back("target");
    return {};
}
void Camera::disconnect() {}
// Preset I/O is not part of the motion unit; these only resolve its unused
// text-conversion dependencies when linking the real quantize implementation.
std::string utf8(const std::wstring &) { throw std::runtime_error("unexpected preset I/O"); }
std::wstring wide(const std::string &) { throw std::runtime_error("unexpected preset I/O"); }
}

int main() {
    try {
        DeviceState state; device = &state;
        ptz::Camera camera;
        require(camera.read().tilt == -84, "fixture must expose stale parked coordinates while asleep");
        state.events.clear();
        camera.setAxis(CameraControl_Pan, 50);
        require(state.measured.pan == 50 && state.measured.tilt == 0,
                "pan-only edit must preserve native tilt instead of cached -84");
        require(state.events == std::vector<std::string>{"wake", "read", "target"},
                "sample native position after waking, before movement");
        for (long pan : {51, 50, 50, -17}) {
            camera.setAxis(CameraControl_Pan, pan);
            require(state.measured.pan == pan && state.measured.tilt == 0,
                    "small steps, repeats and direction changes must retain tilt");
        }
        camera.setAxis(CameraControl_Tilt, 4);
        require(state.measured.pan == -17 && state.measured.tilt == 4,
                "tilt-only edit must preserve native pan instead of cached 49");
        camera.setAxis(CameraControl_Pan, -16);
        require(state.measured.pan == -16 && state.measured.tilt == 4,
                "pan must retain the latest measured tilt");
        require(state.standardWrites.empty(), "gimbal edits must not use V4L2 single-axis writes");
        auto targetCount = state.targets.size();
        camera.setAxis(CameraControl_Zoom, 3);
        require(state.measured.zoom == 3 && state.measured.pan == -16 && state.measured.tilt == 4 &&
                    state.targets.size() == targetCount, "zoom must retain the gimbal position");
        require(state.standardWrites == std::vector<std::pair<long,long>>{{CameraControl_Zoom, 3}},
                "zoom retains its V4L2 transport");
        state.standardWrites.clear(); state.targets.clear();
        camera.move({17, -4, 2});
        require(state.targets.size() == 1 && state.measured.pan == 17 && state.measured.tilt == -4 &&
                    state.measured.zoom == 2, "preset must send one paired gimbal target and its zoom");
        auto events = state.events.size();
        rejects([&] { camera.setAxis(CameraControl_Pan, 131); });
        rejects([&] { camera.move({0, -91, 0}); });
        state.ranges[CameraControl_Tilt].supported = false;
        rejects([&] { camera.setAxis(CameraControl_Tilt, 0); });
        require(state.events.size() == events, "reject invalid/unsupported movement before wake or writes");
        state.ranges[CameraControl_Tilt].supported = true;
        state.ranges[CameraControl_Pan].step = 2;
        camera.setAxis(CameraControl_Pan, 17);
        require(state.measured.pan == ptz::quantize(17, state.ranges[CameraControl_Pan]),
                "native targets must retain driver-reported step quantization");
        std::cout << "PASS: Linux native PTZ preserves measured companion axes across sleep/wake, "
                     "small/repeated/reversed edits, paired presets, zoom and range validation\n";
    } catch (const std::exception &error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
