#include "drivers/X3Power.h"

#include <SDPowerControl.h>

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
  TestUtils::TestRunner runner("X3 deep-sleep pin power");

  testResetGpioEvents();
  papyrix::drivers::prepareX3PinsForDeepSleep();

  runner.expectEq(12, static_cast<int>(testGpioEventCount), "records twelve pin events");
  runner.expectTrue(matches(0, TestGpioEventType::PinMode, papyrix::sd::SD_CHIP_SELECT_PIN, INPUT),
                    "releases SD CS before rail shutdown");
  runner.expectTrue(matches(1, TestGpioEventType::PinMode, papyrix::sd::SD_CLOCK_PIN, INPUT),
                    "releases SD clock before rail shutdown");
  runner.expectTrue(matches(2, TestGpioEventType::PinMode, papyrix::sd::SD_MISO_PIN, INPUT),
                    "releases SD MISO before rail shutdown");
  runner.expectTrue(matches(3, TestGpioEventType::PinMode, papyrix::sd::SD_MOSI_PIN, INPUT),
                    "releases SD MOSI before rail shutdown");
  runner.expectTrue(matches(4, TestGpioEventType::HoldDisable, 13, 0), "releases SD power hold");
  runner.expectTrue(matches(5, TestGpioEventType::PinMode, 13, OUTPUT), "sets SD power output");
  runner.expectTrue(matches(6, TestGpioEventType::DigitalWrite, 13, LOW), "turns SD power off");
  runner.expectTrue(matches(7, TestGpioEventType::HoldEnable, 13, 0), "holds SD power off");
  runner.expectTrue(matches(8, TestGpioEventType::HoldDisable, 5, 0), "releases display reset hold");
  runner.expectTrue(matches(9, TestGpioEventType::PinMode, 5, OUTPUT), "sets display reset output");
  runner.expectTrue(matches(10, TestGpioEventType::DigitalWrite, 5, HIGH), "keeps display out of reset");
  runner.expectTrue(matches(11, TestGpioEventType::HoldEnable, 5, 0), "holds display reset high");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
