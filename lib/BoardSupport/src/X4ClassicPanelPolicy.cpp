#include "X4ClassicPanelPolicy.h"

namespace papyrix::board {

ClassicPanelDecision resolveClassicPanel(bool factoryPresent, uint8_t factoryValue, bool probeValid, uint8_t probeId) {
  using eink::DisplayController;
  ClassicPanelDecision probe{};
  if (probeValid) {
    switch (probeId) {
      case 0x01:
        probe = {true, DisplayController::UC8179_X4PRO, ClassicPanelSource::Probe, probeId};
        break;
      case 0x02:
      case 0x03:
      case 0x67:
      case 0x68:
      case 0x69:
        probe = {true, DisplayController::UC8279_X4PRO, ClassicPanelSource::Probe, probeId};
        break;
    }
  }
  if (factoryPresent) {
    switch (factoryValue) {
      case 1:
      case 0x0B:
        return {true, DisplayController::UC8179_X4PRO, ClassicPanelSource::Factory, 0};
      case 2:
      case 0x0C:
        return {true, DisplayController::UC8279_X4PRO, ClassicPanelSource::Factory,
                static_cast<uint8_t>(
                    probe.resolved && probe.controller == DisplayController::UC8279_X4PRO ? probe.variant : 0)};
      case 3:
        return {true, DisplayController::SSD1677, ClassicPanelSource::Factory, 0};
    }
  }
  return probe;
}

}  // namespace papyrix::board
