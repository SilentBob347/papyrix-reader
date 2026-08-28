#include "HardwareRecovery.h"
#include "test_utils.h"

using namespace papyrix::board;

int main() {
  TestUtils::TestRunner r("Hardware recovery");

  r.expectTrue(fixedPanelFor(BoardId::X3) == papyrix::eink::DisplayController::UC8253,
               "X3 fixed fallback remains UC8253");
  r.expectTrue(fixedPanelFor(BoardId::X4) == papyrix::eink::DisplayController::SSD1677,
               "X4 fixed panel remains SSD1677");
  r.expectTrue(fixedPanelFor(BoardId::X4Pro) == papyrix::eink::DisplayController::SSD1677,
               "X4 Pro fallback stays on the conservative SSD1677 path");

  return r.allPassed() ? 0 : 1;
}
