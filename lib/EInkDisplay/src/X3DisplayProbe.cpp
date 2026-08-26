#include <Arduino.h>
#include <X3DisplayProbe.h>
#include <driver/gpio.h>

#include <cstring>

namespace papyrix::eink {
namespace {

constexpr uint8_t CMD_VER = 0x70;
constexpr uint8_t CMD_FLG = 0x71;
constexpr uint8_t CMD_RMTP = 0xA2;

class ArduinoX3DisplayProbeTransport : public X3DisplayProbeTransport {
 public:
  explicit ArduinoX3DisplayProbeTransport(const X3DisplayProbePins& pins) : pins_(pins) {}

  void readPass(uint8_t resetLowMs, X3DisplayProbeSample& sample) override {
    configurePins();
    reset(resetLowMs);
    commandRead(CMD_FLG, &sample.flg, 1);
    commandRead(CMD_VER, sample.ver, sizeof(sample.ver));
  }

  bool readMtp(uint8_t* output, size_t size) override {
    if (output == nullptr || size != X3_DISPLAY_MTP_SIZE) return false;

    uint8_t raw[X3_DISPLAY_MTP_SIZE + 1] = {};
    commandRead(CMD_RMTP, raw, sizeof(raw));
    memcpy(output, raw + 1, size);
    return true;
  }

  void pause(uint16_t milliseconds) override { delay(milliseconds); }

  void releasePins() override {
    pinMode(pins_.sclk, INPUT);
    pinMode(pins_.sda, INPUT);
    pinMode(pins_.cs, INPUT_PULLUP);
    pinMode(pins_.dc, INPUT);
    if (pins_.reset >= 0) pinMode(pins_.reset, INPUT);
    if (pins_.busy >= 0) pinMode(pins_.busy, INPUT);
  }

 private:
  static void clockDelay() { delayMicroseconds(1); }

  void configurePins() const {
    pinMode(pins_.cs, OUTPUT);
    digitalWrite(pins_.cs, HIGH);
    pinMode(pins_.sclk, OUTPUT);
    digitalWrite(pins_.sclk, LOW);
    pinMode(pins_.dc, OUTPUT);
    digitalWrite(pins_.dc, LOW);
    pinMode(pins_.sda, OUTPUT);
    if (pins_.busy >= 0) pinMode(pins_.busy, INPUT);
  }

  void reset(uint8_t resetLowMs) const {
    if (pins_.reset >= 0) {
      gpio_hold_dis(static_cast<gpio_num_t>(pins_.reset));
      pinMode(pins_.reset, OUTPUT);
      digitalWrite(pins_.reset, HIGH);
      delay(2);
      digitalWrite(pins_.reset, LOW);
      delay(resetLowMs);
      digitalWrite(pins_.reset, HIGH);
    }
    delay(30);
  }

  void writeByte(uint8_t value) const {
    for (uint8_t bit = 0; bit < 8; bit++) {
      digitalWrite(pins_.sda, (value & 0x80) != 0 ? HIGH : LOW);
      clockDelay();
      digitalWrite(pins_.sclk, HIGH);
      clockDelay();
      digitalWrite(pins_.sclk, LOW);
      value <<= 1;
    }
  }

  uint8_t readByte() const {
    uint8_t value = 0;
    for (uint8_t bit = 0; bit < 8; bit++) {
      clockDelay();
      value = static_cast<uint8_t>((value << 1) | (digitalRead(pins_.sda) == HIGH ? 1 : 0));
      digitalWrite(pins_.sclk, HIGH);
      clockDelay();
      digitalWrite(pins_.sclk, LOW);
    }
    return value;
  }

  void commandRead(uint8_t command, uint8_t* output, size_t size) const {
    pinMode(pins_.sda, OUTPUT);
    digitalWrite(pins_.dc, LOW);
    digitalWrite(pins_.cs, LOW);
    clockDelay();
    writeByte(command);
    digitalWrite(pins_.dc, HIGH);
    pinMode(pins_.sda, INPUT_PULLUP);
    clockDelay();
    for (size_t i = 0; i < size; i++) output[i] = readByte();
    digitalWrite(pins_.cs, HIGH);
    pinMode(pins_.sda, OUTPUT);
  }

  const X3DisplayProbePins pins_;
};

}  // namespace

X3DisplayProbeReport probeX3DisplayController(const X3DisplayProbePins& pins) {
  ArduinoX3DisplayProbeTransport transport(pins);
  return runX3DisplayProbe(transport);
}

}  // namespace papyrix::eink
