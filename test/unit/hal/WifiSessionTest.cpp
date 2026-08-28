#include "hal/WifiRadio.h"
#include "hal/Cpu.h"

#include "test_utils.h"

namespace {
int shutdownCount = 0;
}

namespace papyrix::hal {
void WifiRadio::shutdown() { shutdownCount++; }
}  // namespace papyrix::hal

int main() {
  TestUtils::TestRunner runner("Wi-Fi session");
  papyrix::hal::Cpu cpu;
  papyrix::hal::WifiRadio radio;

  cpu.throttle();
  {
    papyrix::hal::WifiSession session(radio, cpu);
    runner.expectFalse(cpu.isThrottled(), "session holds the CPU performance lock");
    cpu.throttle();
    runner.expectFalse(cpu.isThrottled(), "performance lock rejects throttling inside the session");
    runner.expectEq(0, shutdownCount, "session keeps Wi-Fi active inside its scope");
  }
  runner.expectEq(1, shutdownCount, "session shuts Wi-Fi down when its scope ends");
  cpu.throttle();
  runner.expectTrue(cpu.isThrottled(), "session releases the CPU performance lock");

  return runner.allPassed() ? 0 : 1;
}
