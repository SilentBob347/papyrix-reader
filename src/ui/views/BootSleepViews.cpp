#include "BootSleepViews.h"

#include <Display.h>

namespace ui {

void render(const GfxRenderer& r, const Theme& t, const BootView& v) {
  const auto pageHeight = r.getScreenHeight();

  r.clearScreen(0xFF);
  r.drawCenteredText(t.uiFontId, pageHeight / 2, v.status, true);

  if (v.darkMode) {
    r.invertScreen();
  }
  r.displayBuffer();
}

void render(const GfxRenderer& r, const Theme&, const SleepView& v) {
  const auto pageWidth = r.getScreenWidth();
  const auto pageHeight = r.getScreenHeight();
  const int logoY = (pageHeight - v.logoHeight) / 2;

  r.clearScreen(0xFF);

  if (v.mode == SleepView::Mode::Logo) {
    if (v.logoData != nullptr) {
      const int logoX = (pageWidth - v.logoWidth) / 2 + v.logoWidth - 1;
      r.drawImage(v.logoData, logoX, logoY, v.logoWidth, v.logoHeight);
    }

    if (v.darkMode) {
      r.invertScreen();
    }
  } else if (v.mode == SleepView::Mode::Black) {
    r.clearScreen(0x00);
  } else if (v.mode == SleepView::Mode::BookCover || v.mode == SleepView::Mode::Custom) {
    // Image modes: center the image
    if (v.imageData != nullptr) {
      const int imageX = (pageWidth - v.imageWidth) / 2;
      const int imageY = (pageHeight - v.imageHeight) / 2;
      r.drawImage(v.imageData, imageX, imageY, v.imageWidth, v.imageHeight);
    }
  }

  // Use HALF_REFRESH for sleep (matches old SleepActivity)
  r.displayBuffer(papyrix::hal::Display::HALF_REFRESH);
}

}  // namespace ui
