#pragma once

#include <DisplayController.h>

#include <cstdint>

namespace papyrix::board {

enum class ClassicPanelSource : uint8_t { Unresolved, Factory, Probe };
struct ClassicPanelDecision {
  bool resolved = false;
  eink::DisplayController controller = eink::DisplayController::SSD1677;
  ClassicPanelSource source = ClassicPanelSource::Unresolved;
  uint8_t variant = 0;
};

ClassicPanelDecision resolveClassicPanel(bool factoryPresent, uint8_t factoryValue, bool probeValid, uint8_t probeId);

}  // namespace papyrix::board
