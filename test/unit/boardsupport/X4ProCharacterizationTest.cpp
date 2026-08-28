#include "diagnostics/X4ProCharacterization.h"
#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("X4 Pro characterization commands");
  using papyrix::diagnostics::parseX4ProCommand;
  using papyrix::diagnostics::X4ProCommand;

  runner.expectEq(static_cast<int>(X4ProCommand::Status), static_cast<int>(parseX4ProCommand("?")),
                  "accepts the exact status command");
  runner.expectEq(static_cast<int>(X4ProCommand::Sleep), static_cast<int>(parseX4ProCommand("s")),
                  "accepts the exact sleep command");
  runner.expectEq(static_cast<int>(X4ProCommand::PanelProbe), static_cast<int>(parseX4ProCommand("p")),
                  "accepts the exact read-only panel probe command");
  runner.expectEq(static_cast<int>(X4ProCommand::WhiteRefresh), static_cast<int>(parseX4ProCommand("white")),
                  "accepts the exact command-gated white refresh");
  runner.expectEq(static_cast<int>(X4ProCommand::None), static_cast<int>(parseX4ProCommand("\x1b[31m1")),
                  "rejects a terminal escape sequence that contains a command digit");
  runner.expectEq(static_cast<int>(X4ProCommand::None), static_cast<int>(parseX4ProCommand("1?")),
                  "rejects multiple commands on one line");

  return runner.allPassed() ? 0 : 1;
}
