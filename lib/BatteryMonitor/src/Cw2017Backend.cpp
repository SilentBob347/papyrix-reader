#include <Arduino.h>
#include <Cw2017Policy.h>
#include <Wire.h>

#include "BatteryMonitor.h"

namespace {
constexpr uint8_t kVersion = 0x00;
constexpr uint8_t kVcell = 0x02;
constexpr uint8_t kSoc = 0x04;
constexpr uint8_t kMode = 0x08;
constexpr uint8_t kSocAlert = 0x0B;
constexpr uint8_t kBatInfo = 0x10;
constexpr uint8_t kUpdateFlag = 0x80;

bool read8(const BatteryMonitor::Cw2017Config& config, uint8_t reg, uint8_t* value) {
  Wire.beginTransmission(config.address);
  Wire.write(reg);
  if (Wire.endTransmission() != 0 || Wire.requestFrom(static_cast<int>(config.address), 1) != 1) return false;
  *value = Wire.read();
  return true;
}

bool write8(const BatteryMonitor::Cw2017Config& config, uint8_t reg, uint8_t value) {
  Wire.beginTransmission(config.address);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}
}  // namespace

BatteryMonitor::BatteryMonitor(const Cw2017Config& config) : _mode(Mode::Cw2017), _cw2017(config) {}

bool BatteryMonitor::ensureCw2017Profile_() const {
  const unsigned long now = millis();
  if (_cw2017Initialized) return true;
  if (_cw2017LastInitAttemptMs && now - _cw2017LastInitAttemptMs < 1000) return false;
  _cw2017LastInitAttemptMs = now;
  if (!Wire.begin(_cw2017.sdaPin, _cw2017.sclPin, _cw2017.freq)) return false;
  Wire.setTimeOut(10);

  uint8_t mode = 0;
  uint8_t version = 0;
  uint8_t alert = 0;
  if (!read8(_cw2017, kMode, &mode) || !read8(_cw2017, kVersion, &version) || !read8(_cw2017, kSocAlert, &alert)) {
    return false;
  }
  if (_cw2017.profile == nullptr) {
    _cw2017Initialized = mode == 0 && (alert & kUpdateFlag) != 0 && papyrix::battery::cw2017VersionIsRunning(version);
    return _cw2017Initialized;
  }

  bool profileMatches = (alert & kUpdateFlag) != 0;
  if (profileMatches) {
    for (uint8_t i = 0; i < papyrix::battery::kCw2017BatteryProfile.size(); ++i) {
      uint8_t stored = 0;
      if (!read8(_cw2017, static_cast<uint8_t>(kBatInfo + i), &stored)) return false;
      if (stored != _cw2017.profile[i]) {
        profileMatches = false;
        break;
      }
    }
  }
  if (!profileMatches) {
    for (uint8_t i = 0; i < papyrix::battery::kCw2017BatteryProfile.size(); ++i) {
      if (!write8(_cw2017, static_cast<uint8_t>(kBatInfo + i), _cw2017.profile[i])) return false;
    }
    if (!write8(_cw2017, kSocAlert, kUpdateFlag)) return false;
  }

  if (!profileMatches || mode != 0 || !papyrix::battery::cw2017VersionIsRunning(version)) {
    if (!write8(_cw2017, kMode, 0xF0)) return false;
    delay(20);
    if (!write8(_cw2017, kMode, 0x30)) return false;
    delay(20);
    if (!write8(_cw2017, kMode, 0x00)) return false;
    delay(20);
  }

  for (int attempt = 0; attempt < 50; ++attempt) {
    if (read8(_cw2017, kVersion, &version) && papyrix::battery::cw2017VersionIsRunning(version)) {
      _cw2017Initialized = true;
      return true;
    }
    delay(20);
  }
  return false;
}

bool BatteryMonitor::readCw2017Soc_(uint16_t* out) const {
  const unsigned long now = millis();
  if (!_cw2017SocPolled || now - _lastSocPollMs >= kBqPollIntervalMs) {
    _cw2017SocPolled = true;
    _lastSocPollMs = now;
    uint8_t soc = 0;
    if (ensureCw2017Profile_() && read8(_cw2017, kSoc, &soc) && soc <= 100) {
      _lastGoodSoc = soc;
      _haveCw2017Soc = true;
    }
  }
  if (!_haveCw2017Soc) return false;
  *out = _lastGoodSoc;
  return true;
}

bool BatteryMonitor::readCw2017Mv_(uint16_t* out) const {
  const unsigned long now = millis();
  if (!_cw2017MvPolled || now - _lastMvPollMs >= kBqPollIntervalMs) {
    _cw2017MvPolled = true;
    _lastMvPollMs = now;
    uint8_t high = 0;
    uint8_t low = 0;
    if (ensureCw2017Profile_() && read8(_cw2017, kVcell, &high) &&
        read8(_cw2017, static_cast<uint8_t>(kVcell + 1), &low)) {
      _lastGoodMv = papyrix::battery::cw2017Millivolts(static_cast<uint16_t>(high) << 8 | low);
      _haveCw2017Mv = true;
    }
  }
  if (!_haveCw2017Mv) return false;
  *out = _lastGoodMv;
  return true;
}
