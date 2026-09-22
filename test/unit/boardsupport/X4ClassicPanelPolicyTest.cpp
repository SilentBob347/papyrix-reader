#include <X4ClassicPanelPolicy.h>
#include <cassert>

int main() {
  using namespace papyrix::board;
  using papyrix::eink::DisplayController;
  for (unsigned value = 0; value < 256; ++value) {
    const bool supported = value == 1 || value == 2 || value == 3 || value == 0x0B || value == 0x0C;
    const auto factory = resolveClassicPanel(true, value, false, 0);
    assert(factory.resolved == supported);
    if (supported) assert(factory.source == ClassicPanelSource::Factory);
  }
  const auto conflict = resolveClassicPanel(true, 2, true, 1);
  assert(conflict.controller == DisplayController::UC8279_X4PRO && conflict.variant == 0);
  const auto ssd = resolveClassicPanel(true, 3, true, 0x69);
  assert(ssd.controller == DisplayController::SSD1677 && ssd.variant == 0);
  for (unsigned id = 0; id < 256; ++id) {
    const bool supported = id == 1 || id == 2 || id == 3 || id == 0x67 || id == 0x68 || id == 0x69;
    const auto probe = resolveClassicPanel(false, 0, true, id);
    assert(probe.resolved == supported);
    if (supported) assert(probe.source == ClassicPanelSource::Probe && probe.variant == id);
    assert(!resolveClassicPanel(false, 0, false, id).resolved);
  }
  assert(resolveClassicPanel(true, 0xFF, true, 0x68).resolved);
}
