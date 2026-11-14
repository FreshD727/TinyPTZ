#include "core.hpp"
#include "protocol.hpp"

namespace ptz {
void Camera::writePanTilt(long pan, long tilt) {
    // A V4L2 single-axis write preserves the other half of the UVC control,
    // which can still contain the parked tilt after a native sleep/wake.
    // Use the same absolute motor-angle command as the Windows backend.
    request(0x6444, true, motorAnglePayload(pan, tilt), false);
}

void Camera::setAxis(long property, long value) {
    auto limits = range(property);
    if (!limits.supported || value < limits.min || value > limits.max)
        throw std::runtime_error("Position outside the supported camera range.");
    wake(true);
    value = quantize(value, limits);
    if (property == CameraControl_Pan || property == CameraControl_Tilt) {
        // Read after waking: asleep V4L2 coordinates may be the parked pose.
        auto measured = read();
        if (property == CameraControl_Pan)
            measured.pan = value;
        else
            measured.tilt = value;
        writePanTilt(measured.pan, measured.tilt);
    } else {
        standardWrite(false, property, value, false);
    }
}

void Camera::move(Position position) {
    auto pan = range(CameraControl_Pan), tilt = range(CameraControl_Tilt),
         zoom = range(CameraControl_Zoom);
    if (!pan.supported || !tilt.supported || !zoom.supported ||
        position.pan < pan.min || position.pan > pan.max ||
        position.tilt < tilt.min || position.tilt > tilt.max ||
        position.zoom < zoom.min || position.zoom > zoom.max)
        throw std::runtime_error("Preset outside camera limits.");
    wake(true);
    writePanTilt(quantize(position.pan, pan), quantize(position.tilt, tilt));
    standardWrite(false, CameraControl_Zoom, quantize(position.zoom, zoom), false);
}
} // namespace ptz
