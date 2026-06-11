#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""
Shared board/target/core configuration mappings.
"""

import os

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

TARGET_BOARD_MAP = {
    "sample-app": "alif_e7_dk",
}

TARGET_TTY_MAP = {
    "sample-app": "ttyACM0",
}

TARGET_CONSOLE_MAP = {
    "sample-app": {"he_app": "ttyACM1", "hp_app": "ttyACM1"},
}

SOC_QUALIFIER = "ae722f80f55d5xx"

CORE_CONFIG = {
    "he_app": {"core_suffix": "rtss_he"},
    "hp_app": {"core_suffix": "rtss_hp"},
}


def get_app_path(target: str, core: str) -> str:
    """Relative path to the app source directory for a given core."""
    return f"project/apps/{core}"


def get_build_dir(target: str, core: str) -> str:
    """Relative path to the west build directory for a given core."""
    return f"build/apps/{core}"


BOARD_DEVICE_CONFIG = {
    "alif_e7_dk": "project/boards/toc-configs/e7_he_hp.json",
}

TOOL_CONFIG = {}


def get_device_config_path(target: str) -> tuple[str, str]:
    """Get device config path and filename for a target.

    Returns:
        Tuple of (absolute_path, filename)
    """
    import sys
    board = get_board_name(target)
    rel_path = BOARD_DEVICE_CONFIG.get(board)
    if not rel_path:
        print(f"Error: No device config for board '{board}'", file=sys.stderr)
        sys.exit(1)
    abs_path = os.path.join(REPO_ROOT, rel_path)
    return abs_path, os.path.basename(rel_path)


def get_board_name(target: str) -> str:
    """Map target name to board name. Exits on unknown target."""
    import sys
    board = TARGET_BOARD_MAP.get(target)
    if not board:
        print(f"Error: Unknown target '{target}'. Valid: {list(TARGET_BOARD_MAP.keys())}", file=sys.stderr)
        sys.exit(1)
    return board


def get_full_board(target: str, core: str) -> str:
    """Build full board qualifier: board/soc/core_suffix."""
    board = get_board_name(target)
    core_cfg = CORE_CONFIG.get(core)
    if not core_cfg:
        import sys
        print(f"Error: Unknown core '{core}'. Valid: {list(CORE_CONFIG.keys())}", file=sys.stderr)
        sys.exit(1)
    return f"{board}/{SOC_QUALIFIER}/{core_cfg['core_suffix']}"
