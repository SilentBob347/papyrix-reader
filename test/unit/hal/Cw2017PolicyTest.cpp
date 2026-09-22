#include <BatteryMonitor.h>
#include <Cw2017Policy.h>
#include <Wire.h>

extern bool testManualMillisEnabled;
extern unsigned long testManualMillisValue;

#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("CW2017 policy");
  const BatteryMonitor::Cw2017Config proConfig{39, 38, 100000, 0x63,
                                             papyrix::battery::kCw2017BatteryProfile.data()};
  runner.expectTrue(papyrix::battery::cw2017VersionIsRunning(0x0D), "version 0x0D is running");
  runner.expectTrue(papyrix::battery::cw2017VersionIsRunning(0x0F), "version 0x0F is running");
  runner.expectFalse(papyrix::battery::cw2017VersionIsRunning(0xA0), "startup version is not ready");
  runner.expectEq(4096, static_cast<int>(papyrix::battery::cw2017Millivolts(0x3333)),
                  "VCELL conversion uses the OEM formula");
  Wire.reset();
  Wire.setPresent(0x63, true);
  Wire.setRegister(0x63, 0x00, 0x0D);
  Wire.setRegister(0x63, 0x0B, 0x80);
  for (size_t i = 0; i < papyrix::battery::kCw2017BatteryProfile.size(); ++i) {
    Wire.setRegister(0x63, 0x10 + i, papyrix::battery::kCw2017BatteryProfile[i]);
  }
  Wire.setRegister(0x63, 0x04, 75);
  Wire.setRegister(0x63, 0x02, 0x33);
  Wire.setRegister(0x63, 0x03, 0x33);
  testManualMillisEnabled = true;
  testManualMillisValue = 100;
  BatteryMonitor gauge(proConfig);
  auto status = gauge.readStatus();
  runner.expectTrue(status.percentageKnown && status.millivoltsKnown, "initial gauge sample is available");
  runner.expectEq(uint16_t{75}, status.percentage, "gauge reports its measured charge");
  Wire.setRegister(0x63, 0x04, 10);
  runner.expectEq(uint16_t{75}, gauge.readStatus().percentage, "rapid reads reuse the cached sample");
  testManualMillisValue = 1100;
  Wire.setPresent(0x63, false);
  status = gauge.readStatus();
  runner.expectTrue(status.percentageKnown && status.millivoltsKnown, "transient NACK retains last good sample");
  runner.expectEq(uint16_t{75}, status.percentage, "transient NACK does not report an empty battery");
  Wire.setPresent(0x63, true);
  Wire.setRegister(0x63, 0x10, 0x12);
  Wire.setRegister(0x63, 0x04, 40);
  testManualMillisValue = 2100;
  runner.expectEq(uint16_t{40}, gauge.readStatus().percentage, "polling resumes after a transient failure");
  runner.expectEq(uint8_t{0x12}, Wire.getRegister(0x63, 0x10), "sample failure does not rewrite the gauge profile");
  testManualMillisValue = 3100;
  Wire.setRegister(0x63, 0x04, 255);
  runner.expectEq(uint16_t{40}, gauge.readStatus().percentage, "invalid SOC retains last good sample");
  runner.expectEq(size_t{1}, Wire.beginCount(), "gauge polling does not reinitialize the shared I2C bus");

  // === Transient profile-read failure: no rewrite, no mode restart, later recovery ===
  Wire.reset();
  Wire.setPresent(0x63, true);
  Wire.setRegister(0x63, 0x00, 0x0D);
  Wire.setRegister(0x63, 0x08, 0x00);
  Wire.setRegister(0x63, 0x0B, 0x80);
  for (size_t i = 0; i < papyrix::battery::kCw2017BatteryProfile.size(); ++i) {
    Wire.setRegister(0x63, static_cast<uint16_t>(0x10 + i), papyrix::battery::kCw2017BatteryProfile[i]);
  }
  Wire.setRegister(0x63, 0x02, 0x33);
  Wire.setRegister(0x63, 0x03, 0x33);
  Wire.setRegister(0x63, 0x04, 75);
  Wire.setTransientReadFault(0x63, 0x10, 1);
  testManualMillisEnabled = true;
  testManualMillisValue = 100;
  BatteryMonitor recovering(proConfig);
  const auto nacked = recovering.readStatus();
  runner.expectFalse(nacked.percentageKnown, "failed profile verification reports unknown charge");
  runner.expectFalse(nacked.millivoltsKnown, "failed profile verification reports unknown voltage");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x08),
                  "failed profile read does not restart the gauge mode");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x0B),
                  "failed profile read does not rewrite the update flag");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x10), "failed profile read does not rewrite the profile");
  testManualMillisValue = 1200;
  const auto recovered = recovering.readStatus();
  runner.expectTrue(recovered.percentageKnown && recovered.millivoltsKnown,
                    "gauge recovers after a transient profile-read failure");
  runner.expectEq(uint16_t{75}, recovered.percentage, "recovered gauge reports its measured charge");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x08), "recovery does not restart the gauge mode");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x0B), "recovery does not rewrite the update flag");
  runner.expectEq(size_t{0}, Wire.registerWriteCount(0x63, 0x10), "recovery does not rewrite the profile");

  // === Confirmed mismatch is distinct from a read failure: repair and restart ===
  Wire.reset();
  Wire.setPresent(0x63, true);
  Wire.setRegister(0x63, 0x00, 0x0D);
  Wire.setRegister(0x63, 0x08, 0x00);
  Wire.setRegister(0x63, 0x0B, 0x80);
  for (size_t i = 0; i < papyrix::battery::kCw2017BatteryProfile.size(); ++i) {
    Wire.setRegister(0x63, static_cast<uint16_t>(0x10 + i), papyrix::battery::kCw2017BatteryProfile[i]);
  }
  Wire.setRegister(0x63, 0x10, 0x12);
  Wire.setRegister(0x63, 0x02, 0x33);
  Wire.setRegister(0x63, 0x03, 0x33);
  Wire.setRegister(0x63, 0x04, 75);
  testManualMillisValue = 100;
  BatteryMonitor repairing(proConfig);
  const auto repaired = repairing.readStatus();
  runner.expectTrue(repaired.percentageKnown, "confirmed mismatch completes initialization");
  runner.expectEq(uint16_t{75}, repaired.percentage, "repaired gauge reports its measured charge");
  runner.expectEq(uint8_t{0x50}, Wire.getRegister(0x63, 0x10), "confirmed mismatch repairs the stored profile byte");
  runner.expectEq(size_t{3}, Wire.registerWriteCount(0x63, 0x08), "confirmed mismatch restarts the gauge mode");
  runner.expectEq(size_t{1}, Wire.registerWriteCount(0x63, 0x0B), "confirmed mismatch rewrites the update flag");
  runner.expectEq(size_t{1}, Wire.registerWriteCount(0x63, 0x10), "confirmed mismatch rewrites the profile byte");

  for (bool ready : {true, false}) {
    Wire.reset();
    Wire.setPresent(0x63, true);
    Wire.setRegister(0x63, 0x00, ready ? 0x0D : 0xA0);
    Wire.setRegister(0x63, 0x0B, ready ? 0x80 : 0);
    Wire.setRegister(0x63, 0x10, 0x12);
    Wire.setRegister(0x63, 0x04, 61);
    testManualMillisValue = 100;
    BatteryMonitor classic(BatteryMonitor::Cw2017Config{39, 38});
    const auto preserved = classic.readStatus();
    runner.expectTrue(preserved.percentageKnown == ready, "Classic reports only a running factory gauge");
    if (ready) runner.expectEq(uint16_t{61}, preserved.percentage, "Classic reads factory-calibrated SOC");
    bool untouched = true;
    for (uint16_t reg = 0; reg < 0x60; ++reg) untouched &= Wire.registerWriteCount(0x63, reg) == 0;
    runner.expectTrue(untouched, "Classic never writes gauge calibration");
    runner.expectEq(uint8_t{0x12}, Wire.getRegister(0x63, 0x10), "Classic preserves a different factory table");
  }
  Wire.reset();
  testManualMillisValue = 100;
  BatteryMonitor absent(BatteryMonitor::Cw2017Config{39, 38});
  runner.expectFalse(absent.readStatus().percentageKnown, "Classic I2C failure reports unknown charge");
  testManualMillisEnabled = false;
  return runner.allPassed() ? 0 : 1;
}
