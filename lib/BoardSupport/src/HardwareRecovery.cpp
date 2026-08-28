#include "HardwareRecovery.h"

#include <cstring>

#ifdef ARDUINO
#include <Arduino.h>
#include <Logging.h>

#include "TargetConfig.h"
#endif

namespace papyrix::board {

eink::DisplayController fixedPanelFor(BoardId board) {
  if (board == BoardId::X4Pro) return eink::DisplayController::SSD1677;
  if (board == BoardId::X3) return eink::DisplayController::UC8253;
  return eink::DisplayController::SSD1677;
}

#ifdef ARDUINO
namespace {

bool readRetryCommand() {
  static char line[16] = {};
  static size_t used = 0;
  while (logSerial.available() > 0) {
    const char value = static_cast<char>(logSerial.read());
    if (value == '\r') continue;
    if (value == '\n') {
      line[used] = '\0';
      const bool retry = strcmp(line, "retry") == 0;
      used = 0;
      return retry;
    }
    if (used + 1 < sizeof(line)) line[used++] = value;
  }
  return false;
}

}  // namespace

bool retryButtonPressed(const BoardProfile& profile) {
  if (profile.input.power == kPinUnused) return false;
  pinMode(profile.input.power, profile.input.activeHigh ? INPUT_PULLDOWN : INPUT_PULLUP);
  return digitalRead(profile.input.power) == (profile.input.activeHigh ? HIGH : LOW);
}

void waitForRecoveryRetry(const BoardProfile& profile) {
  logSerial.println("display recovery: press power or send retry");
  bool released = false;
  while (true) {
    if (!retryButtonPressed(profile)) {
      released = true;
    } else if (released || readRetryCommand()) {
      return;
    }
    if (readRetryCommand()) return;
    delay(10);
  }
}
#endif

}  // namespace papyrix::board
