#pragma once

#include <cstddef>
#include <cstdint>
#include <ctime>

class GfxRenderer;

namespace papyrix::clock_face_primitives {

inline constexpr int TOP_ZONE_HEIGHT = 58;
inline constexpr int BOTTOM_SYSTEM_HEIGHT = 56;
inline constexpr int CONTENT_BOTTOM_GAP = 8;

struct Layout {
  int width;
  int height;
  int centerX;
  int top;
  int bottom;
  int contentHeight;
  int sideMargin;
};

struct DisplayTime {
  int hour;
  int minute;
  bool showLeadingZero;
  const char* period;
};

Layout makeLayout(const GfxRenderer& renderer);
DisplayTime displayTime(const std::tm& time, bool use24h);
void formatDate(char* out, size_t outSize, const std::tm& time, int8_t dateFormat);
void drawBlockDigit(const GfxRenderer& renderer, int x, int y, int digit, int cellSize, bool color);
void drawSevenSegmentDigit(const GfxRenderer& renderer, int x, int y, int digit, int width, int height, int thickness,
                           bool color);
void drawCircle(const GfxRenderer& renderer, int cx, int cy, int radius, int thickness, bool color);
void fillCircle(const GfxRenderer& renderer, int cx, int cy, int radius, bool color);
void drawThickLine(const GfxRenderer& renderer, int x1, int y1, int x2, int y2, int thickness, bool color);

}  // namespace papyrix::clock_face_primitives
