#pragma once

#include <cmath>

using std::isnan;

#if defined(__LIBRETRO__) && defined(__SWITCH2__)
#include <chrono>
#include <ctime>

inline time_t timegm(struct tm* value)
{
  using namespace std::chrono;
  const auto date = sys_days{year{value->tm_year + 1900} / January / 1 + months{value->tm_mon}};
  return duration_cast<seconds>((date + days{value->tm_mday - 1}).time_since_epoch()).count() +
         value->tm_hour * 3600 + value->tm_min * 60 + value->tm_sec;
}
#endif
