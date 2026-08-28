#pragma once

#include <DisplayController.h>

#include "BoardProfile.h"

namespace papyrix::board {

eink::DisplayController fixedPanelFor(BoardId board);

#ifdef ARDUINO
bool retryButtonPressed(const BoardProfile& profile);
void waitForRecoveryRetry(const BoardProfile& profile);
#endif

}  // namespace papyrix::board
