#include <unity.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "status_parser.h"

namespace {

constexpr uint8_t kMaxFixtureMessageBytes = 250;

std::string fixturePath() {
  const std::string source_file = __FILE__;
  const size_t slash = source_file.find_last_of("/\\");
  const std::string directory =
      slash == std::string::npos ? "." : source_file.substr(0, slash);
  return directory + "/../fixtures/v5_sequence_42.jsonl";
}

std::vector<std::string> readFixture() {
  std::ifstream input(fixturePath());
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    if (!line.empty()) {
      lines.push_back(line);
    }
  }
  return lines;
}

StatusParseResult feed(StatusParser& parser, const std::string& message,
                       uint32_t now_ms, AppState& state, uint32_t& sequence) {
  return parser.ingest(
      reinterpret_cast<const uint8_t*>(message.data()), message.size(),
      now_ms, state, sequence);
}

std::string withSequence(const std::string& message, uint32_t sequence) {
  const std::string from = "\"s\":42";
  const std::string to = "\"s\":" + std::to_string(sequence);
  const size_t position = message.find(from);
  if (position == std::string::npos) {
    return message;
  }
  return message.substr(0, position) + to +
         message.substr(position + from.size());
}

const char* kDerivedPercentBatch[] = {
    "{\"v\":5,\"s\":7,\"k\":\"s\",\"t\":1800000000,\"o\":0,\"d\":0,"
    "\"f\":0,\"z\":0,\"y\":1000,\"l\":0,\"m\":\"m\",\"h\":0,\"n\":0,"
    "\"b\":0,\"c\":1,\"w\":1,\"r\":100}",
    "{\"v\":5,\"s\":7,\"k\":\"x\",\"a\":[0,0,0,0,0,0,0,0,\"-\",0]}",
    "{\"v\":5,\"s\":7,\"k\":\"p\",\"i\":0,\"c\":1,\"id\":\"p\","
    "\"m\":\"Provider\"}",
    "{\"v\":5,\"s\":7,\"k\":\"w\",\"p\":0,\"i\":0,\"w\":1,"
    "\"id\":\"daily\",\"m\":\"Daily\",\"ok\":1,\"u\":30}",
    "{\"v\":5,\"s\":7,\"k\":\"e\",\"c\":1,\"w\":1}",
};

}  // namespace

void setUp() {}
void tearDown() {}

void test_golden_batch_commits_atomically() {
  const std::vector<std::string> lines = readFixture();
  TEST_ASSERT_EQUAL_UINT32(12, lines.size());

  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  for (size_t index = 0; index < lines.size(); ++index) {
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        kMaxFixtureMessageBytes, static_cast<uint32_t>(lines[index].size()));
    const StatusParseResult result =
        feed(parser, lines[index], 1000 + index * 10, committed, sequence);
    if (index + 1 == lines.size()) {
      TEST_ASSERT_EQUAL(StatusParseResult::kCommitted, result);
    } else {
      TEST_ASSERT_EQUAL(StatusParseResult::kStaged, result);
      TEST_ASSERT_TRUE(parser.awaitingCommit());
      TEST_ASSERT_FALSE(committed.data_valid);
    }
  }

  TEST_ASSERT_FALSE(parser.awaitingCommit());
  TEST_ASSERT_EQUAL_UINT32(42, sequence);
  TEST_ASSERT_EQUAL_UINT32(42, parser.lastCommittedSequence());
  TEST_ASSERT_TRUE(committed.data_valid);
  TEST_ASSERT_TRUE(committed.time_valid);
  TEST_ASSERT_EQUAL_UINT64(1800000000ULL, committed.unix_time);
  TEST_ASSERT_EQUAL_INT16(480, committed.utc_offset_minutes);
  TEST_ASSERT_EQUAL_UINT64(162, committed.tokens_today);
  TEST_ASSERT_EQUAL_UINT32(7, committed.calls_today);
  TEST_ASSERT_EQUAL_UINT32(2, committed.errors_today);
  TEST_ASSERT_EQUAL_UINT16(714, committed.success_permille);
  TEST_ASSERT_EQUAL_UINT32(1250, committed.average_ttft_ms);
  TEST_ASSERT_EQUAL_STRING("gpt-5.6-sol", committed.latest_model);
  TEST_ASSERT_EQUAL_UINT16(200, committed.latest_status);
  TEST_ASSERT_EQUAL_UINT32(9876, committed.latest_latency_ms);
  TEST_ASSERT_FALSE(committed.latest_failed);
  TEST_ASSERT_EQUAL_INT16(5, committed.lowest_remaining_percent);
  TEST_ASSERT_FALSE(committed.batch_truncated);

  TEST_ASSERT_EQUAL_UINT16(40, committed.session_count);
  TEST_ASSERT_EQUAL_UINT32(500, committed.total_calls);
  TEST_ASSERT_EQUAL_UINT64(123456, committed.total_input);
  TEST_ASSERT_EQUAL_UINT64(23456, committed.total_output);
  TEST_ASSERT_EQUAL_UINT64(987654, committed.total_cache_read);
  TEST_ASSERT_EQUAL_UINT64(34567, committed.total_reasoning);
  TEST_ASSERT_EQUAL_UINT32(6110000, committed.total_cost_micros);
  TEST_ASSERT_EQUAL_UINT16(889, committed.cache_hit_permille);
  TEST_ASSERT_EQUAL_STRING("gpt-5.6-sol", committed.top_model);
  TEST_ASSERT_EQUAL_UINT32(450, committed.top_model_calls);

  TEST_ASSERT_EQUAL_UINT8(3, committed.provider_count);
  TEST_ASSERT_EQUAL_STRING("codex-fc9469", committed.providers[0].id);
  TEST_ASSERT_EQUAL_STRING("Codex", committed.providers[0].name);
  TEST_ASSERT_EQUAL_UINT8(2, committed.providers[0].window_count);
  TEST_ASSERT_EQUAL_STRING("7-days", committed.providers[0].windows[0].id);
  TEST_ASSERT_EQUAL_INT16(95,
                          committed.providers[0].windows[0].used_percent);
  TEST_ASSERT_EQUAL_INT16(
      5, committed.providers[0].windows[0].remaining_percent);
  TEST_ASSERT_EQUAL_INT64(
      1793086420, committed.providers[0].windows[0].resets_at);
  TEST_ASSERT_TRUE(committed.providers[0].windows[0].valid);

  TEST_ASSERT_EQUAL_STRING("claude-a8f07f", committed.providers[1].id);
  TEST_ASSERT_EQUAL_UINT8(2, committed.providers[1].window_count);
  TEST_ASSERT_EQUAL_INT16(25,
                          committed.providers[1].windows[1].used_percent);
  TEST_ASSERT_EQUAL_INT16(
      75, committed.providers[1].windows[1].remaining_percent);

  TEST_ASSERT_EQUAL_STRING("gemini", committed.providers[2].id);
  TEST_ASSERT_EQUAL_UINT8(2, committed.providers[2].window_count);
  TEST_ASSERT_TRUE(committed.providers[2].windows[1].unlimited);
  TEST_ASSERT_TRUE(committed.providers[2].windows[1].valid);
  TEST_ASSERT_EQUAL_INT16(
      -1, committed.providers[2].windows[1].used_percent);
  TEST_ASSERT_EQUAL_INT16(
      -1, committed.providers[2].windows[1].remaining_percent);
}

void test_committed_sequence_is_ignored_until_reconnect() {
  const std::vector<std::string> lines = readFixture();
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  for (size_t index = 0; index < lines.size(); ++index) {
    feed(parser, lines[index], 1000 + static_cast<uint32_t>(index) * 10,
         committed, sequence);
  }
  TEST_ASSERT_TRUE(committed.data_valid);

  const StatusParseResult duplicate =
      feed(parser, lines.front(), 2000, committed, sequence);
  TEST_ASSERT_EQUAL(StatusParseResult::kIgnored, duplicate);
  TEST_ASSERT_EQUAL_STRING("already committed", parser.lastError());

  parser.resetConnection();
  TEST_ASSERT_EQUAL_UINT32(0, parser.lastCommittedSequence());

  for (size_t index = 0; index < lines.size(); ++index) {
    const StatusParseResult result =
        feed(parser, lines[index], 3000 + static_cast<uint32_t>(index) * 10,
             committed, sequence);
    if (index + 1 == lines.size()) {
      TEST_ASSERT_EQUAL(StatusParseResult::kCommitted, result);
    }
  }
  TEST_ASSERT_TRUE(committed.data_valid);
  TEST_ASSERT_EQUAL_UINT32(42, sequence);
}

void test_staging_timeout_preserves_previous_snapshot() {
  const std::vector<std::string> lines = readFixture();
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  for (size_t index = 0; index < 3; ++index) {
    TEST_ASSERT_EQUAL(StatusParseResult::kStaged,
                      feed(parser, lines[index], 1000 + index * 10,
                           committed, sequence));
  }
  TEST_ASSERT_TRUE(parser.awaitingCommit());

  parser.expire(6021);
  TEST_ASSERT_FALSE(parser.awaitingCommit());

  TEST_ASSERT_EQUAL(StatusParseResult::kIgnored,
                    feed(parser, lines.back(), 6030, committed, sequence));
  TEST_ASSERT_FALSE(committed.data_valid);
  TEST_ASSERT_TRUE(parser.lastError()[0] != '\0');
}

void test_incomplete_batch_does_not_commit() {
  const std::vector<std::string> lines = readFixture();
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  for (size_t index = 0; index < lines.size(); ++index) {
    if (index == 7) {
      continue;  // Drop one window message.
    }
    const StatusParseResult result =
        feed(parser, lines[index], 1000 + static_cast<uint32_t>(index) * 10,
             committed, sequence);
    if (index + 1 == lines.size()) {
      TEST_ASSERT_EQUAL(StatusParseResult::kIgnored, result);
    }
  }

  TEST_ASSERT_FALSE(parser.awaitingCommit());
  TEST_ASSERT_FALSE(committed.data_valid);
  TEST_ASSERT_TRUE(parser.lastError()[0] != '\0');
}

void test_derives_missing_percentage() {
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  for (size_t index = 0;
       index < sizeof(kDerivedPercentBatch) / sizeof(kDerivedPercentBatch[0]);
       ++index) {
    const StatusParseResult result =
        feed(parser, kDerivedPercentBatch[index],
             1000 + static_cast<uint32_t>(index) * 10, committed, sequence);
    if (index + 1 ==
        sizeof(kDerivedPercentBatch) / sizeof(kDerivedPercentBatch[0])) {
      TEST_ASSERT_EQUAL(StatusParseResult::kCommitted, result);
    }
  }

  TEST_ASSERT_TRUE(committed.data_valid);
  TEST_ASSERT_EQUAL_UINT8(1, committed.provider_count);
  TEST_ASSERT_EQUAL_INT16(30,
                          committed.providers[0].windows[0].used_percent);
  TEST_ASSERT_EQUAL_INT16(
      70, committed.providers[0].windows[0].remaining_percent);
}

void test_message_count_limit_is_enforced() {
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  const std::string summary =
      "{\"v\":5,\"s\":9,\"k\":\"s\",\"c\":0,\"w\":0,\"r\":-1}";
  TEST_ASSERT_EQUAL(StatusParseResult::kStaged,
                    feed(parser, summary, 1000, committed, sequence));

  const std::string unknown = "{\"v\":5,\"s\":9,\"k\":\"z\"}";
  for (size_t index = 0; index < 31; ++index) {
    TEST_ASSERT_EQUAL(StatusParseResult::kStaged,
                      feed(parser, unknown,
                           1001 + static_cast<uint32_t>(index), committed,
                           sequence));
  }
  TEST_ASSERT_EQUAL(StatusParseResult::kIgnored,
                    feed(parser, unknown, 1101, committed, sequence));
  TEST_ASSERT_EQUAL_STRING("batch exceeds 32 messages", parser.lastError());
  TEST_ASSERT_FALSE(parser.awaitingCommit());
}

void test_newer_sequence_replaces_incomplete_staging() {
  const std::vector<std::string> lines = readFixture();
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  TEST_ASSERT_EQUAL(StatusParseResult::kStaged,
                    feed(parser, lines[0], 1000, committed, sequence));
  TEST_ASSERT_EQUAL(StatusParseResult::kStaged,
                    feed(parser, lines[1], 1010, committed, sequence));

  for (size_t index = 0; index < lines.size(); ++index) {
    const std::string message = withSequence(lines[index], 43);
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(
        kMaxFixtureMessageBytes, static_cast<uint32_t>(message.size()));
    const StatusParseResult result =
        feed(parser, message, 2000 + static_cast<uint32_t>(index) * 10,
             committed, sequence);
    if (index + 1 == lines.size()) {
      TEST_ASSERT_EQUAL(StatusParseResult::kCommitted, result);
    }
  }

  TEST_ASSERT_EQUAL_UINT32(43, sequence);
  TEST_ASSERT_TRUE(committed.data_valid);
  TEST_ASSERT_EQUAL_UINT8(3, committed.provider_count);
}

void test_window_before_provider_does_not_commit() {
  StatusParser parser;
  AppState committed;
  uint32_t sequence = 0;

  TEST_ASSERT_EQUAL(
      StatusParseResult::kStaged,
      feed(parser,
           "{\"v\":5,\"s\":10,\"k\":\"s\",\"t\":1800000000,\"o\":0,"
           "\"d\":0,\"f\":0,\"z\":0,\"y\":1000,\"l\":0,\"m\":\"m\","
           "\"h\":0,\"n\":0,\"b\":0,\"c\":1,\"w\":1,\"r\":50}",
           1000, committed, sequence));
  TEST_ASSERT_EQUAL(
      StatusParseResult::kStaged,
      feed(parser,
           "{\"v\":5,\"s\":10,\"k\":\"x\","
           "\"a\":[0,0,0,0,0,0,0,0,\"-\",0]}",
           1010, committed, sequence));
  TEST_ASSERT_EQUAL(
      StatusParseResult::kIgnored,
      feed(parser,
           "{\"v\":5,\"s\":10,\"k\":\"w\",\"p\":0,\"i\":0,\"w\":1,"
           "\"id\":\"daily\",\"m\":\"Daily\",\"ok\":1,\"u\":50,\"r\":50}",
           1020, committed, sequence));
  TEST_ASSERT_EQUAL_STRING("window before provider", parser.lastError());
  TEST_ASSERT_FALSE(parser.awaitingCommit());

  TEST_ASSERT_EQUAL(
      StatusParseResult::kIgnored,
      feed(parser,
           "{\"v\":5,\"s\":10,\"k\":\"p\",\"i\":0,\"c\":1,"
           "\"id\":\"p\",\"m\":\"Provider\"}",
           1030, committed, sequence));
  TEST_ASSERT_EQUAL(
      StatusParseResult::kIgnored,
      feed(parser, "{\"v\":5,\"s\":10,\"k\":\"e\",\"c\":1,\"w\":1}",
           1040, committed, sequence));
  TEST_ASSERT_FALSE(parser.awaitingCommit());
  TEST_ASSERT_FALSE(committed.data_valid);
  TEST_ASSERT_EQUAL_UINT32(0, parser.lastCommittedSequence());
}

void test_oversized_counts_are_rejected() {
  const char* summaries[] = {
      "{\"v\":5,\"s\":11,\"k\":\"s\",\"c\":256,\"w\":0}",
      "{\"v\":5,\"s\":12,\"k\":\"s\",\"c\":0,\"w\":256}",
  };

  for (size_t index = 0;
       index < sizeof(summaries) / sizeof(summaries[0]); ++index) {
    StatusParser parser;
    AppState committed;
    uint32_t sequence = 0;
    TEST_ASSERT_EQUAL(StatusParseResult::kIgnored,
                      feed(parser, summaries[index], 1000, committed,
                           sequence));
    TEST_ASSERT_EQUAL_STRING("batch exceeds device limits",
                             parser.lastError());
    TEST_ASSERT_FALSE(parser.awaitingCommit());
    TEST_ASSERT_FALSE(committed.data_valid);
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_golden_batch_commits_atomically);
  RUN_TEST(test_committed_sequence_is_ignored_until_reconnect);
  RUN_TEST(test_staging_timeout_preserves_previous_snapshot);
  RUN_TEST(test_incomplete_batch_does_not_commit);
  RUN_TEST(test_derives_missing_percentage);
  RUN_TEST(test_message_count_limit_is_enforced);
  RUN_TEST(test_newer_sequence_replaces_incomplete_staging);
  RUN_TEST(test_window_before_provider_does_not_commit);
  RUN_TEST(test_oversized_counts_are_rejected);
  return UNITY_END();
}
