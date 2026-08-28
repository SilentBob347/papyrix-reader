#include <GfxRenderer.h>

#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("GfxRenderer background hint");

  papyrix::hal::Display display(8, 10, 21, 4, 5, 6);
  GfxRenderer renderer(display);
  renderer.begin();

  renderer.clearScreen(0x00);
  renderer.displayBuffer(papyrix::hal::Display::FAST_REFRESH, false);
  runner.expectTrue(display.backgroundHint(), "black B/W clear sets dark hint");

  renderer.clearScreen(0xFF);
  renderer.displayBuffer(papyrix::hal::Display::FAST_REFRESH, false);
  runner.expectFalse(display.backgroundHint(), "white B/W clear clears dark hint");

  renderer.clearScreen(0x00);
  renderer.displayBuffer(papyrix::hal::Display::FAST_REFRESH, false);

  renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
  renderer.clearScreen(0xFF);
  renderer.displayBuffer(papyrix::hal::Display::FAST_REFRESH, false);
  runner.expectTrue(display.backgroundHint(), "grayscale clear preserves dark hint");

  renderer.setRenderMode(GfxRenderer::BW);
  renderer.invertScreen();
  renderer.displayBufferDriveAll(false);
  runner.expectFalse(display.backgroundHint(), "B/W inversion toggles dark hint");

  renderer.clearScreen(0x00);
  renderer.displayWindow(0, 0, 16, 16, false);
  runner.expectTrue(display.backgroundHint(), "window display receives dark hint");

  display.setDisplayX3();
  renderer.begin();
  renderer.setOrientation(GfxRenderer::Portrait);
  renderer.clearScreen(0xFF);
  renderer.drawPixel(0, 0, true);
  runner.expectEq(528, renderer.getScreenWidth(), "X3 portrait width uses runtime geometry");
  runner.expectEq(792, renderer.getScreenHeight(), "X3 portrait height uses runtime geometry");
  runner.expectEq(uint8_t{0x7F}, display.getFrameBuffer()[527 * 99],
                  "X3 logical origin maps to the last physical row with a 99-byte stride");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
