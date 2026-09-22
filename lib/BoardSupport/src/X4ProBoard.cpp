#include "X4ProBoard.h"

#include <Arduino.h>
#include <TargetConfig.h>
#include <driver/gpio.h>

namespace papyrix::board::x4pro {

#if PAPYRIX_TARGET_X4PRO
namespace {

void setOutput(int8_t pin, bool high) {
  if (pin == kPinUnused) return;
  gpio_hold_dis(static_cast<gpio_num_t>(pin));
  pinMode(pin, OUTPUT);
  digitalWrite(pin, high ? HIGH : LOW);
}

void setInput(int8_t pin) {
  if (pin == kPinUnused) return;
  gpio_hold_dis(static_cast<gpio_num_t>(pin));
  pinMode(pin, INPUT);
}

}  // namespace

void enableTouch(const TouchConfig& touch) {
  setOutput(touch.powerPin, touch.powerActiveHigh);
  delay(50);

  if (touch.irq == kPinUnused || touch.rst == kPinUnused) return;
  gpio_hold_dis(static_cast<gpio_num_t>(touch.rst));
  gpio_hold_dis(static_cast<gpio_num_t>(touch.irq));
  pinMode(touch.irq, OUTPUT);
  pinMode(touch.rst, OUTPUT);
  digitalWrite(touch.rst, LOW);
  digitalWrite(touch.irq, LOW);
  delay(10);
  digitalWrite(touch.rst, HIGH);
  delay(10);
  digitalWrite(touch.irq, LOW);
  delay(50);
  pinMode(touch.irq, INPUT);
  delay(50);
}

void prepareCharacterization(const BoardProfile& profile) {
  setInput(profile.display.sclk);
  setInput(profile.display.mosi);
  setInput(profile.display.cs);
  setInput(profile.display.dc);
  setInput(profile.display.rst);
  setInput(profile.display.busy);
  setOutput(profile.storage.powerPin, !profile.storage.powerActiveHigh);
  setOutput(profile.frontLight.gpio, !profile.frontLight.activeHigh);
  setOutput(profile.frontLight.warmGpio, !profile.frontLight.activeHigh);
}

#endif

}  // namespace papyrix::board::x4pro
