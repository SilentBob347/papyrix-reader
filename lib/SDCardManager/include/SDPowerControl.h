#pragma once

#include <cstdint>

namespace papyrix::sd {

constexpr int8_t SD_CLOCK_PIN = 8;
constexpr int8_t SD_MISO_PIN = 7;
constexpr int8_t SD_MOSI_PIN = 10;
constexpr int8_t SD_CHIP_SELECT_PIN = 12;

void enableSdPower(int8_t powerEnablePin);
void prepareSdForDisplayProbe(int8_t powerEnablePin);
void disableSdPower(int8_t powerEnablePin);

}  // namespace papyrix::sd
