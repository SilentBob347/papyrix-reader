#include "DeepSleep.h"

#include <InputManager.h>
#include <SDCardManager.h>
#include <SPI.h>
#include <driver/gpio.h>
#include <esp_sleep.h>

#include "Device.h"
#include "X3Power.h"

namespace papyrix::drivers {

[[noreturn]] void enterDeepSleepWithHardwareShutdown() {
  SdMan.end();
  SPI.end();
  disableGpioPullsForSleep();
  if (Device::instance().isX3()) prepareX3PinsForDeepSleep();
  gpio_deep_sleep_hold_en();
  esp_deep_sleep_start();
  while (true) {
  }
}

}  // namespace papyrix::drivers
