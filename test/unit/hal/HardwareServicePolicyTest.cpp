#include <HardwareServicePolicy.h>
#include <BoardProfiles.h>

#include "test_utils.h"

using namespace papyrix::board;

int main() {
  TestUtils::TestRunner runner("Hardware service policy");

  const auto& profile = bootProfile();
  const auto policy = hardwareServicePolicy(profile);
  runner.expectTrue(policy.battery == profile.battery.backend, "battery backend follows the active profile");
  runner.expectTrue(policy.rtc == profile.rtc.type, "RTC backend follows the active profile");
  runner.expectEq(hasFrontLight(profile), policy.frontLightAvailable, "front-light availability follows the profile");
  runner.expectEq(hasUsbDetect(profile), policy.usbDetectionAvailable, "USB detection follows the profile");
  runner.expectEq(hasBattery(profile), policy.batteryAvailable, "battery availability follows the profile");

  return runner.allPassed() ? 0 : 1;
}
