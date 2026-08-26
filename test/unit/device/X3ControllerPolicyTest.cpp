#include "drivers/X3ControllerPolicy.h"

#include "test_utils.h"

#include <cstring>

using papyrix::drivers::ControllerSelectionSource;
using papyrix::drivers::controllerSelectionSourceName;
using papyrix::drivers::kControllerCacheVersion;
using papyrix::drivers::kStoredAuto;
using papyrix::drivers::kStoredUc8253;
using papyrix::drivers::kStoredUc8279X3;
using papyrix::drivers::resolveProbedX3Controller;
using papyrix::drivers::resolveStoredX3Controller;
using papyrix::eink::DisplayController;
using papyrix::eink::X3DisplayVerdict;

int main() {
  TestUtils::TestRunner runner("X3 controller policy");

  runner.expectEq(0, static_cast<int>(kStoredAuto), "automatic NVS value");
  runner.expectEq(1, static_cast<int>(kStoredUc8253), "UC8253 NVS value");
  runner.expectEq(2, static_cast<int>(kStoredUc8279X3), "UC8279 NVS value");
  runner.expectEq(1, static_cast<int>(kControllerCacheVersion), "cache version");

  auto decision = resolveStoredX3Controller(kStoredAuto, kStoredAuto, kControllerCacheVersion);
  runner.expectFalse(decision.resolved, "empty controller cache requires probe");
  runner.expectFalse(decision.invalidOverride, "automatic override is valid");
  runner.expectFalse(decision.invalidCache, "empty cache is valid");

  decision = resolveStoredX3Controller(kStoredUc8279X3, kStoredUc8253, kControllerCacheVersion);
  runner.expectTrue(decision.resolved, "override resolves");
  runner.expectEq(static_cast<int>(DisplayController::UC8279_X3), static_cast<int>(decision.controller),
                  "override wins over cache");
  runner.expectEq(static_cast<int>(ControllerSelectionSource::Override), static_cast<int>(decision.source),
                  "override source");

  decision = resolveStoredX3Controller(kStoredUc8253, kStoredUc8279X3, 0);
  runner.expectEq(static_cast<int>(DisplayController::UC8253), static_cast<int>(decision.controller),
                  "override ignores stale cache");

  decision = resolveStoredX3Controller(kStoredAuto, kStoredUc8253, kControllerCacheVersion);
  runner.expectTrue(decision.resolved, "UC8253 cache resolves");
  runner.expectEq(static_cast<int>(DisplayController::UC8253), static_cast<int>(decision.controller),
                  "UC8253 cache value");
  runner.expectEq(static_cast<int>(ControllerSelectionSource::Cache), static_cast<int>(decision.source),
                  "UC8253 cache source");

  decision = resolveStoredX3Controller(kStoredAuto, kStoredUc8279X3, kControllerCacheVersion);
  runner.expectTrue(decision.resolved, "UC8279 cache resolves");
  runner.expectEq(static_cast<int>(DisplayController::UC8279_X3), static_cast<int>(decision.controller),
                  "UC8279 cache value");

  decision = resolveStoredX3Controller(kStoredAuto, kStoredUc8279X3, 0);
  runner.expectFalse(decision.resolved, "stale cache requires probe");
  runner.expectTrue(decision.invalidCache, "stale cache is reported");

  decision = resolveStoredX3Controller(9, kStoredUc8253, kControllerCacheVersion);
  runner.expectTrue(decision.invalidOverride, "invalid override is reported");
  runner.expectTrue(decision.resolved, "valid cache follows invalid override");
  runner.expectEq(static_cast<int>(DisplayController::UC8253), static_cast<int>(decision.controller),
                  "cache follows invalid override");

  decision = resolveStoredX3Controller(kStoredAuto, 9, kControllerCacheVersion);
  runner.expectFalse(decision.resolved, "invalid cache requires probe");
  runner.expectTrue(decision.invalidCache, "invalid cache is reported");

  decision = resolveProbedX3Controller(X3DisplayVerdict::UC8279Confirmed);
  runner.expectTrue(decision.resolved, "confirmed UC8279 resolves");
  runner.expectEq(static_cast<int>(DisplayController::UC8279_X3), static_cast<int>(decision.controller),
                  "confirmed UC8279 selection");
  runner.expectTrue(decision.writeCache, "confirmed UC8279 is cached");
  runner.expectEq(static_cast<int>(kStoredUc8279X3), static_cast<int>(decision.storedValue),
                  "confirmed UC8279 cache value");

  decision = resolveProbedX3Controller(X3DisplayVerdict::UC8253StableDefault);
  runner.expectEq(static_cast<int>(DisplayController::UC8253), static_cast<int>(decision.controller),
                  "stable default selection");
  runner.expectTrue(decision.writeCache, "stable default is cached");
  runner.expectEq(static_cast<int>(kStoredUc8253), static_cast<int>(decision.storedValue),
                  "stable default cache value");

  decision = resolveProbedX3Controller(X3DisplayVerdict::Inconclusive);
  runner.expectTrue(decision.resolved, "inconclusive probe resolves current boot");
  runner.expectEq(static_cast<int>(DisplayController::UC8253), static_cast<int>(decision.controller),
                  "inconclusive falls back to UC8253");
  runner.expectEq(static_cast<int>(ControllerSelectionSource::Fallback), static_cast<int>(decision.source),
                  "inconclusive fallback source");
  runner.expectFalse(decision.writeCache, "inconclusive is not cached");

  runner.expectTrue(strcmp("x4-default", controllerSelectionSourceName(ControllerSelectionSource::X4Default)) == 0,
                    "X4 source name");
  runner.expectTrue(strcmp("override", controllerSelectionSourceName(ControllerSelectionSource::Override)) == 0,
                    "override source name");
  runner.expectTrue(strcmp("cache", controllerSelectionSourceName(ControllerSelectionSource::Cache)) == 0,
                    "cache source name");
  runner.expectTrue(strcmp("probe", controllerSelectionSourceName(ControllerSelectionSource::Probe)) == 0,
                    "probe source name");
  runner.expectTrue(strcmp("fallback", controllerSelectionSourceName(ControllerSelectionSource::Fallback)) == 0,
                    "fallback source name");

  runner.printSummary();
  return runner.allPassed() ? 0 : 1;
}
