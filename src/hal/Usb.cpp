#include "Usb.h"

#include <Arduino.h>
#include <HardwareIdentity.h>
#include <driver/usb_serial_jtag.h>

#include "Battery.h"
#include "UsbPolicy.h"

namespace papyrix::hal {

void Usb::init(const Battery& battery) {
  battery_ = &battery;
  const auto& config = board::HardwareIdentity::instance().profile().usb;
  if (config.detectPin != board::kPinUnused) pinMode(config.detectPin, INPUT);
}

Usb::Status Usb::readStatus() const {
  const auto& config = board::HardwareIdentity::instance().profile().usb;
  Battery::Status batteryStatus;
  if (config.viaBatteryStatus && battery_) batteryStatus = battery_->readStatus();
  const bool pinHigh = config.detectPin != board::kPinUnused && digitalRead(config.detectPin) == HIGH;
  const bool nativeAvailable = config.nativeSerialJtag;
  const bool nativeConnected = nativeAvailable && usb_serial_jtag_is_connected();
  const auto status = usb_policy::evaluate(
      config, {batteryStatus.chargingKnown, batteryStatus.charging, pinHigh, nativeAvailable, nativeConnected});
  return {status.available, status.connected};
}

}  // namespace papyrix::hal
