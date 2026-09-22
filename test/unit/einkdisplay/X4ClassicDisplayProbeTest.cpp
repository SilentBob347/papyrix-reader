#include <Arduino.h>
#include <X4ClassicDisplayProbe.h>
#include <cassert>

namespace {
constexpr papyrix::eink::X3DisplayProbePins pins{12, 11, 13, 14, 10, 18};
int clockLevel = LOW;
unsigned samples = 0;
bool busy = false;
int readPin(int pin) {
  if (pin == pins.busy) return busy ? LOW : HIGH;
  assert(pin == pins.sda && clockLevel == HIGH && samples < 24);
  const uint8_t data[] = {0x00, 0x0F, 0x68};
  const int bit = (data[samples / 8] >> (7 - samples % 8)) & 1;
  ++samples;
  return bit;
}
void writePin(int pin, int value) {
  if (pin == pins.sclk) clockLevel = value;
}
}

int main() {
  testSetManualMillis(0);
  testSetDigitalReadHook(readPin);
  testSetDigitalWriteHook(writePin);
  const auto found = papyrix::eink::probeX4ClassicDisplayController(pins);
  assert(found.valid && found.id == 0x68 && samples == 24);
  busy = true;
  samples = 0;
  testSetManualMillis(0);
  const auto timeout = papyrix::eink::probeX4ClassicDisplayController(pins);
  assert(!timeout.valid && samples == 0 && millis() <= 400);
  testSetDigitalReadHook(nullptr);
  testSetDigitalWriteHook(nullptr);
  testUseRealtimeMillis();
}
