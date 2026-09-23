#include "core/CrashDebug.h"
#include "test_utils.h"

int main() {
  TestUtils::TestRunner runner("Crash breadcrumbs");
  namespace crash = papyrix::crashdebug;
  crash::clear();
  crash::mark(crash::CrashPhase::HomeMetadataLoad);
  crash::logBootInfo(ESP_RST_PANIC);
  runner.expectTrue(crash::shouldSkipHomeMetadata(), "panic during home metadata load skips it on this boot");
  runner.expectFalse(crash::shouldSkipHomeMetadata(), "home metadata skip is consumed");
  return runner.allPassed() ? 0 : 1;
}
