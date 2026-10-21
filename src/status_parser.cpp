#include "status_parser.h"

#include <ArduinoJson.h>
#include <cstring>

namespace {

constexpr uint32_t kStagingTimeoutMs = 5000;
constexpr size_t kMaxMessageBytes = 250;
constexpr uint8_t kMaxBatchMessages = 32;
constexpr uint8_t kProtocolVersion = 5;

void copyText(char* destination, size_t size, const char* source,
              const char* fallback = "-") {
  const char* value =
      (source != nullptr && source[0] != '\0') ? source : fallback;
  size_t index = 0;
  while (index + 1 < size && value[index] != '\0') {
    destination[index] = value[index];
    ++index;
  }
  destination[index] = '\0';
}

int16_t parsePercent(JsonVariantConst value) {
  if (value.isNull() || value.is<bool>() || !value.is<int>()) {
    return -1;
  }
  const int raw = value.as<int>();
  if (raw < 0 || raw > 100) {
    return -1;
  }
  return static_cast<int16_t>(raw);
}

bool readInt(JsonVariantConst value, int& output) {
  if (value.isNull() || value.is<bool>() || !value.is<int>()) {
    return false;
  }
  output = value.as<int>();
  return true;
}

uint16_t clampPermille(uint32_t value) {
  return static_cast<uint16_t>(value > 1000 ? 1000 : value);
}

int16_t clampOffset(int value) {
  if (value < -720) {
    return -720;
  }
  if (value > 840) {
    return 840;
  }
  return static_cast<int16_t>(value);
}

uint16_t clampU16(uint32_t value) {
  return static_cast<uint16_t>(value > 65535 ? 65535 : value);
}

uint32_t clampU32(uint64_t value) {
  return static_cast<uint32_t>(
      value > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : value);
}

}  // namespace

void StatusParser::clearBatchData() {
  staging_.state = AppState{};
  staging_.summary_seen = false;
  staging_.stats_seen = false;
  staging_.expected_providers = 0;
  staging_.expected_windows = 0;
  staging_.providers_seen = 0;
  staging_.windows_seen = 0;
  memset(staging_.provider_seen, 0, sizeof(staging_.provider_seen));
  memset(staging_.window_expected, 0, sizeof(staging_.window_expected));
  memset(staging_.window_seen_count, 0, sizeof(staging_.window_seen_count));
  memset(staging_.window_seen, 0, sizeof(staging_.window_seen));
}

void StatusParser::resetConnection() {
  staging_ = Staging{};
  last_committed_sequence_ = 0;
  last_error_ = "";
}

void StatusParser::expire(uint32_t now_ms) {
  if (staging_.active &&
      static_cast<uint32_t>(now_ms - staging_.updated_at) >=
          kStagingTimeoutMs) {
    staging_.active = false;
    last_error_ = "staging timeout";
  }
}

StatusParseResult StatusParser::ingest(const uint8_t* data, size_t length,
                                       uint32_t now_ms,
                                       AppState& committed,
                                       uint32_t& committed_sequence) {
  last_error_ = "";

  if (data == nullptr || length == 0 || length > kMaxMessageBytes) {
    last_error_ = "invalid message length";
    return StatusParseResult::kIgnored;
  }

  JsonDocument document;
  const DeserializationError error =
      deserializeJson(document, data, length);
  if (error) {
    last_error_ = "invalid JSON";
    return StatusParseResult::kIgnored;
  }
  if ((document["v"] | 0) != kProtocolVersion) {
    last_error_ = "unsupported protocol";
    return StatusParseResult::kIgnored;
  }

  const uint32_t sequence = document["s"].as<uint32_t>();
  const char* kind = document["k"] | "";
  if (sequence == 0 || kind[0] == '\0') {
    last_error_ = "missing sequence or kind";
    return StatusParseResult::kIgnored;
  }

  if (staging_.active && sequence == staging_.sequence) {
    // Continue the batch already in progress.
  } else if (staging_.active && sequence < staging_.sequence) {
    last_error_ = "stale batch";
    return StatusParseResult::kIgnored;
  } else {
    if (sequence <= last_committed_sequence_) {
      last_error_ = "already committed";
      return StatusParseResult::kIgnored;
    }
    staging_ = Staging{};
    staging_.active = true;
    staging_.sequence = sequence;
  }
  staging_.updated_at = now_ms;

  auto fail = [this](const char* message) {
    last_error_ = message;
    staging_.active = false;
    return StatusParseResult::kIgnored;
  };

  if (staging_.message_count >= kMaxBatchMessages) {
    return fail("batch exceeds 32 messages");
  }
  ++staging_.message_count;

  if (strcmp(kind, "s") == 0) {
    if (staging_.summary_seen) {
      const uint32_t batch_sequence = staging_.sequence;
      clearBatchData();
      staging_.active = true;
      staging_.sequence = batch_sequence;
      staging_.updated_at = now_ms;
    }

    int provider_count = 0;
    int window_count = 0;
    if (!readInt(document["c"], provider_count) ||
        !readInt(document["w"], window_count) || provider_count < 0 ||
        provider_count > kMaxProviders || window_count < 0 ||
        window_count > kMaxTotalWindows) {
      return fail("batch exceeds device limits");
    }

    AppState& state = staging_.state;
    state.unix_time = document["t"].as<uint64_t>();
    state.utc_offset_minutes = clampOffset(document["o"] | 0);
    state.tokens_today = document["d"].as<uint64_t>();
    state.calls_today = document["f"].as<uint32_t>();
    state.errors_today = document["z"].as<uint32_t>();
    state.success_permille =
        clampPermille(document["y"] | 0);
    state.average_ttft_ms = document["l"].as<uint32_t>();
    copyText(state.latest_model, sizeof(state.latest_model),
             document["m"] | "-");
    state.latest_status =
        clampU16(document["h"] | 0);
    state.latest_latency_ms = document["n"].as<uint32_t>();
    state.latest_failed = (document["b"] | 0) == 1;

    const int lowest = document["r"] | -1;
    state.lowest_remaining_percent =
        (lowest >= 0 && lowest <= 100) ? static_cast<int16_t>(lowest)
                                       : -1;
    state.batch_truncated = (document["tr"] | 0) == 1;
    state.batch_truncated_count =
        static_cast<uint16_t>(document["to"] | 0);

    staging_.expected_providers = static_cast<uint8_t>(provider_count);
    staging_.expected_windows = static_cast<uint8_t>(window_count);
    staging_.summary_seen = true;
    return StatusParseResult::kStaged;
  }

  if (!staging_.summary_seen) {
    return fail("message before summary");
  }

  if (strcmp(kind, "x") == 0) {
    JsonArrayConst values = document["a"].as<JsonArrayConst>();
    if (values.isNull() || values.size() < 10) {
      return fail("invalid statistics message");
    }

    AppState& state = staging_.state;
    state.session_count = clampU16(values[0].as<uint32_t>());
    state.total_calls = values[1].as<uint32_t>();
    state.total_input = values[2].as<uint64_t>();
    state.total_output = values[3].as<uint64_t>();
    state.total_cache_read = values[4].as<uint64_t>();
    state.total_reasoning = values[5].as<uint64_t>();
    state.total_cost_micros =
        clampU32(values[6].as<uint64_t>());
    state.cache_hit_permille =
        clampPermille(values[7] | 0);
    copyText(state.top_model, sizeof(state.top_model),
             values[8] | "-");
    state.top_model_calls = values[9].as<uint32_t>();
    staging_.stats_seen = true;
    return StatusParseResult::kStaged;
  }

  if (strcmp(kind, "p") == 0) {
    int provider_count = 0;
    int index = 0;
    if (!readInt(document["c"], provider_count) ||
        !readInt(document["i"], index) ||
        provider_count != staging_.expected_providers || index < 0 ||
        index >= provider_count || index >= kMaxProviders) {
      return fail("invalid provider message");
    }

    if (!staging_.provider_seen[index]) {
      staging_.state.providers[index] = ProviderState{};
      staging_.provider_seen[index] = true;
      ++staging_.providers_seen;
    }
    ProviderState& provider = staging_.state.providers[index];
    copyText(provider.id, sizeof(provider.id),
             document["id"] | "-");
    copyText(provider.name, sizeof(provider.name),
             document["m"] | "-");
    provider.read_at = document["ta"].as<int64_t>();
    return StatusParseResult::kStaged;
  }

  if (strcmp(kind, "w") == 0) {
    int provider_index = 0;
    int window_index = 0;
    int window_count = 0;
    if (!readInt(document["p"], provider_index) ||
        !readInt(document["i"], window_index) ||
        !readInt(document["w"], window_count) || provider_index < 0 ||
        provider_index >= staging_.expected_providers ||
        window_count <= 0 ||
        window_count > kMaxWindowsPerProvider || window_index < 0 ||
        window_index >= window_count) {
      return fail("invalid window message");
    }
    if (!staging_.provider_seen[provider_index]) {
      return fail("window before provider");
    }
    if (staging_.window_expected[provider_index] == 0) {
      staging_.window_expected[provider_index] = window_count;
    } else if (staging_.window_expected[provider_index] != window_count) {
      return fail("inconsistent window count");
    }

    if (!staging_.window_seen[provider_index][window_index]) {
      staging_.window_seen[provider_index][window_index] = true;
      ++staging_.windows_seen;
      ++staging_.window_seen_count[provider_index];
    }

    QuotaWindowState& window =
        staging_.state.providers[provider_index].windows[window_index];
    copyText(window.id, sizeof(window.id),
             document["id"] | "-");
    copyText(window.name, sizeof(window.name),
             document["m"] | "-");
    window.unlimited = (document["ul"] | 0) == 1;
    window.valid = ((document["ok"] | 0) == 1) || window.unlimited;
    window.resets_at = document["ra"].as<int64_t>();
    window.used_percent =
        window.unlimited ? -1 : parsePercent(document["u"]);
    window.remaining_percent =
        window.unlimited ? -1 : parsePercent(document["r"]);
    if (!window.unlimited) {
      if (window.used_percent < 0 && window.remaining_percent >= 0) {
        window.used_percent =
            static_cast<int16_t>(100 - window.remaining_percent);
      } else if (window.remaining_percent < 0 &&
                 window.used_percent >= 0) {
        window.remaining_percent =
            static_cast<int16_t>(100 - window.used_percent);
      }
    }
    return StatusParseResult::kStaged;
  }

  if (strcmp(kind, "e") == 0) {
    if (!staging_.stats_seen) {
      return fail("missing statistics before end");
    }

    int provider_count = 0;
    int window_count = 0;
    if (!readInt(document["c"], provider_count) ||
        !readInt(document["w"], window_count) ||
        provider_count != staging_.expected_providers ||
        window_count != staging_.expected_windows ||
        provider_count != staging_.providers_seen ||
        window_count != staging_.windows_seen) {
      return fail("batch count mismatch");
    }

    uint16_t counted_windows = 0;
    for (int index = 0; index < provider_count; ++index) {
      if (!staging_.provider_seen[index] ||
          staging_.window_seen_count[index] !=
              staging_.window_expected[index]) {
        return fail("incomplete provider data");
      }
      for (uint8_t window = 0;
           window < staging_.window_expected[index]; ++window) {
        if (!staging_.window_seen[index][window]) {
          return fail("incomplete window data");
        }
      }
      counted_windows =
          static_cast<uint16_t>(counted_windows +
                                staging_.window_expected[index]);
    }
    if (counted_windows != window_count) {
      return fail("window total mismatch");
    }

    if (document["tr"].is<int>()) {
      staging_.state.batch_truncated =
          (document["tr"] | 0) == 1;
      staging_.state.batch_truncated_count =
          static_cast<uint16_t>(document["to"] | 0);
    }

    if (!commit(now_ms, committed, committed_sequence)) {
      return fail("commit failed");
    }
    return StatusParseResult::kCommitted;
  }

  return StatusParseResult::kStaged;
}

bool StatusParser::commit(uint32_t /*now_ms*/, AppState& committed,
                          uint32_t& committed_sequence) {
  if (!staging_.active || !staging_.summary_seen ||
      !staging_.stats_seen) {
    return false;
  }

  staging_.state.provider_count = staging_.expected_providers;
  for (uint8_t index = 0;
       index < staging_.expected_providers; ++index) {
    staging_.state.providers[index].window_count =
        staging_.window_expected[index];
  }
  staging_.state.connection = ConnectionState::kLinked;
  staging_.state.time_valid = staging_.state.unix_time > 0;
  staging_.state.data_valid = true;

  committed = staging_.state;
  committed_sequence = staging_.sequence;
  last_committed_sequence_ = staging_.sequence;
  staging_.active = false;
  last_error_ = "";
  return true;
}
