#include <Arduino.h>

#include "test_utils.h"

#include "BatteryMonitor.h"

namespace {
int adcReadCount = 0;

uint32_t unstableAdc(uint8_t) {
  adcReadCount++;
  return adcReadCount == 1 ? 2000 : 1000;
}
}  // namespace

// The static checks lock the LiPo curve. The status check verifies that ADC
// percentage and millivolts come from one hardware sample.

int main() {
  TestUtils::TestRunner runner("BatteryMonitorPercentageTest");

  // === Operating range (3500–4150 mV) — monotonic, meaningful readings ===
  runner.expectEq(uint16_t(15), BatteryMonitor::percentageFromMillivolts(3500), "3500mV -> 15%");
  runner.expectEq(uint16_t(41), BatteryMonitor::percentageFromMillivolts(3700), "3700mV -> 41%");
  runner.expectEq(uint16_t(55), BatteryMonitor::percentageFromMillivolts(3800), "3800mV -> 55%");
  runner.expectEq(uint16_t(70), BatteryMonitor::percentageFromMillivolts(3900), "3900mV -> 70%");
  runner.expectEq(uint16_t(84), BatteryMonitor::percentageFromMillivolts(4000), "4000mV -> 84%");
  runner.expectEq(uint16_t(96), BatteryMonitor::percentageFromMillivolts(4100), "4100mV -> 96%");

  // === Curve is monotonically non-decreasing across the operating range ===
  {
    bool monotonic = true;
    uint16_t prev = BatteryMonitor::percentageFromMillivolts(3500);
    for (uint16_t mv = 3501; mv <= 4150; ++mv) {
      const uint16_t cur = BatteryMonitor::percentageFromMillivolts(mv);
      if (cur < prev) {
        monotonic = false;
        break;
      }
      prev = cur;
    }
    runner.expectTrue(monotonic, "monotonic non-decreasing in 3500–4150 mV");
  }

  // === Upper clamp (>= ~4150 mV pegs at 100%) ===
  runner.expectEq(uint16_t(100), BatteryMonitor::percentageFromMillivolts(4150), "4150mV -> 100% (clamp)");
  runner.expectEq(uint16_t(100), BatteryMonitor::percentageFromMillivolts(4200), "4200mV -> 100% (clamp)");
  runner.expectEq(uint16_t(100), BatteryMonitor::percentageFromMillivolts(4500), "4500mV -> 100% (clamp)");

  // === Lower clamp (very high mV makes the cubic go negative -> 0%) ===
  runner.expectEq(uint16_t(0), BatteryMonitor::percentageFromMillivolts(5000), "5000mV -> 0% (cubic clamps low)");

  // === Boundary semantics: result always within [0, 100] across full uint16 range ===
  {
    bool inRange = true;
    for (uint32_t mv = 0; mv <= 6000; mv += 25) {
      const uint16_t pct = BatteryMonitor::percentageFromMillivolts(static_cast<uint16_t>(mv));
      if (pct > 100) {
        inRange = false;
        break;
      }
    }
    runner.expectTrue(inRange, "0–6000 mV: result always in [0, 100]");
  }

  // === millivoltsFromRawAdc is currently identity (calibration hook) ===
  runner.expectEq(uint16_t(0), BatteryMonitor::millivoltsFromRawAdc(0), "millivoltsFromRawAdc(0) -> 0");
  runner.expectEq(uint16_t(3700), BatteryMonitor::millivoltsFromRawAdc(3700), "millivoltsFromRawAdc(3700) -> 3700");
  runner.expectEq(uint16_t(4200), BatteryMonitor::millivoltsFromRawAdc(4200), "millivoltsFromRawAdc(4200) -> 4200");
  {
    adcReadCount = 0;
    testAnalogMillivoltsHook() = unstableAdc;
    BatteryMonitor monitor(0);
    const auto status = monitor.readStatus();
    runner.expectEq(1, adcReadCount, "ADC status uses one voltage sample");
    runner.expectEq(uint16_t(4000), status.millivolts, "ADC status reports the sampled battery voltage");
    runner.expectEq(uint16_t(84), status.percentage, "ADC status derives percentage from the same voltage");
    testAnalogMillivoltsHook() = nullptr;
  }

  return runner.allPassed() ? 0 : 1;
}
