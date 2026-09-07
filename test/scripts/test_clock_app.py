#!/usr/bin/env python3
"""Exercise Clock menu and startup paths with host hardware substitutes."""

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def block(source, marker):
    start = source.index(marker)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


HARNESS = r'''
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <cstdint>
#include <iterator>
#include <string>
#define LOG_INF(...)
#define LOG_ERR(...)
unsigned long nowMs = 0;
unsigned long millis() { return nowMs; }
std::string settingsFile;
int writes = 0, syncs = 0;
bool ntpSynced = true;
constexpr int SNTP_SYNC_STATUS_COMPLETED = 1;
int esp_sntp_get_sync_status() { return ntpSynced ? SNTP_SYNC_STATUS_COMPLETED : 0; }
void delay(int ms) { nowMs += ms; }
void configTime(long, int, const char*, const char*, const char*) {
  ++syncs;
  setenv("TZ", "UTC0", 1);
  tzset();
}
struct FsFile {
  void write(const uint8_t* data, size_t length) {
    settingsFile.assign(reinterpret_cast<const char*>(data), length);
    ++writes;
  }
  void close() {}
};
enum class Button { Up, Down, Left, Right, Back, Center };
enum class SyncMode { None, NtpSync, FileTransfer, CalibreWireless };
struct Result { size_t value = 0; bool ok() const { return true; } };
struct Core {
  struct Clock {
    bool valid = true;
    bool updateSucceeds = true;
    bool updateFromSystem() { return updateSucceeds; }
    bool localTime(std::tm& out) {
      if (!valid) return false;
      const std::time_t epoch = 1780000000 + nowMs / 1000;
      out = *std::localtime(&epoch);
      return true;
    }
  } clock;
  struct Storage {
    Result readToBuffer(const char*, char* buf, size_t size) {
      std::snprintf(buf, size, "%s", settingsFile.c_str());
      return {settingsFile.size()};
    }
    void mkdir(const char*) {}
    Result openWrite(const char*, FsFile&) { return {}; }
  } storage;
  struct Cpu {
    bool throttled = false;
    void unthrottle() { throttled = false; }
    void throttle() { throttled = true; }
  } cpu;
  struct Input { void resetIdleTimer() {} } input;
  struct Wifi { bool isConnected() { return false; } } wifi;
  SyncMode pendingSync = SyncMode::None;
  int pendingAppId = -1;
};
namespace papyrix::hal {
struct Display { static constexpr int HALF_REFRESH = 0; };
}
namespace hal {
struct WifiSession { WifiSession(Core::Wifi&, Core::Cpu&) {} };
}
struct Renderer {
  void displayBuffer(int = 0) {}
  void clearScreen(int) {}
} renderer;
struct Theme { int uiFontId = 0, backgroundColor = 0; } THEME;
namespace ui {
void centeredMessage(Renderer&, Theme&, int, const char*) {}
struct AppMenuView { static constexpr int EXTRA_COUNT = 2; };
}
namespace clock_app {
@STATE@
@TIMEZONE@
@LOAD@
@SAVE@
@PARSE_NTP@
@SYNC_NTP@
void syncNtpAutoConnect(Core& core) { syncNtpWithConnection(core); }
@ENTER@
@UPDATE@
@MENU@
@EXIT@
}
struct App {
  bool (*update)(Core&);
  void (*onMenuButton)(Core&, Button);
} APPS[] = {{clock_app::update, clock_app::onMenuButton}};
constexpr int APP_CLOCK = 0;
class AppLauncherState {
 public:
  enum class Mode { App, Overlay };
  Mode mode_ = Mode::Overlay;
  int activeApp_ = 0;
  bool needsRender_ = false, goNetwork_ = false, launched = false;
  struct MenuView { int selected = ui::AppMenuView::EXTRA_COUNT; } menuView_;
  void launchApp(Core& core) { launched = true; clock_app::enter(core); }
  void hideOverlay() { mode_ = Mode::App; }
  void activateMenuItem(Core& core);
  void poll(Core& core) { @POLL@ }
  void press(Core& core, Button button) {
    struct { Button button; } e{button};
    @OVERLAY@
  }
};
@ACTIVATE@
int main(int argc, char** argv) {
  assert(argc == 2);
  Core core;
  using namespace clock_app;
  const std::string scenario = argv[1];
  if (scenario == "overlay") {
    AppLauncherState app;
    state.lastRenderedMin = -1;
    app.poll(core);
    assert(!app.needsRender_);
    assert(core.cpu.throttled);
    app.needsRender_ = true;
    app.poll(core);
    assert(!core.cpu.throttled);
    app.needsRender_ = false;
    app.poll(core);
    assert(core.cpu.throttled);
    app.press(core, Button::Down);
    assert(!core.cpu.throttled);
    nowMs = NTP_SYNC_INTERVALS[0];
    app.poll(core);
    assert(syncs == 0);
  } else if (scenario == "menu") {
    AppLauncherState app;
    applyTimezone(0);
    std::tm before{};
    core.clock.localTime(before);
    state.lastRenderedMin = before.tm_min;
    state.lastNtpSyncMs = millis();
    for (int i = 0; i < 3; ++i) app.press(core, Button::Right);
    assert(writes == 0 && syncs == 0);
    app.needsRender_ = false;
    nowMs += 60000;
    for (int i = 0; i < 3; ++i) app.poll(core);
    assert(!app.needsRender_);
    nowMs = NTP_SYNC_INTERVALS[0];
    app.poll(core);
    assert(syncs == 0 && !app.needsRender_);
    app.press(core, Button::Back);
    assert(writes == 1 && settingsFile.find("utcOffset=3\n") != std::string::npos);
    std::tm after{};
    nowMs = 0;
    core.clock.localTime(after);
    assert(after.tm_hour == (before.tm_hour + 3) % 24);
    assert(syncs == 1);
    app.mode_ = AppLauncherState::Mode::Overlay;
    app.press(core, Button::Back);
    assert(writes == 1 && syncs == 1);
    app.mode_ = AppLauncherState::Mode::Overlay;
    app.press(core, Button::Left);
    clock_app::exit(core);
    assert(writes == 2 && settingsFile.find("utcOffset=2\n") != std::string::npos);
  } else if (scenario == "startup") {
    AppLauncherState app;
    app.activateMenuItem(core);
    assert(app.launched && !app.goNetwork_ && syncs == 0);
    nowMs = NTP_SYNC_INTERVALS[0];
    clock_app::update(core);
    assert(syncs == 1);
    state.menuSelected = static_cast<int8_t>(MenuItem::SyncNow);
    onMenuButton(core, Button::Center);
    assert(syncs == 1);
    onMenuButton(core, Button::Back);
    assert(syncs == 2);
    state.ntpSyncSetting = NTP_SYNC_COUNT - 1;
    nowMs += 86400000;
    clock_app::update(core);
    assert(syncs == 2);
    core.clock.valid = false;
    AppLauncherState unsynced;
    unsynced.activateMenuItem(core);
    assert(!unsynced.launched && unsynced.goNetwork_);
    assert(core.pendingSync == SyncMode::NtpSync && core.pendingAppId == APP_CLOCK);
    enter(core);
    assert(syncs == 3);
  } else if (scenario == "timeout" || scenario == "sync-failure") {
    state.utcOffset = 3;
    applyTimezone(state.utcOffset);
    std::tm before{};
    core.clock.localTime(before);
    onMenuButton(core, Button::Right);
    ntpSynced = scenario != "timeout";
    core.clock.updateSucceeds = scenario != "sync-failure";
    onMenuButton(core, Button::Back);
    std::tm after{};
    core.clock.localTime(after);
    assert(after.tm_hour == (before.tm_hour + 1) % 24);
    assert(settingsFile.find("utcOffset=4\n") != std::string::npos);
  } else if (scenario == "confirm") {
    const int oldOffset = state.utcOffset;
    onMenuButton(core, Button::Center);
    assert(state.utcOffset == oldOffset + 1);
    onMenuButton(core, Button::Down);
    onMenuButton(core, Button::Center);
    assert(!state.use24h);
    onMenuButton(core, Button::Down);
    onMenuButton(core, Button::Center);
    assert(state.dateFormat == 1);
    onMenuButton(core, Button::Down);
    onMenuButton(core, Button::Center);
    assert(state.ntpSyncSetting == 1);
    assert(writes == 0 && syncs == 0);
    onMenuButton(core, Button::Back);
    assert(writes == 1 && syncs == 1);
  } else {
    assert(false);
  }
}
'''


def main():
    clock = (ROOT / "src/apps/ClockApp.cpp").read_text()
    launcher = (ROOT / "src/states/AppLauncherState.cpp").read_text()
    state = clock[clock.index("static constexpr const char* SETTINGS_PATH"):
                  clock.index("static void applyTimezone")]
    replacements = {
        "STATE": state,
        "TIMEZONE": block(clock, "static void applyTimezone("),
        "LOAD": block(clock, "static void loadSettings("),
        "SAVE": block(clock, "static void saveSettings("),
        "PARSE_NTP": block(clock, "static int parseNtpServers("),
        "SYNC_NTP": block(clock, "static void syncNtpWithConnection("),
        "ENTER": block(clock, "void enter("),
        "UPDATE": block(clock, "bool update("),
        "MENU": block(clock, "void onMenuButton("),
        "EXIT": block(clock, "void exit("),
        "POLL": block(launcher, "if ((mode_ == Mode::App"),
        "OVERLAY": launcher[launcher.index("case Mode::Overlay:"):].split(
            "if (e.type == EventType::ButtonRepeat) break;", 1
        )[1].split("\n        break;", 1)[0],
        "ACTIVATE": block(launcher, "void AppLauncherState::activateMenuItem("),
    }
    harness = HARNESS
    for marker, code in replacements.items():
        harness = harness.replace(f"@{marker}@", code)
    with tempfile.TemporaryDirectory(prefix="papyrix-clock-") as directory:
        path = Path(directory)
        source = path / "clock.cpp"
        source.write_text(harness)
        binary = path / "clock"
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++17", str(source), "-o", str(binary)], check=True)
        failed = False
        for scenario in ("overlay", "menu", "startup", "timeout", "sync-failure", "confirm"):
            result = subprocess.run([str(binary), scenario])
            failed |= result.returncode != 0
            print(f"{'PASS' if result.returncode == 0 else 'FAIL'}: Clock {scenario}", flush=True)
        if failed:
            raise SystemExit(1)


if __name__ == "__main__":
    main()
