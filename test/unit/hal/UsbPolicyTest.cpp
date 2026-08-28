#include "hal/UsbPolicy.h"

#include "test_utils.h"

using papyrix::board::UsbConfig;
using papyrix::board::kPinUnused;
using papyrix::hal::usb_policy::evaluate;
using papyrix::hal::usb_policy::Inputs;
using papyrix::hal::usb_policy::StateTracker;

int main() {
  TestUtils::TestRunner runner("USB policy");

  const auto directConnected = evaluate(UsbConfig{20, false, false}, Inputs{false, false, true, false, false});
  runner.expectTrue(directConnected.available, "direct USB detection is available");
  runner.expectTrue(directConnected.connected, "direct USB detection reads a high pin as connected");

  const auto directDisconnected = evaluate(UsbConfig{20, false, false}, Inputs{false, true, false, false, false});
  runner.expectTrue(directDisconnected.available, "direct USB detection ignores gauge availability");
  runner.expectFalse(directDisconnected.connected, "direct USB detection reads a low pin as disconnected");

  const auto gaugeConnected =
      evaluate(UsbConfig{kPinUnused, true, false}, Inputs{true, true, false, false, false});
  runner.expectTrue(gaugeConnected.available, "gauge USB detection is available when charging state is known");
  runner.expectTrue(gaugeConnected.connected, "gauge charging state reports USB connected");

  const auto nativeConnected =
      evaluate(UsbConfig{kPinUnused, false, true}, Inputs{false, false, false, true, true});
  runner.expectTrue(nativeConnected.available, "native USB detection is available");
  runner.expectTrue(nativeConnected.connected, "native USB connection overrides an absent GPIO source");

  const auto nativeDisconnected =
      evaluate(UsbConfig{kPinUnused, false, true}, Inputs{false, false, false, true, false});
  runner.expectTrue(nativeDisconnected.available, "native USB remains observable while disconnected");
  runner.expectFalse(nativeDisconnected.connected, "native USB reports an unplugged host");

  const auto gaugeUnknown =
      evaluate(UsbConfig{kPinUnused, true, false}, Inputs{false, false, false, false, false});
  runner.expectFalse(gaugeUnknown.available, "unknown gauge charging state reports USB unavailable");
  runner.expectFalse(gaugeUnknown.connected, "unavailable USB detection does not report connected");

  const auto unsupported =
      evaluate(UsbConfig{kPinUnused, false, false}, Inputs{true, true, true, false, false});
  runner.expectFalse(unsupported.available, "an unconfigured USB source reports unavailable");
  runner.expectFalse(unsupported.connected, "an unconfigured USB source does not sample an unverified pin");
  StateTracker tracker;
  runner.expectFalse(tracker.update(false), "initial USB observation is not an edge");
  runner.expectFalse(tracker.update(false), "stable disconnected state is not an edge");
  runner.expectTrue(tracker.update(true), "USB connection is an edge");
  runner.expectFalse(tracker.update(true), "stable connected state is not an edge");
  runner.expectTrue(tracker.update(false), "USB disconnection is an edge");

  return runner.allPassed() ? 0 : 1;
}
