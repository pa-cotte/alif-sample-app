#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""
Generate JLink scripts based on .config.json.
- application.jlink : flashes all configured app binaries (he_app, hp_app, or both).

Note: system.jlink (first-flash TOC + MCUboots in dual core) is generated at flash time by
flash.py, because its TOC address depends on the runtime-generated
app-package-map.txt.
"""

import argparse
import os
import sys

from board_config import get_board_name
from get_config import VALID_MODES, config_dir, load_config

CORE_MAP = {
    "alif_e7_dk": {
        "he_app": {
            "device": "AE722F80F55D5_HE",
            "mram_no_boot": 0x80000000,
            "mram_slot0": 0x8000C000,
        },
        "hp_app": {
            "device": "AE722F80F55D5_HP",
            "mram_no_boot": 0x80292000,
            "mram_slot0": 0x8029E000,
        },
    },
}


def get_images_dir() -> str:
    se_tools = os.environ.get("ALIF_SE_TOOLS_DIR") or os.environ.get("ALIF_SE_TOOLS")
    if not se_tools:
        print("Error: ALIF_SE_TOOLS_DIR or ALIF_SE_TOOLS not set", file=sys.stderr)
        sys.exit(1)
    return os.path.join(se_tools, "build", "images")


def get_core_config(board: str, core: str) -> dict:
    """Get JLink config for a board/core pair."""
    board_cfg = CORE_MAP.get(board)
    if not board_cfg:
        print(f"Error: No JLink config for board '{board}'", file=sys.stderr)
        sys.exit(1)
    cfg = board_cfg.get(core)
    if not cfg:
        print(f"Error: No JLink config for core '{core}' on board '{board}'", file=sys.stderr)
        sys.exit(1)
    return cfg


def bin_path(images_dir: str, core: str, bootloader: bool) -> str:
    if bootloader:
        return os.path.join(images_dir, f"{core}.signed.bin")
    return os.path.join(images_dir, f"{core}.bin")


def generate_application_jlink(board: str, cores: list, bootloader: bool, images_dir: str, output_dir: str) -> str:
    """Generate a single application.jlink that flashes every configured core's app binary.

    The target device is intentionally NOT baked into the script: it is passed
    as a JLinkExe CLI argument (-device) so flash.py can retry on the alternate
    core when the active SWD interface is tied to the other core.
    """
    lines = [
        f"// Auto-generated JLink application script ({board}, cores={','.join(cores)})",
        "si SWD",
        "speed auto",
        "connect",
        "",
    ]

    for core in cores:
        cfg = get_core_config(board, core)
        path = bin_path(images_dir, core, bootloader)
        addr = cfg["mram_slot0"]
        lines += [
            f"// {core.upper()} ({'slot0' if bootloader else 'no bootloader'})",
            f"loadbin {path}, 0x{addr:08X}",
            f"verifybin {path}, 0x{addr:08X}",
            "",
        ]

    lines += ["r", "g", "", "exit", ""]

    output_path = os.path.join(output_dir, "application.jlink")
    with open(output_path, "w") as f:
        f.write("\n".join(lines))
    return output_path


def main():
    parser = argparse.ArgumentParser(description="Generate JLink flash script")
    parser.add_argument("-m", "--mode", default="apps", choices=VALID_MODES, help="Config mode (apps or tests)")
    args = parser.parse_args()

    config = load_config(args.mode)

    target = config.get("target")
    if target == "native_sim":
        print("JLink skipped (native_sim)")
        return

    cores = config.get("cores", [])
    if not cores:
        print("Error: 'cores' not set in config", file=sys.stderr)
        sys.exit(1)

    board = get_board_name(target)
    bootloader = config.get("bootloader", False)
    images_dir = get_images_dir()

    output_dir = config_dir(args.mode)
    os.makedirs(output_dir, exist_ok=True)

    output_path = generate_application_jlink(board, cores, bootloader, images_dir, output_dir)
    print(f"Generated {output_path} (board={board}, cores={cores}, bootloader={bootloader})")


if __name__ == "__main__":
    main()
