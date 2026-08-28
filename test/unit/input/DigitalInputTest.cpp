#include <Arduino.h>
#include <InputManager.h>

#include <cassert>

int main() {
  InputManager input;
  testResetGpioEvents();
  input.begin();
  for (size_t i = 0; i < testGpioEventCount; ++i) {
    assert(testGpioEvents[i].pin != 1 && testGpioEvents[i].pin != 2);
  }
  testSetDigitalReadHook([](int pin) { return pin == 7 ? LOW : HIGH; });
  assert(input.getState() == (1 << InputManager::BTN_DOWN));
  testSetDigitalReadHook([](int pin) { return pin == 0 ? LOW : HIGH; });
  assert(input.getState() == (1 << InputManager::BTN_UP));
  testSetDigitalReadHook([](int pin) { return pin == 3 ? LOW : HIGH; });
  assert(input.getState() == (1 << InputManager::BTN_POWER));
  testSetDigitalReadHook([](int) { return HIGH; });
  assert(input.getState() == 0);
  assert(testAnalogReadCount == 0);
}
