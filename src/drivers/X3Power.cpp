#include "X3Power.h"

#include <Arduino.h>
#include <SDPowerControl.h>
#include <driver/gpio.h>

namespace papyrix::drivers {

void prepareX3PinsForDeepSleep() {
  papyrix::sd::disableSdPower(X3_SD_POWER_PIN);
  gpio_hold_en(static_cast<gpio_num_t>(X3_SD_POWER_PIN));

  gpio_hold_dis(static_cast<gpio_num_t>(X3_DISPLAY_RESET_PIN));
  pinMode(X3_DISPLAY_RESET_PIN, OUTPUT);
  digitalWrite(X3_DISPLAY_RESET_PIN, HIGH);
  gpio_hold_en(static_cast<gpio_num_t>(X3_DISPLAY_RESET_PIN));
}

}  // namespace papyrix::drivers
