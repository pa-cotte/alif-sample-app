#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""Update the west workspace and apply patches.

What this script does (in order):

  1. Run `west update` (shallow fetch when possible)
  2. Reset all west project trees to a clean state before patching.
     Required so that running the script twice does not fail with
     "patch already applied" on a workspace that was previously patched.
  3. Apply project patches via `west patch apply`
     (reads project/zephyr/patches.yml, applies to zephyr / hal_infineon).
  4. Fetch HAL Infineon BLE firmware blobs (`west blobs fetch hal_infineon`).
     Required for CYW55513 / Murata 2FY BLE support: Alif v2.3.0 ships
     WiFi only; the BLE firmware HCD files must be fetched separately.

Usage:
    ./tools/scripts/update.py               # update + patch + blobs
    ./tools/scripts/update.py --no-blobs    # skip blob fetch (faster, no BLE)
    ./tools/scripts/update.py --no-patch    # west update only
"""

import argparse
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]


def run(cmd: list[str], check: bool = True, **kwargs) -> subprocess.CompletedProcess:
    print(f"$ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run(cmd, check=check, cwd=REPO_ROOT, **kwargs)


def reset_modules_to_clean_state() -> None:
    """Discard working-tree changes (including previously applied patches).

    `west patch apply` modifies tracked files in zephyr / hal_infineon.
    Re-running update.py without this reset would fail with "patch already
    applied". `clean -fd` (no `-x`) spares gitignored files such as cached
    blobs downloaded by `west blobs fetch`.
    """
    run(
        ["west", "forall", "-c", "git reset --hard HEAD && git clean -fd"],
        check=False,
    )


def west_update() -> None:
    run(["west", "update", "--fetch-opt=--filter=tree:0", "--fetch-opt=--depth=1"])


def apply_patches() -> None:
    # west patch defaults: manifest_dir/zephyr/patches.yml + manifest_dir/zephyr/patches/
    # manifest_dir = project/ (the manifest project root) — matches our layout exactly.
    run(["west", "patch", "apply"])


def fetch_blobs() -> None:
    run(["west", "blobs", "fetch", "hal_infineon"])


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "--no-patch",
        action="store_true",
        help="Skip patch application (west update only)",
    )
    parser.add_argument(
        "--no-blobs",
        action="store_true",
        help="Skip blob fetch (faster, BLE firmware will be missing)",
    )
    args = parser.parse_args()

    west_update()

    if not args.no_patch:
        reset_modules_to_clean_state()
        apply_patches()

    if not args.no_blobs and not args.no_patch:
        fetch_blobs()

    return 0


if __name__ == "__main__":
    sys.exit(main())
