#include "ClockFacePrimitives.h"

#include <GfxRenderer.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace papyrix::clock_face_primitives {
namespace {

static constexpr uint8_t BLOCK_DIGITS[10][7] = {
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}, {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}, {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E},
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}, {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E},
    {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E}, {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}, {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E},
};

static constexpr uint8_t SEGMENT_MAP[10] = {0x7E, 0x30, 0x6D, 0x79, 0x33, 0x5B, 0x5F, 0x70, 0x7F, 0x7B};
static constexpr const char* WEEKDAYS[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};

void drawArc(const GfxRenderer& renderer, int cx, int cy, int radius, int xDirection, int yDirection, int thickness,
             bool color) {
  const int stroke = std::min(thickness, radius);
  const int innerRadius = radius - stroke;
  const int outerRadiusSquared = radius * radius;
  const int innerRadiusSquared = innerRadius * innerRadius;
  int outerX = radius;
  int innerX = innerRadius;

  for (int deltaY = 0; deltaY <= radius; ++deltaY) {
    while (outerX > 0 && outerX * outerX + deltaY * deltaY > outerRadiusSquared) {
      --outerX;
    }
    while (innerX > 0 && (innerX - 1) * (innerX - 1) + deltaY * deltaY >= innerRadiusSquared) {
      --innerX;
    }
    if (outerX < innerX) continue;

    const int x0 = cx + xDirection * innerX;
    const int x1 = cx + xDirection * outerX;
    renderer.fillRect(std::min(x0, x1), cy + yDirection * deltaY, std::abs(x1 - x0) + 1, 1, color);
  }
}

}  // namespace

Layout makeLayout(const GfxRenderer& renderer) {
  Layout layout{};
  layout.width = renderer.getScreenWidth();
  layout.height = renderer.getScreenHeight();
  layout.centerX = layout.width / 2;
  layout.top = TOP_ZONE_HEIGHT;
  layout.bottom = layout.height - BOTTOM_SYSTEM_HEIGHT - CONTENT_BOTTOM_GAP;
  layout.contentHeight = layout.bottom - layout.top;
  layout.sideMargin = layout.width >= 520 ? 22 : 18;
  return layout;
}

DisplayTime displayTime(const std::tm& time, bool use24h) {
  if (use24h) return {time.tm_hour, time.tm_min, true, ""};

  int hour = time.tm_hour % 12;
  if (hour == 0) hour = 12;
  return {hour, time.tm_min, false, time.tm_hour < 12 ? "AM" : "PM"};
}

void formatDate(char* out, size_t outSize, const std::tm& time, int8_t dateFormat) {
  if (!out || outSize == 0) return;

  const char* weekday = time.tm_wday >= 0 && time.tm_wday < 7 ? WEEKDAYS[time.tm_wday] : "---";
  const int year = time.tm_year + 1900;
  const int month = time.tm_mon + 1;
  switch (dateFormat) {
    case 1:
      snprintf(out, outSize, "%s %02d/%02d/%04d", weekday, time.tm_mday, month, year);
      break;
    case 2:
      snprintf(out, outSize, "%s %02d/%02d/%04d", weekday, month, time.tm_mday, year);
      break;
    case 3:
      snprintf(out, outSize, "%s %02d.%02d.%04d", weekday, time.tm_mday, month, year);
      break;
    default:
      snprintf(out, outSize, "%s %04d/%02d/%02d", weekday, year, month, time.tm_mday);
      break;
  }
}

void drawBlockDigit(const GfxRenderer& renderer, int x, int y, int digit, int cellSize, bool color) {
  if (digit < 0 || digit > 9 || cellSize <= 0) return;

  for (int row = 0; row < 7; ++row) {
    for (int column = 0; column < 5; ++column) {
      if ((BLOCK_DIGITS[digit][row] & (1U << (4 - column))) != 0) {
        renderer.fillRect(x + column * cellSize, y + row * cellSize, cellSize, cellSize, color);
      }
    }
  }
}

void drawSevenSegmentDigit(const GfxRenderer& renderer, int x, int y, int digit, int width, int height, int thickness,
                           bool color) {
  if (digit < 0 || digit > 9 || width <= 0 || height <= 0 || thickness <= 0) return;

  const int gap = std::max(1, thickness / 3);
  const int halfHeight = height / 2;
  if (width <= 2 * gap || halfHeight <= 2 * gap) return;

  const uint8_t segments = SEGMENT_MAP[digit];
  if (segments & 0x40) renderer.fillRect(x + gap, y, width - 2 * gap, thickness, color);
  if (segments & 0x20) renderer.fillRect(x + width - thickness, y + gap, thickness, halfHeight - 2 * gap, color);
  if (segments & 0x10)
    renderer.fillRect(x + width - thickness, y + halfHeight + gap, thickness, halfHeight - 2 * gap, color);
  if (segments & 0x08) renderer.fillRect(x + gap, y + height - thickness, width - 2 * gap, thickness, color);
  if (segments & 0x04) renderer.fillRect(x, y + halfHeight + gap, thickness, halfHeight - 2 * gap, color);
  if (segments & 0x02) renderer.fillRect(x, y + gap, thickness, halfHeight - 2 * gap, color);
  if (segments & 0x01) renderer.fillRect(x + gap, y + halfHeight - thickness / 2, width - 2 * gap, thickness, color);
}

void drawCircle(const GfxRenderer& renderer, int cx, int cy, int radius, int thickness, bool color) {
  if (radius <= 0 || thickness <= 0) return;

  drawArc(renderer, cx, cy, radius, 1, 1, thickness, color);
  drawArc(renderer, cx, cy, radius, -1, 1, thickness, color);
  drawArc(renderer, cx, cy, radius, 1, -1, thickness, color);
  drawArc(renderer, cx, cy, radius, -1, -1, thickness, color);
}

void fillCircle(const GfxRenderer& renderer, int cx, int cy, int radius, bool color) {
  if (radius <= 0) return;

  const int radiusSquared = radius * radius;
  int x = radius;
  for (int deltaY = 0; deltaY <= radius; ++deltaY) {
    while (x > 0 && x * x + deltaY * deltaY > radiusSquared) {
      --x;
    }
    renderer.fillRect(cx - x, cy + deltaY, 2 * x + 1, 1, color);
    if (deltaY != 0) renderer.fillRect(cx - x, cy - deltaY, 2 * x + 1, 1, color);
  }
}

void drawThickLine(const GfxRenderer& renderer, int x1, int y1, int x2, int y2, int thickness, bool color) {
  if (thickness <= 0) return;

  const int firstOffset = -(thickness / 2);
  const int lastOffset = firstOffset + thickness - 1;
  if (std::abs(y2 - y1) > std::abs(x2 - x1)) {
    for (int offset = firstOffset; offset <= lastOffset; ++offset) {
      renderer.drawLine(x1 + offset, y1, x2 + offset, y2, color);
    }
  } else {
    for (int offset = firstOffset; offset <= lastOffset; ++offset) {
      renderer.drawLine(x1, y1 + offset, x2, y2 + offset, color);
    }
  }
}

}  // namespace papyrix::clock_face_primitives
