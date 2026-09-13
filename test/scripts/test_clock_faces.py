#!/usr/bin/env python3
"""Exercise clock face rendering with host renderer substitutes."""

import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

GFX_RENDERER = r'''#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

class GfxRenderer {
 public:
  struct Draw {
    char kind;
    int x;
    int y;
    int width;
    int height;
    bool color;
    std::string text;
  };

  GfxRenderer(int width, int height)
      : width_(width), height_(height), pixels_(static_cast<size_t>(width) * height, 0) {}

  int getScreenWidth() const { return width_; }
  int getScreenHeight() const { return height_; }
  int getLineHeight(int) const { return 16; }
  int getTextWidth(int, const char* text) const { return static_cast<int>(std::strlen(text)) * 8; }
  void clear(bool color) const { std::fill(pixels_.begin(), pixels_.end(), color); }
  void drawPixel(int x, int y, bool color = true) const {
    draws.push_back({'P', x, y, 1, 1, color, {}});
    setPixel(x, y, color);
  }
  void fillRect(int x, int y, int width, int height, bool color = true) const {
    draws.push_back({'F', x, y, width, height, color, {}});
    for (int py = y; py < y + height; ++py) {
      for (int px = x; px < x + width; ++px) setPixel(px, py, color);
    }
  }
  void drawRect(int x, int y, int width, int height, bool color = true) const {
    draws.push_back({'R', x, y, width, height, color, {}});
    rasterLine(x, y, x + width - 1, y, color);
    rasterLine(x + width - 1, y, x + width - 1, y + height - 1, color);
    rasterLine(x + width - 1, y + height - 1, x, y + height - 1, color);
    rasterLine(x, y + height - 1, x, y, color);
  }
  void drawLine(int x1, int y1, int x2, int y2, bool color = true) const {
    draws.push_back({'L', std::min(x1, x2), std::min(y1, y2), std::abs(x2 - x1) + 1,
                     std::abs(y2 - y1) + 1, color, {}});
    rasterLine(x1, y1, x2, y2, color);
  }
  void drawText(int, int x, int y, const char* text, bool color = true) const {
    const int width = getTextWidth(0, text);
    const int height = getLineHeight(0);
    draws.push_back({'T', x, y, width, height, color, text});
    for (int px = x; px < x + width; ++px) {
      setPixel(px, y, color);
      setPixel(px, y + height - 1, color);
    }
  }
  void drawCenteredText(int fontId, int y, const char* text, bool color = true) const {
    drawText(fontId, (width_ - getTextWidth(fontId, text)) / 2, y, text, color);
  }
  bool allDrawsInside(int left, int top, int right, int bottom) const {
    return std::all_of(draws.begin(), draws.end(), [&](const Draw& draw) {
      return draw.x >= left && draw.y >= top && draw.x + draw.width <= right && draw.y + draw.height <= bottom;
    });
  }
  bool hasText(const char* text) const {
    return std::any_of(draws.begin(), draws.end(), [&](const Draw& draw) { return draw.text == text; });
  }
  int textY(const char* text) const {
    const auto draw = std::find_if(draws.begin(), draws.end(), [&](const Draw& value) { return value.text == text; });
    return draw == draws.end() ? -1 : draw->y;
  }
  bool hasDraw(char kind, int width, int height, bool color) const {
    return std::any_of(draws.begin(), draws.end(), [&](const Draw& draw) {
      return draw.kind == kind && draw.width == width && draw.height == height && draw.color == color;
    });
  }
  bool pixelAt(int x, int y) const { return pixels_[static_cast<size_t>(y) * width_ + x] != 0; }
  int countPixels(int left, int top, int right, int bottom, bool color) const {
    int count = 0;
    for (int y = top; y < bottom; ++y) {
      for (int x = left; x < right; ++x) {
        if (pixelAt(x, y) == color) ++count;
      }
    }
    return count;
  }
  void writePbm(const std::string& path) const {
    std::ofstream output(path, std::ios::binary);
    output << "P4\n" << width_ << ' ' << height_ << '\n';
    for (int y = 0; y < height_; ++y) {
      for (int x = 0; x < width_; x += 8) {
        uint8_t byte = 0;
        for (int bit = 0; bit < 8 && x + bit < width_; ++bit) {
          if (pixels_[static_cast<size_t>(y) * width_ + x + bit]) byte |= 0x80 >> bit;
        }
        output.put(static_cast<char>(byte));
      }
    }
  }

  mutable std::vector<Draw> draws;

 private:
  void setPixel(int x, int y, bool color) const {
    if (x >= 0 && x < width_ && y >= 0 && y < height_) {
      pixels_[static_cast<size_t>(y) * width_ + x] = color;
    }
  }
  void rasterLine(int x1, int y1, int x2, int y2, bool color) const {
    const int dx = std::abs(x2 - x1);
    const int sx = x1 < x2 ? 1 : -1;
    const int dy = -std::abs(y2 - y1);
    const int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;
    while (true) {
      setPixel(x1, y1, color);
      if (x1 == x2 && y1 == y2) break;
      const int twiceError = 2 * error;
      if (twiceError >= dy) {
        error += dy;
        x1 += sx;
      }
      if (twiceError <= dx) {
        error += dx;
        y1 += sy;
      }
    }
  }

  int width_;
  int height_;
  mutable std::vector<uint8_t> pixels_;
};
'''

THEME = r'''#pragma once
#include <cstdint>

struct Theme {
  bool invertedMode = false;
  bool primaryTextBlack = true;
  bool secondaryTextBlack = true;
  uint8_t backgroundColor = 0xFF;
  int uiFontId = 0;
  int smallFontId = 1;
};

inline Theme lightTheme() { return {}; }
inline Theme darkTheme() {
  Theme theme;
  theme.invertedMode = true;
  theme.primaryTextBlack = false;
  theme.secondaryTextBlack = false;
  theme.backgroundColor = 0x00;
  return theme;
}
'''

HARNESS = r'''#include <cassert>
#include <cstring>
#include <ctime>
#include <string>
#include <utility>

#include <GfxRenderer.h>
#include <Theme.h>
#include "apps/ClockFacePrimitives.h"
#include "apps/ClockFaces.h"

int main(int argc, char** argv) {
  using namespace papyrix::clock_face_primitives;

  for (const auto [width, height] : {std::pair{480, 800}, std::pair{528, 792}}) {
    GfxRenderer renderer(width, height);
    const Layout layout = makeLayout(renderer);
    assert(layout.width == width);
    assert(layout.height == height);
    assert(layout.centerX == width / 2);
    assert(layout.top == 58);
    assert(layout.bottom == height - 64);
    assert(layout.sideMargin == (width == 528 ? 22 : 18));
  }

  std::tm time{};
  time.tm_year = 126;
  time.tm_mon = 10;
  time.tm_mday = 23;
  time.tm_wday = 1;
  time.tm_hour = 0;
  time.tm_min = 7;

  char date[24];
  formatDate(date, sizeof(date), time, 0);
  assert(std::strcmp(date, "MON 2026/11/23") == 0);
  formatDate(date, sizeof(date), time, 1);
  assert(std::strcmp(date, "MON 23/11/2026") == 0);
  formatDate(date, sizeof(date), time, 2);
  assert(std::strcmp(date, "MON 11/23/2026") == 0);
  formatDate(date, sizeof(date), time, 3);
  assert(std::strcmp(date, "MON 23.11.2026") == 0);

  const DisplayTime midnight = displayTime(time, false);
  assert(midnight.hour == 12 && midnight.minute == 7);
  assert(!midnight.showLeadingZero && std::strcmp(midnight.period, "AM") == 0);
  const DisplayTime midnight24 = displayTime(time, true);
  assert(midnight24.hour == 0 && midnight24.minute == 7);
  assert(midnight24.showLeadingZero && std::strcmp(midnight24.period, "") == 0);

  GfxRenderer primitiveRenderer(480, 800);
  drawBlockDigit(primitiveRenderer, 20, 60, 8, 10, true);
  drawSevenSegmentDigit(primitiveRenderer, 100, 60, 8, 80, 160, 14, true);
  drawCircle(primitiveRenderer, 240, 300, 100, 3, true);
  fillCircle(primitiveRenderer, 240, 300, 50, true);
  drawThickLine(primitiveRenderer, 240, 300, 300, 240, 5, true);
  assert(primitiveRenderer.allDrawsInside(0, 0, 480, 800));

  const size_t drawCount = primitiveRenderer.draws.size();
  drawBlockDigit(primitiveRenderer, 0, 0, -1, 10, true);
  drawSevenSegmentDigit(primitiveRenderer, 0, 0, 10, 80, 160, 14, true);
  assert(primitiveRenderer.draws.size() == drawCount);

  using namespace papyrix::clock_faces;
  static_assert(static_cast<uint8_t>(Face::Big) == 0);
  static_assert(static_cast<uint8_t>(Face::Classic) == 1);
  static_assert(static_cast<uint8_t>(Face::Analog) == 2);
  static_assert(static_cast<uint8_t>(Face::Retro) == 3);
  static_assert(static_cast<uint8_t>(Face::Flip) == 4);
  static_assert(static_cast<uint8_t>(Face::Minimal) == 5);
  static_assert(static_cast<uint8_t>(Face::Serif) == 6);
  static_assert(static_cast<uint8_t>(Face::DayNight) == 7);
  assert(std::strcmp(name(Face::DayNight), "Day & Night") == 0);

  const auto clockBounds = [](const GfxRenderer& renderer, const char* date, int right) {
    int top = 10000;
    int bottom = -1;
    for (const auto& draw : renderer.draws) {
      if (draw.text == date || draw.x + draw.width > right) continue;
      top = std::min(top, draw.y);
      bottom = std::max(bottom, draw.y + draw.height);
    }
    return std::pair{top, bottom};
  };
  const auto dayNightIconY = [](const GfxRenderer& renderer, bool use24h) {
    const Layout layout = makeLayout(renderer);
    constexpr int timeHeight = 7 * 12;
    constexpr int iconHeight = 2 * 70;
    const int periodHeight = use24h ? 0 : 8 + renderer.getLineHeight(0);
    const int labelHeight = renderer.getLineHeight(0);
    const int groupHeight = timeHeight + periodHeight + 24 + iconHeight + 12 + labelHeight;
    const int timeY = layout.top + (layout.contentHeight - groupHeight) / 2;
    return timeY + timeHeight + periodHeight + 24 + iconHeight / 2;
  };

  assert(selectRelative(Face::Big, 1) == Face::Analog);
  assert(selectRelative(Face::Analog, 1) == Face::Retro);
  assert(selectRelative(Face::Retro, 1) == Face::Flip);
  assert(selectRelative(Face::Flip, 1) == Face::DayNight);
  assert(selectRelative(Face::DayNight, 1) == Face::Big);
  assert(selectRelative(Face::Big, -1) == Face::DayNight);
  assert(selectRelative(Face::Classic, 1) == Face::Big);
  assert(!isSelectable(Face::Classic));
  assert(!isSelectable(Face::Minimal));
  assert(!isSelectable(Face::Serif));

  const Face faces[] = {Face::Big, Face::Analog, Face::Retro, Face::Flip, Face::DayNight};
  const char* fileNames[] = {"big", "analog", "retro", "flip", "day-night"};
  for (const auto [width, height] : {std::pair{480, 800}, std::pair{528, 792}}) {
    for (size_t index = 0; index < std::size(faces); ++index) {
      GfxRenderer renderer(width, height);
      Theme theme = lightTheme();
      renderer.clear(theme.backgroundColor == 0x00);
      std::tm value{};
      value.tm_year = 126;
      value.tm_mon = 8;
      value.tm_mday = 9;
      value.tm_wday = 3;
      value.tm_hour = 19;
      value.tm_min = 52;
      render({renderer, theme, value, true, 3, 0}, faces[index]);
      assert(renderer.allDrawsInside(0, 58, width, height - 64));
      assert(renderer.hasText("WED 09.09.2026"));
      assert(renderer.textY("WED 09.09.2026") == makeLayout(renderer).top + 12);
      const Layout faceLayout = makeLayout(renderer);
      const auto [clockTop, clockBottom] = clockBounds(renderer, "WED 09.09.2026", width);
      assert(std::abs(clockTop + clockBottom - faceLayout.top - faceLayout.bottom) <= 2);
      if (faces[index] == Face::DayNight) {
        const auto [timeTop, timeBottom] =
            clockBounds(renderer, "WED 09.09.2026", faceLayout.centerX - 70);
        const auto phaseLabel = std::find_if(renderer.draws.begin(), renderer.draws.end(), [](const auto& draw) {
          return draw.kind == 'T' && draw.text != "WED 09.09.2026";
        });
        assert(timeTop < timeBottom);
        assert(phaseLabel != renderer.draws.end());
        assert(phaseLabel->y >= timeBottom);
      }
      if (argc == 2) {
        renderer.writePbm(std::string(argv[1]) + "/" + fileNames[index] + "-" + std::to_string(width) + "x" +
                          std::to_string(height) + ".pbm");
      }
    }
  }

  for (Theme theme : {lightTheme(), darkTheme()}) {
    const bool backgroundBlack = theme.backgroundColor == 0x00;
    GfxRenderer flipRenderer(480, 800);
    flipRenderer.clear(backgroundBlack);
    render({flipRenderer, theme, time, true, 0, 0}, Face::Flip);
    assert(flipRenderer.hasDraw('F', 286, 150, theme.primaryTextBlack));
    assert(flipRenderer.hasDraw('L', 286, 1, backgroundBlack));
    assert(flipRenderer.hasDraw('F', 16, 16, !theme.primaryTextBlack));

    std::tm noon = time;
    noon.tm_hour = 12;
    GfxRenderer skyRenderer(480, 800);
    skyRenderer.clear(backgroundBlack);
    render({skyRenderer, theme, noon, true, 0, 0}, Face::DayNight);
    const int sunY = dayNightIconY(skyRenderer, true);
    assert(skyRenderer.pixelAt(240, sunY - 68) == theme.primaryTextBlack);
    assert(skyRenderer.pixelAt(248, sunY - 68) == backgroundBlack);
    const auto [skyTimeTop, skyTimeBottom] = clockBounds(skyRenderer, "MON 2026/11/23", 170);
    assert(skyTimeTop < skyTimeBottom);
    assert(std::any_of(skyRenderer.draws.begin(), skyRenderer.draws.end(), [&](const auto& draw) {
      return draw.kind == 'F' && draw.width >= 80 && draw.height == 1 && draw.y >= skyTimeBottom;
    }));
    GfxRenderer nightRenderer(480, 800);
    nightRenderer.clear(backgroundBlack);
    render({nightRenderer, theme, time, true, 0, 0}, Face::DayNight);
    const auto [nightTimeTop, nightTimeBottom] = clockBounds(nightRenderer, "MON 2026/11/23", 170);
    assert(nightTimeTop < nightTimeBottom);
    assert(skyTimeTop == nightTimeTop);
  }

  for (int hour : {5, 6, 17, 18}) {
    std::tm boundary{};
    boundary.tm_year = 100;
    boundary.tm_mon = 0;
    boundary.tm_mday = 7;
    boundary.tm_hour = hour;
    GfxRenderer renderer(480, 800);
    Theme boundaryTheme = lightTheme();
    renderer.clear(false);
    render({renderer, boundaryTheme, boundary, true, 0, 0}, Face::DayNight);
    const bool daytime = hour >= 6 && hour < 18;
    assert(std::count_if(renderer.draws.begin(), renderer.draws.end(),
                         [](const auto& draw) { return draw.kind == 'T'; }) == (daytime ? 1 : 2));
    assert(renderer.pixelAt(240, dayNightIconY(renderer, true) - 68) ==
           (daytime ? boundaryTheme.primaryTextBlack : false));
  }

  std::tm september{};
  september.tm_year = 126;
  september.tm_mon = 8;
  september.tm_mday = 13;
  september.tm_hour = 20;
  GfxRenderer septemberRenderer(480, 800);
  Theme septemberTheme = lightTheme();
  septemberRenderer.clear(false);
  render({septemberRenderer, septemberTheme, september, true, 1, 7}, Face::DayNight);
  assert(septemberRenderer.hasText("Waxing Crescent"));

  september.tm_hour = 0;
  for (int8_t utcOffset : {7, 14}) {
    GfxRenderer earlySeptemberRenderer(480, 800);
    earlySeptemberRenderer.clear(false);
    render({earlySeptemberRenderer, septemberTheme, september, true, 1, utcOffset}, Face::DayNight);
    assert(earlySeptemberRenderer.hasText("Waxing Crescent"));
  }

  struct PhaseCase {
    int year;
    int month;
    int day;
    const char* name;
  };
  const PhaseCase phaseCases[] = {
      {100, 0, 6, "New Moon"},         {100, 0, 10, "Waxing Crescent"},
      {100, 0, 14, "First Quarter"},   {100, 0, 17, "Waxing Gibbous"},
      {100, 0, 21, "Full Moon"},       {100, 0, 25, "Waning Gibbous"},
      {100, 0, 28, "Last Quarter"},    {100, 1, 1, "Waning Crescent"},
  };
  int phasePixels[std::size(phaseCases)]{};
  for (size_t index = 0; index < std::size(phaseCases); ++index) {
    std::tm phaseTime{};
    phaseTime.tm_year = phaseCases[index].year;
    phaseTime.tm_mon = phaseCases[index].month;
    phaseTime.tm_mday = phaseCases[index].day;
    phaseTime.tm_hour = 23;
    GfxRenderer renderer(480, 800);
    Theme phaseTheme = lightTheme();
    renderer.clear(false);
    render({renderer, phaseTheme, phaseTime, true, 0, 0}, Face::DayNight);
    assert(renderer.hasText(phaseCases[index].name));
    assert(renderer.allDrawsInside(0, 58, 480, 736));
    const int phaseY = dayNightIconY(renderer, true);
    phasePixels[index] = renderer.countPixels(170, phaseY - 75, 310, phaseY + 76, phaseTheme.primaryTextBlack);
  }
  assert(phasePixels[0] < phasePixels[1]);
  assert(phasePixels[1] < phasePixels[2]);
  assert(phasePixels[2] < phasePixels[3]);
  assert(phasePixels[3] < phasePixels[4]);

  std::tm outlineTime{};
  outlineTime.tm_year = 100;
  outlineTime.tm_mon = 0;
  outlineTime.tm_mday = 6;
  outlineTime.tm_hour = 23;
  GfxRenderer outlineRenderer(480, 800);
  Theme outlineTheme = lightTheme();
  outlineRenderer.clear(false);
  render({outlineRenderer, outlineTheme, outlineTime, true, 0, 0}, Face::DayNight);
  assert(outlineRenderer.pixelAt(290, dayNightIconY(outlineRenderer, true)) == outlineTheme.primaryTextBlack);

  struct OffsetPhaseCase {
    int day;
    int hour;
    int minute;
    int8_t utcOffset;
    const char* name;
  };
  const OffsetPhaseCase offsetCases[] = {
      {8, 3, 30, 14, "New Moon"},
      {8, 3, 30, -12, "Waxing Crescent"},
  };
  for (const OffsetPhaseCase& phaseCase : offsetCases) {
    std::tm localTime{};
    localTime.tm_year = 100;
    localTime.tm_mon = 0;
    localTime.tm_mday = phaseCase.day;
    localTime.tm_hour = phaseCase.hour;
    localTime.tm_min = phaseCase.minute;
    GfxRenderer renderer(480, 800);
    Theme offsetTheme = lightTheme();
    renderer.clear(false);
    render({renderer, offsetTheme, localTime, true, 0, phaseCase.utcOffset}, Face::DayNight);
    assert(renderer.hasText(phaseCase.name));
  }

  for (Theme phaseTheme : {lightTheme(), darkTheme()}) {
    const bool backgroundBlack = phaseTheme.backgroundColor == 0x00;
    std::tm quarter{};
    quarter.tm_year = 100;
    quarter.tm_mon = 0;
    quarter.tm_mday = 14;
    quarter.tm_hour = 23;
    GfxRenderer waxingRenderer(480, 800);
    waxingRenderer.clear(backgroundBlack);
    render({waxingRenderer, phaseTheme, quarter, true, 0, 0}, Face::DayNight);
    const int phaseY = dayNightIconY(waxingRenderer, true);
    assert(waxingRenderer.pixelAt(265, phaseY) == phaseTheme.primaryTextBlack);
    assert(waxingRenderer.pixelAt(215, phaseY) == backgroundBlack);

    quarter.tm_mday = 17;
    GfxRenderer waxingGibbousRenderer(480, 800);
    waxingGibbousRenderer.clear(backgroundBlack);
    render({waxingGibbousRenderer, phaseTheme, quarter, true, 0, 0}, Face::DayNight);
    assert(waxingGibbousRenderer.pixelAt(240, phaseY) == phaseTheme.primaryTextBlack);
    assert(waxingGibbousRenderer.pixelAt(200, phaseY) == backgroundBlack);
    assert(waxingGibbousRenderer.pixelAt(215, phaseY) == phaseTheme.primaryTextBlack);
    assert(waxingGibbousRenderer.pixelAt(215, phaseY - 40) == backgroundBlack);

    quarter.tm_mday = 25;
    GfxRenderer waningGibbousRenderer(480, 800);
    waningGibbousRenderer.clear(backgroundBlack);
    render({waningGibbousRenderer, phaseTheme, quarter, true, 0, 0}, Face::DayNight);
    assert(waningGibbousRenderer.pixelAt(240, phaseY) == phaseTheme.primaryTextBlack);
    assert(waningGibbousRenderer.pixelAt(280, phaseY) == backgroundBlack);
    assert(waningGibbousRenderer.pixelAt(265, phaseY) == phaseTheme.primaryTextBlack);
    assert(waningGibbousRenderer.pixelAt(265, phaseY - 40) == backgroundBlack);

    quarter.tm_mday = 28;
    GfxRenderer waningRenderer(480, 800);
    waningRenderer.clear(backgroundBlack);
    render({waningRenderer, phaseTheme, quarter, true, 0, 0}, Face::DayNight);
    assert(waningRenderer.pixelAt(215, phaseY) == phaseTheme.primaryTextBlack);
    assert(waningRenderer.pixelAt(265, phaseY) == backgroundBlack);
  }

  for (Face face : {Face::Big, Face::Retro, Face::Flip, Face::DayNight}) {
    GfxRenderer renderer(480, 800);
    Theme theme = lightTheme();
    render({renderer, theme, time, false, 0, 0}, face);
    assert(renderer.hasText("AM"));
  }
  GfxRenderer analogRenderer(480, 800);
  Theme theme = lightTheme();
  render({analogRenderer, theme, time, false, 0, 0}, Face::Analog);
  assert(!analogRenderer.hasText("AM"));
}
'''

PREVIEW_NAMES = {
    f"{face}-{width}x{height}.pbm"
    for face in ("big", "analog", "retro", "flip", "day-night")
    for width, height in ((480, 800), (528, 792))
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preview-dir", type=Path)
    args = parser.parse_args()

    if args.preview_dir:
        args.preview_dir.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory() as directory:
        temp = Path(directory)
        (temp / "GfxRenderer.h").write_text(GFX_RENDERER)
        (temp / "Theme.h").write_text(THEME)
        source = temp / "clock_faces_test.cpp"
        source.write_text(HARNESS)
        executable = temp / "clock_faces_test"
        subprocess.run(
            [
                "g++",
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-I",
                str(temp),
                "-I",
                str(ROOT / "src"),
                str(source),
                str(ROOT / "src/apps/ClockFacePrimitives.cpp"),
                str(ROOT / "src/apps/ClockFaces.cpp"),
                "-o",
                str(executable),
            ],
            check=True,
        )
        command = [str(executable)]
        if args.preview_dir:
            command.append(str(args.preview_dir))
        subprocess.run(command, check=True)

    if args.preview_dir:
        actual = {path.name for path in args.preview_dir.glob("*.pbm")}
        if actual != PREVIEW_NAMES:
            raise AssertionError(f"unexpected preview files: {sorted(actual)}")

    print("clock face tests passed")


if __name__ == "__main__":
    main()
