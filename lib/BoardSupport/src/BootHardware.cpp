#include "BootHardware.h"

#include <Arduino.h>
#include <SDPowerControl.h>
#include <driver/gpio.h>

#include "TargetConfig.h"
#include "X4ProBoard.h"

namespace papyrix::board {

void earlyInit(const BoardProfile& profile) {
#if PAPYRIX_TARGET_X4PRO
  if (profile.id == BoardId::X4Pro) {
    x4pro::earlyInit(profile);
    return;
  }
#endif
  if (profile.power.latchPin == kPinUnused) return;
  gpio_hold_dis(static_cast<gpio_num_t>(profile.power.latchPin));
  pinMode(profile.power.latchPin, OUTPUT);
  digitalWrite(profile.power.latchPin, profile.power.latchActiveHigh ? HIGH : LOW);
}

void preparePanelProbe(const BoardProfile& profile) {
  if (profile.storage.transport != StorageTransport::Spi || profile.storage.powerPin == kPinUnused) return;
  papyrix::sd::prepareSdForDisplayProbe(profile.storage.powerPin);
}

}  // namespace papyrix::board
