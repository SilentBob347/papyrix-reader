#include "hal/BatteryChargePolicy.h"

#include "test_utils.h"

using papyrix::board::kPinUnused;
using papyrix::hal::battery_policy::resolveCharging;

int main() {
  TestUtils::TestRunner runner("Battery charge-status policy");

  const auto gauge = resolveCharging(true, true, 21, true, false);
  runner.expectTrue(gauge.known, "known gauge state has precedence");
  runner.expectTrue(gauge.charging, "known gauge charging state is preserved");

  const auto activeHigh = resolveCharging(false, false, 21, true, true);
  runner.expectTrue(activeHigh.known, "configured active-high status is available");
  runner.expectTrue(activeHigh.charging, "high level means charging on X4 Pro");

  const auto inactiveHigh = resolveCharging(false, false, 21, true, false);
  runner.expectTrue(inactiveHigh.known, "configured inactive level remains observable");
  runner.expectFalse(inactiveHigh.charging, "low level means not charging on X4 Pro");

  const auto unavailable = resolveCharging(false, false, kPinUnused, false, true);
  runner.expectFalse(unavailable.known, "unconfigured charge status stays unavailable");
  runner.expectFalse(unavailable.charging, "unavailable charge status does not report charging");

  return runner.allPassed() ? 0 : 1;
}
