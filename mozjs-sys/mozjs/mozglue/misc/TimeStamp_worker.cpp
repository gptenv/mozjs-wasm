/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Cloudflare Workers provide time through the JavaScript host rather than a
// POSIX libc. The Worker embedding supplies this monotonic nanosecond ABI.

#include "mozilla/TimeStamp.h"

extern "C" uint64_t servo_worker_monotonic_now_ns();

namespace mozilla {

double BaseTimeDurationPlatformUtils::ToSeconds(int64_t aTicks) {
  return double(aTicks) / 1000000000.0;
}

int64_t BaseTimeDurationPlatformUtils::TicksFromMilliseconds(
    double aMilliseconds) {
  double result = aMilliseconds * 1000000.0;
  if (result >= double(INT64_MAX)) {
    return INT64_MAX;
  }
  if (result <= double(INT64_MIN)) {
    return INT64_MIN;
  }
  return int64_t(result);
}

void TimeStamp::Startup() {}

void TimeStamp::Shutdown() {}

TimeStamp TimeStamp::Now(bool) {
  return TimeStamp(servo_worker_monotonic_now_ns());
}

uint64_t TimeStamp::ComputeProcessUptime() { return 0; }

}  // namespace mozilla
