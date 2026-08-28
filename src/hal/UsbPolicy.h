#pragma once

#include <BoardProfile.h>

namespace papyrix::hal::usb_policy {

struct Status {
  bool available;
  bool connected;
};

struct Inputs {
  bool gaugeAvailable;
  bool gaugeCharging;
  bool pinHigh;
  bool nativeAvailable;
  bool nativeConnected;
};

class StateTracker {
 public:
  bool update(bool connected) {
    if (!initialized_) {
      initialized_ = true;
      connected_ = connected;
      return false;
    }
    if (connected_ == connected) return false;
    connected_ = connected;
    return true;
  }

 private:
  bool initialized_ = false;
  bool connected_ = false;
};

constexpr Status evaluate(const board::UsbConfig& config, const Inputs& inputs) {
  bool available = false;
  bool connected = false;
  if (config.viaBatteryStatus && inputs.gaugeAvailable) {
    available = true;
    connected = inputs.gaugeCharging;
  }
  if (config.detectPin != board::kPinUnused) {
    available = true;
    connected = connected || inputs.pinHigh;
  }
  if (config.nativeSerialJtag && inputs.nativeAvailable) {
    available = true;
    connected = connected || inputs.nativeConnected;
  }
  return {available, connected};
}

}  // namespace papyrix::hal::usb_policy
