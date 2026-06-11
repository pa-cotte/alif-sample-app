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
  4. Fetch CYW55513 BLE firmware blobs directly from Infineon GitHub.
     hal_infineon is embedded in the zephyr_alif fork (not a separate west
     module), so `west blobs fetch hal_infineon` is unavailable. Blobs are
     downloaded to zephyr/modules/hal_infineon/zephyr/blobs/... and their
     SHA256 checksums are verified before accepting them.

Usage:
    ./tools/scripts/update.py               # update + patch + blobs
    ./tools/scripts/update.py --no-blobs    # skip blob fetch (faster, no BLE)
    ./tools/scripts/update.py --no-patch    # west update only
"""

import argparse
import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]

# BLE firmware blobs for CYW55513 / Murata 2FY.
# Paths are relative to zephyr/modules/hal_infineon/zephyr/blobs/.
# URLs and SHA256 mirror the entries added by cyw55513-firmware-blobs.patch.
_BLE_BLOBS = [
    {
        "path": "img/bluetooth/firmware/COMPONENT_CYW55513/COMPONENT_BTFW/bt_firmware.hcd",
        "url": "https://github.com/Infineon/bt-fw-ifx-cyw55500a1/raw/release-v2.2.0/COMPONENT_wlbga_iPA_sLNA_ANT0_LHL_XTAL_IN/btfw.hcd",
        "sha256": "248d39e6de3a50123dcd37769c0ba5d093eb5a1907a30d0c34011d5d1c517ff6",
        "description": "CYW55513 discrete BT firmware v2.2.0.2580",
    },
    {
        "path": "img/bluetooth/firmware/COMPONENT_CYW55513/COMPONENT_BTFW_MURATA-2FY/bt_firmware.hcd",
        "url": "https://github.com/Infineon/bt-fw-mur-cyw55513/raw/release-v1.0.0/COMPONENT_MURATA-2FY/btfw.hcd",
        "sha256": "1653cb542e6be9c45555e79d63eb281837abff039ee8d9d29ea8729c3a511cec",
        "description": "CYW55513 Murata 2FY BT firmware v1.0.0.8",
    },
]


def run(cmd: list[str], check: bool = True, **kwargs) -> subprocess.CompletedProcess:
    print(f"$ {' '.join(str(c) for c in cmd)}", flush=True)
    return subprocess.run(cmd, check=check, cwd=REPO_ROOT, **kwargs)


def reset_modules_to_clean_state() -> None:
    """Discard working-tree changes (including previously applied patches).

    `west patch apply` modifies tracked files in zephyr / hal_infineon.
    Re-running update.py without this reset would fail with "patch already
    applied". `clean -fd` (no `-x`) spares untracked blob files downloaded
    here — they live outside the git index and would be silently kept.
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
    """Download CYW55513 BLE firmware blobs to the embedded hal_infineon tree.

    hal_infineon is bundled inside the zephyr_alif fork at
    zephyr/modules/hal_infineon/, not as a standalone west project.
    `west blobs fetch hal_infineon` is therefore unavailable; blobs are
    downloaded directly from Infineon GitHub and placed where
    btstack-integration/CMakeLists.txt expects them:
      ${ZEPHYR_HAL_INFINEON_MODULE_DIR}/zephyr/blobs/<path>
    """
    blobs_root = REPO_ROOT / "zephyr" / "modules" / "hal_infineon" / "zephyr" / "blobs"

    for blob in _BLE_BLOBS:
        dest = blobs_root / blob["path"]

        if dest.exists():
            digest = hashlib.sha256(dest.read_bytes()).hexdigest()
            if digest == blob["sha256"]:
                print(f"blob already present and verified: {blob['path']}", flush=True)
                continue
            print(f"checksum mismatch for {blob['path']}, re-downloading", flush=True)

        print(f"downloading {blob['description']} ...", flush=True)
        print(f"  url : {blob['url']}", flush=True)
        print(f"  dest: {dest}", flush=True)

        dest.parent.mkdir(parents=True, exist_ok=True)
        urllib.request.urlretrieve(blob["url"], dest)

        digest = hashlib.sha256(dest.read_bytes()).hexdigest()
        if digest != blob["sha256"]:
            dest.unlink(missing_ok=True)
            raise RuntimeError(
                f"SHA256 mismatch for {blob['path']}\n"
                f"  expected: {blob['sha256']}\n"
                f"  got     : {digest}"
            )

        print(f"  OK ({digest[:16]}...)", flush=True)

    print(f"BLE blobs ready in {blobs_root}", flush=True)


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
