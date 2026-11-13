#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace ptz {
// Tiny SE protocol-3 power report, matched against the installed SDK USB transfer.
std::array<unsigned char, 60> wakePacket(bool awake, std::uint16_t sequence);

struct MotorAngles {
    double pan, tilt;
};
MotorAngles decodeMotorAngles(const std::vector<unsigned char> &payload);
long roundedAngle(double degrees);
// Native absolute gimbal target: unchanged roll, inverted UI tilt, pan (float32 LE).
std::vector<unsigned char> motorAnglePayload(double pan, double tilt);
// Explicit power actions must not be skipped based on a cached/reported run state.
bool needsPowerCommand(bool awake, unsigned char reportedState, bool sessionAwake,
                       bool explicitRequest);
} // namespace ptz
