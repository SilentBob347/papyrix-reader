#pragma once

#include <BoardProfile.h>
#include <DisplayController.h>

namespace papyrix::hal {

class DeviceInfo {
 public:
  board::BoardId board() const;
  const board::BoardProfile& profile() const;
  const char* renderCacheDir() const;
  eink::DisplayController panel() const;
  bool isX3() const;
  bool isX4() const;
};

}  // namespace papyrix::hal
