#include "Recovery.h"

namespace papyrix::hal {

DisplayRecoveryAction DisplayRecoveryPolicy::recordFailure() {
  if (failures_ < 2) ++failures_;
  return failures_ == 1 ? DisplayRecoveryAction::ResetAndRetry : DisplayRecoveryAction::ShutdownAndWait;
}

}  // namespace papyrix::hal
