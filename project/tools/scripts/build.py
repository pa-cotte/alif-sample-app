#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

import argparse
import os
import re
import subprocess
import sys

from board_config import CORE_CONFIG, get_app_path, get_build_dir, get_full_board
from get_config import load_config

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# Fields of the Zephyr VERSION file overwritten from a release tag.
# VERSION_TWEAK is intentionally left untouched.
_VERSION_FIELDS = ("VERSION_MAJOR", "VERSION_MINOR", "PATCHLEVEL", "EXTRAVERSION")


def parse_release_tag(tag: str) -> dict[str, str]:
    """Parse a release tag like `v1.2.3` or `v1.2.3-RC1` into VERSION fields.

    Mirrors the previous shell logic: strip a leading `v`, split the numeric
    core from an optional suffix on the first `-`, and default EXTRAVERSION to
    `stable` when no suffix is present.
    """
    body = tag[1:] if tag.startswith("v") else tag
    numeric, _, suffix = body.partition("-")
    parts = numeric.split(".")
    if len(parts) != 3 or not all(p.isdigit() for p in parts):
        raise ValueError(f"Invalid release tag '{tag}': expected v<major>.<minor>.<patch>[-suffix]")
    major, minor, patchlevel = parts
    return {
        "VERSION_MAJOR": major,
        "VERSION_MINOR": minor,
        "PATCHLEVEL": patchlevel,
        "EXTRAVERSION": suffix or "stable",
    }


def _rewrite_version_file(path: str, fields: dict[str, str]) -> None:
    """Overwrite the given VERSION fields in-place, preserving every other line."""
    with open(path, "r", encoding="utf-8") as f:
        lines = f.readlines()

    out = []
    for line in lines:
        for key in _VERSION_FIELDS:
            if re.match(rf"^{key}\s*=", line):
                line = f"{key} = {fields[key]}\n"
                break
        out.append(line)

    with open(path, "w", encoding="utf-8") as f:
        f.writelines(out)


def bump_version(target: str, cores: list[str], tag: str) -> None:
    """Stamp the release tag into the VERSION file of each built (target, core)."""
    fields = parse_release_tag(tag)
    for core in cores:
        version_file = os.path.join(REPO_ROOT, get_app_path(target, core), "VERSION")
        if not os.path.isfile(version_file):
            raise FileNotFoundError(f"VERSION file not found: {version_file}")
        _rewrite_version_file(version_file, fields)
        print(f"Stamped {tag} -> {os.path.relpath(version_file, REPO_ROOT)} "
              f"({fields['VERSION_MAJOR']}.{fields['VERSION_MINOR']}.{fields['PATCHLEVEL']}-{fields['EXTRAVERSION']})")


def build_core(core: str, target: str, mcuboot: bool, snippets: list[str] | None = None, pristine: bool = False) -> int:
    core_cfg = CORE_CONFIG.get(core)
    if not core_cfg:
        print(f"Error: Unknown core '{core}'", file=sys.stderr)
        return 1

    board = get_full_board(target, core)

    cmd = [
        "west", "build", "-p", "always" if pristine else "auto",
        "-b", board,
        get_app_path(target, core),
        "-d", get_build_dir(target, core),
    ]

    if mcuboot:
        # Both cores use sysbuild with MCUboot:
        # - he_app MCUboot handles upgrade/downgrade for both images
        # - hp_app MCUboot only validates and boots the HP app
        cmd.append("--sysbuild")

    for snippet in (snippets or []):
        cmd.extend(["-S", snippet])

    if mcuboot:
        # SB_CONFIG_BOOTLOADER_MCUBOOT / SB_CONFIG_MCUBOOT_MODE_SWAP_USING_MOVE
        # cannot be set via `default` Kconfig without recursive dependencies;
        # explicitly load the shared fragment.
        sb_extra = os.path.join(REPO_ROOT, "project", "apps", "shared", "sysbuild", f"{core}.conf")
        cmd.extend(["--", f"-DSB_EXTRA_CONF_FILE={sb_extra}"])

    # CI matrix opt-in: strip DWARF debug info to cut ~30% off compile + link
    # wall time. The PR build matrix discards ELFs (no `artifacts:` block in
    # the child pipeline), so debuggability is irrelevant. Release / nightly
    # do NOT set this env var and keep full DWARF for crash post-mortems.
    # In sysbuild, EXTRA_CFLAGS must be passed per sub-image — the app image
    # is named after `core`, MCUboot is the `mcuboot` image.
    if os.environ.get("BUILD_NO_DEBUG_INFO") == "1":
        if "--" not in cmd:
            cmd.append("--")
        cmd.extend([
            f"-D{core}_EXTRA_CFLAGS=-g0",
            f"-D{core}_EXTRA_CXXFLAGS=-g0",
            f"-D{core}_EXTRA_AFLAGS=-g0",
        ])
        if mcuboot:
            cmd.extend([
                "-Dmcuboot_EXTRA_CFLAGS=-g0",
                "-Dmcuboot_EXTRA_AFLAGS=-g0",
            ])

    label = f"{core.upper()} on {target} ({'with' if mcuboot else 'without'} MCUboot)"
    print(f"Building {label}...")
    print(f"  {' '.join(cmd)}")

    result = subprocess.run(cmd, cwd=REPO_ROOT)
    if result.returncode != 0:
        print(f"Error: Build failed for {core.upper()}", file=sys.stderr)
        return 1

    return 0


def main():
    parser = argparse.ArgumentParser(description="Build firmware")
    parser.add_argument("--pristine", action="store_true", help="Force a pristine (full) rebuild")
    parser.add_argument(
        "--release-tag",
        default="",
        help="Release tag (e.g. v1.2.3 or v1.2.3-RC1). When set, stamps the VERSION "
             "file of each built app/core before building. Empty = no version bump.",
    )
    args = parser.parse_args()

    config = load_config("apps")

    if config.get("erase_mram") == "failed":
        print("Error: Erase MRAM failed during configuration. Please re-configure.", file=sys.stderr)
        sys.exit(1)

    cores = config.get("cores")
    if not cores or not isinstance(cores, list):
        print("Error: 'cores' not set or not an array in .config.json", file=sys.stderr)
        sys.exit(1)

    target = config.get("target")
    if not target:
        print("Error: 'target' not set in .config.json", file=sys.stderr)
        sys.exit(1)

    if args.release_tag:
        try:
            bump_version(target, cores, args.release_tag)
        except (ValueError, FileNotFoundError) as exc:
            print(f"Error: {exc}", file=sys.stderr)
            sys.exit(1)

    mcuboot = config.get("bootloader", False)
    snippets_dir = os.path.join(REPO_ROOT, "project", "snippets", target)
    target_snippet = target if os.path.isdir(snippets_dir) else None
    extra_snippets = config.get("snippets", {}).get("extra", [])
    all_snippets = ([target_snippet] if target_snippet else []) + extra_snippets

    for core in cores:
        ret = build_core(core, target, mcuboot, all_snippets, pristine=args.pristine)
        if ret != 0:
            sys.exit(ret)

    print(f"\nBuild completed for: {', '.join(c.upper() for c in cores)} on {target}")


if __name__ == "__main__":
    main()
