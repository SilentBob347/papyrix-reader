#include <Display.h>
#include <HardwareIdentity.h>
#include <SPI.h>

#include <algorithm>
#include <cstdlib>

#include "test_utils.h"

namespace {
bool failBusy = false;
unsigned reads = 0;
int readBusy(int) {
  testManualMillisValue += 100;
  if (failBusy) return HIGH;
  return reads++ % 4 < 2 ? LOW : HIGH;
}
bool sent(uint8_t value) {
  const size_t count = std::min(SPI.transferCount, MockSPI::RECORD_CAPACITY);
  return std::find(SPI.transferValues, SPI.transferValues + count, value) != SPI.transferValues + count;
}
}

int main() {
  TestUtils::TestRunner runner("Display facade dispatch and sleep");
  using papyrix::eink::DisplayController;
  using papyrix::hal::Display;
  auto& identity = papyrix::board::HardwareIdentity::instance();
  Display display;
  testSetManualMillis(100);
  testSetDigitalReadHook(readBusy);
  for (const auto panel : {DisplayController::UC8179_X4PRO, DisplayController::UC8279_X4PRO}) {
    identity.setPanel(panel, papyrix::board::PanelSelectionSource::Probe, 0x68);
    SPI.reset();
    runner.expectTrue(display.begin() == Display::InitResult::Ok, "facade initializes the selected X4 Pro panel");
    runner.expectTrue(sent(panel == DisplayController::UC8179_X4PRO ? 0x06 : 0x30),
                      "facade emits the selected controller's distinct initialization command");
    reads = 0;
    display.displayBuffer(Display::FULL_REFRESH, false);
    SPI.reset();
    failBusy = true;
    runner.expectFalse(display.deepSleep(), "facade reports a failed power-off handshake");
    runner.expectTrue(sent(0x02) && !sent(0x07), "failed power-off cannot send deep sleep");
    SPI.reset();
    failBusy = false;
    reads = 0;
    runner.expectTrue(display.deepSleep(), "facade can complete sleep after BUSY recovers");
    runner.expectTrue(sent(0x02) && sent(0x07), "sleep recovery retries power-off before deep sleep");
  }
  testSetDigitalReadHook(nullptr);
  testManualMillisEnabled = false;
  std::free(display.getFrameBuffer());
  return runner.allPassed() ? 0 : 1;
}
