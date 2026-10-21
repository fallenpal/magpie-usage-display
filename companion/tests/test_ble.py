import asyncio
import json
import unittest
from unittest.mock import AsyncMock, patch

from companion.codex_display.ble import BleCompanion


class FakeBleClient:
    def __init__(self, expected_writes):
        self.is_connected = True
        self.expected_writes = expected_writes
        self.writes = []

    async def write_gatt_char(self, uuid, payload, response):
        self.writes.append((uuid, payload, response))
        if len(self.writes) >= self.expected_writes:
            self.is_connected = False


class HangingBleClient:
    is_connected = True

    async def write_gatt_char(self, uuid, payload, response):
        await asyncio.Event().wait()

    async def disconnect(self):
        await asyncio.Event().wait()


class BleTests(unittest.IsolatedAsyncioTestCase):
    async def test_connection_refreshes_before_sending_whole_batch(self):
        order = []
        messages = [
            b'{"v":5,"s":1,"k":"s"}',
            b'{"v":5,"s":1,"k":"e"}',
        ]

        async def status():
            order.append("status")
            return messages

        async def refresh():
            order.append("refresh")

        companion = BleCompanion(status, refresh)
        client = FakeBleClient(expected_writes=2)
        client.start_notify = AsyncMock()
        disconnected = asyncio.Event()
        disconnected.set()

        await companion._run_connection(client, disconnected)

        self.assertEqual(order, ["refresh", "status"])
        self.assertEqual(
            [write[1] for write in client.writes], messages
        )
        self.assertFalse(companion.connected_event.is_set())

    async def test_heartbeat_sends_all_batch_messages_in_order(self):
        messages = [
            b'{"v":5,"s":2,"k":"s"}',
            b'{"v":5,"s":2,"k":"p"}',
            b'{"v":5,"s":2,"k":"e"}',
        ]

        async def status():
            return messages

        async def refresh():
            return None

        changed = asyncio.Event()
        companion = BleCompanion(
            status,
            refresh,
            heartbeat_seconds=999,
            status_changed=changed,
        )
        client = FakeBleClient(expected_writes=3)
        changed.set()

        await companion._heartbeat_loop(client)

        self.assertEqual(
            [write[1] for write in client.writes], messages
        )

    async def test_batch_write_stops_when_connection_drops(self):
        async def status():
            return [b"one", b"two", b"three"]

        async def refresh():
            return None

        companion = BleCompanion(status, refresh)
        client = FakeBleClient(expected_writes=1)

        await companion._send_status(client)

        self.assertEqual(len(client.writes), 1)

    async def test_duplicate_request_is_not_executed_twice(self):
        async def status():
            return [b'{"v":5,"s":9,"k":"s"}']

        async def refresh():
            return None

        companion = BleCompanion(status, refresh)
        command = b'{"v":5,"sid":55,"id":7,"a":"focus_magpie"}'
        companion._commands.put_nowait(command)
        companion._commands.put_nowait(command)
        client = FakeBleClient(expected_writes=3)

        action = AsyncMock(return_value=(True, "FOCUSED"))
        with patch("companion.codex_display.ble.perform_action", action):
            await companion._command_loop(client)

        action.assert_awaited_once()
        self.assertEqual(len(client.writes), 3)
        self.assertEqual(client.writes[0][1], client.writes[2][1])

    async def test_invalid_command_returns_error_result(self):
        async def status():
            return [b'{"v":5,"s":9,"k":"s"}']

        async def refresh():
            return None

        companion = BleCompanion(status, refresh)
        companion._commands.put_nowait(
            b'{"v":4,"sid":55,"id":7,"a":"refresh"}'
        )
        client = FakeBleClient(expected_writes=1)

        await companion._command_loop(client)

        result = json.loads(client.writes[0][1])
        self.assertEqual(result["ok"], 0)
        self.assertIn("版本", result["m"])

    async def test_status_write_times_out(self):
        async def status():
            return [b"{}"]

        async def refresh():
            return None

        companion = BleCompanion(status, refresh)
        client = HangingBleClient()

        with patch(
            "companion.codex_display.ble.BLE_OPERATION_TIMEOUT_SECONDS",
            0.01,
        ):
            with self.assertRaises(asyncio.TimeoutError):
                await companion._send_status(client)

    async def test_disconnect_timeout_does_not_block_reconnect(self):
        async def status():
            return [b"{}"]

        async def refresh():
            return None

        companion = BleCompanion(status, refresh)
        client = HangingBleClient()

        with patch(
            "companion.codex_display.ble.BLE_DISCONNECT_TIMEOUT_SECONDS",
            0.01,
        ):
            await companion._disconnect(client)


if __name__ == "__main__":
    unittest.main()
