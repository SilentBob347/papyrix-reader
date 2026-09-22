#include "BatteryMonitor.h"

#include <AdcMutex.h>
#include <Arduino.h>
#include <Wire.h>
#if PAPYRIX_TARGET_X4PRO || PAPYRIX_TARGET_X4CLASSIC
#include <TargetConfig.h>
#endif

inline float min(const float a, const float b) { return a < b ? a : b; }
inline float max(const float a, const float b) { return a > b ? a : b; }

namespace {
constexpr uint8_t I2C_ADDR_BQ27220 = 0x55;
constexpr uint8_t BQ27220_SOC_REG = 0x2C;   // 16-bit LE, percentage
constexpr uint8_t BQ27220_VOLT_REG = 0x08;  // 16-bit LE, mV
constexpr uint8_t BQ27220_CUR_REG = 0x0C;   // 16-bit LE, signed mA (positive = charging)
constexpr uint16_t BQ27220_TIMEOUT_MS = 6;

bool readBq27220Reg16Le(uint8_t reg, uint16_t* out) {
  Wire.beginTransmission(I2C_ADDR_BQ27220);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(I2C_ADDR_BQ27220), 2) != 2) return false;
  if (Wire.available() < 2) return false;
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *out = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}
}  // namespace

BatteryMonitor::BatteryMonitor(uint8_t adcPin, float dividerMultiplier)
    : _mode(Mode::Adc), _adcPin(adcPin), _dividerMultiplier(dividerMultiplier) {}

BatteryMonitor::BatteryMonitor(const Bq27220Config& cfg) : _mode(Mode::Bq27220), _i2c(cfg) {}

uint16_t BatteryMonitor::readBq27220Soc_() const {
  const unsigned long now = millis();
  if (_haveBqReading && _lastSocPollMs != 0 && (now - _lastSocPollMs) < kBqPollIntervalMs) {
    return _lastGoodSoc;
  }

  Wire.begin(_i2c.sdaPin, _i2c.sclPin, _i2c.freq);
  Wire.setTimeOut(BQ27220_TIMEOUT_MS);
  uint16_t soc = 0;
  const bool ok = readBq27220Reg16Le(BQ27220_SOC_REG, &soc);
  Wire.end();
  // Hand pins back so digitalRead(UART0_RXD=20) for USB detection still works.
  pinMode(_i2c.sdaPin, INPUT);
  pinMode(_i2c.sclPin, INPUT);

  _lastSocPollMs = now;
  if (!ok || soc > 100) {
    // Use a safe pre-first-success value so a transient I2C error cannot
    // trigger the low-battery UI before the first valid gauge sample.
    return _haveBqReading ? _lastGoodSoc : 100;
  }
  _lastGoodSoc = soc;
  _haveBqReading = true;
  return soc;
}

uint16_t BatteryMonitor::readBq27220Mv_() const {
  const unsigned long now = millis();
  if (_haveBqReading && _lastMvPollMs != 0 && (now - _lastMvPollMs) < kBqPollIntervalMs) {
    return _lastGoodMv;
  }

  Wire.begin(_i2c.sdaPin, _i2c.sclPin, _i2c.freq);
  Wire.setTimeOut(BQ27220_TIMEOUT_MS);
  uint16_t mv = 0;
  const bool ok = readBq27220Reg16Le(BQ27220_VOLT_REG, &mv);
  Wire.end();
  pinMode(_i2c.sdaPin, INPUT);
  pinMode(_i2c.sclPin, INPUT);

  _lastMvPollMs = now;
  if (!ok || mv < 2500 || mv > 5000) {
    // Mirror the SoC fallback: pre-first-success returns a typical full-charge
    // voltage rather than 0, so debug screens and any voltage→percent fallback
    // in callers don't show "dead battery" on a transient I²C glitch.
    return _haveBqReading ? _lastGoodMv : 4100;
  }
  _lastGoodMv = mv;
  _haveBqReading = true;
  return mv;
}

uint16_t BatteryMonitor::readPercentage() const {
#if PAPYRIX_CAP_BATTERY_CW2017
  uint16_t percentage = 0;
  return readCw2017Soc_(&percentage) ? percentage : 0;
#else
  if (_mode == Mode::Bq27220) return readBq27220Soc_();
  return percentageFromMillivolts(readMillivolts());
#endif
}

uint16_t BatteryMonitor::readMillivolts() const {
#if PAPYRIX_CAP_BATTERY_CW2017
  uint16_t millivolts = 0;
  return readCw2017Mv_(&millivolts) ? millivolts : 0;
#else
  if (_mode == Mode::Bq27220) return readBq27220Mv_();
  return static_cast<uint16_t>(readRawMillivolts() * _dividerMultiplier);
#endif
}

uint16_t BatteryMonitor::readRawMillivolts() const {
#if PAPYRIX_CAP_BATTERY_CW2017
  return readMillivolts();
#else
  if (_mode == Mode::Bq27220) return readBq27220Mv_();
  std::lock_guard<std::mutex> adcLock(papyrix::board::adcMutex);
  return analogReadMilliVolts(_adcPin);
#endif
}

double BatteryMonitor::readVolts() const { return static_cast<double>(readMillivolts()) / 1000.0; }

bool BatteryMonitor::readBq27220Current_(int16_t* outMa) const {
  const unsigned long now = millis();
  if (_haveBqCurrent && _lastCurrentPollMs != 0 && (now - _lastCurrentPollMs) < kBqPollIntervalMs) {
    *outMa = _lastGoodCurrentMa;
    return true;
  }

  Wire.begin(_i2c.sdaPin, _i2c.sclPin, _i2c.freq);
  Wire.setTimeOut(BQ27220_TIMEOUT_MS);
  uint16_t raw = 0;
  const bool ok = readBq27220Reg16Le(BQ27220_CUR_REG, &raw);
  Wire.end();
  pinMode(_i2c.sdaPin, INPUT);
  pinMode(_i2c.sclPin, INPUT);

  _lastCurrentPollMs = now;
  if (!ok) {
    if (_haveBqCurrent) {
      *outMa = _lastGoodCurrentMa;
      return true;
    }
    return false;
  }
  _lastGoodCurrentMa = static_cast<int16_t>(raw);
  _haveBqCurrent = true;
  *outMa = _lastGoodCurrentMa;
  return true;
}

BatteryMonitor::Status BatteryMonitor::readStatus() const {
  Status status;
#if !PAPYRIX_CAP_BATTERY_CW2017
  if (_mode == Mode::Adc) {
    status.supported = true;
    status.millivolts = readMillivolts();
    status.millivoltsKnown = true;
    status.percentage = percentageFromMillivolts(status.millivolts);
    status.percentageKnown = true;
    return status;
  }
#endif
  status.supported = true;
#if PAPYRIX_CAP_BATTERY_CW2017
  status.percentageKnown = readCw2017Soc_(&status.percentage);
  status.millivoltsKnown = readCw2017Mv_(&status.millivolts);
#else
  status.percentage = readPercentage();
  status.millivolts = readMillivolts();
  status.percentageKnown = true;
  status.millivoltsKnown = true;
  if (_mode == Mode::Bq27220) {
    int16_t current = 0;
    status.chargingKnown = readBq27220Current_(&current);
    status.charging = status.chargingKnown && current > 0;
  }
#endif
  return status;
}

bool BatteryMonitor::isCharging() const {
  if (_mode != Mode::Bq27220) {
    return false;
  }
  // Two attempts (with 2 ms settle between) so a single bus glitch doesn't
  // flip the icon. Mirrors crosspoint-reader/lib/hal/HalGPIO.cpp::isUsbConnected.
  for (uint8_t attempt = 0; attempt < 2; ++attempt) {
    int16_t ma = 0;
    if (readBq27220Current_(&ma)) {
      return ma > 0;
    }
    delay(2);
  }
  return false;
}

uint16_t BatteryMonitor::percentageFromMillivolts(uint16_t millivolts) {
  double volts = millivolts / 1000.0;
  // Polynomial derived from LiPo samples
  double y = -144.9390 * volts * volts * volts + 1655.8629 * volts * volts - 6158.8520 * volts + 7501.3202;

  // Clamp to [0,100] and round
  y = max(y, 0.0);
  y = min(y, 100.0);
  y = round(y);
  return static_cast<int>(y);
}

uint16_t BatteryMonitor::millivoltsFromRawAdc(uint16_t adc_raw) { return adc_raw; }
