#include <Bitmap.h>
#include <CoverHelpers.h>
#include <GfxRenderer.h>
#include <I18n.h>
#include <Logging.h>
#include <SDCardManager.h>
#include <SdFat.h>
#include <Theme.h>

#include "Elements.h"
#include "ImageFileView.h"
#include "WrappedText.h"

#define TAG "IMG_VIEW"

namespace ui {

// Same layout as centeredMessage but without the display flush: the caller
// flushes once after this function returns.
void drawImageError(const GfxRenderer& renderer, const Theme& theme, const char* message, const ButtonBar& buttons) {
  const int y = renderer.getScreenHeight() / 2 - renderer.getLineHeight(theme.uiFontId) / 2;
  const int maxWidth = renderer.getScreenWidth() - 2 * (theme.screenMarginSide + theme.itemPaddingX);
  centeredTextWrapped(renderer, theme.uiFontId, y, message, maxWidth, 3, theme.primaryTextBlack, EpdFontFamily::BOLD);
  buttonBar(renderer, theme, buttons);
}

bool renderImageFile(GfxRenderer& renderer, const Theme& theme, const std::string& bmpPath, int viewportHeight,
                     const ButtonBar& buttons) {
  renderer.clearScreen(theme.backgroundColor);

  FsFile file;
  if (!SdMan.openFileForRead(TAG, bmpPath, file)) {
    LOG_ERR(TAG, "Failed to open: %s", bmpPath.c_str());
    drawImageError(renderer, theme, tr(IMAGE_OPEN_FAILED), buttons);
    return false;
  }

  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) {
    LOG_ERR(TAG, "Invalid BMP: %s", bmpPath.c_str());
    file.close();
    drawImageError(renderer, theme, tr(IMAGE_INVALID), buttons);
    return false;
  }

  const auto rect = CoverHelpers::calculateCenteredRect(bitmap.getWidth(), bitmap.getHeight(), 0, 0,
                                                        renderer.getScreenWidth(), viewportHeight);
  renderer.drawBitmapOnWhite(bitmap, rect.x, rect.y, rect.width, rect.height);
  buttonBar(renderer, theme, buttons);
  renderer.displayBuffer();

  if (bitmap.hasGreyscale()) {
    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_LSB);
    renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
    renderer.copyGrayscaleLsbBuffers();

    bitmap.rewindToData();
    renderer.clearScreen(0x00);
    renderer.setRenderMode(GfxRenderer::GRAYSCALE_MSB);
    renderer.drawBitmap(bitmap, rect.x, rect.y, rect.width, rect.height);
    renderer.copyGrayscaleMsbBuffers();

    renderer.displayGrayBuffer();
    renderer.setRenderMode(GfxRenderer::BW);

    bitmap.rewindToData();
    renderer.clearScreen(theme.backgroundColor);
    // Match the initial BW pass: the reconstruction becomes the controller's
    // differential-refresh baseline, so both buffers must agree.
    renderer.drawBitmapOnWhite(bitmap, rect.x, rect.y, rect.width, rect.height);
    buttonBar(renderer, theme, buttons);
    renderer.cleanupGrayscaleWithFrameBuffer();
  }

  file.close();
  return true;
}

}  // namespace ui
