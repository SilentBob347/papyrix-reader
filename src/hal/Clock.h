#pragma once

#include <RtcBackend.h>

#include <ctime>

namespace papyrix::hal {

class Clock {
 public:
  void init();

  bool isRtcAvailable() const { return rtcAvailable_; }
  bool readRtc(board::DateTime& out);
  bool localTime(std::tm& out) const;
  bool updateFromSystem();

 private:
  static constexpr unsigned long RTC_POLL_MS = 10000;

  board::RtcBackend rtc_;
  board::DateTime cachedRtc_{};
  unsigned long lastRtcPollMs_ = 0;
  bool rtcAvailable_ = false;
  bool hasCachedRtc_ = false;
};

}  // namespace papyrix::hal
