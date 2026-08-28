#include <Arduino.h>
#include <Clock.h>
#include <ClockPolicy.h>

#include <ctime>

#include "test_utils.h"

namespace {

bool fakeBeginResult = false;
bool fakeReadResult = false;
bool fakeWriteResult = false;
int fakeReadCount = 0;
int fakeWriteCount = 0;
papyrix::board::DateTime fakeReadValue{};
papyrix::board::DateTime fakeWrittenValue{};

void resetFakeRtc() {
  fakeBeginResult = false;
  fakeReadResult = false;
  fakeWriteResult = false;
  fakeReadCount = 0;
  fakeWriteCount = 0;
  fakeReadValue = {};
  fakeWrittenValue = {};
  testSetManualMillis(0);
}

bool sameDateTime(const papyrix::board::DateTime& left, const papyrix::board::DateTime& right) {
  return left.year == right.year && left.month == right.month && left.day == right.day && left.hour == right.hour &&
         left.minute == right.minute && left.second == right.second && left.weekday == right.weekday;
}

}  // namespace

namespace papyrix::board {

bool RtcBackend::begin() {
  available_ = fakeBeginResult;
  return available_;
}

bool RtcBackend::read(DateTime& out) {
  fakeReadCount++;
  if (!fakeReadResult) return false;
  out = fakeReadValue;
  return true;
}

bool RtcBackend::write(const DateTime& value) {
  fakeWriteCount++;
  fakeWrittenValue = value;
  return fakeWriteResult;
}

}  // namespace papyrix::board

int main() {
  TestUtils::TestRunner runner("Clock HAL");

  runner.expectTrue(papyrix::hal::clock_policy::isValid({2024, 2, 29, 23, 59, 59, 4}),
                    "accepts a valid leap day");
  runner.expectFalse(papyrix::hal::clock_policy::isValid({2023, 2, 29, 0, 0, 0, 3}),
                     "rejects an invalid leap day");
  runner.expectFalse(papyrix::hal::clock_policy::isValid({2024, 1, 1, 24, 0, 0, 1}),
                     "rejects an invalid hour");
  runner.expectEq(std::time_t{0}, papyrix::hal::clock_policy::toUnixSeconds({1970, 1, 1, 0, 0, 0, 4}),
                  "converts the Unix epoch");
  runner.expectEq(std::time_t{951782400},
                  papyrix::hal::clock_policy::toUnixSeconds({2000, 2, 29, 0, 0, 0, 2}),
                  "converts a leap day to Unix time");

  resetFakeRtc();
  papyrix::hal::Clock unavailableClock;
  unavailableClock.init();
  runner.expectFalse(unavailableClock.isRtcAvailable(), "reports an unavailable RTC");
  papyrix::board::DateTime unavailableTime;
  runner.expectFalse(unavailableClock.readRtc(unavailableTime), "does not read an unavailable RTC");

  resetFakeRtc();
  fakeBeginResult = true;
  fakeReadResult = true;
  fakeReadValue = {2026, 8, 28, 12, 34, 56, 5};
  papyrix::hal::Clock clock;
  clock.init();
  runner.expectTrue(clock.isRtcAvailable(), "reports an available RTC");
  runner.expectEq(1, fakeReadCount, "reads the RTC during initialization");

  papyrix::board::DateTime current;
  testSetManualMillis(5000);
  runner.expectTrue(clock.readRtc(current), "returns cached RTC time inside poll interval");
  runner.expectTrue(sameDateTime(fakeReadValue, current), "cached RTC time matches the last successful read");
  runner.expectEq(1, fakeReadCount, "cached read does not access the RTC");

  fakeReadResult = false;
  testSetManualMillis(10001);
  runner.expectTrue(clock.readRtc(current), "returns cached RTC time after a transient read failure");
  runner.expectTrue(sameDateTime(fakeReadValue, current), "transient failure preserves cached RTC time");
  runner.expectEq(2, fakeReadCount, "expired cache polls the RTC once");

  fakeWriteResult = true;
  runner.expectTrue(clock.updateFromSystem(), "accepts a valid synchronized system time");
  runner.expectEq(1, fakeWriteCount, "writes synchronized system time to the RTC");
  runner.expectTrue(fakeWrittenValue.year >= 2024, "writes a current UTC year");

  std::tm local{};
  runner.expectTrue(clock.localTime(local), "returns valid local system time");

  testUseRealtimeMillis();
  return runner.allPassed() ? 0 : 1;
}
