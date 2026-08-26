#pragma once

#include <cstdint>

namespace papyrix::drivers {

constexpr int8_t X3_SD_POWER_PIN = 13;
constexpr int8_t X3_DISPLAY_RESET_PIN = 5;
constexpr int8_t NO_POWER_PIN = -1;

void prepareX3PinsForDeepSleep();

}  // namespace papyrix::drivers
