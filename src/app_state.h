#pragma once

#include <Arduino.h>

#include "app_state_types.h"

String formatCompactTokens(uint64_t tokens);
String formatCountdown(int64_t seconds);
String formatDateTime(const AppState& state, uint32_t elapsed_seconds = 0);
String formatResetTime(int64_t resets_at, int16_t utc_offset_minutes);
String formatQuotaPercent(int16_t percent);
const char* connectionLabel(ConnectionState state);
