#pragma once

#include <FrontLightBackend.h>

#include <cstdint>

namespace papyrix::hal {

class FrontLight {
 public:
  void init();
  void shutdown();

  bool isAvailable() const { return backend_.available(); }
  uint8_t brightness() const { return brightness_; }
  uint8_t warmth() const { return warmth_; }
  bool setBrightness(uint8_t percent);
  bool setWarmth(uint8_t percent);

 private:
  bool persist(const char* key, uint8_t value);

  board::FrontLightBackend backend_;
  uint8_t brightness_ = 0;
  uint8_t warmth_ = 50;
  bool initialized_ = false;
};

}  // namespace papyrix::hal
