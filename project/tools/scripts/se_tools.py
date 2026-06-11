#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""
Shared helpers for Alif SE-Tools flash operations.
"""

import os
import subprocess
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))


def get_se_tools_dir() -> str:
    se_tools = os.environ.get("ALIF_SE_TOOLS_DIR") or os.environ.get("ALIF_SE_TOOLS")
    if not se_tools:
        print("Error: ALIF_SE_TOOLS_DIR or ALIF_SE_TOOLS not set", file=sys.stderr)
        sys.exit(1)
    return se_tools


def run(cmd, **kwargs):
    print(f"  $ {' '.join(cmd)}")
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        print(f"Error: command failed (exit code {result.returncode})", file=sys.stderr)
        sys.exit(1)


def tools_config():
    """Configure SE-Tools for Alif E7."""
    se_tools = get_se_tools_dir()
    run(["sudo", os.path.join(se_tools, "tools-config"),
         "-p", "E7 (AE722F80F55D5LS) - 5.5 MRAM / 13.5 SRAM", "-r", "B4"],
        cwd=se_tools)


def gen_toc(conductor_path: str):
    """Generate TOC from conductor.alif."""
    se_tools = get_se_tools_dir()
    run(["sudo", os.path.join(se_tools, "app-gen-toc"), "-f", conductor_path], cwd=se_tools)


def write_mram():
    """Write MRAM via SE-Tools."""
    se_tools = get_se_tools_dir()
    run(["sudo", os.path.join(se_tools, "app-write-mram"), "-p"], cwd=se_tools)
