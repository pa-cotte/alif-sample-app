#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""
Flash firmware based on .config.json.

- "flash" == "se-tools": sudo app-write-mram -p
- "flash" == "jlink": JLinkExe
    - first flash (SE_ready=False): system.jlink (TOC + MCUboots in dual core) + application.jlink
    - subsequent flashes: application.jlink only

SE-Tools prep (copy_bin, gen_conductor, tools-config, app-gen-toc) is handled
by the task dependsOn chain before this script runs.
"""

import json
import os
import re
import subprocess
import sys

from board_config import get_board_name
from gen_jlink import get_core_config, get_images_dir
from get_config import load_config
from se_tools import REPO_ROOT, SCRIPTS_DIR, get_se_tools_dir, run, write_mram
from usb_serial import ensure_usb_serial_devices


def get_toc_address(se_tools_build_dir: str) -> int:
    """Parse app-package-map.txt to retrieve the APP Package start address, generated during the build process by the se tools."""
    map_path = os.path.join(se_tools_build_dir, "app-package-map.txt")
    with open(map_path) as f:
        content = f.read()
    match = re.search(r"APP Package Start Address\s*:\s*(0x[0-9a-fA-F]+)", content)
    if not match:
        print(f"Error: 'APP Package Start Address' not found in {map_path}", file=sys.stderr)
        sys.exit(1)
    return int(match.group(1), 16)


def generate_system_jlink(config) -> str:
    """Generate system.jlink (TOC + MCUboots in dual core) using the runtime-computed TOC address."""
    cores = config["cores"]
    bootloader = config.get("bootloader", False)
    target = config["target"]
    board = get_board_name(target)

    se_tools_build_dir = os.path.join(get_se_tools_dir(), "build")
    toc_addr = get_toc_address(se_tools_build_dir)
    toc_bin = os.path.join(se_tools_build_dir, "AppTocPackage.bin")
    images_dir = get_images_dir()

    lines = [
        "// Auto-generated JLink system script (first flash)",
        "si SWD",
        "speed auto",
        "connect",
        "halt",
        "",
        "// AppTocPackage",
        f"loadbin {toc_bin}, 0x{toc_addr:08X}",
        f"verifybin {toc_bin}, 0x{toc_addr:08X}",
        "",
    ]

    if bootloader:
        for core in cores:
            cfg = get_core_config(board, core)
            mcuboot_bin = os.path.join(images_dir, f"{core}_bootloader.bin")
            mcuboot_addr = cfg["mram_no_boot"]
            lines += [
                f"// {core.upper()} MCUBoot",
                f"loadbin {mcuboot_bin}, 0x{mcuboot_addr:08X}",
                f"verifybin {mcuboot_bin}, 0x{mcuboot_addr:08X}",
                "",
            ]

    lines += ["", "exit", ""]

    output_path = os.path.join(REPO_ROOT, ".config", "apps", "system.jlink")
    with open(output_path, "w") as f:
        f.write("\n".join(lines))
    print(f"Generated {output_path} (TOC @ 0x{toc_addr:08X}{', MCUboots' if bootloader else ''})")
    return output_path


def set_config_key(key: str, value):
    run([os.path.join(SCRIPTS_DIR, "set_config.py"),
         "-m", "apps", "-k", key, "-v", json.dumps(value)], cwd=REPO_ROOT)


def flash_se_tools():
    """Write MRAM via SE-Tools and mark SE_ready."""
    ensure_usb_serial_devices()
    write_mram()
    set_config_key("SE_ready", True)


JLINK_DEVICES = ["AE722F80F55D5_HE", "AE722F80F55D5_HP"]

# JLinkExe frequently exits 0 even after a fatal error, so we scan its
# stdout for the "****** Error:" marker it emits on real failures, plus a
# couple of specific phrases seen on Alif when the selected core is
# unreachable (typical case: SWD bound to the other core, WDT still
# running, failure during halt/reset sequence).
JLINK_FAILURE_MARKERS = (
    "****** Error:",
    "Failed to halt CPU",
    "Could not find core in Coresight setup",
    "Cannot connect to target",
)


def _run_jlink(cmd: list, stdin: str | None = None, echo: bool = True) -> bool:
    """Run JLinkExe, return True on success.

    JLinkExe often exits 0 on genuine errors, so we always scan the captured
    output for JLINK_FAILURE_MARKERS. `echo` controls whether output is also
    streamed live to stdout.
    """
    print(f"  $ {' '.join(cmd)}" + (" (stdin)" if stdin else "")
          + ("" if echo else " (quiet)"))
    proc = subprocess.Popen(
        cmd, cwd=REPO_ROOT,
        stdin=subprocess.PIPE if stdin else None,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
    )
    if stdin:
        assert proc.stdin is not None
        proc.stdin.write(stdin)
        proc.stdin.close()
    assert proc.stdout is not None
    chunks = []
    for line in proc.stdout:
        if echo:
            sys.stdout.write(line)
            sys.stdout.flush()
        chunks.append(line)
    proc.wait()
    if proc.returncode != 0:
        return False
    output = "".join(chunks)
    return not any(mark in output for mark in JLINK_FAILURE_MARKERS)


def jlink_reset(device: str, full_reset: bool) -> bool:
    """Reset the target via JLinkExe, driven directly from Python (no script).
    full_reset=True  → RSetType 2 (pin reset via nRST — triggers the full SE
                       boot sequence: TOC re-evaluation, MCUboots relaunch).
                       Output is muted: JLinkExe prints plenty of scary-but-
                       benign reconnect messages while the chip comes back.
    full_reset=False → RSetType 0 (normal core reset — used to probe which
                       core responds via SWD).
    """
    rset = 2 if full_reset else 0
    commands = "\n".join([
        "si SWD",
        "speed auto",
        "connect",
        f"RSetType {rset}",
        "r",
        "exit",
        "",
    ])
    return _run_jlink(
        ["JLinkExe", "-device", device],
        stdin=commands,
        echo=not full_reset,
    )


def run_jlink_script(script_name: str, label: str, device: str):
    """Run a JLink script on the given device. Fail fast on error."""
    script = os.path.join(REPO_ROOT, ".config", "apps", script_name)
    print(f"\n--- {label} (via {device}) ---")
    if not _run_jlink(["JLinkExe", "-device", device, "-CommanderScript", script]):
        print(f"Error: JLink flash failed ({label})", file=sys.stderr)
        sys.exit(1)


def probe_jlink_device() -> str:
    """Pick the JLink device to use for the first flash.
    Runs a normal reset on HE, then on HP if HE fails. Returns the first
    device that succeeds; exits otherwise.
    """
    for device in JLINK_DEVICES:
        print(f"\n--- Probing {device} (core reset) ---")
        if jlink_reset(device, full_reset=False):
            print(f"  -> {device} responded; using it for this first flash")
            return device
        print(f"  -> {device} did not respond")
    print("Error: neither HE nor HP responded to SWD. Check board power, "
          "cabling, and JLink probe, then re-run Flash.", file=sys.stderr)
    sys.exit(1)


def flash_jlink(config, system: bool = False):
    """Flash via JLink. When `system` is True, generate and run system.jlink (TOC + MCUboots in dual core)."""
    cores = config.get("cores", [])
    if not cores:
        print("Error: 'cores' not set in .config.json", file=sys.stderr)
        sys.exit(1)

    board = get_board_name(config["target"])

    if system:
        # Determine which core is reachable via SWD by pin-resetting each in
        # turn; the surviving device is used for system + application flash.
        device = probe_jlink_device()
        generate_system_jlink(config)
        run_jlink_script("system.jlink", "First flash: system (TOC + MCUboots in dual core)", device)
    else:
        device = get_core_config(board, cores[0])["device"]

    run_jlink_script("application.jlink",
                     f"Flashing application ({', '.join(c.upper() for c in cores)})",
                     device)

    # First-flash only: once the TOC + apps are written, mark SE_ready so
    # that a later pin-reset failure doesn't force replaying the whole
    # first-flash flow. Then issue the pin reset (nRST) to trigger the full
    # SE boot sequence (TOC re-evaluation, MCUboots relaunch, app start).
    # Subsequent flashes rely on the core reset embedded in application.jlink.
    if system:
        set_config_key("SE_ready", True)
        print(f"\n--- Pin reset (full SE boot) via {device} ---")
        jlink_reset(device, full_reset=True)


def main():
    config = load_config("apps")

    method = config.get("flash")
    if not method:
        print("Error: 'flash' not set in .config.json. Run 'Configure apps' first.", file=sys.stderr)
        sys.exit(1)

    se_ready = config.get("SE_ready", False)

    if method == "se-tools":
        flash_se_tools()

    elif method == "jlink":
        if not se_ready:
            print("First flash: system + app binaries via JLink...")
            flash_jlink(config, system=True)
        else:
            flash_jlink(config)

    else:
        print(f"Error: Unknown flash method '{method}'", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
