#pragma once

#include <stddef.h>
#include <stdint.h>

#include "app_state_types.h"

enum class StatusParseResult : uint8_t {
  kIgnored,
  kStaged,
  kCommitted,
};

class StatusParser {
 public:
  StatusParseResult ingest(const uint8_t* data, size_t length, uint32_t now_ms,
                           AppState& committed,
                           uint32_t& committed_sequence);
  void resetConnection();
  void expire(uint32_t now_ms);
  bool awaitingCommit() const { return staging_.active; }
  const char* lastError() const { return last_error_; }
  uint32_t lastCommittedSequence() const {
    return last_committed_sequence_;
  }

 private:
  struct Staging {
    AppState state;
    uint32_t sequence = 0;
    uint32_t updated_at = 0;
    uint8_t message_count = 0;
    bool active = false;
    bool summary_seen = false;
    bool stats_seen = false;
    uint8_t expected_providers = 0;
    uint8_t expected_windows = 0;
    uint8_t providers_seen = 0;
    uint8_t windows_seen = 0;
    bool provider_seen[kMaxProviders] = {};
    uint8_t window_expected[kMaxProviders] = {};
    uint8_t window_seen_count[kMaxProviders] = {};
    bool window_seen[kMaxProviders][kMaxWindowsPerProvider] = {};
  };

  void clearBatchData();
  bool commit(uint32_t now_ms, AppState& committed,
              uint32_t& committed_sequence);

  Staging staging_;
  uint32_t last_committed_sequence_ = 0;
  const char* last_error_ = "";
};
