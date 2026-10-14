#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved
"""Check the USB CDC-ECM link to the Alif E7 DK from a Linux host.

Run it on the Linux PC the board is plugged into, as root (it configures the interface):

    sudo ./tools/scripts/usb_ecm_check.py

What it does:

  1. Finds the board on the USB bus (VID 0x2fe3, PID 0x0009) and prints the link speed the host
     negotiated, the bulk IN packet size the board announced, and the related kernel messages.
  2. Gives the host side of the link 192.0.2.2/24, the board being 192.0.2.1.
  3. Pings the board with every payload size whose Ethernet frame is a multiple of the bulk packet
     size (64 bytes at full speed, 512 at high speed), next to a control size one byte larger.
     Those frames need a zero-length packet to end on the bus: without it the host keeps waiting
     for the rest of the frame, and the echo reply is lost or merged with the next one.

Exit code: 0 when every ping came back, 1 otherwise, 2 when the board or its interface is missing.
"""

import argparse
import re
import subprocess
import sys
from pathlib import Path

VID = 0x2FE3
PID = 0x0009
BOARD_IP = "192.0.2.1"
HOST_ADDRESS = "192.0.2.2/24"

# Ethernet (14) + IPv4 (20) + ICMP (8) headers around the ping payload.
FRAME_OVERHEAD = 42
# Largest payload of an unfragmented ping on a 1500-byte MTU.
MAX_PAYLOAD = 1472

USB_DEVICES = Path("/sys/bus/usb/devices")


def read(path: Path) -> str:
    return path.read_text().strip()


def find_board() -> Path | None:
    for device in USB_DEVICES.iterdir():
        try:
            if int(read(device / "idVendor"), 16) == VID and int(read(device / "idProduct"), 16) == PID:
                return device
        except (FileNotFoundError, ValueError):
            continue
    return None


def find_interface(board: Path) -> tuple[str | None, int | None]:
    """The network interface the host created for the board, and its bulk IN packet size."""
    iface = None
    bulk_in_mps = None
    for interface in board.glob(f"{board.name}:*"):
        net = interface / "net"
        if net.is_dir():
            names = [n.name for n in net.iterdir()]
            iface = names[0] if names else None
        for endpoint in interface.glob("ep_*"):
            try:
                if read(endpoint / "type") == "Bulk" and read(endpoint / "direction") == "in":
                    bulk_in_mps = int(read(endpoint / "wMaxPacketSize"), 16)
            except FileNotFoundError:
                continue
    return iface, bulk_in_mps


def kernel_messages(board: Path) -> list[str]:
    """Kernel lines about the board's endpoints or bus errors, the trace of a wrong packet size."""
    try:
        log = subprocess.run(["dmesg"], capture_output=True, text=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return ["(dmesg not readable: run as root to see the kernel messages)"]
    pattern = re.compile(rf"usb {re.escape(board.name)}:.*(maxpacket|babble|error)|cdc_ether.*{re.escape(board.name)}", re.I)
    return [line for line in log.splitlines() if pattern.search(line)][-10:]


def configure(iface: str) -> None:
    subprocess.run(["ip", "addr", "replace", HOST_ADDRESS, "dev", iface], check=True)
    subprocess.run(["ip", "link", "set", iface, "up"], check=True)


def ping(iface: str, payload: int, count: int) -> int:
    """Number of echo replies received out of `count`."""
    result = subprocess.run(
        ["ping", "-I", iface, "-c", str(count), "-i", "0.2", "-W", "1", "-s", str(payload), BOARD_IP],
        capture_output=True,
        text=True,
        check=False,
    )
    match = re.search(r"(\d+) received", result.stdout)
    return int(match.group(1)) if match else 0


def payload_sizes(packet_size: int) -> list[tuple[int, bool]]:
    """Payloads whose frame is a multiple of the packet size (needs a ZLP), each followed by a
    control payload one byte larger (no ZLP)."""
    sizes = []
    frame = packet_size
    while frame - FRAME_OVERHEAD <= MAX_PAYLOAD:
        payload = frame - FRAME_OVERHEAD
        if payload > 0:
            sizes.append((payload, True))
            if payload + 1 <= MAX_PAYLOAD:
                sizes.append((payload + 1, False))
        frame += packet_size
    return sizes


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--count", type=int, default=5, help="Pings per size (default 5)")
    parser.add_argument("--no-configure", action="store_true", help="Leave the interface address alone")
    args = parser.parse_args()

    board = find_board()
    if board is None:
        print(f"Board not found on the USB bus (VID {VID:#06x}, PID {PID:#06x}): is it plugged in and running?")
        return 2

    speed_mbps = read(board / "speed")
    full_speed = speed_mbps == "12"
    iface, bulk_in_mps = find_interface(board)

    print(f"Board at USB {board.name}, link speed {speed_mbps} Mb/s ({'full' if full_speed else 'high'} speed)")
    print(f"Bulk IN packet size seen by the host: {bulk_in_mps} bytes")
    for line in kernel_messages(board):
        print(f"  kernel: {line}")

    if iface is None:
        print("No network interface for the board: the host has not bound cdc_ether to it.")
        return 2

    if not args.no_configure:
        configure(iface)

    # The packet size the frames have to end on is the one of the link, whatever the board announced.
    packet_size = 64 if full_speed else 512
    print(f"\nInterface {iface}, pinging {BOARD_IP}; frames multiple of {packet_size} bytes need a zero-length packet\n")
    print(f"{'payload':>8} {'frame':>6} {'needs ZLP':>10} {'replies':>8}")

    failures = 0
    for payload, needs_zlp in payload_sizes(packet_size):
        received = ping(iface, payload, args.count)
        failures += received < args.count
        mark = "" if received == args.count else "  <-- lost"
        print(f"{payload:>8} {payload + FRAME_OVERHEAD:>6} {'yes' if needs_zlp else 'no':>10} {received:>4}/{args.count}{mark}")

    print(f"\n{'PASS' if failures == 0 else 'FAIL'}: {failures} size(s) with lost replies")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
