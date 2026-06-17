# Ethernet Throughput — HE vs HP (tuned driver)

Alif E7 DK, direct cable board ⇄ PC (192.168.0.2 ⇄ 192.168.0.1), zperf ⇄ iperf
2.x, 10 s/test. **Both cores run the same tuned Alif MAC driver** (larger RX
DMA ring, no per-packet busy-wait, zero-copy RX) from `project/apps/common/` —
so the differences below are pure CPU (RTSS-HE high-efficiency vs RTSS-HP
high-performance), not driver config. One core owns the MAC at a time (HE built
with Ethernet by default; HP with `-DAPP_ETHERNET=ON`, flashed alone).

## Results

| Test | HE (RTSS-HE) | HP (RTSS-HP) | Notes |
|---|---:|---:|---|
| **UDP ↓ download** (PC→board, RX) | ~62 Mb/s | **~95.7 Mb/s (wire)** | RX is CPU-bound above the ring; HP reaches 100BASE-T line rate, HE caps ~62 |
| **UDP ↑ upload** (board→PC, TX) | ~29 Mb/s | ~67 Mb/s | copy TX path, CPU-bound; HP ~2.3× |
| TCP ↑ upload | ~7 Mb/s | ~9 Mb/s | erratic (see below) |
| TCP ↓ download | ~22 Mb/s | ~22 Mb/s | erratic |

```text
UDP ↓ download   HE │█████████████████████░░░░░░░░░░░│ ~62 Mb/s
                 HP │████████████████████████████████│ ~96 Mb/s (wire)
UDP ↑ upload     HE │██████████░░░░░░░░░░░░░░░░░░░░░░░│ ~29 Mb/s
                 HP │███████████████████████░░░░░░░░░│ ~67 Mb/s
```

Both UDP measurements are stable run-to-run. UDP download loss stays <0.1 % up
to each core's ceiling, then the board saturates (drops + console spam corrupt
the readout).

## Takeaway

After tuning, Ethernet throughput **scales with the core**: HP is ~1.5× (RX) to
~2.3× (TX) faster than HE, because the remaining bottleneck is per-packet CPU
(net-stack + socket-layer copy), which the driver tuning cannot remove. HP hits
the 100BASE-T wire on RX; HE tops out ~62 Mb/s.

Pick the core by need: **HP for max Ethernet throughput**, HE when its lower
power/clock is preferred and ~60 Mb/s RX / ~30 Mb/s TX is enough.

## TCP

TCP stays erratic on both cores (sessions intermittently stall — tiny window /
retransmits); single numbers aren't trustworthy. Not addressed by the driver
work; see perf_tuning.md.

## Reproduce

```sh
# HE (Ethernet default-ON): build, erase+flash HE only, serial routing -> HE (uart2)
west build -p always -b alif_e7_dk/ae722f80f55d5xx/rtss_he project/apps/he_app -d build/he
# HP (Ethernet opt-in): build with the flag, erase+flash HP only, serial routing -> HP (uart4)
west build -p always -b alif_e7_dk/ae722f80f55d5xx/rtss_hp project/apps/hp_app -d build/hp -- -DAPP_ETHERNET=ON
./project/tools/scripts/perf_test.py        # then run the matrix
```
