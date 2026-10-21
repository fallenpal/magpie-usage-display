import asyncio
import hashlib
import logging
import math
import re
import time
from dataclasses import dataclass, replace
from datetime import datetime, timezone
from typing import Any, Dict, Optional, Tuple

logger = logging.getLogger(__name__)

MAX_PROVIDERS = 8
MAX_WINDOWS_PER_PROVIDER = 4
MAX_TOTAL_WINDOWS = 16

PROVIDER_PRIORITY = {"codex": 0, "claude": 1}
ACCOUNT_KEYS = (
    "accountId",
    "account_id",
    "account",
    "email",
    "userId",
    "user_id",
)


@dataclass(frozen=True)
class WindowSnapshot:
    id: str
    name: str
    used_percent: Optional[int]
    remaining_percent: Optional[int]
    resets_at: Optional[int]
    unlimited: bool
    valid: bool


@dataclass(frozen=True)
class ProviderSnapshot:
    id: str
    name: str
    read_at: Optional[int]
    windows: Tuple[WindowSnapshot, ...]


@dataclass(frozen=True)
class Snapshot:
    generated_at: int
    utc_offset_minutes: int
    providers: Tuple[ProviderSnapshot, ...]
    lowest_remaining_percent: Optional[int]
    truncated: bool
    truncated_count: int
    tokens_today: int
    tokens_7d: int
    errors_today: int
    calls_today: int
    success_permille: int
    average_ttft_ms: int
    latest_model: str
    latest_status: int
    latest_latency_ms: int
    latest_failed: bool
    session_count: int
    total_calls: int
    total_input: int
    total_output: int
    total_cache_read: int
    total_reasoning: int
    total_cost_micros: int
    cache_hit_permille: int
    top_model: str
    top_model_calls: int


def build_snapshot(
    quotas_result: Dict[str, Any],
    usage_result: Dict[str, Any],
    requests_result: Optional[Dict[str, Any]] = None,
    now: Optional[int] = None,
    utc_offset_minutes: Optional[int] = None,
) -> Snapshot:
    now = int(time.time()) if now is None else now
    if utc_offset_minutes is None:
        utc_offset_minutes = int(datetime_offset_seconds(now) // 60)

    providers, truncated, truncated_count = _build_providers(
        quotas_result.get("data") or []
    )
    lowest_remaining = _lowest_remaining(providers)

    series = usage_result.get("series") or []
    today = series[-1] if series else {}
    recent = series[-7:]
    tokens_today = _tokens(today)
    tokens_7d = sum(_tokens(bucket) for bucket in recent)
    calls_today = max(0, int(today.get("calls") or 0))
    errors_today = min(calls_today, max(0, int(today.get("errors") or 0)))
    success_permille = (
        round((calls_today - errors_today) * 1000 / calls_today)
        if calls_today
        else 1000
    )
    timed_today = max(0, int(today.get("timed") or 0))
    average_ttft_ms = (
        max(0, int(today.get("ttft_ms") or 0)) // timed_today
        if timed_today
        else 0
    )
    latest = ((requests_result or {}).get("rows") or [{}])[0]
    latest_model = _truncate_utf8(
        str(latest.get("served") or latest.get("model") or ""), 23
    )
    latest_status = max(0, int(latest.get("status") or 0))
    total_input = max(0, int(usage_result.get("input") or 0))
    total_output = max(0, int(usage_result.get("output") or 0))
    total_cache_read = max(0, int(usage_result.get("cache_read") or 0))
    total_reasoning = max(0, int(usage_result.get("reasoning") or 0))
    cache_base = total_input + total_cache_read
    cache_hit_permille = (
        round(total_cache_read * 1000 / cache_base) if cache_base else 0
    )
    models = usage_result.get("models") or []
    top_model_row = max(
        models,
        key=lambda item: max(0, int(item.get("calls") or 0)),
        default={},
    )

    return Snapshot(
        generated_at=now,
        utc_offset_minutes=utc_offset_minutes,
        providers=providers,
        lowest_remaining_percent=lowest_remaining,
        truncated=truncated,
        truncated_count=truncated_count,
        tokens_today=tokens_today,
        tokens_7d=tokens_7d,
        errors_today=errors_today,
        calls_today=calls_today,
        success_permille=success_permille,
        average_ttft_ms=average_ttft_ms,
        latest_model=latest_model,
        latest_status=latest_status,
        latest_latency_ms=max(0, int(latest.get("ms") or 0)),
        latest_failed=bool(
            latest.get("err")
            or latest.get("error")
            or latest_status < 200
            or latest_status >= 400
        ),
        session_count=max(0, int(usage_result.get("sessionCount") or 0)),
        total_calls=max(0, int(usage_result.get("calls") or 0)),
        total_input=total_input,
        total_output=total_output,
        total_cache_read=total_cache_read,
        total_reasoning=total_reasoning,
        total_cost_micros=max(
            0, round(float(usage_result.get("cost") or 0) * 1_000_000)
        ),
        cache_hit_permille=cache_hit_permille,
        top_model=_truncate_utf8(
            str(
                top_model_row.get("name")
                or top_model_row.get("model")
                or ""
            ),
            23,
        ),
        top_model_calls=max(0, int(top_model_row.get("calls") or 0)),
    )


def _build_providers(
    rows: Any,
) -> Tuple[Tuple[ProviderSnapshot, ...], bool, int]:
    providers = []
    for source_index, row in enumerate(rows):
        if not isinstance(row, dict):
            continue
        provider_key = _clean_text(
            row.get("provider") or row.get("name") or "unknown", 23
        )
        display_name = _clean_text(
            row.get("name") or row.get("provider") or "Unknown", 23
        )
        windows = _build_windows(
            provider_key, row.get("windows") or []
        )
        providers.append(
            (
                provider_key,
                source_index,
                row,
                ProviderSnapshot(
                    id="",
                    name=display_name,
                    read_at=_iso_epoch(row.get("readAt")),
                    windows=windows,
                ),
            )
        )

    providers.sort(
        key=lambda item: (
            PROVIDER_PRIORITY.get(item[0].lower(), 99),
            item[1],
        )
    )
    ordered = [
        replace(provider, id=_provider_id(provider_key, row))
        for provider_key, _, row, provider in providers
    ]
    ordered = _unique_provider_ids(ordered)
    return _apply_limits(tuple(ordered))


def _provider_id(provider_key: str, row: Dict[str, Any]) -> str:
    base = _slug(provider_key, 15) or "provider"
    account = _account_identifier(row)
    if account is not None:
        digest = hashlib.sha256(account.encode("utf-8")).hexdigest()[:6]
        return _truncate_utf8(f"{base}-{digest}", 23)
    return _truncate_utf8(base, 23)


def _unique_provider_ids(
    providers: list[ProviderSnapshot],
) -> list[ProviderSnapshot]:
    counts: Dict[str, int] = {}
    for provider in providers:
        counts[provider.id] = counts.get(provider.id, 0) + 1
    used: Dict[str, int] = {}
    result = []
    for index, provider in enumerate(providers):
        if counts[provider.id] > 1:
            used[provider.id] = used.get(provider.id, 0) + 1
            suffix = f"-{used[provider.id]}"
            base = provider.id[: 23 - len(suffix)]
            result.append(replace(provider, id=base + suffix))
        else:
            result.append(provider)
    return result


def _account_identifier(row: Dict[str, Any]) -> Optional[str]:
    for key in ACCOUNT_KEYS:
        value = row.get(key)
        if isinstance(value, (str, int)) and str(value).strip():
            return str(value).strip()
    return None


def _build_windows(
    provider_key: str, rows: Any
) -> Tuple[WindowSnapshot, ...]:
    windows = []
    for source_index, row in enumerate(rows):
        if not isinstance(row, dict):
            continue
        name = _clean_text(row.get("name") or "Window", 23)
        unlimited = bool(row.get("unlimited"))
        used = _optional_percent(row.get("used"))
        remaining = _optional_percent(row.get("remaining"))
        if used is None and remaining is not None:
            used = 100 - remaining
        elif remaining is None and used is not None:
            remaining = 100 - used
        elif (
            used is not None
            and remaining is not None
            and used + remaining != 100
        ):
            logger.warning(
                "quota window conflict provider=%s window=%s "
                "used=%s remaining=%s; using used",
                provider_key,
                name,
                used,
                remaining,
            )
            remaining = 100 - used
        valid = unlimited or used is not None or remaining is not None
        windows.append(
            WindowSnapshot(
                id="",
                name=name,
                used_percent=used,
                remaining_percent=remaining,
                resets_at=_iso_epoch(row.get("resetsAt")),
                unlimited=unlimited,
                valid=valid,
            )
        )

    windows.sort(key=_window_sort_key)
    ordered = [
        replace(window, id=_window_id(window, index))
        for index, window in enumerate(windows)
    ]
    return tuple(_unique_window_ids(ordered))


def _window_id(window: WindowSnapshot, index: int) -> str:
    base = _slug(window.name, 15) or f"window-{index + 1}"
    return _truncate_utf8(base, 15)


def _unique_window_ids(
    windows: list[WindowSnapshot],
) -> list[WindowSnapshot]:
    counts: Dict[str, int] = {}
    for window in windows:
        counts[window.id] = counts.get(window.id, 0) + 1
    used: Dict[str, int] = {}
    result = []
    for window in windows:
        if counts[window.id] > 1:
            used[window.id] = used.get(window.id, 0) + 1
            suffix = f"-{used[window.id]}"
            base = window.id[: 15 - len(suffix)]
            result.append(replace(window, id=base + suffix))
        else:
            result.append(window)
    return result


def _window_sort_key(window: WindowSnapshot) -> tuple:
    if (
        window.valid
        and not window.unlimited
        and window.remaining_percent is not None
    ):
        return (
            0,
            window.remaining_percent,
            window.resets_at or 2**63,
            window.name,
        )
    if window.valid and not window.unlimited:
        return (1, window.resets_at or 2**63, window.name)
    if window.unlimited:
        return (2, window.name)
    return (3, window.name)


def _apply_limits(
    providers: Tuple[ProviderSnapshot, ...],
) -> Tuple[Tuple[ProviderSnapshot, ...], bool, int]:
    truncated = False
    omitted = 0
    if len(providers) > MAX_PROVIDERS:
        dropped = providers[MAX_PROVIDERS:]
        omitted += len(dropped) + sum(
            len(provider.windows) for provider in dropped
        )
        providers = providers[:MAX_PROVIDERS]
        truncated = True

    limited = []
    total_windows = 0
    for provider in providers:
        allowed = min(
            MAX_WINDOWS_PER_PROVIDER,
            MAX_TOTAL_WINDOWS - total_windows,
        )
        windows = provider.windows
        if len(windows) > allowed and allowed == 0:
            omitted += len(provider.windows) + 1
            truncated = True
            continue
        if len(windows) > allowed:
            omitted += len(windows) - allowed
            windows = windows[:allowed]
            truncated = True
        limited.append(replace(provider, windows=windows))
        total_windows += len(windows)

    return tuple(limited), truncated, omitted


def _lowest_remaining(
    providers: Tuple[ProviderSnapshot, ...],
) -> Optional[int]:
    values = [
        window.remaining_percent
        for provider in providers
        for window in provider.windows
        if window.valid
        and not window.unlimited
        and window.remaining_percent is not None
    ]
    return min(values) if values else None


def _optional_percent(value: Any) -> Optional[int]:
    if value is None or isinstance(value, bool):
        return None
    try:
        number = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(number) or number < 0 or number > 100:
        return None
    return int(round(number))


def _tokens(bucket: Dict[str, Any]) -> int:
    return sum(
        max(0, int(bucket.get(key) or 0))
        for key in ("input", "output", "reasoning", "cache_read", "cache_write")
    )


def _iso_epoch(value: Any) -> Optional[int]:
    if not isinstance(value, str) or not value:
        return None
    try:
        parsed = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except (TypeError, ValueError):
        return None
    if parsed.tzinfo is None:
        parsed = parsed.replace(tzinfo=timezone.utc)
    return int(parsed.timestamp())


def _clean_text(value: Any, max_bytes: int) -> str:
    text = " ".join(str(value or "").split())
    text = _truncate_utf8(text, max_bytes)
    return text or "-"


def _truncate_utf8(text: str, max_bytes: int) -> str:
    encoded = text.encode("utf-8")
    if len(encoded) <= max_bytes:
        return text
    encoded = encoded[:max_bytes]
    while encoded:
        try:
            return encoded.decode("utf-8")
        except UnicodeDecodeError:
            encoded = encoded[:-1]
    return ""


def _slug(value: Any, max_length: int) -> str:
    text = str(value or "").strip().lower()
    text = re.sub(r"[^a-z0-9]+", "-", text).strip("-")
    if not text:
        return ""
    return text[:max_length].strip("-")


def datetime_offset_seconds(now: Optional[int] = None) -> int:
    now = int(time.time()) if now is None else now
    local = time.localtime(now)
    if local.tm_isdst > 0 and time.daylight:
        return -time.altzone
    return -time.timezone


async def collect_snapshot(client: Any) -> Snapshot:
    await client.health()
    quotas, usage, requests = await asyncio.gather(
        client.get("/v1/magpie/quotas"),
        client.get("/v1/magpie/usage"),
        client.get("/v1/magpie/usage/requests?limit=1"),
    )
    return build_snapshot(quotas, usage, requests)
