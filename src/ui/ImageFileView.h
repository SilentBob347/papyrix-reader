#pragma once

#include <string>

class GfxRenderer;
struct Theme;

namespace ui {

struct ButtonBar;

// Vertical space reserved for the button bar drawn by renderImageFile
// (touch::buttonBarButtonRect places 46px boxes at screenHeight - 50).
inline constexpr int IMAGE_VIEW_BUTTON_BAR_HEIGHT = 50;

// Hidden cache path for an image: "/dir/photo.jpg" -> "/dir/.photo.jpg.bmp".
std::string hiddenImageCachePath(const std::string& imagePath);

// Remove the hidden cache sibling of an image (after the source was moved,
// renamed, or deleted).
void removeImageCache(const std::string& imagePath);

// Remove all generated image cache files inside a directory (before rmdir).
void removeImageCachesInDir(const std::string& dirPath);

// True when ensureRenderableBmp will run a conversion (source is not BMP and
// no fresh hidden cache exists).
bool needsBmpConversion(const std::string& imagePath);

// Resolve an image path to a renderable BMP path.
// BMP input returns as-is. JPEG/PNG convert once to a hidden sibling
// (".<name>.bmp" next to the source) and reuse it while it is not older
// than the source. Returns an empty string on failure.
std::string ensureRenderableBmp(const std::string& imagePath, int maxWidth, int maxHeight, const char* logTag);

// Draw a centered error message with the button bar (no display flush; the
// caller flushes once).
void drawImageError(const GfxRenderer& renderer, const Theme& theme, const char* message, const ButtonBar& buttons);

// Render a BMP centered in a screen-wide viewport of viewportHeight, with the
// button bar below. Does the grayscale two-pass refresh when the BMP has
// grayscale data. Returns false on open/parse failure; the error message is
// drawn but the caller must flush the display.
bool renderImageFile(GfxRenderer& renderer, const Theme& theme, const std::string& bmpPath, int viewportHeight,
                     const ButtonBar& buttons);

}  // namespace ui
