#pragma once

#include "BoardProfile.h"

namespace papyrix::board {

struct HardwareServicePolicy {
  BatteryBackend battery;
  RtcType rtc;
  bool batteryAvailable;
  bool frontLightAvailable;
  bool usbDetectionAvailable;
};

constexpr HardwareServicePolicy hardwareServicePolicy(const BoardProfile& profile) {
  return {
      profile.battery.backend, profile.rtc.type, hasBattery(profile), hasFrontLight(profile), hasUsbDetect(profile),
  };
}

}  // namespace papyrix::board
