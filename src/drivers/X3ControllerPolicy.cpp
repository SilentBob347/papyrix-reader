#include "X3ControllerPolicy.h"

namespace papyrix::drivers {
namespace {

using papyrix::eink::DisplayController;
using papyrix::eink::X3DisplayVerdict;

ControllerDecision storedDecision(DisplayController controller, ControllerSelectionSource source) {
  ControllerDecision decision;
  decision.resolved = true;
  decision.controller = controller;
  decision.source = source;
  return decision;
}

}  // namespace

ControllerDecision resolveStoredX3Controller(uint8_t overrideValue, uint8_t cacheValue, uint8_t cacheVersion) {
  ControllerDecision decision;

  if (overrideValue == kStoredUc8253) {
    return storedDecision(DisplayController::UC8253, ControllerSelectionSource::Override);
  }
  if (overrideValue == kStoredUc8279X3) {
    return storedDecision(DisplayController::UC8279_X3, ControllerSelectionSource::Override);
  }
  decision.invalidOverride = overrideValue != kStoredAuto;

  if (cacheValue == kStoredAuto) return decision;
  if (cacheVersion != kControllerCacheVersion) {
    decision.invalidCache = true;
    return decision;
  }
  if (cacheValue == kStoredUc8253) {
    ControllerDecision cached = storedDecision(DisplayController::UC8253, ControllerSelectionSource::Cache);
    cached.invalidOverride = decision.invalidOverride;
    return cached;
  }
  if (cacheValue == kStoredUc8279X3) {
    ControllerDecision cached = storedDecision(DisplayController::UC8279_X3, ControllerSelectionSource::Cache);
    cached.invalidOverride = decision.invalidOverride;
    return cached;
  }

  decision.invalidCache = true;
  return decision;
}

ControllerDecision resolveProbedX3Controller(X3DisplayVerdict verdict) {
  ControllerDecision decision;
  decision.resolved = true;

  switch (verdict) {
    case X3DisplayVerdict::UC8279Confirmed:
      decision.controller = DisplayController::UC8279_X3;
      decision.source = ControllerSelectionSource::Probe;
      decision.writeCache = true;
      decision.storedValue = kStoredUc8279X3;
      break;
    case X3DisplayVerdict::UC8253StableDefault:
      decision.controller = DisplayController::UC8253;
      decision.source = ControllerSelectionSource::Probe;
      decision.writeCache = true;
      decision.storedValue = kStoredUc8253;
      break;
    case X3DisplayVerdict::Inconclusive:
      decision.controller = DisplayController::UC8253;
      decision.source = ControllerSelectionSource::Fallback;
      break;
  }

  return decision;
}

const char* controllerSelectionSourceName(ControllerSelectionSource source) {
  switch (source) {
    case ControllerSelectionSource::X4Default:
      return "x4-default";
    case ControllerSelectionSource::Override:
      return "override";
    case ControllerSelectionSource::Cache:
      return "cache";
    case ControllerSelectionSource::Probe:
      return "probe";
    case ControllerSelectionSource::Fallback:
      return "fallback";
  }
  return "unknown";
}

}  // namespace papyrix::drivers
