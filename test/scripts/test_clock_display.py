#!/usr/bin/env python3
"""Exercise Clock rendering and panel power recovery through the real display facade."""

import os
from pathlib import Path
import subprocess
import tempfile

from test_clock_app import block

ROOT = Path(__file__).resolve().parents[2]
SOURCES = [
    "lib/EInkDisplay/src/Display.cpp",
    "lib/EInkDisplay/src/Uc8279X3Driver.cpp",
    "lib/EInkDisplay/src/Uc8179X4ProDriver.cpp",
    "lib/EInkDisplay/src/Uc8279X4ProDriver.cpp",
    "lib/EInkDisplay/src/Uc8279SpiBus.cpp",
    "lib/BoardSupport/src/HardwareIdentity.cpp",
    "lib/BoardSupport/src/BoardProfiles.cpp",
    "test/mocks/platform_stubs.cpp",
]
INCLUDES = [
    "lib/EInkDisplay/include",
    "test/mocks",
    "lib/BoardSupport/include",
    "src",
]
TARGETS = {
    "PAPYRIX_TARGET_XTEINK_C3": ("ssd1677", "uc8253", "uc8279-x3"),
    "PAPYRIX_TARGET_X4PRO": ("uc8179-pro", "uc8279-pro"),
}

HARNESS = r'''
#include <Display.h>
#include <HardwareIdentity.h>
#include <SPI.h>
#include <apps/MiniApp.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using papyrix::hal::Display;
using papyrix::eink::DisplayController;
DisplayController panel;
int dcPin, csPin, busyPin;
int dc = HIGH, cs = HIGH;
unsigned busyReads = 2;
bool waiting = false;

void check(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}

struct Command {
  uint8_t code;
  std::vector<uint8_t> data;
};
std::vector<Command> commands;

void recordPin(int pin, int value) {
  if (pin == dcPin) dc = value;
  if (pin == csPin) cs = value;
}

void recordSpi(const uint8_t* data, size_t length) {
  check(cs == LOW, "SPI write requires chip selection");
  if (dc == LOW) {
    check(length == 1, "command must contain one byte");
    commands.push_back({data[0], {}});
    // ponytail: PON checks cover power-off recovery. Extend the model for powered-on UC8253 resync.
    if (panel == DisplayController::SSD1677 ? data[0] == 0x20
                                          : data[0] == 0x04 || data[0] == 0x12 || data[0] == 0x02) {
      check(!waiting, "new operation starts before BUSY completes");
      busyReads = 0;
      waiting = true;
    }
  } else {
    check(!commands.empty(), "data requires a preceding command");
    commands.back().data.insert(commands.back().data.end(), data, data + length);
  }
}

int readBusy(int pin) {
  check(pin == busyPin, "display reads its selected BUSY pin");
  const bool active = busyReads++ < 2;
  if (!active) waiting = false;
  return panel == DisplayController::SSD1677 ? (active ? HIGH : LOW) : (active ? LOW : HIGH);
}

size_t commandCount(uint8_t code) {
  return std::count_if(commands.begin(), commands.end(), [=](const Command& cmd) { return cmd.code == code; });
}

size_t lastCommand(uint8_t code) {
  for (size_t i = commands.size(); i > 0; --i) {
    if (commands[i - 1].code == code) return i - 1;
  }
  throw std::runtime_error("missing command " + std::to_string(code));
}

const std::vector<uint8_t>& lastData(uint8_t code) { return commands[lastCommand(code)].data; }

namespace papyrix::board {
void suppressTouchUntilIdle() {}
}

class GfxRenderer {
 public:
  Display& display_;
  bool darkBackground_ = false;
  void logRenderTime() const {}
  void clearScreen(uint8_t color) { display_.clearScreen(color); }
  int getScreenHeight() const { return display_.getDisplayHeight(); }
  int getScreenWidth() const { return display_.getDisplayWidth(); }
  void clearArea(int, int, int, int, uint8_t) {}
  void displayBuffer(Display::RefreshMode mode = Display::FAST_REFRESH, bool turnOffScreen = false) const;
};

@GFX_DISPLAY@

namespace papyrix {
struct Core {
  Display& display;
  uint8_t seed = 0;
  bool ownsDisplay = false;
  std::vector<uint8_t> frame;
};

void paint(Core& core) {
  core.frame.resize(core.display.getBufferSize());
  const auto width = core.display.getDisplayWidthBytes();
  for (uint16_t y = 0; y < core.display.getDisplayHeight(); ++y) {
    for (uint16_t x = 0; x < width; ++x) {
      core.frame[static_cast<size_t>(y) * width + x] = static_cast<uint8_t>(core.seed + y + 3 * x);
    }
  }
  std::copy(core.frame.begin(), core.frame.end(), core.display.getFrameBuffer());
}

bool renderApp(Core& core) {
  paint(core);
  if (core.ownsDisplay) core.display.displayBuffer(Display::FAST_REFRESH, false);
  return core.ownsDisplay;
}

const int8_t APP_CLOCK = 1;
const int8_t APP_IMAGEVIEWER = 0;
const uint8_t APP_COUNT = 2;
const MiniApp APPS[] = {
    {"Image", nullptr, nullptr, nullptr, renderApp, nullptr, paint, nullptr},
    {"Clock", nullptr, nullptr, nullptr, renderApp, nullptr, paint, nullptr},
};
struct { uint8_t backgroundColor = 0xFF; } theme;
#define THEME theme
constexpr int BACK = 0, CONFIRM = 1;
const char* tr(int) { return ""; }

namespace ui {
struct AppMenuView { bool needsRender = false; };
struct ButtonBar { ButtonBar(const char*, const char*, const char*, const char*) {} };
void buttonBar(GfxRenderer&, const decltype(theme)&, const ButtonBar&) {}
void render(GfxRenderer&, const decltype(theme)&, AppMenuView&, const MiniApp*) {}
}

class AppLauncherState {
 public:
  enum class Mode { Menu, App, Overlay };
  GfxRenderer& renderer_;
  Mode mode_ = Mode::App;
  int8_t activeApp_ = APP_CLOCK;
  bool needsRender_ = true;
  ui::AppMenuView menuView_;
  void render(Core& core);
};

@LAUNCHER_RENDER@
}

void checkFrame(const papyrix::Core& core, bool turnOff, bool cold) {
  check(!waiting, "refresh must wait for BUSY completion");
  std::vector<uint8_t> expected;
  if (panel == DisplayController::UC8279_X4PRO) expected.assign(12000, 0xFF);
  const bool reversed = panel != DisplayController::SSD1677 && panel != DisplayController::UC8279_X4PRO;
  const size_t width = core.display.getDisplayWidthBytes();
  const size_t height = core.display.getDisplayHeight();
  for (size_t y = 0; y < height; ++y) {
    const size_t row = reversed ? height - 1 - y : y;
    expected.insert(expected.end(), core.frame.begin() + row * width, core.frame.begin() + (row + 1) * width);
  }
  if (panel == DisplayController::UC8179_X4PRO) expected.insert(expected.end(), 12000, 0xFF);

  if (panel == DisplayController::SSD1677) {
    check(commandCount(0x10) == 0, "Clock must not put SSD1677 into deep sleep");
    check(commandCount(0x20) == 1, "launcher submits one SSD1677 refresh");
    check(lastData(0x24) == expected, "SSD1677 current plane contains the submitted frame");
    check(lastData(0x26) == expected, "SSD1677 baseline contains the submitted frame");
    if (turnOff) {
      check(lastData(0x21) == std::vector<uint8_t>{0x40}, "powered-off SSD1677 uses HALF baseline recovery");
      check(lastData(0x22) == std::vector<uint8_t>{0xD7}, "Clock HALF update powers SSD1677 on and off");
    } else {
      check(lastData(0x22).size() == 1 && (lastData(0x22)[0] & 0x03) == 0,
            "other apps must not power SSD1677 off");
    }
    return;
  }

  check(commandCount(0x07) == 0, "Clock must not put UC controllers into deep sleep");
  if (!cold) check(commandCount(0x12) == 1, "launcher must not submit a duplicate UC refresh");
  check(lastData(0x13) == expected, "UC current plane contains the submitted frame");
  check(lastData(0x10) == expected, "UC old plane contains the submitted frame");
  check(commandCount(0x02) == (turnOff ? 1 : 0), "only Clock requests UC panel power-off");
  if (turnOff) {
    check(lastCommand(0x04) < lastCommand(0x12), "UC panel powers on before refresh");
    check(lastCommand(0x12) < lastCommand(0x02), "UC panel powers off after refresh");
    if (panel == DisplayController::UC8253) {
      if (!cold) {
        check(lastCommand(0x02) < lastCommand(0x10), "UC8253 restores RED RAM after power-off");
        check(lastData(0x50) == std::vector<uint8_t>({0x29, 0x07}), "UC8253 retains FAST after power-off");
      }
    } else {
      check(lastCommand(0x12) < lastCommand(0x10) && lastCommand(0x10) < lastCommand(0x02),
            "UC old plane is synchronized between refresh and power-off");
    }
  }
}

int main(int argc, char** argv) {
  try {
    check(argc == 3, "expected controller and app/overlay scenario");
    using namespace papyrix;
    using namespace papyrix::board;
    const std::string name = argv[1];
    BoardSelection selection;
#if PAPYRIX_TARGET_XTEINK_C3
    check(name == "ssd1677" || name == "uc8253" || name == "uc8279-x3", "unsupported C3 controller");
    selection.board = name == "ssd1677" ? BoardId::X4 : BoardId::X3;
    panel = name == "ssd1677" ? DisplayController::SSD1677
          : name == "uc8253" ? DisplayController::UC8253 : DisplayController::UC8279_X3;
#else
    check(name == "uc8179-pro" || name == "uc8279-pro", "unsupported Pro controller");
    selection.board = BoardId::X4Pro;
    panel = name == "uc8179-pro" ? DisplayController::UC8179_X4PRO : DisplayController::UC8279_X4PRO;
#endif
    auto& identity = HardwareIdentity::instance();
    selection.source = BoardSelectionSource::Override;
    check(identity.applyBoardSelection(selection, {}), "board profile must be available");
    identity.setPanel(panel, PanelSelectionSource::Override, 0x68);
    dcPin = identity.profile().display.dc;
    csPin = identity.profile().display.cs;
    busyPin = identity.profile().display.busy;
    testSetManualMillis(100);
    testSetDigitalWriteHook(recordPin);
    testSetSpiWriteHook(recordSpi);
    testSetDigitalReadHook(readBusy);
    Display display;
    check(display.begin() == Display::InitResult::Ok, "real facade initializes the selected panel");
    std::unique_ptr<uint8_t, decltype(&std::free)> buffer(nullptr, std::free);
#if PAPYRIX_TARGET_X4PRO
    buffer.reset(display.getFrameBuffer());
#endif
    GfxRenderer renderer{display};
    Core core{display};
    AppLauncherState launcher{renderer};
    const std::string scenario = argv[2];
    check(scenario == "app" || scenario == "overlay", "unsupported render scenario");
    launcher.mode_ = scenario == "app" ? AppLauncherState::Mode::App : AppLauncherState::Mode::Overlay;
    for (int frame = 0; frame < 6; ++frame) {
      renderer.darkBackground_ = frame >= 3;
      theme.backgroundColor = renderer.darkBackground_ ? 0x00 : 0xFF;
      core.seed = static_cast<uint8_t>(17 + frame * 37);
      commands.clear();
      launcher.needsRender_ = true;
      launcher.render(core);
      checkFrame(core, true, frame == 0);
      commands.clear();
      launcher.render(core);
      check(commands.empty(), "clean Clock frame must not refresh");
    }
    launcher.activeApp_ = APP_IMAGEVIEWER;
    launcher.mode_ = AppLauncherState::Mode::App;
    for (int frame = 0; frame < 3; ++frame) {
      if (frame == 2) launcher.mode_ = AppLauncherState::Mode::Overlay;
      ++core.seed;
      commands.clear();
      launcher.needsRender_ = true;
      launcher.render(core);
      checkFrame(core, false, false);
      if (frame == 1 && panel == DisplayController::SSD1677) {
        check(lastData(0x21) == std::vector<uint8_t>{0x00}, "powered-on SSD1677 resumes differential refresh");
      }
    }
    launcher.mode_ = AppLauncherState::Mode::App;
    core.ownsDisplay = true;
    ++core.seed;
    commands.clear();
    launcher.needsRender_ = true;
    launcher.render(core);
    checkFrame(core, false, false);
    testSetDigitalReadHook(nullptr);
    testSetDigitalWriteHook(nullptr);
    testSetSpiWriteHook(nullptr);
    testUseRealtimeMillis();
    return 0;
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}
'''


def main():
    launcher = (ROOT / "src/states/AppLauncherState.cpp").read_text()
    gfx = (ROOT / "lib/GfxRenderer/src/GfxRenderer.cpp").read_text()
    harness = HARNESS.replace("@LAUNCHER_RENDER@", block(launcher, "void AppLauncherState::render("))
    harness = harness.replace("@GFX_DISPLAY@", block(gfx, "void GfxRenderer::displayBuffer("))
    with tempfile.TemporaryDirectory(prefix="papyrix-clock-display-") as directory:
        path = Path(directory)
        source = path / "clock_display.cpp"
        source.write_text(harness)
        failed = False
        for target, panels in TARGETS.items():
            binary = path / target
            subprocess.run([
                os.environ.get("CXX", "c++"), "-std=c++17", "-DTEST_BUILD",
                "-DEINK_DISPLAY_SINGLE_BUFFER_MODE=1", f"-D{target}=1",
                *(f"-I{ROOT / include}" for include in INCLUDES),
                str(source), *(str(ROOT / item) for item in SOURCES), "-o", str(binary),
            ], check=True)
            for panel in panels:
                for scenario in ("app", "overlay"):
                    result = subprocess.run([str(binary), panel, scenario], capture_output=True, text=True)
                    if result.returncode:
                        print(result.stdout, end="")
                        print(result.stderr, end="")
                        failed = True
                    print(f"{'FAIL' if result.returncode else 'PASS'}: Clock display {panel} {scenario}", flush=True)
        if failed:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
