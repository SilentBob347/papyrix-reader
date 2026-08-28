#pragma once

#include <cstdint>

namespace papyrix::emergency_update {

inline constexpr char kNoticeTitle[] = "Firmware update in progress";
inline constexpr char kNoticeWarning[] = "Do not power off";
inline constexpr uint8_t kNoticeBackground = 0xFF;
inline constexpr bool kNoticeTextBlack = true;
inline constexpr bool kTurnOffDuringRefresh = false;
inline constexpr bool kSleepAfterRender = true;

enum class NoticeAction : uint8_t { Render, ContinueHeadless };

constexpr NoticeAction noticeActionFor(const bool displayReady) {
  return displayReady ? NoticeAction::Render : NoticeAction::ContinueHeadless;
}

}  // namespace papyrix::emergency_update
