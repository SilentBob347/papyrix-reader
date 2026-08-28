#include <cstdint>

#include "hal/BatteryEstimatePolicy.h"
#include "test_utils.h"

using papyrix::hal::battery_estimate::RetainedEstimate;
using papyrix::hal::battery_estimate::displayPercent;
using papyrix::hal::battery_estimate::isUsable;
using papyrix::hal::battery_estimate::seed;
using papyrix::hal::battery_estimate::update;

int main() {
  TestUtils::TestRunner runner("Battery estimate policy");

  const auto initial = seed(86);
  runner.expectTrue(isUsable(initial, 86), "seeded estimate is usable");
  runner.expectEq(uint16_t(86), displayPercent(initial), "first sample displays raw percentage");

  const RetainedEstimate retained94 = seed(94);
  runner.expectTrue(isUsable(retained94, 86), "plausible retained estimate survives restart");
  runner.expectEq(uint16_t(94), displayPercent(retained94), "restart keeps the last displayed percentage");

  const auto filtered = update(retained94, 86);
  runner.expectEq(uint16_t(93), displayPercent(filtered), "EMA moves gradually toward raw percentage");

  const auto afterRestart = update(filtered, 85);
  runner.expectEq(uint16_t(92), displayPercent(afterRestart), "retained EMA continues after restart");

  RetainedEstimate corrupt = retained94;
  corrupt.magic = 0;
  runner.expectFalse(isUsable(corrupt, 86), "invalid magic rejects retained estimate");

  RetainedEstimate outOfRange = retained94;
  outOfRange.scaledPercent = 1001;
  runner.expectFalse(isUsable(outOfRange, 86), "out-of-range retained estimate is rejected");

  runner.expectFalse(isUsable(seed(100), 40), "implausible retained delta is rejected");
  runner.expectEq(uint16_t(40), displayPercent(update(seed(100), 40)),
                  "implausible retained delta resets to raw percentage");

  return runner.allPassed() ? 0 : 1;
}
