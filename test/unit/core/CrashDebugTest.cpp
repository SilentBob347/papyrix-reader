#include <Preferences.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "core/CrashDebug.h"
#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("Crash breadcrumbs");
  namespace crash = papyrix::crashdebug;
  crash::markDisplayFailure(3, 2);
  crash::clear();
  FILE* capture = std::tmpfile();
  if (!capture) return 1;
  std::fflush(stdout);
  const int saved = dup(fileno(stdout));
  if (saved < 0 || dup2(fileno(capture), fileno(stdout)) < 0) return 1;
  crash::logBootInfo(ESP_RST_POWERON);
  std::fflush(stdout);
  const long softwareOffset = std::ftell(capture);
  crash::logBootInfo(ESP_RST_SW);
  std::fflush(stdout);
  const long panicOffset = std::ftell(capture);
  crash::logBootInfo(ESP_RST_PANIC);
  std::fflush(stdout);
  dup2(saved, fileno(stdout));
  close(saved);
  std::rewind(capture);
  char log[1024]{};
  std::fread(log, 1, sizeof(log) - 1, capture);
  std::fclose(capture);
  runner.expectTrue(std::strstr(log, "result=3") && std::strstr(log, "attempt=2"),
                    "power-on reports the persistent display failure without an RTC marker");
  runner.expectTrue(softwareOffset >= 0 && softwareOffset < static_cast<long>(sizeof(log) - 5) &&
                        std::strncmp(log + softwareOffset, "[INF]", 5) == 0,
                    "software restart is informational");
  runner.expectTrue(panicOffset >= 0 && panicOffset < static_cast<long>(sizeof(log) - 5) &&
                        std::strncmp(log + panicOffset, "[ERR]", 5) == 0,
                    "panic remains an error");
  Preferences preferences;
  preferences.begin("papyrix_diag");
  runner.expectFalse(preferences.isKey("display_result"), "reported display failure is consumed");
  preferences.end();
  crash::mark(crash::CrashPhase::HomeMetadataLoad);
  crash::clearDisplayFailure();
  crash::logBootInfo(ESP_RST_PANIC);
  runner.expectTrue(crash::shouldSkipHomeMetadata(), "display success preserves an unrelated crash marker");
  return runner.allPassed() ? 0 : 1;
}
