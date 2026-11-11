#pragma once
#include <array>
#include <cstdint>
namespace ptz {
// Tiny SE power message, derived from the installed libdev legacy encoder.
std::array<unsigned char, 60> wakePacket(bool awake, std::uint16_t sequence);

} // namespace ptz
