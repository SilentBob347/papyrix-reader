#include "test_utils.h"

#include <string>

#include "core/EmergencyFirmwareUpdatePolicy.h"

using papyrix::emergency_update::NoticeAction;
using papyrix::emergency_update::noticeActionFor;

int main() {
  TestUtils::TestRunner runner("Emergency firmware update policy");

  runner.expectEq(static_cast<int>(NoticeAction::Render), static_cast<int>(noticeActionFor(true)),
                  "available display renders the update notice");
  runner.expectEq(static_cast<int>(NoticeAction::ContinueHeadless), static_cast<int>(noticeActionFor(false)),
                  "display failure does not block the update");
  runner.expectEq(std::string("Firmware update in progress"),
                  std::string(papyrix::emergency_update::kNoticeTitle), "notice identifies the operation");
  runner.expectEq(std::string("Do not power off"), std::string(papyrix::emergency_update::kNoticeWarning),
                  "notice warns against power loss");
  runner.expectEq(uint8_t(0xFF), papyrix::emergency_update::kNoticeBackground,
                  "notice uses a white background");
  runner.expectTrue(papyrix::emergency_update::kNoticeTextBlack, "notice uses black text");
  runner.expectFalse(papyrix::emergency_update::kTurnOffDuringRefresh,
                     "refresh completes before display power-down");
  runner.expectTrue(papyrix::emergency_update::kSleepAfterRender,
                    "display sleeps after the notification is stable");

  return runner.allPassed() ? 0 : 1;
}
