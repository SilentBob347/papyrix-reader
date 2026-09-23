#include "HardwareRecovery.h"

#ifdef ARDUINO
#include <Arduino.h>
#endif

namespace papyrix::board {

eink::DisplayController fixedPanelFor(BoardId board) {
  if (board == BoardId::X4Pro) return eink::DisplayController::SSD1677;
  if (board == BoardId::X3) return eink::DisplayController::UC8253;
  return eink::DisplayController::SSD1677;
}

#ifdef ARDUINO
bool retryButtonPressed(const BoardProfile& profile) {
  if (profile.input.power == kPinUnused) return false;
  pinMode(profile.input.power, profile.input.activeHigh ? INPUT_PULLDOWN : INPUT_PULLUP);
  return digitalRead(profile.input.power) == (profile.input.activeHigh ? HIGH : LOW);
}

void waitForRecoveryRetry(const BoardProfile& profile) {
  bool released = false;
  while (true) {
    if (!retryButtonPressed(profile)) {
      released = true;
    } else if (released) {
      return;
    }
    delay(10);
  }
}
#endif

}  // namespace papyrix::board
