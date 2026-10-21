#include "app_state.h"

#include <time.h>

String formatCompactTokens(uint64_t tokens) {
  if (tokens >= 1000000000000ULL) {
    const double trillions = static_cast<double>(tokens) / 1000000000000.0;
    const uint8_t decimals =
        trillions >= 100 ? 0 : (trillions >= 10 ? 1 : 2);
    return String(trillions, static_cast<unsigned int>(decimals)) + "T";
  }
  if (tokens >= 1000000000ULL) {
    const double billions = static_cast<double>(tokens) / 1000000000.0;
    const uint8_t decimals = billions >= 100 ? 0 : (billions >= 10 ? 1 : 2);
    return String(billions, static_cast<unsigned int>(decimals)) + "B";
  }
  if (tokens >= 1000000ULL) {
    const double millions = static_cast<double>(tokens) / 1000000.0;
    const uint8_t decimals = millions >= 100 ? 0 : (millions >= 10 ? 1 : 2);
    return String(millions, static_cast<unsigned int>(decimals)) + "M";
  }
  if (tokens >= 1000ULL) {
    return String(static_cast<double>(tokens) / 1000.0, 1) + "K";
  }
  return String(static_cast<unsigned long>(tokens));
}

String formatCountdown(int64_t seconds) {
  if (seconds <= 0) {
    return "RESET DUE";
  }

  const int64_t days = seconds / 86400LL;
  const int64_t hours = (seconds % 86400LL) / 3600LL;
  const int64_t minutes = (seconds % 3600LL) / 60LL;
  const int64_t remainder = seconds % 60LL;

  if (days > 0) {
    return String(static_cast<unsigned long>(days)) + "d " +
           String(static_cast<unsigned long>(hours)) + "h";
  }
  if (hours > 0) {
    return String(static_cast<unsigned long>(hours)) + "h " +
           String(static_cast<unsigned long>(minutes)) + "m";
  }
  if (minutes > 0) {
    return String(static_cast<unsigned long>(minutes)) + "m " +
           String(static_cast<unsigned long>(remainder)) + "s";
  }
  return String(static_cast<unsigned long>(remainder)) + "s";
}

String formatDateTime(const AppState& state, uint32_t elapsed_seconds) {
  if (!state.time_valid) {
    return "--/-- --:--";
  }

  return formatResetTime(
      static_cast<int64_t>(state.unix_time + elapsed_seconds) +
          static_cast<int64_t>(state.utc_offset_minutes) * 60LL,
      0);
}

String formatResetTime(int64_t resets_at, int16_t utc_offset_minutes) {
  if (resets_at <= 0) {
    return "--/-- --:--";
  }

  const time_t local_time = static_cast<time_t>(
      resets_at + static_cast<int64_t>(utc_offset_minutes) * 60LL);
  struct tm value {};
  gmtime_r(&local_time, &value);

  char output[16];
  strftime(output, sizeof(output), "%m/%d %H:%M", &value);
  return String(output);
}

String formatQuotaPercent(int16_t percent) {
  if (percent < 0) {
    return "--";
  }
  return String(static_cast<int>(percent)) + "%";
}

const char* connectionLabel(ConnectionState state) {
  switch (state) {
    case ConnectionState::kDisconnected:
      return "OFFLINE";
    case ConnectionState::kSyncing:
      return "SYNCING";
    case ConnectionState::kLinked:
      return "LINKED";
    case ConnectionState::kStale:
      return "STALE";
  }
  return "OFFLINE";
}
