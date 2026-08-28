#pragma once

#include <cstddef>
#include <cstdint>

#include "Uc8279X3Driver.h"

namespace papyrix::eink {

enum class Uc8179RefreshMode : uint8_t { Full, Half, Fast };

class Uc8179X4ProDriver {
 public:
  static constexpr uint16_t WIDTH = 800;
  static constexpr uint16_t HEIGHT = 480;
  static constexpr uint16_t ADDRESSED_HEIGHT = 600;
  static constexpr uint16_t WIDTH_BYTES = WIDTH / 8;
  static constexpr uint32_t BUFFER_SIZE = static_cast<uint32_t>(WIDTH_BYTES) * HEIGHT;
  static constexpr uint32_t TRANSFER_SIZE = static_cast<uint32_t>(WIDTH_BYTES) * ADDRESSED_HEIGHT;

  Uc8179X4ProDriver() = default;
  ~Uc8179X4ProDriver();
  Uc8179X4ProDriver(const Uc8179X4ProDriver&) = delete;
  Uc8179X4ProDriver& operator=(const Uc8179X4ProDriver&) = delete;

  bool begin(Uc8279Bus& bus);
  bool display(Uc8279Bus& bus, const uint8_t* frame, Uc8179RefreshMode mode, bool turnOff);
  void requestResync();
  bool copyGrayscaleLsb(Uc8279Bus& bus, const uint8_t* plane);
  bool copyGrayscaleMsb(Uc8279Bus& bus, const uint8_t* plane);
  bool displayGray(Uc8279Bus& bus, bool turnOff);
  bool cleanupGrayscale(Uc8279Bus& bus, const uint8_t* bwFrame);
  bool deepSleep(Uc8279Bus& bus);

 private:
  bool allocateGrayBase();
  void releaseGrayBase();
  void initController(Uc8279Bus& bus);
  void streamPlane(Uc8279Bus& bus, uint8_t command, const uint8_t* plane, bool invert = false);
  void streamPlaneXor(Uc8279Bus& bus, uint8_t command, const uint8_t* lhs, const uint8_t* rhs);
  void fillPlane(Uc8279Bus& bus, uint8_t command, uint8_t value);
  bool transitionGrayscaleBase(Uc8279Bus& bus, const uint8_t* frame, bool turnOff);
  bool powerOn(Uc8279Bus& bus, const char* operation);
  void invalidate();

  uint8_t* grayBase_ = nullptr;
  bool grayBaseValid_ = false;
  bool absoluteGrayPlanes_ = false;
  bool screenOn_ = false;
  bool needFullClear_ = true;
  bool oldPlaneValid_ = false;
  bool redriveAfterGray_ = false;
  bool lsbValid_ = false;
  bool msbValid_ = false;
};

Uc8179X4ProDriver& uc8179X4ProDriver();

}  // namespace papyrix::eink
