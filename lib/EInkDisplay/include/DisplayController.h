#pragma once

#include <cstdint>

namespace papyrix::eink {

enum class DisplayController : uint8_t { SSD1677, UC8253, UC8279_X3 };

inline const char* displayControllerName(DisplayController controller) {
  switch (controller) {
    case DisplayController::SSD1677:
      return "SSD1677";
    case DisplayController::UC8253:
      return "UC8253";
    case DisplayController::UC8279_X3:
      return "UC8279_X3";
  }
  return "unknown";
}

}  // namespace papyrix::eink
