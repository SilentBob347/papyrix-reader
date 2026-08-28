#pragma once

#include <cstdint>

namespace papyrix::eink {

enum class Ssd1677RefreshMode : uint8_t { Full, Fast, Window };

struct Ssd1677UpdatePlan {
  bool writeBwBeforeRefresh;
  bool writeRedBeforeRefresh;
  bool syncRedAfterRefresh;
  uint32_t transferBytes;
};

constexpr Ssd1677UpdatePlan makeSsd1677UpdatePlan(Ssd1677RefreshMode mode, uint32_t transferBytes) {
  return {true, mode == Ssd1677RefreshMode::Full, true, transferBytes};
}

}  // namespace papyrix::eink
