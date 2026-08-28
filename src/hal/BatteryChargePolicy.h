#pragma once

#include <BoardProfile.h>

namespace papyrix::hal::battery_policy {

struct ChargingStatus {
  bool known;
  bool charging;
};

constexpr ChargingStatus resolveCharging(bool sourceKnown, bool sourceCharging, int8_t statusPin, bool activeHigh,
                                         bool pinHigh) {
  if (sourceKnown) return {true, sourceCharging};
  if (statusPin == board::kPinUnused) return {false, false};
  return {true, pinHigh == activeHigh};
}

}  // namespace papyrix::hal::battery_policy
