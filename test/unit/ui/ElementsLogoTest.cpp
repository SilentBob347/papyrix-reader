#include "test_utils.h"
#include "ui/Elements.h"

#include <string>

namespace {

bool inBox(const GfxRenderer::FillRectCall& call, int x0, int y0, int size) {
  return call.x >= x0 && call.y >= y0 && call.w > 0 && call.h > 0 && call.x + call.w <= x0 + size &&
         call.y + call.h <= y0 + size;
}

void expectMark(TestUtils::TestRunner& runner, GfxRenderer& renderer, const Theme& theme, const char* name,
                void (*draw)(const GfxRenderer&, const Theme&, int, int)) {
  constexpr int kSize = 192;
  constexpr int kCx = 240;
  constexpr int kCy = 200;
  const int x0 = kCx - kSize / 2;
  const int y0 = kCy - kSize / 2;
  renderer.clearFillRects();
  draw(renderer, theme, kCx, kCy);
  const auto& rects = renderer.fillRects();
  runner.expectTrue(!rects.empty(), std::string(name) + " draws ink");
  bool inBounds = true;
  bool sawInk = false;
  bool extraColor = false;
  for (const auto& call : rects) {
    if (!inBox(call, x0, y0, kSize)) inBounds = false;
    if (call.color == theme.primaryTextBlack) sawInk = true;
    if (call.color != theme.primaryTextBlack) extraColor = true;
  }
  runner.expectTrue(inBounds, std::string(name) + " stays in 192x192");
  runner.expectTrue(sawInk, std::string(name) + " uses primaryTextBlack");
  runner.expectTrue(!extraColor, std::string(name) + " ink only");
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("App marks");
  papyrix::hal::Display display;
  GfxRenderer renderer(display);

  Theme light{};
  light.primaryTextBlack = true;
  expectMark(runner, renderer, light, "localsend light", ui::localsendLogo);
  expectMark(runner, renderer, light, "printer light", ui::printerLogo);

  Theme dark{};
  dark.primaryTextBlack = false;
  dark.invertedMode = true;
  dark.backgroundColor = 0x00;
  expectMark(runner, renderer, dark, "localsend dark", ui::localsendLogo);
  expectMark(runner, renderer, dark, "printer dark", ui::printerLogo);

  return runner.allPassed() ? 0 : 1;
}
