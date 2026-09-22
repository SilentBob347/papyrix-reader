#pragma once

#include <X3DisplayProbe.h>

namespace papyrix::eink {

struct X4ClassicProbeResult {
  bool valid;
  uint8_t id;
};

X4ClassicProbeResult probeX4ClassicDisplayController(const X3DisplayProbePins& pins);

}  // namespace papyrix::eink
