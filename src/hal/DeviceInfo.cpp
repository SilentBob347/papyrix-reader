#include "DeviceInfo.h"

#include <HardwareIdentity.h>

namespace papyrix::hal {

namespace {

board::HardwareIdentity& identity() { return board::HardwareIdentity::instance(); }

}  // namespace

board::BoardId DeviceInfo::board() const { return identity().board(); }

const board::BoardProfile& DeviceInfo::profile() const { return identity().profile(); }

const char* DeviceInfo::renderCacheDir() const { return identity().cacheDir(); }

eink::DisplayController DeviceInfo::panel() const { return identity().panel(); }

bool DeviceInfo::isX3() const { return board() == board::BoardId::X3; }

bool DeviceInfo::isX4() const { return board() == board::BoardId::X4; }

}  // namespace papyrix::hal
