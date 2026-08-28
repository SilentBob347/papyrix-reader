#include "test_utils.h"
#include "ui/Elements.h"

int main() {
  TestUtils::TestRunner runner("Battery indicator");
  papyrix::hal::Display display;
  GfxRenderer renderer(display);
  Theme theme{};
  ui::battery(renderer, theme, 0, 0, -1);
  runner.expectTrue(renderer.lastText() == "--%", "unknown charge does not display a negative percentage");
  ui::battery(renderer, theme, 0, 0, 0);
  runner.expectTrue(renderer.lastText() == "0%", "empty battery remains distinct from unknown charge");
  return runner.allPassed() ? 0 : 1;
}
