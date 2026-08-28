#pragma once

#include <cstdint>

namespace papyrix::hal {

enum class DisplayRecoveryAction : uint8_t { ResetAndRetry, ShutdownAndWait };

class DisplayRecoveryPolicy {
 public:
  DisplayRecoveryAction recordFailure();
  void recordSuccess() { failures_ = 0; }
  uint8_t failureCount() const { return failures_; }

 private:
  uint8_t failures_ = 0;
};

}  // namespace papyrix::hal
