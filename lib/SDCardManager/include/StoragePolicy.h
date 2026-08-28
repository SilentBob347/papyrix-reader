#pragma once

#include <BoardProfile.h>

namespace papyrix::sd {

struct StorageMountPlan {
  board::StorageTransport transport;
  bool controlsPowerPin;
  bool inactiveLevel;
  bool activeLevel;
};

constexpr StorageMountPlan makeStorageMountPlan(const board::StorageConfig& config) {
  return {
      config.transport,
      config.powerPin != board::kPinUnused,
      !config.powerActiveHigh,
      config.powerActiveHigh,
  };
}

}  // namespace papyrix::sd
