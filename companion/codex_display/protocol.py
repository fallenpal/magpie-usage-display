import json
from typing import Any, Dict, List, Sequence

from .constants import PROTOCOL_VERSION
from .metrics import ProviderSnapshot, Snapshot, WindowSnapshot

ALLOWED_ACTIONS = {"focus_magpie", "refresh"}
MAX_MESSAGE_BYTES = 250
MAX_BATCH_MESSAGES = 32


def encode_batch(snapshot: Snapshot, sequence: int) -> List[bytes]:
    provider_count = len(snapshot.providers)
    total_windows = sum(
        len(provider.windows) for provider in snapshot.providers
    )
    messages = [
        encode_summary(snapshot, sequence, total_windows),
        encode_stats(snapshot, sequence),
    ]
    for provider_index, provider in enumerate(snapshot.providers):
        messages.append(
            encode_provider(
                provider, provider_index, provider_count, sequence
            )
        )
        for window_index, window in enumerate(provider.windows):
            messages.append(
                encode_window(
                    window,
                    provider_index,
                    window_index,
                    len(provider.windows),
                    sequence,
                )
            )
    messages.append(
        encode_end(
            provider_count,
            total_windows,
            snapshot.truncated,
            snapshot.truncated_count,
            sequence,
        )
    )
    if len(messages) > MAX_BATCH_MESSAGES:
        raise ValueError(
            f"状态批次超过 {MAX_BATCH_MESSAGES} 条消息上限"
        )
    for message in messages:
        if len(message) > MAX_MESSAGE_BYTES:
            raise ValueError(
                f"状态消息超过 {MAX_MESSAGE_BYTES} 字节 BLE 预算"
            )
    return messages


def encode_summary(
    snapshot: Snapshot, sequence: int, total_windows: int
) -> bytes:
    payload: Dict[str, Any] = {
        "v": PROTOCOL_VERSION,
        "s": sequence,
        "k": "s",
        "t": snapshot.generated_at,
        "o": snapshot.utc_offset_minutes,
        "d": snapshot.tokens_today,
        "f": snapshot.calls_today,
        "z": snapshot.errors_today,
        "y": snapshot.success_permille,
        "l": snapshot.average_ttft_ms,
        "m": snapshot.latest_model[:23],
        "h": snapshot.latest_status,
        "n": snapshot.latest_latency_ms,
        "b": 1 if snapshot.latest_failed else 0,
        "c": len(snapshot.providers),
        "w": total_windows,
    }
    if snapshot.lowest_remaining_percent is not None:
        payload["r"] = snapshot.lowest_remaining_percent
    if snapshot.truncated:
        payload["tr"] = 1
        payload["to"] = snapshot.truncated_count
    return _encode(payload)


def encode_stats(snapshot: Snapshot, sequence: int) -> bytes:
    payload: Dict[str, Any] = {
        "v": PROTOCOL_VERSION,
        "s": sequence,
        "k": "x",
        "a": [
            snapshot.session_count,
            snapshot.total_calls,
            snapshot.total_input,
            snapshot.total_output,
            snapshot.total_cache_read,
            snapshot.total_reasoning,
            snapshot.total_cost_micros,
            snapshot.cache_hit_permille,
            snapshot.top_model[:23],
            snapshot.top_model_calls,
        ],
    }
    return _encode(payload)


def encode_provider(
    provider: ProviderSnapshot,
    provider_index: int,
    provider_count: int,
    sequence: int,
) -> bytes:
    payload: Dict[str, Any] = {
        "v": PROTOCOL_VERSION,
        "s": sequence,
        "k": "p",
        "i": provider_index,
        "c": provider_count,
        "id": provider.id,
        "m": provider.name,
    }
    if provider.read_at is not None:
        payload["ta"] = provider.read_at
    return _encode(payload)


def encode_window(
    window: WindowSnapshot,
    provider_index: int,
    window_index: int,
    window_count: int,
    sequence: int,
) -> bytes:
    payload: Dict[str, Any] = {
        "v": PROTOCOL_VERSION,
        "s": sequence,
        "k": "w",
        "p": provider_index,
        "i": window_index,
        "w": window_count,
        "id": window.id,
        "m": window.name,
        "ok": 1 if window.valid else 0,
    }
    if window.used_percent is not None:
        payload["u"] = window.used_percent
    if window.remaining_percent is not None:
        payload["r"] = window.remaining_percent
    if window.resets_at is not None:
        payload["ra"] = window.resets_at
    if window.unlimited:
        payload["ul"] = 1
    return _encode(payload)


def encode_end(
    provider_count: int,
    window_count: int,
    truncated: bool,
    truncated_count: int,
    sequence: int,
) -> bytes:
    payload: Dict[str, Any] = {
        "v": PROTOCOL_VERSION,
        "s": sequence,
        "k": "e",
        "c": provider_count,
        "w": window_count,
    }
    if truncated:
        payload["tr"] = 1
        payload["to"] = truncated_count
    return _encode(payload)


def _encode(payload: Dict[str, Any]) -> bytes:
    encoded = json.dumps(
        payload, separators=(",", ":"), ensure_ascii=False
    ).encode("utf-8")
    if len(encoded) > MAX_MESSAGE_BYTES:
        raise ValueError(
            f"状态消息超过 {MAX_MESSAGE_BYTES} 字节 BLE 预算"
        )
    return encoded


def decode_command(data: bytes) -> Dict[str, Any]:
    try:
        command = json.loads(data.decode("utf-8"))
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise ValueError("无效的命令 JSON") from error

    if command.get("v") != PROTOCOL_VERSION:
        raise ValueError("不支持的协议版本")
    if not isinstance(command.get("sid"), int) or command["sid"] < 1:
        raise ValueError("无效的 session id")
    if not isinstance(command.get("id"), int) or command["id"] < 1:
        raise ValueError("无效的 request id")
    if command.get("a") not in ALLOWED_ACTIONS:
        raise ValueError("不允许的动作")
    return command


def encode_result(request_id: int, ok: bool, message: str) -> bytes:
    payload = {
        "v": PROTOCOL_VERSION,
        "id": request_id,
        "ok": 1 if ok else 0,
        "m": message[:48],
    }
    return json.dumps(payload, separators=(",", ":")).encode("utf-8")
