#include <Arduino.h>
#include <X4ClassicDisplayProbe.h>
#include <driver/gpio.h>

namespace papyrix::eink {

X4ClassicProbeResult probeX4ClassicDisplayController(const X3DisplayProbePins& pins) {
  const auto release = [&]() {
    pinMode(pins.sclk, INPUT);
    pinMode(pins.sda, INPUT);
    pinMode(pins.cs, INPUT_PULLUP);
    pinMode(pins.dc, INPUT);
    pinMode(pins.reset, INPUT);
    pinMode(pins.busy, INPUT);
  };
  pinMode(pins.cs, OUTPUT);
  digitalWrite(pins.cs, HIGH);
  pinMode(pins.sclk, OUTPUT);
  digitalWrite(pins.sclk, LOW);
  pinMode(pins.dc, OUTPUT);
  digitalWrite(pins.dc, LOW);
  pinMode(pins.sda, OUTPUT);
  pinMode(pins.busy, INPUT);
  gpio_hold_dis(static_cast<gpio_num_t>(pins.reset));
  pinMode(pins.reset, OUTPUT);
  digitalWrite(pins.reset, HIGH);
  delay(10);
  digitalWrite(pins.reset, LOW);
  delay(50);
  digitalWrite(pins.reset, HIGH);
  const uint32_t start = millis();
  while (digitalRead(pins.busy) == LOW) {
    if (millis() - start >= 300) {
      release();
      return {false, 0};
    }
    delay(1);
  }
  digitalWrite(pins.cs, LOW);
  uint8_t command = 0x70;
  for (unsigned bit = 0; bit < 8; ++bit) {
    digitalWrite(pins.sda, (command & 0x80) ? HIGH : LOW);
    delayMicroseconds(1);
    digitalWrite(pins.sclk, HIGH);
    delayMicroseconds(1);
    digitalWrite(pins.sclk, LOW);
    command <<= 1;
  }
  digitalWrite(pins.dc, HIGH);
  pinMode(pins.sda, INPUT);
  uint8_t bytes[3]{};
  for (auto& byte : bytes) {
    for (unsigned bit = 0; bit < 8; ++bit) {
      delayMicroseconds(1);
      digitalWrite(pins.sclk, HIGH);
      delayMicroseconds(1);
      byte = static_cast<uint8_t>((byte << 1) | (digitalRead(pins.sda) == HIGH));
      digitalWrite(pins.sclk, LOW);
    }
  }
  digitalWrite(pins.cs, HIGH);
  release();
  return {true, bytes[2]};
}

}  // namespace papyrix::eink
