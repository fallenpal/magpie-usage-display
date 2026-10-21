import json
import unittest
from dataclasses import replace
from pathlib import Path

from companion.codex_display.metrics import WindowSnapshot, build_snapshot
from companion.codex_display.protocol import (
    MAX_BATCH_MESSAGES,
    MAX_MESSAGE_BYTES,
    decode_command,
    encode_batch,
    encode_result,
    encode_window,
)

FIXTURES = Path(__file__).parent / "fixtures"


def load_fixture(name):
    return json.loads((FIXTURES / name).read_text(encoding="utf-8"))


class ProtocolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.snapshot = build_snapshot(
            load_fixture("codex_claude_quotas.json"),
            load_fixture("usage.json"),
            load_fixture("requests.json"),
            now=1_800_000_000,
            utc_offset_minutes=480,
        )
        cls.messages = encode_batch(cls.snapshot, 42)
        cls.decoded = [json.loads(message) for message in cls.messages]

    def test_golden_batch_matches_fixture(self):
        expected = [
            json.loads(line)
            for line in (
                FIXTURES / "v5_sequence_42.jsonl"
            ).read_text(encoding="utf-8").splitlines()
        ]
        self.assertEqual(self.decoded, expected)

    def test_batch_structure_and_counts(self):
        self.assertEqual(self.decoded[0]["k"], "s")
        self.assertEqual(self.decoded[1]["k"], "x")
        self.assertEqual(self.decoded[-1]["k"], "e")
        self.assertEqual(
            [message["k"] for message in self.decoded].count("p"), 3
        )
        self.assertEqual(
            [message["k"] for message in self.decoded].count("w"), 6
        )
        summary = self.decoded[0]
        end = self.decoded[-1]
        self.assertEqual((summary["c"], summary["w"]), (3, 6))
        self.assertEqual((end["c"], end["w"]), (3, 6))
        self.assertEqual(summary["r"], 5)
        for message in self.decoded:
            self.assertEqual(message["v"], 5)
            self.assertEqual(message["s"], 42)

    def test_every_message_fits_ble_budget(self):
        self.assertLessEqual(len(self.messages), MAX_BATCH_MESSAGES)
        for message in self.messages:
            self.assertLessEqual(len(message), MAX_MESSAGE_BYTES)

    def test_unknown_unlimited_and_finite_encoding(self):
        result = build_snapshot(
            {
                "data": [
                    {
                        "provider": "p",
                        "name": "P",
                        "windows": [
                            {
                                "name": "Finite",
                                "used": 10,
                                "remaining": 90,
                                "resetsAt": "2027-01-01T00:00:00Z",
                            },
                            {"name": "Unlimited", "unlimited": True},
                            {"name": "Missing"},
                        ],
                    }
                ]
            },
            {},
            now=1_800_000_000,
            utc_offset_minutes=0,
        )
        decoded = [
            json.loads(message) for message in encode_batch(result, 3)
        ]
        windows = {
            message["m"]: message
            for message in decoded
            if message["k"] == "w"
        }
        self.assertEqual(windows["Finite"]["ok"], 1)
        self.assertEqual(windows["Finite"]["u"], 10)
        self.assertEqual(windows["Finite"]["r"], 90)
        self.assertIn("ra", windows["Finite"])
        self.assertEqual(windows["Unlimited"]["ok"], 1)
        self.assertEqual(windows["Unlimited"]["ul"], 1)
        self.assertNotIn("u", windows["Unlimited"])
        self.assertNotIn("r", windows["Unlimited"])
        self.assertNotIn("ra", windows["Unlimited"])
        self.assertEqual(windows["Missing"]["ok"], 0)
        self.assertNotIn("u", windows["Missing"])
        self.assertNotIn("r", windows["Missing"])
        self.assertNotIn("ul", windows["Missing"])
        self.assertEqual(decoded[0]["r"], 90)

    def test_batch_contains_no_account_identifiers(self):
        result = build_snapshot(
            {
                "data": [
                    {
                        "provider": "acme",
                        "name": "Acme",
                        "email": "secret@example.com",
                        "windows": [{"name": "Daily", "remaining": 50}],
                    }
                ]
            },
            {},
            now=1_800_000_000,
            utc_offset_minutes=0,
        )
        raw = b"".join(encode_batch(result, 1)).decode("utf-8")
        self.assertNotIn("secret@example.com", raw)
        self.assertNotIn("example.com", raw)
        self.assertNotIn("@", raw)

    def test_truncation_flags_are_encoded(self):
        rows = [
            {
                "provider": f"p{index}",
                "name": f"P{index}",
                "windows": [{"name": "Daily", "remaining": 50}],
            }
            for index in range(10)
        ]
        result = build_snapshot(
            {"data": rows}, {}, now=1_800_000_000, utc_offset_minutes=0
        )
        decoded = [
            json.loads(message) for message in encode_batch(result, 7)
        ]
        summary = decoded[0]
        end = decoded[-1]
        self.assertEqual(summary["tr"], 1)
        self.assertEqual(summary["to"], 4)
        self.assertEqual(summary["c"], 8)
        self.assertEqual(summary["w"], 8)
        self.assertEqual(end["tr"], 1)
        self.assertEqual(end["to"], 4)

    def test_worst_case_batch_stays_within_limits(self):
        rows = []
        for provider_index in range(8):
            rows.append(
                {
                    "provider": f"provider-{provider_index}",
                    "name": f"Provider {provider_index} long name",
                    "accountId": f"acct-{provider_index}",
                    "windows": [
                        {
                            "name": f"Window {window_index} long label",
                            "used": 90 - window_index,
                            "resetsAt": "2026-12-31T23:59:59Z",
                        }
                        for window_index in range(2)
                    ],
                }
            )
        usage = {
            "series": [
                {
                    "input": 999_999_999,
                    "output": 999_999_999,
                    "reasoning": 999_999_999,
                    "cache_read": 999_999_999,
                    "cache_write": 999_999_999,
                    "calls": 999_999_999,
                    "errors": 999_999_998,
                    "timed": 999_999_999,
                    "ttft_ms": 999_999_999,
                }
            ],
            "input": 999_999_999,
            "output": 999_999_999,
            "cache_read": 999_999_999,
            "reasoning": 999_999_999,
            "cost": 999_999_999.99,
            "sessionCount": 999_999_999,
            "calls": 999_999_999,
            "models": [{"name": "Model " * 8, "calls": 999_999_999}],
        }
        requests = {
            "rows": [
                {
                    "model": "Model " * 8,
                    "status": 200,
                    "ms": 999_999_999,
                }
            ]
        }
        result = build_snapshot(
            {"data": rows},
            usage,
            requests,
            now=1_800_000_000,
            utc_offset_minutes=480,
        )
        messages = encode_batch(result, 26)
        self.assertLessEqual(len(messages), MAX_BATCH_MESSAGES)
        self.assertEqual(len(messages), 27)
        for message in messages:
            self.assertLessEqual(len(message), MAX_MESSAGE_BYTES)

    def test_batch_message_count_is_capped(self):
        provider = self.snapshot.providers[0]
        providers = tuple(
            replace(
                provider,
                id=f"provider-{index}",
                name=f"Provider {index}",
                windows=(provider.windows[0],),
            )
            for index in range(16)
        )
        oversized = replace(self.snapshot, providers=providers)
        with self.assertRaisesRegex(ValueError, str(MAX_BATCH_MESSAGES)):
            encode_batch(oversized, 1)

    def test_oversized_message_is_rejected(self):
        window = WindowSnapshot(
            id="x",
            name="x" * 300,
            used_percent=1,
            remaining_percent=99,
            resets_at=None,
            unlimited=False,
            valid=True,
        )
        with self.assertRaisesRegex(ValueError, str(MAX_MESSAGE_BYTES)):
            encode_window(window, 0, 0, 1, 1)

    def test_summary_omits_lowest_without_finite_window(self):
        result = build_snapshot(
            {
                "data": [
                    {
                        "provider": "p",
                        "name": "P",
                        "windows": [{"name": "Unlimited", "unlimited": True}],
                    }
                ]
            },
            {},
            now=1_800_000_000,
            utc_offset_minutes=0,
        )
        summary = json.loads(encode_batch(result, 1)[0])
        self.assertNotIn("r", summary)
        self.assertEqual(summary["c"], 1)
        self.assertEqual(summary["w"], 1)

    def test_accepts_v5_allowlisted_command(self):
        command = decode_command(
            b'{"v":5,"sid":9,"id":7,"a":"refresh"}'
        )
        self.assertEqual(command["id"], 7)
        self.assertEqual(command["a"], "refresh")

    def test_rejects_unknown_action(self):
        with self.assertRaisesRegex(ValueError, "不允许"):
            decode_command(b'{"v":5,"sid":9,"id":7,"a":"shell"}')

    def test_rejects_wrong_protocol(self):
        with self.assertRaisesRegex(ValueError, "协议版本"):
            decode_command(b'{"v":4,"sid":9,"id":7,"a":"refresh"}')

    def test_rejects_invalid_command_fields(self):
        with self.assertRaisesRegex(ValueError, "JSON"):
            decode_command(b"not json")
        with self.assertRaisesRegex(ValueError, "session id"):
            decode_command(b'{"v":5,"sid":0,"id":7,"a":"refresh"}')
        with self.assertRaisesRegex(ValueError, "request id"):
            decode_command(b'{"v":5,"sid":9,"id":0,"a":"refresh"}')

    def test_result_is_bounded(self):
        payload = encode_result(8, False, "x" * 100)
        decoded = json.loads(payload)
        self.assertEqual(decoded["v"], 5)
        self.assertEqual(decoded["ok"], 0)
        self.assertEqual(len(decoded["m"]), 48)


if __name__ == "__main__":
    unittest.main()
