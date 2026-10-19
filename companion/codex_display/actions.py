import asyncio
import platform
from typing import Awaitable, Callable, Tuple


async def _run(*args: str) -> Tuple[bool, str]:
    process = await asyncio.create_subprocess_exec(
        *args,
        stdout=asyncio.subprocess.DEVNULL,
        stderr=asyncio.subprocess.PIPE,
    )
    _, stderr = await process.communicate()
    if process.returncode == 0:
        return True, ""
    return False, stderr.decode("utf-8", errors="replace").strip()


async def perform_action(
    action: str, refresh: Callable[[], Awaitable[None]]
) -> Tuple[bool, str]:
    if action not in {"refresh", "focus_magpie"}:
        return False, "NOT ALLOWED"

    if action == "refresh":
        await refresh()
        return True, "UPDATED"

    if platform.system() != "Darwin":
        return False, "MACOS ONLY"

    if action == "focus_magpie":
        ok, _ = await _run("open", "-a", "magpie")
        return (True, "FOCUSED") if ok else (False, "APP NOT FOUND")

    raise AssertionError("allowlisted action was not handled")
