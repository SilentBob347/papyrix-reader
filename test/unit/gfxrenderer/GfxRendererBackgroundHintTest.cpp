#include <GfxRenderer.h>

#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("GfxRenderer background hint");

  EInkDisplay display(8, 10, 21, 4, 5, 6);
  GfxRenderer renderer(display);
  renderer.begin();

  renderer.clearScreen(0x00);
  renderer.displayBuffer(EInkDisplay::FAST_REFRESH, false);
  runner.expectTrue(display.backgroundHint(), "black B/W clear sets dark hint");

  renderer.clearScreen(0xFF);
  renderer.displayBuffer(EInkDisplay::FAST_REFRESH, false);
  runner.expectFalse(display.backgroundHint(), "white B/W clear clears dark hint");

  renderer.clearScreen(0x00);
  renderer.displayBuffer(EInkDisplay::FAST_REFRESH, false);

  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(EInkDisplay::FAST_REFRESH, false);
  runner.expectTrue(display.backgroundHint(), "grayscale clear preserves dark hint");

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.invertScreen();
  renderer.displayBufferDriveAll(false);
  runner.expectFalse(display.backgroundHint(), "B/W inversion toggles dark hint");

  renderer.clearScreen(0x00);
  renderer.displayWindow(0, 0, 16, 16, false);
  runner.expectTrue(display.backgroundHint(), "window display receives dark hint");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
