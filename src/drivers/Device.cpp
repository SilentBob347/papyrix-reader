#include "Device.h"

#include <Arduino.h>
#include <Logging.h>
#include <Preferences.h>
#include <Wire.h>
#include <X3DisplayProbe.h>

#include <cstdio>

#define TAG "DEVICE"

namespace papyrix {
namespace drivers {

namespace {

// X3 I2C bus pins (verified against crosspoint-reader/lib/hal/HalGPIO.h).
constexpr int X3_I2C_SDA = 20;
constexpr int X3_I2C_SCL = 0;
constexpr uint32_t X3_I2C_FREQ = 400000;
constexpr uint16_t I2C_TIMEOUT_MS = 6;
constexpr uint16_t INTER_PASS_DELAY_MS = 2;

// I2C device addresses (X3-only chips).
constexpr uint8_t I2C_ADDR_BQ27220 = 0x55;
constexpr uint8_t I2C_ADDR_DS3231 = 0x68;
constexpr uint8_t I2C_ADDR_QMI8658 = 0x6B;
constexpr uint8_t I2C_ADDR_QMI8658_ALT = 0x6A;

// Registers used for sane-value signature checks.
constexpr uint8_t BQ27220_SOC_REG = 0x2C;       // 16-bit LE
constexpr uint8_t BQ27220_VOLT_REG = 0x08;      // 16-bit LE, mV
constexpr uint8_t DS3231_SEC_REG = 0x00;        // 8-bit BCD
constexpr uint8_t QMI8658_WHO_AM_I_REG = 0x00;  // 8-bit
constexpr uint8_t QMI8658_WHO_AM_I_VALUE = 0x05;

// NVS layout. Distinct namespace from crosspoint-reader's "cphw" so a user
// flashing back and forth between firmwares cannot read the wrong cache.
constexpr const char* HW_NAMESPACE = "papyrix_hw";
constexpr const char* NVS_KEY_OVERRIDE = "dev_ovr";
constexpr const char* NVS_KEY_CACHE = "dev_det";
constexpr const char* NVS_KEY_CONTROLLER_OVERRIDE = "epd_ovr";
constexpr const char* NVS_KEY_CONTROLLER_CACHE = "epd_det";
constexpr const char* NVS_KEY_CONTROLLER_VERSION = "epd_ver";

constexpr papyrix::eink::X3DisplayProbePins X3_DISPLAY_PROBE_PINS{8, 10, 21, 4, 5, 6};

bool readI2CReg8(uint8_t addr, uint8_t reg, uint8_t* out) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(addr), 1) != 1) return false;
  if (!Wire.available()) return false;
  *out = Wire.read();
  return true;
}

bool readI2CReg16LE(uint8_t addr, uint8_t reg, uint16_t* out) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(static_cast<int>(addr), 2) != 2) return false;
  if (Wire.available() < 2) return false;
  const uint8_t lo = Wire.read();
  const uint8_t hi = Wire.read();
  *out = (static_cast<uint16_t>(hi) << 8) | lo;
  return true;
}

// SOC ∈ [0, 100] AND voltage ∈ [2500, 5000] mV — rules out random ACK matches.
bool probeBQ27220Signature() {
  uint16_t soc = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_SOC_REG, &soc)) return false;
  if (soc > 100) return false;
  uint16_t voltageMv = 0;
  if (!readI2CReg16LE(I2C_ADDR_BQ27220, BQ27220_VOLT_REG, &voltageMv)) return false;
  return voltageMv >= 2500 && voltageMv <= 5000;
}

// BCD seconds: tens digit ≤ 5, ones digit ≤ 9. Mask tens to 3 bits because
// the high bit of that nibble is the DS3231 oscillator-halt flag.
bool probeDS3231Signature() {
  uint8_t sec = 0;
  if (!readI2CReg8(I2C_ADDR_DS3231, DS3231_SEC_REG, &sec)) return false;
  const uint8_t tens = (sec >> 4) & 0x07;
  const uint8_t ones = sec & 0x0F;
  return tens <= 5 && ones <= 9;
}

bool probeQMI8658Signature() {
  uint8_t whoami = 0;
  if (readI2CReg8(I2C_ADDR_QMI8658, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  if (readI2CReg8(I2C_ADDR_QMI8658_ALT, QMI8658_WHO_AM_I_REG, &whoami) && whoami == QMI8658_WHO_AM_I_VALUE) {
    return true;
  }
  return false;
}

const char* x3DisplayVerdictName(papyrix::eink::X3DisplayVerdict verdict) {
  switch (verdict) {
    case papyrix::eink::X3DisplayVerdict::UC8253StableDefault:
      return "uc8253-stable-default";
    case papyrix::eink::X3DisplayVerdict::UC8279Confirmed:
      return "uc8279-confirmed";
    case papyrix::eink::X3DisplayVerdict::Inconclusive:
      return "inconclusive";
  }
  return "unknown";
}

uint8_t readHardwareByte(const char* key) {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) return 0;
  const uint8_t value = prefs.getUChar(key, 0);
  prefs.end();
  return value;
}

}  // namespace

Device& Device::instance() {
  static Device inst;
  return inst;
}

const char* Device::cacheDir() const { return type_ == Type::X3 ? "/.papyrix/cache/x3" : "/.papyrix/cache"; }

Device::ProbeReport Device::runProbePass_() {
  ProbeReport r;
  r.bq27220 = probeBQ27220Signature();
  r.ds3231 = probeDS3231Signature();
  r.qmi8658 = probeQMI8658Signature();
  r.score = static_cast<uint8_t>(r.bq27220 + r.ds3231 + r.qmi8658);
  return r;
}

Device::Type Device::runFullProbe_() {
  Wire.begin(X3_I2C_SDA, X3_I2C_SCL, X3_I2C_FREQ);
  Wire.setTimeOut(I2C_TIMEOUT_MS);

  const ProbeReport pass1 = runProbePass_();
  delay(INTER_PASS_DELAY_MS);
  const ProbeReport pass2 = runProbePass_();

  LOG_INF(TAG, "probe: pass1=%u(bq=%d rtc=%d imu=%d) pass2=%u(bq=%d rtc=%d imu=%d)", pass1.score, pass1.bq27220,
          pass1.ds3231, pass1.qmi8658, pass2.score, pass2.bq27220, pass2.ds3231, pass2.qmi8658);

  // Hand the bus back to whoever owns it next (SD/SPI driver, etc.).
  Wire.end();
  pinMode(X3_I2C_SDA, INPUT);
  pinMode(X3_I2C_SCL, INPUT);

  lastProbe_ = pass2;

  if (pass1.score >= 2 && pass2.score >= 2) return Type::X3;
  if (pass1.score == 0 && pass2.score == 0) return Type::X4;
  return Type::Unknown;
}

Device::Type Device::readOverride_() const {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) return Type::Unknown;
  const uint8_t v = prefs.getUChar(NVS_KEY_OVERRIDE, 0);
  prefs.end();
  if (v == 1) return Type::X4;
  if (v == 2) return Type::X3;
  return Type::Unknown;
}

Device::Type Device::readCache_() const {
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, true)) return Type::Unknown;
  const uint8_t v = prefs.getUChar(NVS_KEY_CACHE, 0);
  prefs.end();
  if (v == 1) return Type::X4;
  if (v == 2) return Type::X3;
  return Type::Unknown;
}

void Device::writeCache_(Type t) {
  if (t == Type::Unknown) return;
  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) return;
  const uint8_t existing = prefs.getUChar(NVS_KEY_CACHE, 0);
  const uint8_t next = static_cast<uint8_t>(t);
  if (existing != next) {
    prefs.putUChar(NVS_KEY_CACHE, next);
  }
  prefs.end();
}

uint8_t Device::readControllerOverride_() const { return readHardwareByte(NVS_KEY_CONTROLLER_OVERRIDE); }

uint8_t Device::readControllerCache_() const { return readHardwareByte(NVS_KEY_CONTROLLER_CACHE); }

uint8_t Device::readControllerCacheVersion_() const { return readHardwareByte(NVS_KEY_CONTROLLER_VERSION); }

bool Device::writeControllerCache_(uint8_t value) {
  if (value != kStoredUc8253 && value != kStoredUc8279X3) return false;

  Preferences prefs;
  if (!prefs.begin(HW_NAMESPACE, false)) return false;

  const uint8_t existingValue = prefs.getUChar(NVS_KEY_CONTROLLER_CACHE, 0);
  const uint8_t existingVersion = prefs.getUChar(NVS_KEY_CONTROLLER_VERSION, 0);
  bool valueStored = existingValue == value;
  if (!valueStored) valueStored = prefs.putUChar(NVS_KEY_CONTROLLER_CACHE, value) == sizeof(value);
  if (valueStored && existingVersion != kControllerCacheVersion) {
    prefs.putUChar(NVS_KEY_CONTROLLER_VERSION, kControllerCacheVersion);
  }
  prefs.end();
  return valueStored;
}

void Device::resolveDisplayController_() {
  if (isX4()) {
    displayController_ = papyrix::eink::DisplayController::SSD1677;
    controllerSelectionSource_ = ControllerSelectionSource::X4Default;
    LOG_INF(TAG, "display controller: SSD1677 (x4-default)");
    return;
  }

  const uint8_t overrideValue = readControllerOverride_();
  const uint8_t cacheValue = readControllerCache_();
  const uint8_t cacheVersion = readControllerCacheVersion_();
  const ControllerDecision stored = resolveStoredX3Controller(overrideValue, cacheValue, cacheVersion);
  if (stored.invalidOverride) LOG_WRN(TAG, "invalid display controller override: %u", overrideValue);
  if (stored.invalidCache) {
    LOG_WRN(TAG, "invalid display controller cache: value=%u version=%u expected=%u", cacheValue, cacheVersion,
            kControllerCacheVersion);
  }
  if (stored.resolved) {
    displayController_ = stored.controller;
    controllerSelectionSource_ = stored.source;
    LOG_INF(TAG, "display controller: %s (%s)", papyrix::eink::displayControllerName(displayController_),
            controllerSelectionSourceName(controllerSelectionSource_));
    return;
  }

  LOG_INF(TAG, "no display controller cache, running live probe");
  const papyrix::eink::X3DisplayProbeReport report = papyrix::eink::probeX3DisplayController(X3_DISPLAY_PROBE_PINS);
  LOG_INF(TAG, "display probe: VER1=%02X %02X %02X %02X %02X FLG1=%02X", report.pass1.ver[0], report.pass1.ver[1],
          report.pass1.ver[2], report.pass1.ver[3], report.pass1.ver[4], report.pass1.flg);
  LOG_INF(TAG, "display probe: VER2=%02X %02X %02X %02X %02X FLG2=%02X", report.pass2.ver[0], report.pass2.ver[1],
          report.pass2.ver[2], report.pass2.ver[3], report.pass2.ver[4], report.pass2.flg);
  if (report.mtpValid) {
    char mtpLine[145] = {};
    size_t used = 0;
    for (size_t i = 0; i < sizeof(report.mtp); i++) {
      const size_t remaining = sizeof(mtpLine) - used;
      const int written =
          snprintf(mtpLine + used, remaining, i == 0 ? "%02X" : " %02X", static_cast<unsigned int>(report.mtp[i]));
      if (written <= 0 || static_cast<size_t>(written) >= remaining) break;
      used += static_cast<size_t>(written);
    }
    LOG_INF(TAG, "display probe MTP: %s%s", mtpLine, report.mtpRepeatable ? " (repeat matched)" : "");
  }
  LOG_INF(TAG, "display probe verdict: %s", x3DisplayVerdictName(report.verdict));

  const ControllerDecision probed = resolveProbedX3Controller(report.verdict);
  displayController_ = probed.controller;
  controllerSelectionSource_ = probed.source;
  const bool cached = probed.writeCache && writeControllerCache_(probed.storedValue);
  LOG_INF(TAG, "display controller: %s (%s%s)", papyrix::eink::displayControllerName(displayController_),
          controllerSelectionSourceName(controllerSelectionSource_),
          cached              ? ", cached"
          : probed.writeCache ? ", cache write failed"
                              : ", not cached");
}

void Device::probeDeviceType() {
  const Type ovr = readOverride_();
  if (ovr != Type::Unknown) {
    type_ = ovr;
    LOG_INF(TAG, "override active: %s", isX3() ? "X3" : "X4");
  } else {
    const Type cached = readCache_();
    if (cached != Type::Unknown) {
      type_ = cached;
      LOG_INF(TAG, "using cache: %s", isX3() ? "X3" : "X4");
    } else {
      LOG_INF(TAG, "no cache, running I2C probe");
      const Type detected = runFullProbe_();
      if (detected == Type::Unknown) {
        type_ = Type::X4;
        LOG_INF(TAG, "probe inconclusive (score=%u), defaulting to X4 (no cache write)", lastProbe_.score);
      } else {
        type_ = detected;
        writeCache_(detected);
        LOG_INF(TAG, "probe: %s detected, cached", isX3() ? "X3" : "X4");
      }
    }
  }
}

void Device::selectDisplayController() { resolveDisplayController_(); }

}  // namespace drivers
}  // namespace papyrix
