#include "hal/Recovery.h"
#include "test_utils.h"

using namespace papyrix::hal;

int main() {
  TestUtils::TestRunner r("Display recovery");

  DisplayRecoveryPolicy policy;
  r.expectTrue(policy.recordFailure() == DisplayRecoveryAction::ResetAndRetry,
               "first display failure resets and retries");
  r.expectTrue(policy.recordFailure() == DisplayRecoveryAction::ShutdownAndWait,
               "second display failure enters safe wait");
  r.expectTrue(policy.failureCount() == 2, "failure count is bounded");
  policy.recordSuccess();
  r.expectTrue(policy.failureCount() == 0, "success clears failure count");

  return r.allPassed() ? 0 : 1;
}
