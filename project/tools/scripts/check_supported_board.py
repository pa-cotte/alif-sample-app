#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved
"""
Validate that the Zephyr board passed by west is in the app's `app.yml`.

Called from each app's CMakeLists.txt:

    python3 check_supported_board.py <app.yml> <core> <BOARD/QUALIFIERS>

Exits 0 when the board is listed under <core>.boards, 1 otherwise.
"""

import sys
from pathlib import Path

try:
    import yaml
except ImportError:
    print("Error: PyYAML is required (pip install pyyaml)", file=sys.stderr)
    sys.exit(1)


def main() -> int:
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <app.yml> <core> <board[/qualifiers]>", file=sys.stderr)
        return 2

    app_file = Path(sys.argv[1])
    core = sys.argv[2].strip()
    requested = sys.argv[3].strip().strip("/")

    if not app_file.is_file():
        print(f"Error: app.yml not found: {app_file}", file=sys.stderr)
        return 1

    with app_file.open("r", encoding="utf-8") as f:
        data = yaml.safe_load(f) or {}

    core_section = data.get(core)
    if not isinstance(core_section, dict):
        print(f"Error: core '{core}' not declared in {app_file}", file=sys.stderr)
        return 1

    boards = core_section.get("boards") or []
    if not isinstance(boards, list):
        print(f"Error: '{app_file}' must declare a list '{core}.boards'", file=sys.stderr)
        return 1

    supported = [b.strip() for b in boards if isinstance(b, str)]
    if requested not in supported:
        listing = "\n  - ".join(supported)
        print(
            f"Error: board '{requested}' not supported by core '{core}' of app "
            f"at {app_file.parent}.\nSupported boards:\n  - {listing}",
            file=sys.stderr,
        )
        return 1

    return 0


if __name__ == "__main__":
    sys.exit(main())
