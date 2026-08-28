#pragma once

#include "BoardProfile.h"
#include "TargetConfig.h"

namespace papyrix::board {

// Return null when this artifact does not contain the profile.
const BoardProfile* findProfile(BoardId id);

const BoardProfile& bootProfile();

bool targetSupports(BoardId id);

}  // namespace papyrix::board
