# Confidential - Copyright PA.COTTE: All rights reserved

"""
Ensure /dev/ttyUSB* and /dev/ttyACM* nodes exist in Docker containers.

In privileged Docker containers, udev may not create /dev/ttyUSB* or /dev/ttyACM*
device nodes when USB serial adapters are hot-plugged. This module detects devices
known to the kernel (via /sys) and creates any missing /dev nodes.
"""

import glob
import os
import subprocess

_TTY_PATTERNS = ["/sys/class/tty/ttyUSB*", "/sys/class/tty/ttyACM*"]


def ensure_usb_serial_devices() -> None:
    """Create or refresh /dev/ttyUSB* and /dev/ttyACM* nodes from kernel-detected devices.

    After hot-plug in a Docker container, existing device nodes may be stale
    (wrong major:minor, or orphaned after unplug). This function:
    1. Removes orphaned /dev/ttyUSB*/ttyACM* nodes not backed by a kernel device
    2. Recreates nodes whose underlying device changed after hot-plug
    3. Creates missing nodes for newly detected devices
    """
    # Phase 1: Remove orphaned /dev nodes not backed by kernel devices
    for pattern in ["/dev/ttyUSB*", "/dev/ttyACM*"]:
        for dev_path in glob.glob(pattern):
            dev_name = os.path.basename(dev_path)
            sys_path = f"/sys/class/tty/{dev_name}"
            if not os.path.exists(sys_path):
                subprocess.run(["sudo", "rm", "-f", dev_path], capture_output=True)
                print(f"Removed orphaned device node {dev_path}")

    # Phase 2: Create or refresh nodes for kernel-detected devices
    sys_devices = []
    for pattern in _TTY_PATTERNS:
        sys_devices.extend(glob.glob(pattern))
    if not sys_devices:
        return

    for sys_path in sys_devices:
        dev_name = os.path.basename(sys_path)
        dev_path = f"/dev/{dev_name}"

        dev_number_file = os.path.join(sys_path, "dev")
        try:
            with open(dev_number_file, "r") as f:
                major, minor = f.read().strip().split(":")
        except Exception as e:
            print(f"Warning: Could not read {dev_number_file}: {e}")
            continue

        expected_rdev = os.makedev(int(major), int(minor))

        # Check if existing node is functional
        if os.path.exists(dev_path):
            stale = False
            try:
                actual_rdev = os.stat(dev_path).st_rdev
                if actual_rdev != expected_rdev:
                    stale = True
                else:
                    # rdev matches but device may be dead after hot-plug;
                    # verify with an actual open() syscall
                    fd = os.open(dev_path, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
                    os.close(fd)
                    continue  # node exists and is functional
            except OSError:
                stale = True

            if stale:
                subprocess.run(
                    ["sudo", "rm", "-f", dev_path],
                    capture_output=True,
                )
                print(f"Removed stale device node {dev_path}")

        try:
            subprocess.run(
                ["sudo", "mknod", dev_path, "c", major, minor],
                check=True, capture_output=True,
            )
            subprocess.run(
                ["sudo", "chmod", "666", dev_path],
                check=True, capture_output=True,
            )
            print(f"Created device node {dev_path} ({major}:{minor})")
        except Exception as e:
            print(f"Warning: Could not create {dev_path}: {e}")
