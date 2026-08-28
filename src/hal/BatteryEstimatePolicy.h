#pragma once

#include <cstdint>

namespace papyrix::hal::battery_estimate {

inline constexpr uint32_t kMagic = 0x42415454;
inline constexpr uint16_t kMaxRetainedDelta = 25;

struct RetainedEstimate {
  uint32_t magic = 0;
  uint16_t scaledPercent = 0;
};

constexpr uint16_t displayPercent(const RetainedEstimate& estimate) {
  return static_cast<uint16_t>((estimate.scaledPercent + 5) / 10);
}

constexpr RetainedEstimate seed(uint16_t rawPercent) { return {kMagic, static_cast<uint16_t>(rawPercent * 10)}; }

constexpr bool isUsable(const RetainedEstimate& estimate, uint16_t rawPercent) {
  if (estimate.magic != kMagic || estimate.scaledPercent > 1000 || rawPercent > 100) return false;
  const uint16_t retainedPercent = displayPercent(estimate);
  const uint16_t delta = retainedPercent > rawPercent ? retainedPercent - rawPercent : rawPercent - retainedPercent;
  return delta <= kMaxRetainedDelta;
}

constexpr RetainedEstimate update(const RetainedEstimate& previous, uint16_t rawPercent) {
  if (!isUsable(previous, rawPercent)) return seed(rawPercent);
  return {kMagic, static_cast<uint16_t>((previous.scaledPercent * 9 + rawPercent * 10) / 10)};
}

}  // namespace papyrix::hal::battery_estimate
