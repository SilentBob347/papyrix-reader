#pragma once

#include <cstdint>
#include <ctime>

class GfxRenderer;
struct Theme;

namespace papyrix::clock_faces {

enum class Face : uint8_t {
  Big = 0,
  Classic = 1,
  Analog = 2,
  Retro = 3,
  Flip = 4,
  Minimal = 5,
  Serif = 6,
  DayNight = 7,
  Count = 8,
};

struct Context {
  GfxRenderer& renderer;
  const Theme& theme;
  const std::tm& time;
  bool use24h;
  int8_t dateFormat;
  int8_t utcOffset;
};

bool isSelectable(Face face);
Face selectRelative(Face face, int delta);
const char* name(Face face);
void render(const Context& context, Face face);

}  // namespace papyrix::clock_faces
