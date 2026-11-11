#include "protocol.hpp"
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
bool needsPowerCommand(bool awake, unsigned char reportedState, bool sessionAwake,
                       bool explicitRequest) {
    return explicitRequest || reportedState != (awake ? 1 : 3) || (awake && !sessionAwake);
}
} // namespace ptz
