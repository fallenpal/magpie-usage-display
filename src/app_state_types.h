#pragma once

#include <stdint.h>

constexpr uint8_t kMaxProviders = 8;
constexpr uint8_t kMaxWindowsPerProvider = 4;
constexpr uint8_t kMaxTotalWindows = 16;

enum class ConnectionState : uint8_t {
  kDisconnected,
  kSyncing,
  kLinked,
  kStale,
};

struct QuotaWindowState {
  char id[24] = "";
  char name[24] = "-";
  int16_t used_percent = -1;
  int16_t remaining_percent = -1;
  int64_t resets_at = 0;
  bool unlimited = false;
  bool valid = false;
};

struct ProviderState {
  char id[24] = "";
  char name[24] = "-";
  int64_t read_at = 0;
  QuotaWindowState windows[kMaxWindowsPerProvider];
  uint8_t window_count = 0;
};

struct AppState {
  ProviderState providers[kMaxProviders];
  uint8_t provider_count = 0;
  int16_t lowest_remaining_percent = -1;
  bool batch_truncated = false;
  uint16_t batch_truncated_count = 0;

  uint64_t tokens_today = 0;
  uint32_t calls_today = 0;
  uint32_t errors_today = 0;
  uint16_t success_permille = 1000;
  uint32_t average_ttft_ms = 0;
  char latest_model[32] = "-";
  uint16_t latest_status = 0;
  uint32_t latest_latency_ms = 0;
  bool latest_failed = false;
  uint16_t session_count = 0;
  uint32_t total_calls = 0;
  uint64_t total_input = 0;
  uint64_t total_output = 0;
  uint64_t total_cache_read = 0;
  uint64_t total_reasoning = 0;
  uint32_t total_cost_micros = 0;
  uint16_t cache_hit_permille = 0;
  char top_model[24] = "-";
  uint32_t top_model_calls = 0;

  uint64_t unix_time = 0;
  int16_t utc_offset_minutes = 0;
  ConnectionState connection = ConnectionState::kDisconnected;
  bool time_valid = false;
  bool data_valid = false;
};
