#pragma once

#include <BatteryMonitor.h>
#include <TargetConfig.h>

#include <optional>

namespace papyrix::hal {

class Battery {
 public:
  using Status = BatteryMonitor::Status;

  void init();
  bool isInitialized() const { return initialized_; }
  bool isAvailable() const { return monitor_.has_value(); }
  Status readStatus() const;

 private:
  std::optional<BatteryMonitor> monitor_;
#if PAPYRIX_CAP_BATTERY_ADC
  mutable Status adcCachedStatus_{};
  mutable uint32_t lastAdcPollMs_ = 0;
  mutable bool adcEstimateInitialized_ = false;
  bool adcBackend_ = false;
#endif
  bool initialized_ = false;
};

}  // namespace papyrix::hal
