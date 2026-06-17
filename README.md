# alif-sample-app

This repository is used to highlight features and bugs related to ALIF's E7 devkit.
Each of them will be in its own branch, and there is a template branch to start from.

## Prerequisites

### Git & Docker

```bash
sudo apt install git
sudo apt install docker
sudo apt install docker-buildx
sudo usermod -aG docker $USER
```

### UDEV rules

```bash
wget -O 60-openocd.rules https://sf.net/p/openocd/code/ci/master/tree/contrib/60-openocd.rules?format=raw
sudo cp 60-openocd.rules /etc/udev/rules.d
sudo udevadm control --reload
```

## Ethernet performance measurement (zperf / iperf)

The `he_app` enables the Alif Ethernet driver, a static IP (`192.168.0.2`) and
the `zperf` shell tool, so the board can be benchmarked over a direct cable to
the PC. The script
[`project/tools/scripts/perf_test.py`](project/tools/scripts/perf_test.py)
runs the full TCP/UDP × both-directions matrix automatically: it drives `zperf`
on the board over the serial console while running `iperf` on the PC, then
prints a report with throughput and UDP packet-loss stats.

### Setup

1. Flash the `he_app` on the HE core. Ethernet and the static IP come up at boot.
2. Connect the board directly to the PC with an Ethernet cable.
3. Configure the PC NIC with a static address on the same subnet:

   ```bash
   sudo ip addr add 192.168.0.1/24 dev <eth-iface>
   sudo ip link set <eth-iface> up
   ```

4. Make sure `iperf` **2.x** is available (already installed in the devcontainer
   via the Dockerfile). It must NOT be `iperf3`, which is not wire-compatible
   with `zperf`:

   ```bash
   iperf --version   # expect "iperf version 2.x"
   ```

### Running the benchmark

```bash
# Full matrix with defaults (board 192.168.0.2, PC 192.168.0.1, console ttyACM1)
./project/tools/scripts/perf_test.py

# Subset of tests, longer runs, higher UDP target rate
./project/tools/scripts/perf_test.py --tests udp_up udp_down --duration 20 --udp-rate 120M

# Custom serial port / IPs
./project/tools/scripts/perf_test.py --serial /dev/ttyACM1 --board-ip 192.168.0.2 --peer-ip 192.168.0.1
```

Available test keys for `--tests`: `tcp_up`, `tcp_down`, `udp_up`, `udp_down`
(`up` = board→PC, `down` = PC→board). Run `perf_test.py --help` for all options.

The consolidated report is printed to the console and written to
`perf_report.txt`. For UDP, packet loss is reported from the receiving side (the
authoritative one) and the raw `zperf`/`iperf` output of each test is appended
for auditing.

> Note: the PHY runs at 10/100 Mbit/s (RMII), so the expected ceiling is
> ~95 Mbit/s.