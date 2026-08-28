#include "HardwareIdentity.h"

namespace papyrix::board {

HardwareIdentity& HardwareIdentity::instance() {
  static HardwareIdentity identity;
  return identity;
}

HardwareIdentity::HardwareIdentity()
    : profile_(&bootProfile()),
      boardSource_(PAPYRIX_TARGET_XTEINK_C3 ? BoardSelectionSource::Fallback : BoardSelectionSource::Fixed) {}

bool HardwareIdentity::applyBoardSelection(const BoardSelection& selection, const BoardProbeReport& report) {
  const BoardProfile* selected = findProfile(selection.board);
  if (selected == nullptr) return false;
  profile_ = selected;
  boardSource_ = selection.source;
  lastProbe_ = report;
  boardSelected_ = true;
  return true;
}

void HardwareIdentity::setPanel(eink::DisplayController panel, PanelSelectionSource source, uint8_t variant) {
  panel_ = panel;
  panelSource_ = source;
  panelVariant_ = variant;
}

const char* panelSelectionSourceName(PanelSelectionSource source) {
  switch (source) {
    case PanelSelectionSource::Fixed:
      return "fixed";
    case PanelSelectionSource::X4Default:
      return "x4-default";
    case PanelSelectionSource::Override:
      return "override";
    case PanelSelectionSource::Cache:
      return "cache";
    case PanelSelectionSource::Probe:
      return "probe";
    case PanelSelectionSource::Fallback:
      return "fallback";
  }
  return "unknown";
}

}  // namespace papyrix::board
