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
  4. Fetch CYW55513 BLE and CYW55500 WiFi firmware blobs directly from
     Infineon GitHub.  Both sets go to the standalone hal_infineon west module
     (modules/hal/infineon/zephyr/blobs/...) where ZEPHYR_HAL_INFINEON_MODULE_DIR
     resolves, and their SHA256 checksums are verified before accepting them.

Usage:
    ./tools/scripts/update.py               # update + patch + blobs
    ./tools/scripts/update.py --no-blobs    # skip blob fetch (faster, no BLE/WiFi)
    ./tools/scripts/update.py --no-patch    # west update only
"""

import argparse
import hashlib
import subprocess
import sys
import urllib.request
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[3]

# BLE firmware blobs for CYW55513 / Murata 2FY.
# Paths are relative to modules/hal/infineon/zephyr/blobs/.
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

# WiFi firmware blobs for CYW55500 / CYW955513SDM2WLIPA (Murata 2FY).
# Paths are relative to modules/hal/infineon/zephyr/blobs/.
# URLs and SHA256 sourced from modules/hal/infineon/zephyr/module.yml.
_WIFI_BLOBS = [
    {
        "path": "img/whd/resources/firmware/COMPONENT_55500/COMPONENT_SM/55500A1.trxcse",
        "url": "https://github.com/Infineon/whd-expansion/raw/release-v1.1.0/WHD/COMPONENT_WIFI6/resources/firmware/COMPONENT_55500/COMPONENT_SM/55500A1.trxcse",
        "sha256": "9e4b9e143d6abe58dc43ea003adf8a2668da3468859e4dc70fc330f7da418b74",
        "description": "CYW55500 Wi-Fi firmware v1.1.0",
    },
    {
        "path": "img/whd/resources/clm/COMPONENT_55500/COMPONENT_CYW955513SDM2WLIPA/55500A1.clm_blob",
        "url": "https://github.com/Infineon/wifi-resources/raw/release-v2.0.0/clm/COMPONENT_WIFI6/COMPONENT_55500/COMPONENT_CYW955513SDM2WLIPA/55500A1.clm_blob",
        "sha256": "1a761eeea3e9c779da87376cc1927b65f598f8e0d2514bd866d4df7cddb1d25c",
        "description": "CYW55500 Wi-Fi CLM for CYW955513SDM2WLIPA (Murata 2FY) v2.0.0",
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


def _fetch_blob_set(blobs: list[dict], blobs_root: Path, label: str) -> None:
    """Download a set of firmware blobs into blobs_root, verifying SHA256."""
    for blob in blobs:
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

    print(f"{label} blobs ready in {blobs_root}", flush=True)


def fetch_blobs() -> None:
    """Download BLE and WiFi firmware blobs from Infineon GitHub.

    BLE (CYW55513) and WiFi (CYW55500) blobs both go to the standalone
    hal_infineon west module, which is what ZEPHYR_HAL_INFINEON_MODULE_DIR
    resolves to when it is present in the manifest:
      modules/hal/infineon/zephyr/blobs/<path>
    """
    blobs_root = REPO_ROOT / "modules" / "hal" / "infineon" / "zephyr" / "blobs"
    _fetch_blob_set(_BLE_BLOBS, blobs_root, "BLE")
    _fetch_blob_set(_WIFI_BLOBS, blobs_root, "WiFi")


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
        help="Skip blob fetch (faster, BLE/WiFi firmware will be missing)",
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
