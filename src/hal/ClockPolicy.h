#pragma once

#include <RtcBackend.h>

#include <cstdint>
#include <ctime>

namespace papyrix::hal::clock_policy {

constexpr bool isLeapYear(uint16_t year) { return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0); }

constexpr uint8_t daysInMonth(uint16_t year, uint8_t month) {
  constexpr uint8_t DAYS[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  return month == 2 && isLeapYear(year) ? 29 : month >= 1 && month <= 12 ? DAYS[month - 1] : 0;
}

constexpr bool isValid(const board::DateTime& value) {
  return value.year >= 1970 && value.year <= 2099 && value.month >= 1 && value.month <= 12 && value.day >= 1 &&
         value.day <= daysInMonth(value.year, value.month) && value.hour < 24 && value.minute < 60 &&
         value.second < 60 && value.weekday < 7;
}

constexpr std::time_t toUnixSeconds(const board::DateTime& value) {
  if (!isValid(value)) return static_cast<std::time_t>(-1);
  int64_t year = value.year;
  const uint8_t month = value.month;
  year -= month <= 2;
  const int64_t era = year / 400;
  const uint32_t yearOfEra = static_cast<uint32_t>(year - era * 400);
  const uint32_t dayOfYear = (153U * static_cast<uint32_t>(month + (month > 2 ? -3 : 9)) + 2U) / 5U + value.day - 1U;
  const uint32_t dayOfEra = yearOfEra * 365U + yearOfEra / 4U - yearOfEra / 100U + dayOfYear;
  const int64_t days = era * 146097 + dayOfEra - 719468;
  return static_cast<std::time_t>(days * 86400 + value.hour * 3600 + value.minute * 60 + value.second);
}

inline board::DateTime fromTm(const std::tm& value) {
  return {static_cast<uint16_t>(value.tm_year + 1900), static_cast<uint8_t>(value.tm_mon + 1),
          static_cast<uint8_t>(value.tm_mday),         static_cast<uint8_t>(value.tm_hour),
          static_cast<uint8_t>(value.tm_min),          static_cast<uint8_t>(value.tm_sec),
          static_cast<uint8_t>(value.tm_wday)};
}

}  // namespace papyrix::hal::clock_policy
