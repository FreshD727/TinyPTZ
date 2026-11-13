#include "protocol.hpp"
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace ptz {
namespace {
std::uint16_t crc(const unsigned char *data, size_t length) {
    std::uint16_t value = 0xffff;
    for (size_t i = 0; i < length; ++i) {
        value ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            value = (value >> 1) ^ ((value & 1) ? 0xa001 : 0);
    }
    return value ^ 0xffff;
}
void word(std::array<unsigned char, 60> &b, size_t offset, std::uint16_t value) {
    b[offset] = (unsigned char)value;
    b[offset + 1] = (unsigned char)(value >> 8);
}
} // namespace
std::array<unsigned char, 60> wakePacket(bool awake, std::uint16_t sequence) {
    std::array<unsigned char, 60> frame{};
    frame[0] = 0xaa;
    frame[1] = 0x25;
    word(frame, 2, sequence);
    word(frame, 4, 12);
    frame[8] = 0x0a;
    frame[9] = 2;
    word(frame, 10, 0xa0c2);
    word(frame, 6, crc(frame.data(), 12));
    word(frame, 12, 4);
    frame[16] = awake ? 0 : 1; // Protocol 3 uint32 sleep flag, not DevStatus enum.
    word(frame, 14, crc(frame.data() + 12, 8));
    // Required fixed-report compatibility byte observed in the vendor's USB
    // transfer and verified independently of its incidental camera-ID padding.
    frame[32] = 1;
    return frame;
}
MotorAngles decodeMotorAngles(const std::vector<unsigned char> &payload) {
    if (payload.size() != 18 && payload.size() != 24)
        throw std::runtime_error("Unsupported native gimbal telemetry layout");
    auto angle = [&](size_t offset) {
        auto raw = (std::int16_t)((unsigned)payload[offset] | ((unsigned)payload[offset + 1] << 8));
        return raw / 10.0;
    };
    MotorAngles result{angle(10), -angle(8)};
    if (result.pan < -130 || result.pan > 130 || result.tilt < -90 || result.tilt > 90)
        throw std::runtime_error("Native motor angles are outside camera limits");
    return result;
}
long roundedAngle(double degrees) {
    if (!std::isfinite(degrees) || degrees < -180 || degrees > 180)
        throw std::runtime_error("Invalid gimbal angle");
    return std::lround(degrees);
}
std::vector<unsigned char> motorAnglePayload(double pan, double tilt) {
    if (!std::isfinite(pan) || !std::isfinite(tilt) || pan < -180 || pan > 180 ||
        tilt < -90 || tilt > 90)
        throw std::runtime_error("Invalid native motor target");
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
    std::vector<unsigned char> payload;
    for (float value : {-1000.f, static_cast<float>(-tilt), static_cast<float>(pan)}) {
        std::uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        for (int byte = 0; byte < 4; ++byte)
            payload.push_back(static_cast<unsigned char>(bits >> (8 * byte)));
    }
    return payload;
}
bool needsPowerCommand(bool awake, unsigned char reportedState, bool sessionAwake,
                       bool explicitRequest) {
    return explicitRequest || reportedState != (awake ? 1 : 3) || (awake && !sessionAwake);
}
} // namespace ptz
