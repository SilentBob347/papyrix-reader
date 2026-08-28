#pragma once

#include <cstdint>

#include "BoardProfile.h"

namespace papyrix::board {

enum class DisplayOrientation : uint8_t {
  Portrait = 0,
  LandscapeClockwise = 1,
  PortraitInverted = 2,
  LandscapeCounterClockwise = 3,
};

struct PanelPoint {
  int16_t x = 0;
  int16_t y = 0;
};

void setDisplayOrientation(DisplayOrientation orientation);
DisplayOrientation currentDisplayOrientation();
uint8_t displayOrientationRevision();

void logicalSize(DisplayOrientation orientation, int16_t panelWidth, int16_t panelHeight, int16_t& logicalWidth,
                 int16_t& logicalHeight);
bool panelFromLogical(DisplayOrientation orientation, int16_t panelWidth, int16_t panelHeight, int16_t logicalX,
                      int16_t logicalY, PanelPoint& panel);
bool logicalFromPanel(DisplayOrientation orientation, int16_t panelWidth, int16_t panelHeight, int16_t panelX,
                      int16_t panelY, PanelPoint& logical);
bool rawToPanel(const TouchConfig& config, uint16_t rawX, uint16_t rawY, int16_t panelWidth, int16_t panelHeight,
                PanelPoint& panel);
bool rawToLogical(const TouchConfig& config, uint16_t rawX, uint16_t rawY, int16_t panelWidth, int16_t panelHeight,
                  DisplayOrientation orientation, PanelPoint& logical);

}  // namespace papyrix::board
