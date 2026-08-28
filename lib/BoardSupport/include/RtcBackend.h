#pragma once

#include <cstdint>

namespace papyrix::board {

struct DateTime {
  uint16_t year = 2000;
  uint8_t month = 1;
  uint8_t day = 1;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t second = 0;
  uint8_t weekday = 0;
};

class RtcBackend {
 public:
  bool begin();
  bool read(DateTime& out);
  bool write(const DateTime& value);
  bool available() const { return available_; }

 private:
  bool available_ = false;
};

constexpr uint8_t bcdToDecimal(uint8_t value) { return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F)); }
constexpr uint8_t decimalToBcd(uint8_t value) { return static_cast<uint8_t>((value / 10) << 4 | value % 10); }

}  // namespace papyrix::board
