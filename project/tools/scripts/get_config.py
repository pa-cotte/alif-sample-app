#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

import argparse
import json
import os
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
VALID_MODES = ("apps", "tests", "tools")


def config_dir(mode: str = "apps") -> str:
    if mode not in VALID_MODES:
        print(f"Error: invalid mode '{mode}', must be one of {VALID_MODES}", file=sys.stderr)
        sys.exit(1)
    return os.path.join(REPO_ROOT, ".config", mode)


def config_file(mode: str = "apps") -> str:
    return os.path.join(config_dir(mode), "config.json")


def load_config(mode: str = "apps") -> dict:
    path = config_file(mode)
    if os.path.exists(path):
        with open(path, "r") as f:
            return json.load(f)
    return {}


def get_nested(config: dict, keys: list[str]):
    current = config
    for key in keys:
        if not isinstance(current, dict) or key not in current:
            return None
        current = current[key]
    return current


def main():
    parser = argparse.ArgumentParser(description="Get a value from config.json")
    parser.add_argument("-k", "--key", required=True, help="Key path separated by '/' (e.g. toto/titi/myvalue)")
    parser.add_argument("-m", "--mode", default="apps", choices=VALID_MODES, help="Config mode (apps or tests)")
    args = parser.parse_args()

    keys = [k for k in args.key.split("/") if k]
    if not keys:
        print("Error: key cannot be empty", file=sys.stderr)
        sys.exit(1)

    config = load_config(args.mode)
    value = get_nested(config, keys)

    if value is None:
        sys.exit(1)

    print(json.dumps(value))


if __name__ == "__main__":
    main()
