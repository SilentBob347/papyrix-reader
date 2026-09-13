#pragma once

#include <GfxRenderer.h>
#include <Theme.h>

#include <cstdint>
#include <cstring>

#include "../Elements.h"

namespace ui {

// ============================================================================
// BootView - Boot progress screen
// ============================================================================

struct BootView {
  static constexpr int MAX_STATUS_LEN = 32;

  char status[MAX_STATUS_LEN] = "Starting...";
  bool darkMode = false;
  bool needsRender = true;

  void setStatus(const char* s) {
    strncpy(status, s, MAX_STATUS_LEN - 1);
    status[MAX_STATUS_LEN - 1] = '\0';
    needsRender = true;
  }

  void setDarkMode(bool dark) {
    darkMode = dark;
    needsRender = true;
  }
};

void render(const GfxRenderer& r, const Theme& t, const BootView& v);

// ============================================================================
// SleepView - Sleep screen with optional image
// ============================================================================

struct SleepView {
  enum class Mode : uint8_t {
    Logo,       // Show PapyriX logo
    BookCover,  // Show current book cover
    Black,      // Black screen
    Custom      // Custom image
  };

  // External logo pointer (not owned) - for Logo mode
  const uint8_t* logoData = nullptr;
  int16_t logoWidth = 0;
  int16_t logoHeight = 0;

  // External image pointer (not owned) - for BookCover/Custom modes
  const uint8_t* imageData = nullptr;
  int16_t imageWidth = 0;
  int16_t imageHeight = 0;

  Mode mode = Mode::Logo;
  bool darkMode = false;
  bool needsRender = true;

  void setMode(Mode m) {
    mode = m;
    needsRender = true;
  }

  void setLogo(const uint8_t* data, int w, int h) {
    logoData = data;
    logoWidth = static_cast<int16_t>(w);
    logoHeight = static_cast<int16_t>(h);
    needsRender = true;
  }

  void setImage(const uint8_t* data, int w, int h) {
    imageData = data;
    imageWidth = static_cast<int16_t>(w);
    imageHeight = static_cast<int16_t>(h);
    needsRender = true;
  }

  void setDarkMode(bool dark) {
    darkMode = dark;
    needsRender = true;
  }
};

void render(const GfxRenderer& r, const Theme& t, const SleepView& v);

}  // namespace ui
