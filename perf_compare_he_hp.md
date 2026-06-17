# Ethernet Throughput — HE vs HP comparison

Alif E7 DK, direct cable board ⇄ PC (192.168.0.2 ⇄ 192.168.0.1), zperf ⇄ iperf 2.x,
10 s per test, UDP offered rate 30 Mb/s. One core at a time owns the MAC
(HE built with Ethernet, HP built with `-DAPP_ETHERNET=ON`; the other core is
not running Ethernet — see `project/apps/common/eth.overlay`).

## Reliable metric — UDP

UDP is stable run-to-run, so these numbers are meaningful.

| Direction | HE (RTSS-HE) | HP (RTSS-HP) | Notes |
|---|---:|---:|---|
| UDP ↑ upload   (board→PC, TX) | ~22.8 Mb/s, 0 % loss | ~22–27 Mb/s, 0 % loss | comparable, within noise |
| UDP ↓ download (PC→board, RX) | 31.4 Mb/s, <0.5 % loss | 31.4 Mb/s, <0.1 % loss | **identical** |

```text
UDP ↓ download   HE │████████████████████████████████│ 31.4 Mb/s
                 HP │████████████████████████████████│ 31.4 Mb/s
UDP ↑ upload     HE │███████████████████████░░░░░░░░░│ 22.8 Mb/s
                 HP │█████████████████████████░░░░░░░│ ~24  Mb/s
```

## Unreliable metric — TCP (do not over-read)

TCP throughput swings wildly between runs on **both** cores — sessions
intermittently stall (e.g. an upload collapsing to 41 kb/s, a download from
3 to 22 Mb/s). Likely the tiny TCP window + per-packet RX busy-wait +
retransmissions. Single numbers are not trustworthy; ranges observed:

| Direction | HE (range) | HP (range) |
|---|---:|---:|
| TCP ↑ upload   (board→PC) | ~7 Mb/s | 0.04 – 9.3 Mb/s |
| TCP ↓ download (PC→board) | 3.4 – 15 Mb/s | 12 – 22 Mb/s |

HP's TCP download tends higher, but the variance is too large to claim a
firm winner.

## Takeaway

Within the rates this board can actually sustain, **HE ≈ HP for Ethernet**.
The bottleneck is the MAC driver / net-buffer RX path (the board starves on
`net_pkt_rx_alloc_with_buffer()` above ~30 Mb/s), not CPU horsepower — so
moving to the higher-performance core does not raise throughput here. A real
CPU-bound difference would only show above the ~31 Mb/s RX ceiling, which the
driver cannot reach today.

If a core difference matters, the lever is the **RX path** (bigger/zero-copy
buffers, DMA-to-netbuf, dropping the `CONFIG_ETH_ALIF_RX_DELAY_US` busy-wait),
not the core choice.

## How to reproduce

```sh
# HE benchmark (Ethernet is default-ON for HE)
west build -p always -b alif_e7_dk/ae722f80f55d5xx/rtss_he project/apps/he_app -d build/he
#  ... flash HE only, set serial routing to HE (uart2), then:
./project/tools/scripts/perf_test.py

# HP benchmark (Ethernet is opt-in for HP; flash HP only so HE never co-drives the MAC)
west build -p always -b alif_e7_dk/ae722f80f55d5xx/rtss_hp project/apps/hp_app -d build/hp -- -DAPP_ETHERNET=ON
#  ... erase + flash HP only, set serial routing to HP (uart4), then:
./project/tools/scripts/perf_test.py
```
