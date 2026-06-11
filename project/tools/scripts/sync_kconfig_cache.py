#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""Keep the cached Zephyr .config in sync with the Kconfig sources.

Removing a `select` in a Kconfig file does not propagate on an incremental build:
Zephyr reuses the cached build/<...>/zephyr/.config, and a prompt symbol keeps its
previous =y value. The only reliable fixes are a full pristine rebuild or dropping
the cached .config so Zephyr regenerates it from scratch.

This helper drops a build .config only when a Kconfig file is newer than it, so the
resync happens exactly once after an edit and normal C++ builds keep their fast
incremental path.
"""

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# Where build .config files live, and where the Kconfig sources to watch are.
BUILD_ROOT = REPO_ROOT / "build" / "apps"
KCONFIG_ROOT = REPO_ROOT / "project"


def newest_kconfig_mtime() -> float:
    """Most recent mtime among all Kconfig* files under KCONFIG_ROOT (0.0 if none)."""
    latest = 0.0
    for path in KCONFIG_ROOT.rglob("Kconfig*"):
        if path.is_file():
            latest = max(latest, path.stat().st_mtime)
    return latest


def main() -> int:
    if not BUILD_ROOT.exists():
        # Nothing built yet: the next build is a clean configure anyway.
        return 0

    kconfig_mtime = newest_kconfig_mtime()
    removed = []
    for config in BUILD_ROOT.rglob(".config"):
        # zephyr/.config is the generated one; skip anything else just in case.
        if config.parent.name != "zephyr":
            continue
        if kconfig_mtime > config.stat().st_mtime:
            config.unlink()
            removed.append(config.relative_to(REPO_ROOT))

    if removed:
        print("Kconfig changed since last build, resyncing cached .config:")
        for path in removed:
            print(f"  - {path}")
    else:
        print("Cached .config already in sync with Kconfig, nothing to do.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
