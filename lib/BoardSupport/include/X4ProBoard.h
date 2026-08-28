#pragma once

#include "BoardProfile.h"

namespace papyrix::board::x4pro {

void earlyInit(const BoardProfile& profile);
void enableTouch(const TouchConfig& touch);
void prepareCharacterization(const BoardProfile& profile);
void prepareDeepSleep(const BoardProfile& profile);

}  // namespace papyrix::board::x4pro
