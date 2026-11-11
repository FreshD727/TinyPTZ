#pragma once
#include <array>
#include <cstdint>
namespace ptz {
// Tiny SE protocol-3 power report, matched against the installed SDK USB transfer.
std::array<unsigned char, 60> wakePacket(bool awake, std::uint16_t sequence);

// Explicit power actions must not be skipped based on a cached/reported run state.
bool needsPowerCommand(bool awake, unsigned char reportedState, bool sessionAwake,
                       bool explicitRequest);
} // namespace ptz
