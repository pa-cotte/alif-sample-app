#!/usr/bin/env python3
# Confidential - Copyright PA.COTTE: All rights reserved

"""Ethernet throughput benchmark for the HE core (zperf <-> iperf).

Runs the full 2x2 matrix - TCP and UDP, in both directions - by driving the
Zephyr shell (``zperf``) on the board over the serial console while running
``iperf`` (the 2.x protocol, wire-compatible with zperf) on this PC. It then
prints a consolidated report with throughput and, for UDP, packet-loss stats
taken from whichever side is the *receiver* (the authoritative one for loss).

Direction conventions (matching zperf's own wording):

  * "upload"   = board -> PC   (board is client, PC runs ``iperf -s``)
  * "download" = PC   -> board (board is server, PC runs ``iperf -c``)

Loss is read from the receiver:
  * board -> PC : PC ``iperf`` server reports lost/total datagrams
  * PC -> board : board ``zperf ... download`` reports received / lost packets

Prerequisites on this PC:
  * ``iperf`` 2.x in PATH  (NOT iperf3 - iperf3 is not wire-compatible with zperf)
  * pyserial               (``pip install pyserial``)
  * a direct cable to the board, PC NIC set to 192.168.0.1/24
  * board flashed with the HE app (static IP 192.168.0.2, zperf enabled)

Examples:
    ./tools/scripts/perf_test.py
    ./tools/scripts/perf_test.py --serial /dev/ttyACM1 --duration 20 --udp-rate 120M
    ./tools/scripts/perf_test.py --tests udp_up udp_down   # subset only
"""

import argparse
import re
import subprocess
import sys
import time

try:
    import serial  # pyserial
except ImportError:
    sys.exit("Error: pyserial is required (pip install pyserial)")

# Defaults mirror the HE app config: board console is ttyACM1, static IP setup.
DEFAULT_SERIAL = "/dev/ttyACM1"
DEFAULT_BAUD = 115200
DEFAULT_BOARD_IP = "192.168.0.2"
DEFAULT_PEER_IP = "192.168.0.1"
DEFAULT_PORT = 5001
PROMPT = "uart:~$"

# All four cases, in run order. Keys are accepted by --tests.
ALL_TESTS = ["tcp_up", "tcp_down", "udp_up", "udp_down"]


# --------------------------------------------------------------------------- #
# Serial / Zephyr shell helper
# --------------------------------------------------------------------------- #
class ZephyrShell:
    def __init__(self, port, baud, verbose=False):
        self.verbose = verbose
        try:
            self.ser = serial.Serial(port, baud, timeout=0.2)
        except serial.SerialException as exc:
            sys.exit(f"Error: cannot open serial port {port}: {exc}")
        time.sleep(0.3)
        self.ser.reset_input_buffer()

    def _log(self, text):
        if self.verbose and text:
            sys.stdout.write(text)
            sys.stdout.flush()

    def send(self, cmd):
        """Send a shell command (CR/LF terminated)."""
        self.ser.write((cmd + "\r\n").encode())
        self.ser.flush()

    def read_for(self, seconds):
        """Collect everything printed over the next ``seconds`` seconds."""
        buf = []
        deadline = time.time() + seconds
        while time.time() < deadline:
            chunk = self.ser.read(4096).decode(errors="replace")
            if chunk:
                buf.append(chunk)
                self._log(chunk)
        return "".join(buf)

    def read_until(self, marker, timeout):
        """Collect output until ``marker`` appears or ``timeout`` elapses."""
        buf = []
        deadline = time.time() + timeout
        while time.time() < deadline:
            chunk = self.ser.read(4096).decode(errors="replace")
            if chunk:
                buf.append(chunk)
                self._log(chunk)
                if marker in "".join(buf):
                    break
        return "".join(buf)

    def run(self, cmd, settle=0.5):
        """Run a quick command, return its output (up to the next prompt)."""
        self.ser.reset_input_buffer()
        self.send(cmd)
        out = self.read_until(PROMPT, timeout=settle + 2.0)
        return out

    def close(self):
        try:
            self.ser.close()
        except Exception:
            pass


# --------------------------------------------------------------------------- #
# iperf (2.x) helpers
# --------------------------------------------------------------------------- #
def iperf_server(binary, udp, port):
    """Start an iperf server in the background, return the Popen handle."""
    cmd = [binary, "-s", "-p", str(port)]
    if udp:
        cmd.append("-u")
    return subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)


def stop_server(proc, grace):
    """Give the server a moment to print its summary, then stop it."""
    time.sleep(grace)
    proc.terminate()
    try:
        return proc.communicate(timeout=5)[0] or ""
    except subprocess.TimeoutExpired:
        proc.kill()
        return proc.communicate()[0] or ""


def iperf_client(binary, udp, peer_ip, port, duration, rate):
    """Run an iperf client to completion, return its output.

    On timeout we still return whatever the client managed to print (e.g. the
    "connecting to..." banner or a "connect failed" message) so the report can
    show *why* it stalled, instead of swallowing it behind a bare "timed out".
    """
    cmd = [binary, "-c", peer_ip, "-p", str(port), "-t", str(duration)]
    if udp:
        cmd += ["-u", "-b", rate]
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        out = proc.communicate(timeout=duration + 20)[0] or ""
    except subprocess.TimeoutExpired:
        proc.kill()
        out = proc.communicate()[0] or ""
        out += "\n(iperf client timed out - no completed transfer)"
    return out


# --------------------------------------------------------------------------- #
# Output parsing
# --------------------------------------------------------------------------- #
def parse_iperf(text):
    """Pull bandwidth and (UDP) loss from iperf 2.x output."""
    res = {}
    bw = re.findall(r"([\d.]+)\s+([KMG]?bits/sec)", text)
    if bw:
        val, unit = bw[-1]
        res["throughput"] = f"{val} {unit}"
    loss = re.findall(r"(\d+)\s*/\s*(\d+)\s*\(([\d.eE+-]+)\s*%\)", text)
    if loss:
        lost, total, pct = loss[-1]
        res["lost"], res["total"], res["loss_pct"] = int(lost), int(total), pct
    jit = re.search(r"([\d.]+)\s*ms", text)
    if jit:
        res["jitter"] = f"{jit.group(1)} ms"
    return res


def parse_zperf(text):
    """Pull rate / packet counts / loss from zperf output (upload or download)."""
    res = {}
    # The zperf session summary is printed line by line over the same UART that
    # also echoes the shell prompt and any driver log spam, which can splice
    # "uart:~$" / log lines into the middle of a field. Strip that noise (and
    # ANSI) and collapse whitespace so the field regexes below still match.
    text = strip_ansi(text).replace("uart:~$", " ")
    text = re.sub(r"(?im)^.*(?:<err>|<wrn>|messages dropped).*$", " ", text)
    text = re.sub(r"[ \t]+", " ", text)
    rate = re.search(r"(?i)\brate:\s*([\d.]+)\s*([KMG]?bps|[KMG]?bit/s)", text)
    if rate:
        res["throughput"] = f"{rate.group(1)} {rate.group(2)}"
    total = re.search(r"(?i)(?:num packets|received packets|nb packets received)\s*:?\s*(\d+)", text)
    if total:
        res["total"] = int(total.group(1))
    lost = re.search(r"(?i)(?:num packets lost|nb packets lost|packets lost)\s*:?\s*(\d+)", text)
    if lost:
        res["lost"] = int(lost.group(1))
    ratio = re.search(r"(?i)lost packet[s]?\s*ratio\s*:?\s*([\d.]+)", text)
    if ratio:
        res["loss_pct"] = ratio.group(1)
    jit = re.search(r"(?i)jitter:\s*([\d.]+)\s*(\w+)", text)
    if jit:
        res["jitter"] = f"{jit.group(1)} {jit.group(2)}"
    if "total" in res and "lost" in res and "loss_pct" not in res and res["total"]:
        res["loss_pct"] = f"{100.0 * res['lost'] / res['total']:.3f}"
    return res


# --------------------------------------------------------------------------- #
# The four test cases
# --------------------------------------------------------------------------- #
def test_upload(shell, args, udp):
    """board -> PC: PC is iperf server, board is zperf client."""
    proto = "udp" if udp else "tcp"
    psize = args.packet_size if udp else "1K"
    server = iperf_server(args.iperf, udp, args.port)
    time.sleep(1.0)  # let the server bind

    if udp:
        cmd = f"zperf udp upload {args.peer_ip} {args.port} {args.duration} {psize} {args.udp_rate}"
    else:
        cmd = f"zperf tcp upload {args.peer_ip} {args.port} {args.duration} {psize}"

    shell.ser.reset_input_buffer()
    shell.send(cmd)
    board_out = shell.read_until("completed", timeout=args.duration + 15)
    board_out += shell.read_for(1.0)
    pc_out = stop_server(server, grace=1.0)

    return {
        "name": f"{proto.upper()} board->PC (upload)",
        "proto": proto,
        "receiver": "PC (iperf)",
        "board": parse_zperf(board_out),
        "pc": parse_iperf(pc_out),
        "raw_board": board_out,
        "raw_pc": pc_out,
    }


def test_download(shell, args, udp):
    """PC -> board: board is zperf server, PC is iperf client."""
    proto = "udp" if udp else "tcp"
    # Start the board server and KEEP its reply: it prints either
    # "<proto> server started on port N" or an error ("Failed to start ...",
    # "... server already started!"). Throwing it away hides why a download
    # never connects, so fold it into the captured board output.
    start_cmd = f"zperf {proto} download {args.port}"
    start_out = shell.run(start_cmd, settle=1.0)
    shell.ser.reset_input_buffer()

    pc_out = iperf_client(args.iperf, udp, args.board_ip, args.port, args.duration, args.udp_rate)
    # The board prints its session summary asynchronously once the flow ends.
    session_out = shell.read_for(3.0)
    stop_out = shell.run(f"zperf {proto} download stop", settle=1.0)

    board_out = (f"$ {start_cmd}\n{start_out}\n"
                 f"{session_out}\n"
                 f"$ zperf {proto} download stop\n{stop_out}")

    return {
        "name": f"{proto.upper()} PC->board (download)",
        "proto": proto,
        "receiver": "board (zperf)",
        "board": parse_zperf(session_out),
        "pc": parse_iperf(pc_out),
        "raw_board": board_out,
        "raw_pc": pc_out,
    }


# --------------------------------------------------------------------------- #
# Reporting
# --------------------------------------------------------------------------- #
ANSI_RE = re.compile(r"\x1b\[[0-9;?]*[a-zA-Z]")
# Leading Zephyr prompt and/or log timestamp, used to spot repeated log spam.
_NOISE_PREFIX = re.compile(r"^(?:uart:~\$\s*)?(?:\[\d{2}:\d{2}:\d{2}\.\d+,\d+\]\s*)?")


def strip_ansi(text):
    """Remove VT100/ANSI escape sequences (colours, cursor moves, clears)."""
    return ANSI_RE.sub("", text)


def clean_log(text):
    """De-noise a raw serial/iperf capture for embedding in the report.

    Strips ANSI escapes and collapses runs of identical lines (ignoring the
    log timestamp), so a flood like ``net_pkt_rx_alloc_with_buffer() failed``
    shows up once with a repeat count instead of hundreds of times.
    """
    lines = [ln.rstrip() for ln in strip_ansi(text).splitlines()]
    out, i = [], 0
    while i < len(lines):
        norm = _NOISE_PREFIX.sub("", lines[i])
        j = i + 1
        while j < len(lines) and _NOISE_PREFIX.sub("", lines[j]) == norm:
            j += 1
        out.append(lines[i] or "")
        if j - i > 1:
            out.append(f"… (line repeated {j - i}× total)")
        i = j
    return "\n".join(out).strip()


def loss_summary(result):
    """Best packet-loss string, taken from the receiving side (UDP only)."""
    if result["proto"] == "tcp":
        return "n/a (TCP)"
    side = result["pc"] if result["receiver"].startswith("PC") else result["board"]
    if "lost" in side and "total" in side:
        pct = side.get("loss_pct", "?")
        return f"{side['lost']}/{side['total']} ({pct} %)"
    if "loss_pct" in side:
        return f"{side['loss_pct']} %"
    return "?"


def throughput_summary(result):
    # Prefer the receiver's measured rate; fall back to the other side.
    side = result["pc"] if result["receiver"].startswith("PC") else result["board"]
    other = result["board"] if result["receiver"].startswith("PC") else result["pc"]
    return side.get("throughput") or other.get("throughput") or "?"


def jitter_summary(result):
    if result["proto"] == "tcp":
        return "n/a (TCP)"
    side = result["pc"] if result["receiver"].startswith("PC") else result["board"]
    return side.get("jitter", "?")


def throughput_mbps(result):
    """Numeric throughput in Mbit/s for charts, or 0.0 if unparseable."""
    m = re.match(r"\s*([\d.]+)\s*([kKmMgG]?)", throughput_summary(result))
    if not m:
        return 0.0
    factor = {"": 1e-6, "k": 1e-3, "m": 1.0, "g": 1e3}[m.group(2).lower()]
    return float(m.group(1)) * factor


def bar(value, maxv, width=32):
    """A Unicode horizontal bar, scaled to ``maxv`` (renders in any viewer)."""
    if maxv <= 0:
        return "░" * width
    filled = int(round(width * value / maxv))
    return "█" * filled + "░" * (width - filled)


def build_report(results, args):
    stamp = time.strftime("%Y-%m-%d %H:%M:%S")
    L = []
    L.append("# Ethernet Throughput Report — HE core")
    L.append("")
    L.append(f"*zperf ⇄ iperf 2.x · generated {stamp}*")
    L.append("")
    L.append("| Setting | Value |")
    L.append("|---|---|")
    L.append(f"| Board (zperf) | `{args.board_ip}` |")
    L.append(f"| PC (iperf) | `{args.peer_ip}` |")
    L.append(f"| Port | {args.port} |")
    L.append(f"| Duration | {args.duration} s |")
    L.append(f"| UDP target rate | {args.udp_rate} |")
    L.append("")

    # ---- Summary table -------------------------------------------------- #
    L.append("## Summary")
    L.append("")
    L.append("| Test | Throughput | Packet loss | Jitter | Receiver |")
    L.append("|---|---:|---:|---:|---|")
    for r in results:
        tput = throughput_summary(r)
        tput = tput if tput != "?" else "❌ failed"
        L.append(f"| {r['name']} | {tput} | {loss_summary(r)} "
                 f"| {jitter_summary(r)} | {r['receiver']} |")
    L.append("")

    # ---- Throughput chart (Unicode bars, always render) ----------------- #
    maxv = max((throughput_mbps(r) for r in results), default=0.0)
    L.append("## Throughput")
    L.append("")
    L.append("```text")
    for r in results:
        v = throughput_mbps(r)
        label = r["name"].split(" ")[0] + " " + ("↑up  " if "upload" in r["name"] else "↓down")
        shown = f"{v:6.2f} Mb/s" if v else "  —  failed"
        L.append(f"{label:<10} │{bar(v, maxv)}│ {shown}")
    L.append("```")
    L.append("")

    # ---- Same data as a Mermaid bar chart (renders on GitHub/VS Code) --- #
    names = ", ".join(f'"{r["name"].replace(chr(34), "")}"' for r in results)
    values = ", ".join(f"{throughput_mbps(r):.2f}" for r in results)
    L.append("```mermaid")
    L.append("xychart-beta")
    L.append('    title "Throughput per test (Mbit/s)"')
    L.append(f"    x-axis [{names}]")
    L.append('    y-axis "Mbit/s"')
    L.append(f"    bar [{values}]")
    L.append("```")
    L.append("")

    # ---- Raw captures, cleaned, collapsed by default -------------------- #
    L.append("## Raw captures")
    L.append("")
    for r in results:
        L.append(f"<details>")
        L.append(f"<summary>{r['name']}</summary>")
        L.append("")
        L.append("**board (zperf)**")
        L.append("")
        L.append("```text")
        L.append(clean_log(r["raw_board"]) or "(no output)")
        L.append("```")
        L.append("")
        L.append("**PC (iperf)**")
        L.append("")
        L.append("```text")
        L.append(clean_log(r["raw_pc"]) or "(no output)")
        L.append("```")
        L.append("")
        L.append("</details>")
        L.append("")
    return "\n".join(L)


# --------------------------------------------------------------------------- #
def preflight(shell, args):
    """Show the interface state and ping the PC before benchmarking."""
    print("== Preflight ==")
    print(shell.run("net iface", settle=1.0).strip())
    if not args.no_ping:
        print(f"-- ping {args.peer_ip} --")
        print(shell.run(f"net ping {args.peer_ip}", settle=4.0).strip())
    print()


def main():
    p = argparse.ArgumentParser(
        description="Ethernet TCP/UDP throughput benchmark (zperf <-> iperf 2.x).",
        formatter_class=argparse.ArgumentDefaultsHelpFormatter,
    )
    p.add_argument("--serial", default=DEFAULT_SERIAL, help="board console serial port")
    p.add_argument("--baud", type=int, default=DEFAULT_BAUD)
    p.add_argument("--board-ip", default=DEFAULT_BOARD_IP)
    p.add_argument("--peer-ip", default=DEFAULT_PEER_IP, help="this PC's IP on the link")
    p.add_argument("--port", type=int, default=DEFAULT_PORT)
    p.add_argument("--duration", type=int, default=10, help="seconds per test")
    # With the tuned + zero-copy RX MAC driver the board receives UDP at wire
    # rate (~95 Mb/s, <0.02% loss). 90M drives both directions hard while
    # staying clean (download ~94 Mb/s; upload hits the board's ~67 Mb/s TX
    # ceiling at 0% loss). Lower it for a gentler run.
    p.add_argument("--udp-rate", default="90M", help="UDP target rate (iperf/zperf syntax)")
    p.add_argument("--packet-size", default="1K", help="UDP payload size for zperf upload")
    p.add_argument("--iperf", default="iperf", help="iperf 2.x binary (NOT iperf3)")
    p.add_argument("--tests", nargs="+", choices=ALL_TESTS, default=ALL_TESTS)
    p.add_argument("--report", default="perf_report.md", help="write the Markdown report here")
    p.add_argument("--no-ping", action="store_true", help="skip the preflight ping")
    p.add_argument("--verbose", action="store_true", help="echo serial traffic live")
    args = p.parse_args()

    # iperf 2.x sanity check.
    try:
        ver = subprocess.run([args.iperf, "--version"], capture_output=True, text=True)
        banner = (ver.stdout + ver.stderr)
        if "iperf 3" in banner or "iperf3" in banner:
            sys.exit("Error: iperf3 detected. zperf needs iperf 2.x (apt install iperf).")
    except FileNotFoundError:
        sys.exit(f"Error: '{args.iperf}' not found. Install iperf 2.x (e.g. apt install iperf).")

    shell = ZephyrShell(args.serial, args.baud, verbose=args.verbose)
    results = []
    try:
        preflight(shell, args)
        runners = {
            "tcp_up": lambda: test_upload(shell, args, udp=False),
            "tcp_down": lambda: test_download(shell, args, udp=False),
            "udp_up": lambda: test_upload(shell, args, udp=True),
            "udp_down": lambda: test_download(shell, args, udp=True),
        }
        for key in args.tests:
            print(f"== Running {key} ... ==")
            results.append(runners[key]())
    finally:
        shell.close()

    report = build_report(results, args)
    print("\n" + report)
    try:
        with open(args.report, "w") as f:
            f.write(report + "\n")
        print(f"\n[report written to {args.report}]")
    except OSError as exc:
        print(f"\n[could not write report: {exc}]")


if __name__ == "__main__":
    main()
