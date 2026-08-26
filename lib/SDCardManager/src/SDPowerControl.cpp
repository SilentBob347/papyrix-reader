#include "SDPowerControl.h"

#include <Arduino.h>
#include <driver/gpio.h>

namespace papyrix::sd {

void enableSdPower(int8_t powerEnablePin) {
  if (powerEnablePin < 0) return;

  gpio_hold_dis(static_cast<gpio_num_t>(powerEnablePin));
  pinMode(powerEnablePin, OUTPUT);
  digitalWrite(powerEnablePin, HIGH);
  delay(10);
}

void prepareSdForDisplayProbe(int8_t powerEnablePin) {
  if (powerEnablePin < 0) return;

  enableSdPower(powerEnablePin);
  pinMode(SD_CHIP_SELECT_PIN, OUTPUT);
  digitalWrite(SD_CHIP_SELECT_PIN, HIGH);
}

void disableSdPower(int8_t powerEnablePin) {
  if (powerEnablePin < 0) return;

  pinMode(SD_CHIP_SELECT_PIN, INPUT);
  pinMode(SD_CLOCK_PIN, INPUT);
  pinMode(SD_MISO_PIN, INPUT);
  pinMode(SD_MOSI_PIN, INPUT);
  gpio_hold_dis(static_cast<gpio_num_t>(powerEnablePin));
  pinMode(powerEnablePin, OUTPUT);
  digitalWrite(powerEnablePin, LOW);
}

}  // namespace papyrix::sd
