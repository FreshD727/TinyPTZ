#include "protocol.hpp"
namespace ptz {
std::array<unsigned char, 60> wakePacket(bool awake, std::uint16_t sequence) {
    std::array<unsigned char, 60> frame{};
    frame[0] = 0xaa;
    frame[2] = 14;
    frame[3] = 0x10;
    frame[4] = (unsigned char)(sequence >> 8);
    frame[5] = (unsigned char)sequence;
    frame[9] = 0xe1;
    frame[10] = 0x13;
    frame[11] = 0xc2;
    frame[12] = 1;
    frame[13] = awake ? 1 : 0;
    // CRC-16/USB across the 14-byte legacy message; token stored big-endian.
    std::uint16_t crc = 0xffff;
    for (int i = 0; i < 14; ++i) {
        crc ^= frame[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xa001 : 0);
    }
    crc ^= 0xffff;
    frame[6] = (unsigned char)(crc >> 8);
    frame[7] = (unsigned char)crc;
    return frame;
}
} // namespace ptz
