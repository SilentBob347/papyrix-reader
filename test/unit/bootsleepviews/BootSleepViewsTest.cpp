#include "test_utils.h"

#include "ui/views/BootSleepViews.h"
#include "images/PapyrixLogo.h"

#include <algorithm>
#include <array>
#include <cstdint>

uint8_t GfxRenderer::frameBuffer_[papyrix::hal::Display::BUFFER_SIZE];

namespace {

bool bufferIs(const GfxRenderer& renderer, uint8_t value) {
  const uint8_t* buffer = renderer.getFrameBuffer();
  return std::all_of(buffer, buffer + renderer.getBufferSize(), [value](uint8_t byte) { return byte == value; });
}

struct Bounds {
  int minX = 480;
  int minY = 800;
  int maxX = -1;
  int maxY = -1;
};

Bounds blackBounds(const GfxRenderer& renderer) {
  Bounds bounds;
  const uint8_t* buffer = renderer.getFrameBuffer();
  constexpr int panelWidthBytes = papyrix::hal::Display::DISPLAY_WIDTH_BYTES;
  constexpr int panelHeight = papyrix::hal::Display::DISPLAY_HEIGHT;
  for (int y = 0; y < 800; y++) {
    for (int x = 0; x < 480; x++) {
      const int panelX = y;
      const int panelY = panelHeight - 1 - x;
      const uint8_t mask = static_cast<uint8_t>(0x80 >> (panelX % 8));
      if ((buffer[panelY * panelWidthBytes + panelX / 8] & mask) != 0) continue;
      bounds.minX = std::min(bounds.minX, x);
      bounds.minY = std::min(bounds.minY, y);
      bounds.maxX = std::max(bounds.maxX, x);
      bounds.maxY = std::max(bounds.maxY, y);
    }
  }
  return bounds;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("BootSleepViewsTest");
  papyrix::hal::Display display;
  GfxRenderer renderer(display);

  ui::BootView bootView;
  bootView.setStatus("BOOTING");

  renderer.clearCenteredTextCalls();
  ui::render(renderer, BUILTIN_LIGHT_THEME, bootView);
  runner.expectTrue(bufferIs(renderer, 0xFF), "light boot uses a blank background");
  runner.expectTrue(renderer.centeredTextCalls().size() == 1 &&
                        renderer.centeredTextCalls().front().text == "BOOTING" &&
                        renderer.centeredTextCalls().front().y == renderer.getScreenHeight() / 2,
                    "boot shows only the centered status");

  bootView.setDarkMode(true);
  renderer.clearCenteredTextCalls();
  ui::render(renderer, BUILTIN_DARK_THEME, bootView);
  runner.expectTrue(bufferIs(renderer, 0x00), "dark boot reverses the full status screen");
  runner.expectTrue(renderer.centeredTextCalls().size() == 1, "dark boot shows only the status");

  static const std::array<uint8_t, PapyrixLogoSize * PapyrixLogoSize / 8> blackLogo{};
  ui::SleepView centeredSleepView;
  centeredSleepView.setLogo(blackLogo.data(), PapyrixLogoSize, PapyrixLogoSize);
  ui::render(renderer, BUILTIN_LIGHT_THEME, centeredSleepView);
  const Bounds centeredLogoBounds = blackBounds(renderer);
  runner.expectTrue(centeredLogoBounds.minX == 48 && centeredLogoBounds.maxX == 431 &&
                        centeredLogoBounds.minY == 208 && centeredLogoBounds.maxY == 591,
                    "the sleep logo canvas is centered exactly");

  ui::SleepView sleepView;
  sleepView.setLogo(PapyrixLogo, PapyrixLogoSize, PapyrixLogoSize);

  renderer.clearCenteredTextCalls();
  ui::render(renderer, BUILTIN_LIGHT_THEME, sleepView);
  const Bounds logoBounds = blackBounds(renderer);
  runner.expectTrue(logoBounds.maxY - logoBounds.minY > logoBounds.maxX - logoBounds.minX,
                    "the device renders the sleep logo upright");
  runner.expectTrue(logoBounds.maxX - logoBounds.minX > 200 && logoBounds.maxY - logoBounds.minY > 300,
                    "the sleep logo is prominent on the device");
  runner.expectTrue(renderer.centeredTextCalls().empty(), "the final sleep screen shows only the logo");

  std::array<uint8_t, papyrix::hal::Display::BUFFER_SIZE> lightSleepBuffer;
  std::copy(renderer.getFrameBuffer(), renderer.getFrameBuffer() + renderer.getBufferSize(), lightSleepBuffer.begin());
  sleepView.setDarkMode(true);
  ui::render(renderer, BUILTIN_LIGHT_THEME, sleepView);
  runner.expectTrue(std::equal(lightSleepBuffer.begin(), lightSleepBuffer.end(), renderer.getFrameBuffer(),
                              [](uint8_t light, uint8_t dark) {
                                return dark == static_cast<uint8_t>(~light);
                              }),
                    "dark sleep reverses the full logo screen");

  static const std::array<uint8_t, PapyrixLogoSize * 900 / 8> tallImage{};
  ui::SleepView offscreenImageView;
  offscreenImageView.setMode(ui::SleepView::Mode::Custom);
  offscreenImageView.setImage(tallImage.data(), PapyrixLogoSize, 900);
  ui::render(renderer, BUILTIN_LIGHT_THEME, offscreenImageView);
  runner.expectTrue(bufferIs(renderer, 0xFF), "off-screen image anchors do not write to the framebuffer");

  return runner.allPassed() ? 0 : 1;
}
