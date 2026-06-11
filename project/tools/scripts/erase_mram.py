#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

import argparse
import json
import os
import subprocess
import sys

from get_config import VALID_MODES, config_file, load_config
from usb_serial import ensure_usb_serial_devices


def save_config(config: dict, mode: str) -> None:
    path = config_file(mode)
    with open(path, "w") as f:
        json.dump(config, f, indent=4)
        f.write("\n")


def get_se_tools_dir() -> str:
    path = os.environ.get("ALIF_SE_TOOLS_DIR") or os.environ.get("ALIF_SE_TOOLS")
    if not path:
        print("Error: ALIF_SE_TOOLS_DIR or ALIF_SE_TOOLS not set", file=sys.stderr)
        sys.exit(1)
    return path


def main():
    parser = argparse.ArgumentParser(description="Erase MRAM")
    parser.add_argument("-m", "--mode", default="apps", choices=VALID_MODES, help="Config mode (apps or tests)")
    args = parser.parse_args()

    config = load_config(args.mode)
    erase = config.get("erase_mram", False)

    if not erase or erase == "failed":
        print("Erase MRAM skipped")
        return

    if config.get("target") == "native_sim":
        print("Erase MRAM skipped (native_sim)")
        return

    ensure_usb_serial_devices()
    se_tools = get_se_tools_dir()
    cmd = [os.path.join(se_tools, "app-write-mram"), "-e", "APP"]

    print("Erasing MRAM...")
    result = subprocess.run(cmd, cwd=se_tools)

    if result.returncode != 0:
        print("Error: Erase MRAM failed", file=sys.stderr)
        config["erase_mram"] = "failed"
        save_config(config, args.mode)
        sys.exit(1)

    print("Erase MRAM completed")


if __name__ == "__main__":
    main()
