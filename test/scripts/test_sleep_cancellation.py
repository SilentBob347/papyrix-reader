#!/usr/bin/env python3
"""Run failed auto-sleep through the firmware state machine."""

import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]

HARNESS = r'''
#include <core/StateMachine.h>
#include <cassert>
#include <cstdio>
#include <Logging.h>
namespace papyrix {
struct Core {
  struct Input {
    unsigned idle = 60000;
    unsigned idleTimeMs() const { return idle; }
    void resetIdleTimer() { idle = 0; }
  } input;
  struct Display { bool deepSleep() { return false; } } display;
  struct Buffers { char text[128]{}; } buf;
};
Core core;
class SleepState : public State {
 public:
  unsigned entries = 0;
  void enter(Core& core) override {
    ++entries;
    @FAILURE@
  }
  StateTransition update(Core& core) override;
  StateId id() const override { return StateId::Sleep; }
};
@UPDATE@
class ErrorState : public State {
 public:
  unsigned entries = 0;
  void enter(Core&) override { ++entries; }
  StateTransition update(Core&) override { return StateTransition::stay(StateId::Error); }
  StateId id() const override { return StateId::Error; }
};
}
papyrix::StateMachine stateMachine;
void poll() {
  const unsigned autoSleepTimeout = 1000;
  @AUTO_SLEEP@
  stateMachine.update(papyrix::core);
}
int main() {
  papyrix::SleepState sleep;
  papyrix::ErrorState error;
  stateMachine.registerState(&sleep);
  stateMachine.registerState(&error);
  for (int i = 0; i < 4; ++i) poll();
  assert(sleep.entries == 1 && error.entries == 1);
  papyrix::core.input.idle = 1000;
  poll();
  poll();
  assert(sleep.entries == 2 && error.entries == 2);
}
'''


def main():
    sleep = (ROOT / "src/states/SleepState.cpp").read_text()
    start = sleep.index("  if (!core.display.deepSleep())")
    failure = sleep[start:sleep.index("\n  core.frontLight.shutdown()", start)]
    start = sleep.index("StateTransition SleepState::update(")
    update = sleep[start:sleep.index("\n\n", start)]
    loop = (ROOT / "src/main.cpp").read_text()
    start = loop.index("  if (autoSleepTimeout > 0")
    auto_sleep = loop[start:loop.index("\n\n", start)]
    harness = (HARNESS.replace("@FAILURE@", failure)
               .replace("@UPDATE@", update).replace("@AUTO_SLEEP@", auto_sleep))
    with tempfile.TemporaryDirectory(prefix="papyrix-sleep-cancel-") as directory:
        path = Path(directory)
        (path / "sleep.cpp").write_text(harness)
        (path / "Arduino.h").write_text("#pragma once\n")
        (path / "Logging.h").write_text(
            "#pragma once\n#define LOG_INF(...)\n#define LOG_ERR(...)\n#define LOG_DBG(...)\n")
        subprocess.run([
            os.environ.get("CXX", "c++"), "-std=c++17", f"-I{path}", f"-I{ROOT / 'src'}",
            str(path / "sleep.cpp"), str(ROOT / "src/core/StateMachine.cpp"),
            "-o", str(path / "sleep"),
        ], check=True)
        subprocess.run([str(path / "sleep")], check=True)
    print("PASS: cancelled auto-sleep reaches Error and waits before retry")


if __name__ == "__main__":
    main()
