import asyncio
import json
import unittest

from companion.codex_display.main import SnapshotCache
from companion.codex_display.metrics import collect_snapshot


class FakeMagpieClient:
    def __init__(self):
        self.paths = []

    async def health(self):
        return {"name": "magpie"}

    async def get(self, path):
        self.paths.append(path)
        if path == "/v1/magpie/quotas":
            return {"data": [{"windows": [{"name": "7 days", "remaining": 75}]}]}
        if path == "/v1/magpie/usage":
            return {"series": [{"input": 20, "output": 5, "calls": 2}]}
        if path == "/v1/magpie/usage/requests?limit=1":
            return {"rows": [{"served": "test-model", "status": 200, "ms": 50}]}
        raise AssertionError(path)


class MagpieAdapterTests(unittest.TestCase):
    def test_collects_quota_and_usage(self):
        client = FakeMagpieClient()
        snapshot = asyncio.run(collect_snapshot(client))

        self.assertEqual(
            snapshot.providers[0].windows[0].remaining_percent, 75
        )
        self.assertEqual(snapshot.tokens_today, 25)
        self.assertEqual(snapshot.calls_today, 2)
        self.assertCountEqual(
            client.paths,
            [
                "/v1/magpie/quotas",
                "/v1/magpie/usage",
                "/v1/magpie/usage/requests?limit=1",
            ],
        )


class SnapshotCacheTests(unittest.IsolatedAsyncioTestCase):
    async def test_refresh_encodes_protocol_v5_batch(self):
        cache = SnapshotCache(FakeMagpieClient(), refresh_seconds=60)
        await cache.refresh()

        messages = [
            json.loads(message) for message in await cache.batch()
        ]

        self.assertEqual(messages[0]["v"], 5)
        self.assertEqual(messages[0]["k"], "s")
        self.assertEqual(messages[0]["r"], 75)
        self.assertEqual(messages[0]["f"], 2)
        self.assertEqual(messages[0]["m"], "test-model")
        self.assertEqual(messages[1]["k"], "x")
        self.assertEqual(messages[-1]["k"], "e")
        self.assertEqual(messages[-1]["c"], 1)
        self.assertEqual(messages[-1]["w"], 1)


if __name__ == "__main__":
    unittest.main()
