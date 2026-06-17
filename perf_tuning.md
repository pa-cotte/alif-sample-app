# Alif Ethernet driver — config tuning results

Investigation of `project/drivers/eth/eth_alif_mac.c` + networking config to raise
UDP RX throughput (the weakest direction — the board is the receiver and was
starving the MAC). Measured on RTSS-HP, PC→board UDP, board-side zperf goodput.

## Bottlenecks found

| # | Bottleneck | Lever |
|---|---|---|
| 1 | RX DMA ring hard-coded to **8** descriptors/buffers — MAC drops (Rx Buffer Unavailable) as soon as the CPU can't drain in time | new `CONFIG_ETH_ALIF_RX_DESC_COUNT` |
| 2 | Per-packet `k_busy_wait(CONFIG_ETH_ALIF_RX_DELAY_US)` in the RX drain path (ARP workaround) | `CONFIG_ETH_ALIF_RX_DELAY_US=0` (ping/ARP verified still OK) |
| 3 | Small net-pkt RX pool | `CONFIG_NET_PKT_RX_COUNT` / `CONFIG_NET_BUF_RX_COUNT` |
| 4 | Per-packet copy `net_pkt_write()` + cache maintenance — pure CPU | *code* (zero-copy), not config — sets the hard ~75 Mb/s wall |

## Changes (all configuration)

- `RX_DESC_COUNT` 8 → **32** (was a hard `#define`, now `CONFIG_ETH_ALIF_RX_DESC_COUNT`); ETH_DMA region now 95% of 64 KB.
- `CONFIG_ETH_ALIF_RX_DELAY_US` 1 → **0**.
- `CONFIG_NET_PKT_RX_COUNT` 32 → **48**, `CONFIG_NET_BUF_RX_COUNT` 64 → **96**.

(in `project/drivers/eth/Kconfig` + `project/apps/common/eth_zperf.conf`)

## Result — UDP download (PC→board), board-side goodput

| Offered | Before (ring 8, delay 1) | After (ring 32, delay 0, bigger pools) |
|--------:|---|---|
| 30 Mb/s | 31.4 Mb/s, 0.06 % loss | 31.4 Mb/s, 0.01 % loss |
| 40 Mb/s | **saturates** (readout corrupted) | 41.9 Mb/s, 0.03 % loss |
| 50 Mb/s | **saturates** | 52.4 Mb/s, 0.07 % loss |
| 60 Mb/s | **saturates** | 62.8 Mb/s, 0.12 % loss |
| 70 Mb/s | **saturates** | 73.2 Mb/s, 0.32 % loss |
| 80 Mb/s | saturates | still saturates |

**Config-only sustainable UDP RX: ~30 Mb/s → ~70 Mb/s (≈2.3×).**

## Stage 2 — zero-copy RX (driver change, `CONFIG_ETH_ALIF_ZEROCOPY_RX`)

The ~75 Mb/s wall was the per-packet `net_pkt_write()` memcpy (DMA buffer in
SRAM0 → net_buf). Zero-copy wraps the DMA buffer in an external-data `net_buf`
(`net_buf_alloc_with_data`) and recycles it via a free-list (`rx_nb_destroy`),
removing that copy. Needs 2×RX_DESC_COUNT buffers in ETH_DMA, so the ring is 16
(16 armed + 16 spares) here.

| Offered | Stage 1 (copy) | Stage 2 (zero-copy) |
|--------:|---|---|
| 70 Mb/s | 73 Mb/s, 0.32 % | 73 Mb/s, 0.008 % |
| 80 Mb/s | saturates | 84 Mb/s, 0.011 % |
| 90 Mb/s | — | 94 Mb/s, 0.010 % |
| 95–100 Mb/s | — | **95.7 Mb/s, 0.01 % — 100BASE-T wire rate** |

**UDP RX: ~30 Mb/s → ~95.7 Mb/s (≈3.2×), now limited by the 100 Mbit PHY, not
the CPU.** ARP/ping verified; stable over many runs. Enabled in
`project/apps/common/eth_zperf.conf`; the driver Kconfig defaults it off.

## TX path (UDP upload) — investigated, ceiling ~67 Mb/s

UDP upload tops out at ~**67 Mb/s** (≈8 360 pkt/s of 1 KB, 0 % loss). It is the
send path, not generation: when offered 2×, zperf produces ~16 700 pkt/s but
only ~8 360 leave the wire (the rest dropped at `eth_alif_send` -EAGAIN).

Tried, in order:
- **Bigger TX ring (8→32) + enlarged ETH_DMA region** — no change ⇒ TX is not
  descriptor-bound.
- **Zero-copy TX** (`CONFIG_ETH_ALIF_ZEROCOPY_TX`, single-frag DMA + held
  net_pkt ref, with `NET_BUF_DATA_SIZE=1536`) — **regressed** TX to ~23 Mb/s
  with stalls: holding a net_pkt ref per in-flight descriptor starves the TX
  pool faster than TI-interrupt reclaim returns them. **Reverted** (flag left
  default-off; code kept for future work).

So TX stays on the copy path at ~67 Mb/s — bound by per-packet CPU
(zsock_send's copy into 128 B frags + the send-side coalescing copy + per-packet
MMIO), not by the driver ring. Raising it would need a reworked TX reclaim
(faster, not pool-starving) or cutting the upstream socket→net_buf copy — beyond
config tuning.

- **TCP** is still erratic (sessions stall); a separate issue (tiny window /
  retransmits), not addressed here.
