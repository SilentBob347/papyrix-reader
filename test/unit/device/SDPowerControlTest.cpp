#include "SDPowerControl.h"

#include "test_utils.h"

#include <Arduino.h>

namespace {

bool matches(size_t index, TestGpioEventType type, int pin, int value) {
  if (index >= testGpioEventCount) return false;
  const auto& event = testGpioEvents[index];
  return event.type == type && event.pin == pin && event.value == value;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("SD power control");

  testResetGpioEvents();
  papyrix::sd::enableSdPower(-1);
  runner.expectEq(0, static_cast<int>(testGpioEventCount), "unassigned power pin has no events");
  runner.expectEq(0, static_cast<int>(testDelayCallCount()), "unassigned power pin has no delay");

  testResetGpioEvents();
  papyrix::sd::enableSdPower(13);
  runner.expectEq(4, static_cast<int>(testGpioEventCount), "assigned power pin has four events");
  runner.expectTrue(matches(0, TestGpioEventType::HoldDisable, 13, 0), "releases retained SD hold");
  runner.expectTrue(matches(1, TestGpioEventType::PinMode, 13, OUTPUT), "sets SD power output");
  runner.expectTrue(matches(2, TestGpioEventType::DigitalWrite, 13, HIGH), "turns SD power on");
  runner.expectTrue(matches(3, TestGpioEventType::Delay, -1, 10), "waits for SD power");
  runner.expectEq(10, static_cast<int>(testDelayTotalMs()), "uses 10 ms settle delay");

  testResetGpioEvents();
  papyrix::sd::prepareSdForDisplayProbe(-1);
  runner.expectEq(0, static_cast<int>(testGpioEventCount), "unassigned probe preparation has no events");

  testResetGpioEvents();
  papyrix::sd::prepareSdForDisplayProbe(13);
  runner.expectEq(6, static_cast<int>(testGpioEventCount), "display probe preparation has six events");
  runner.expectTrue(matches(0, TestGpioEventType::HoldDisable, 13, 0), "probe preparation releases SD hold");
  runner.expectTrue(matches(1, TestGpioEventType::PinMode, 13, OUTPUT), "probe preparation sets power output");
  runner.expectTrue(matches(2, TestGpioEventType::DigitalWrite, 13, HIGH), "probe preparation powers SD");
  runner.expectTrue(matches(3, TestGpioEventType::Delay, -1, 10), "probe preparation waits for SD power");
  runner.expectTrue(matches(4, TestGpioEventType::PinMode, papyrix::sd::SD_CHIP_SELECT_PIN, OUTPUT),
                    "probe preparation sets SD CS output");
  runner.expectTrue(matches(5, TestGpioEventType::DigitalWrite, papyrix::sd::SD_CHIP_SELECT_PIN, HIGH),
                    "probe preparation deselects SD");

  testResetGpioEvents();
  papyrix::sd::disableSdPower(-1);
  runner.expectEq(0, static_cast<int>(testGpioEventCount), "unassigned SD shutdown has no events");

  testResetGpioEvents();
  papyrix::sd::disableSdPower(13);
  runner.expectEq(7, static_cast<int>(testGpioEventCount), "SD shutdown has seven events");
  runner.expectTrue(matches(0, TestGpioEventType::PinMode, papyrix::sd::SD_CHIP_SELECT_PIN, INPUT),
                    "SD shutdown releases CS");
  runner.expectTrue(matches(1, TestGpioEventType::PinMode, papyrix::sd::SD_CLOCK_PIN, INPUT),
                    "SD shutdown releases clock");
  runner.expectTrue(matches(2, TestGpioEventType::PinMode, papyrix::sd::SD_MISO_PIN, INPUT),
                    "SD shutdown releases MISO");
  runner.expectTrue(matches(3, TestGpioEventType::PinMode, papyrix::sd::SD_MOSI_PIN, INPUT),
                    "SD shutdown releases MOSI");
  runner.expectTrue(matches(4, TestGpioEventType::HoldDisable, 13, 0), "SD shutdown releases power hold");
  runner.expectTrue(matches(5, TestGpioEventType::PinMode, 13, OUTPUT), "SD shutdown sets power output");
  runner.expectTrue(matches(6, TestGpioEventType::DigitalWrite, 13, LOW), "SD shutdown turns power off");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
