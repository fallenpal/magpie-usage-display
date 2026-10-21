import argparse
import asyncio
import logging
import os
from logging.handlers import RotatingFileHandler
from typing import Optional

from .ble import BleCompanion
from .magpie_client import MagpieClient
from .metrics import Snapshot, collect_snapshot
from .protocol import encode_batch


class SnapshotCache:
    def __init__(self, client: MagpieClient, refresh_seconds: float) -> None:
        self._client = client
        self._refresh_seconds = refresh_seconds
        self._snapshot: Optional[Snapshot] = None
        self._updated_at = 0.0
        self._sequence = 0
        self._lock = asyncio.Lock()
        self.status_changed = asyncio.Event()

    async def batch(self) -> list:
        loop = asyncio.get_running_loop()
        if self._snapshot is None or loop.time() - self._updated_at >= self._refresh_seconds:
            await self.refresh()
        assert self._snapshot is not None
        return encode_batch(self._snapshot, self._sequence)

    async def refresh(self) -> None:
        async with self._lock:
            self._snapshot = await collect_snapshot(self._client)
            self._updated_at = asyncio.get_running_loop().time()
            self._sequence += 1
            logging.info(
                "Magpie updated: %d providers, lowest remaining %s%%, "
                "today %d tokens, 7d %d tokens, calls %d, errors %d",
                len(self._snapshot.providers),
                self._snapshot.lowest_remaining_percent,
                self._snapshot.tokens_today,
                self._snapshot.tokens_7d,
                self._snapshot.calls_today,
                self._snapshot.errors_today,
            )


async def run(args: argparse.Namespace) -> None:
    cache = SnapshotCache(MagpieClient(args.magpie_url), args.refresh_seconds)
    if args.once:
        for payload in await cache.batch():
            print(payload.decode("utf-8"))
        return

    companion = BleCompanion(
        cache.batch,
        cache.refresh,
        device_name=args.device,
        heartbeat_seconds=args.heartbeat_seconds,
        status_changed=cache.status_changed,
    )
    await companion.run_forever()


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Magpie Usage Display BLE Companion")
    parser.add_argument("--device", default="Magpie Display")
    parser.add_argument(
        "--magpie-url",
        default=os.environ.get("MAGPIE_URL", "http://127.0.0.1:3425"),
    )
    parser.add_argument("--refresh-seconds", type=float, default=15)
    parser.add_argument("--heartbeat-seconds", type=float, default=15)
    parser.add_argument(
        "--once", action="store_true", help="读取一次本机 Magpie 数据并打印，不连接 BLE"
    )
    parser.add_argument("--verbose", action="store_true")
    return parser


def main() -> None:
    args = build_parser().parse_args()
    log_path = os.environ.get("MAGPIE_DISPLAY_LOG")
    handlers = None
    if log_path:
        handlers = [
            RotatingFileHandler(
                log_path, maxBytes=1024 * 1024, backupCount=1, encoding="utf-8"
            )
        ]
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        handlers=handlers,
    )
    try:
        asyncio.run(run(args))
    except KeyboardInterrupt:
        pass
    except Exception:
        logging.exception("Companion exited unexpectedly")
        raise SystemExit(1)


if __name__ == "__main__":
    main()
