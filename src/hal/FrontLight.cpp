#include "FrontLight.h"

#include <TargetConfig.h>

#if PAPYRIX_CAP_FRONTLIGHT
#include <HardwareIdentity.h>
#include <Preferences.h>
#endif

namespace papyrix::hal {
#if PAPYRIX_CAP_FRONTLIGHT
namespace {
constexpr const char* kNamespace = "frontlight";
}
#endif

void FrontLight::init() {
  if (initialized_) return;
  initialized_ = true;
#if PAPYRIX_CAP_FRONTLIGHT
  if (!backend_.begin()) return;

  const auto boardId = board::HardwareIdentity::instance().board();
  Preferences preferences;
  if (preferences.begin(kNamespace, true)) {
    brightness_ = preferences.getUChar(board::frontLightBrightnessKey(boardId), 0);
    warmth_ = preferences.getUChar(board::frontLightWarmthKey(boardId), 50);
    preferences.end();
  }
  if (brightness_ > 100) brightness_ = 100;
  if (warmth_ > 100) warmth_ = 100;
  backend_.write(brightness_, warmth_);
#endif
}

void FrontLight::shutdown() {
#if PAPYRIX_CAP_FRONTLIGHT
  backend_.end();
#endif
  initialized_ = false;
}

bool FrontLight::setBrightness(uint8_t percent) {
#if PAPYRIX_CAP_FRONTLIGHT
  if (!backend_.available()) return false;
  brightness_ = percent > 100 ? 100 : percent;
  backend_.write(brightness_, warmth_);
  return persist(board::frontLightBrightnessKey(board::HardwareIdentity::instance().board()), brightness_);
#else
  (void)percent;
  return false;
#endif
}

bool FrontLight::setWarmth(uint8_t percent) {
#if PAPYRIX_CAP_FRONTLIGHT
  if (!backend_.available()) return false;
  warmth_ = percent > 100 ? 100 : percent;
  backend_.write(brightness_, warmth_);
  return persist(board::frontLightWarmthKey(board::HardwareIdentity::instance().board()), warmth_);
#else
  (void)percent;
  return false;
#endif
}

bool FrontLight::persist(const char* key, uint8_t value) {
#if PAPYRIX_CAP_FRONTLIGHT
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) return false;
  const bool stored = preferences.getUChar(key, 0xFF) == value || preferences.putUChar(key, value) == sizeof(value);
  preferences.end();
  return stored;
#else
  (void)key;
  (void)value;
  return false;
#endif
}

}  // namespace papyrix::hal
