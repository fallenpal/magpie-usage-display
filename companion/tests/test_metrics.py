import json
import unittest
from pathlib import Path

from companion.codex_display.metrics import (
    MAX_PROVIDERS,
    MAX_TOTAL_WINDOWS,
    MAX_WINDOWS_PER_PROVIDER,
    build_snapshot,
)

FIXTURES = Path(__file__).parent / "fixtures"


def load_fixture(name):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


def snapshot(quotas, usage=None, requests=None):
    return build_snapshot(
        quotas,
        usage or {},
        requests,
        now=1_800_000_000,
        utc_offset_minutes=480,
    )


def single_provider(windows, provider="test", name="Test"):
    return {
        "data": [
            {
                "provider": provider,
                "name": name,
                "windows": windows,
            }
        ]
    }


def windows_by_name(result):
    return {
        window.name: window
        for provider in result.providers
        for window in provider.windows
    }


class MetricsTests(unittest.TestCase):
    def setUp(self):
        self.result = snapshot(
            load_fixture("codex_claude_quotas.json"),
            load_fixture("usage.json"),
            load_fixture("requests.json"),
        )

    def test_fixture_orders_codex_then_claude_then_others(self):
        self.assertEqual(
            [provider.name for provider in self.result.providers],
            ["Codex", "Claude", "Gemini"],
        )
        self.assertTrue(self.result.providers[0].id.startswith("codex-"))
        self.assertTrue(self.result.providers[1].id.startswith("claude-"))
        self.assertEqual(self.result.providers[2].id, "gemini")

    def test_windows_are_sorted_by_lowest_finite_remaining(self):
        codex, claude, gemini = self.result.providers
        self.assertEqual(
            [window.remaining_percent for window in codex.windows],
            [5, 60],
        )
        self.assertEqual(
            [window.remaining_percent for window in claude.windows],
            [40, 75],
        )
        self.assertEqual(
            [window.remaining_percent for window in gemini.windows],
            [90, None],
        )
        self.assertEqual(self.result.lowest_remaining_percent, 5)

    def test_base_stats_are_preserved(self):
        self.assertEqual(self.result.tokens_today, 162)
        self.assertEqual(self.result.tokens_7d, 177)
        self.assertEqual(self.result.calls_today, 7)
        self.assertEqual(self.result.errors_today, 2)
        self.assertEqual(self.result.success_permille, 714)
        self.assertEqual(self.result.average_ttft_ms, 1250)
        self.assertEqual(self.result.latest_model, "gpt-5.6-sol")
        self.assertEqual(self.result.latest_status, 200)
        self.assertEqual(self.result.latest_latency_ms, 9876)
        self.assertFalse(self.result.latest_failed)
        self.assertEqual(self.result.session_count, 40)
        self.assertEqual(self.result.total_calls, 500)
        self.assertEqual(self.result.cache_hit_permille, 889)
        self.assertEqual(self.result.total_cost_micros, 6_110_000)
        self.assertEqual(self.result.top_model, "gpt-5.6-sol")
        self.assertEqual(self.result.top_model_calls, 450)

    def test_empty_responses_have_no_fake_quota(self):
        result = build_snapshot({}, {}, now=1, utc_offset_minutes=0)
        self.assertEqual(result.providers, ())
        self.assertIsNone(result.lowest_remaining_percent)
        self.assertFalse(result.truncated)
        self.assertEqual(result.truncated_count, 0)
        self.assertEqual(result.tokens_today, 0)
        self.assertEqual(result.success_permille, 1000)
        self.assertEqual(result.latest_model, "")

    def test_used_and_remaining_are_reconciled(self):
        result = snapshot(
            single_provider(
                [
                    {"name": "Only used", "used": 80},
                    {"name": "Only remaining", "remaining": 30},
                    {"name": "Zero", "remaining": 0},
                ]
            )
        )
        windows = windows_by_name(result)
        self.assertEqual(windows["Only used"].used_percent, 80)
        self.assertEqual(windows["Only used"].remaining_percent, 20)
        self.assertEqual(windows["Only remaining"].used_percent, 70)
        self.assertEqual(windows["Only remaining"].remaining_percent, 30)
        self.assertEqual(windows["Zero"].used_percent, 100)
        self.assertEqual(windows["Zero"].remaining_percent, 0)
        self.assertEqual(result.lowest_remaining_percent, 0)

    def test_conflicting_percentages_use_used(self):
        with self.assertLogs(
            "companion.codex_display.metrics", level="WARNING"
        ) as logs:
            result = snapshot(
                single_provider(
                    [{"name": "Conflict", "used": 70, "remaining": 50}]
                )
            )
        window = result.providers[0].windows[0]
        self.assertEqual(window.used_percent, 70)
        self.assertEqual(window.remaining_percent, 30)
        self.assertIn("conflict", "\n".join(logs.output))

    def test_invalid_percentages_do_not_become_zero(self):
        result = snapshot(
            single_provider(
                [
                    {"name": "Too high", "used": 140},
                    {"name": "Negative", "used": -5},
                    {"name": "Boolean", "used": True},
                ]
            )
        )
        for window in result.providers[0].windows:
            self.assertIsNone(window.used_percent)
            self.assertIsNone(window.remaining_percent)
            self.assertFalse(window.valid)
        self.assertIsNone(result.lowest_remaining_percent)

    def test_finite_unlimited_and_missing_are_distinct(self):
        result = snapshot(
            single_provider(
                [
                    {"name": "Finite", "remaining": 90},
                    {"name": "Unlimited", "unlimited": True},
                    {"name": "Missing"},
                ]
            )
        )
        windows = result.providers[0].windows
        self.assertEqual(
            [window.name for window in windows],
            ["Finite", "Unlimited", "Missing"],
        )
        finite, unlimited, missing = windows
        self.assertTrue(finite.valid)
        self.assertFalse(finite.unlimited)
        self.assertEqual(finite.remaining_percent, 90)
        self.assertTrue(unlimited.valid)
        self.assertTrue(unlimited.unlimited)
        self.assertIsNone(unlimited.remaining_percent)
        self.assertFalse(missing.valid)
        self.assertFalse(missing.unlimited)
        self.assertIsNone(missing.remaining_percent)
        self.assertEqual(result.lowest_remaining_percent, 90)

    def test_duplicate_window_names_get_unique_ids(self):
        result = snapshot(
            single_provider(
                [
                    {"name": "Daily", "remaining": 50},
                    {"name": "Daily", "remaining": 20},
                ]
            )
        )
        windows = result.providers[0].windows
        self.assertEqual(
            [window.remaining_percent for window in windows], [20, 50]
        )
        self.assertEqual(
            {window.id for window in windows}, {"daily-1", "daily-2"}
        )

    def test_duplicate_provider_names_get_unique_ids(self):
        result = snapshot(
            {
                "data": [
                    {
                        "provider": "custom",
                        "name": "Custom A",
                        "windows": [{"name": "A", "remaining": 50}],
                    },
                    {
                        "provider": "custom",
                        "name": "Custom B",
                        "windows": [{"name": "B", "remaining": 40}],
                    },
                ]
            }
        )
        ids = [provider.id for provider in result.providers]
        self.assertEqual(ids, ["custom-1", "custom-2"])

    def test_account_identifier_is_hashed(self):
        result = snapshot(
            {
                "data": [
                    {
                        "provider": "acme",
                        "name": "Acme",
                        "email": "person@example.com",
                        "windows": [{"name": "Daily", "remaining": 50}],
                    }
                ]
            }
        )
        provider_id = result.providers[0].id
        self.assertRegex(provider_id, r"^acme-[0-9a-f]{6}$")
        self.assertNotIn("person", provider_id)
        self.assertNotIn("@", provider_id)

    def test_provider_limit_sets_truncation(self):
        rows = [
            {
                "provider": f"p{index}",
                "name": f"P{index}",
                "windows": [{"name": "Daily", "remaining": 50}],
            }
            for index in range(MAX_PROVIDERS + 4)
        ]
        result = snapshot({"data": rows})
        self.assertEqual(len(result.providers), MAX_PROVIDERS)
        self.assertTrue(result.truncated)
        self.assertEqual(result.truncated_count, 8)

    def test_window_limit_per_provider(self):
        windows = [
            {"name": f"W{index}", "remaining": 10 * (index + 1)}
            for index in range(MAX_WINDOWS_PER_PROVIDER + 2)
        ]
        result = snapshot(single_provider(windows))
        kept = result.providers[0].windows
        self.assertEqual(len(kept), MAX_WINDOWS_PER_PROVIDER)
        self.assertEqual(
            [window.name for window in kept], ["W0", "W1", "W2", "W3"]
        )
        self.assertTrue(result.truncated)
        self.assertEqual(result.truncated_count, 2)

    def test_total_window_limit_drops_provider_without_room(self):
        rows = []
        for provider_index in range(5):
            rows.append(
                {
                    "provider": f"p{provider_index}",
                    "name": f"P{provider_index}",
                    "windows": [
                        {
                            "name": f"W{window_index}",
                            "remaining": 10 * window_index + 1,
                        }
                        for window_index in range(4)
                    ],
                }
            )
        result = snapshot({"data": rows})
        self.assertEqual(len(result.providers), 4)
        self.assertEqual(
            sum(len(provider.windows) for provider in result.providers),
            MAX_TOTAL_WINDOWS,
        )
        self.assertTrue(result.truncated)
        self.assertEqual(result.truncated_count, 5)
        self.assertEqual(result.lowest_remaining_percent, 1)

    def test_reset_time_is_parsed_as_utc(self):
        codex = next(
            provider
            for provider in self.result.providers
            if provider.id.startswith("codex-")
        )
        weekly = next(
            window for window in codex.windows if window.name == "7 days"
        )
        self.assertEqual(weekly.resets_at, 1793086420)
        self.assertEqual(codex.read_at, 1792638605)

    def test_provider_level_reset_is_ignored(self):
        result = snapshot(
            {
                "data": [
                    {
                        "provider": "p",
                        "name": "P",
                        "resetsAt": "2026-10-27T00:00:00Z",
                        "windows": [{"name": "Daily", "remaining": 50}],
                    }
                ]
            }
        )
        self.assertIsNone(result.providers[0].windows[0].resets_at)


if __name__ == "__main__":
    unittest.main()
