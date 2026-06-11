#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

import argparse
import json
import os
import sys

from get_config import VALID_MODES, config_file


def load_config(mode: str = "apps") -> dict:
    path = config_file(mode)
    if os.path.exists(path):
        with open(path, "r") as f:
            return json.load(f)
    return {}


def save_config(config: dict, mode: str = "apps") -> None:
    path = config_file(mode)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        json.dump(config, f, indent=4)
        f.write("\n")


def set_nested(config: dict, keys: list[str], value):
    current = config
    for key in keys[:-1]:
        if key not in current or not isinstance(current[key], dict):
            current[key] = {}
        current = current[key]
    current[keys[-1]] = value


def parse_value(raw: str):
    """Try to interpret the value as JSON (bool, int, float, list, etc.), fallback to string."""
    try:
        return json.loads(raw)
    except (json.JSONDecodeError, ValueError):
        # Handle [he,hp] -> ["he", "hp"] (shell strips inner quotes)
        if raw.startswith("[") and raw.endswith("]"):
            return [item.strip() for item in raw[1:-1].split(",") if item.strip()]
        return raw


def main():
    parser = argparse.ArgumentParser(description="Set a value in config.json")
    parser.add_argument("-m", "--mode", default="apps", choices=VALID_MODES, help="Config mode (apps or tests)")
    parser.add_argument("-d", "--delete", action="store_true", help="Delete the config.json file")
    parser.add_argument("-k", "--key", help="Key path separated by '/' (e.g. toto/titi/myvalue)")
    parser.add_argument("-v", "--value", help="Value to set (auto-detected as JSON type, fallback to string)")
    args = parser.parse_args()

    if args.delete:
        path = config_file(args.mode)
        if os.path.exists(path):
            os.remove(path)
            print(f"Deleted {path}")
        else:
            print(f"No config to delete for mode '{args.mode}'")
        return

    if not args.key or args.value is None:
        parser.error("-k/--key and -v/--value are required when not using -d/--delete")

    keys = [k for k in args.key.split("/") if k]
    if not keys:
        print("Error: key cannot be empty", file=sys.stderr)
        sys.exit(1)

    config = load_config(args.mode)
    value = parse_value(args.value)
    set_nested(config, keys, value)

    # Auto-derive bootloader from cores: 1 core = no mcuboot, 2 cores = mcuboot
    if args.key == "cores" and isinstance(value, list):
        mcuboot = len(value) > 1
        set_nested(config, ["bootloader"], mcuboot)
        print(f"bootloader = {json.dumps(mcuboot)} (auto)")

    save_config(config, args.mode)

    print(f"{args.key} = {json.dumps(value)}")


if __name__ == "__main__":
    main()
