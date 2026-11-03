# alif-sample-app

This repository is used to highlight features and bugs related to ALIF's E7 devkit.
Each of them will be in its own branch, and there is a template branch to start from.

## This branch: spi_dw DMA transfers and the CPU data cache

With `CONFIG_SPI_DW_USE_DMA`, the `spi_dw` driver of `zephyr_alif` v2.3.0 hands the caller's
buffers straight to the DMA controller, which reads and writes memory behind the CPU data cache.
The driver does no cache maintenance, and `dma_pl330` only cleans its own microcode. As soon as the
buffers are in cacheable memory and the D-cache is on, the data sent and the data received are
wrong.

The HP application runs SPI1 in internal loopback (`SPI_MODE_LOOP`, nothing to wire) through DMA0:
TX on peripheral request 21, RX on request 17, both in group 2. Its buffers are in SRAM1, which is
cacheable; the default RAM of the target is the DTCM, which is never cached and would hide the
issue. Every 10 s it runs six cases of 20 transfers of 1024 bytes. Before each transfer it writes a
new pattern to the TX buffer and fills the RX buffer, and a margin around it, with a canary byte
(`0xC5`). It then sorts each wrong byte received:

- **canary**: the CPU reads its own cached line instead of what the DMA wrote;
- **previous TX**: the DMA sent the pattern of the previous transfer, still in SRAM while the new one
  sits in the cache;
- **other**: anything else, for instance an older pattern.

It also counts the canary bytes overwritten around the RX buffer.

### The patches

| Patch | Fix |
| ----- | --- |
| `spi-dw-dma-cache-maintenance.patch` | For each chunk, clean the TX range (or the dummy TX word) before the DMA starts. Invalidate the RX range before and after the DMA when it starts and ends on d-cache lines. Any other RX buffer shares lines with other data that an invalidation would discard: it receives through a per-instance bounce buffer aligned on d-cache lines (`CONFIG_SPI_DW_DMA_RX_BOUNCE_SIZE`, 256 bytes by default, only for instances with an `rxdma` channel), and the driver copies it out. A transfer to an unaligned RX buffer longer than the bounce buffer is split into several DMA transfers. |

`alif-ensemble-enable-caches-at-boot.patch` is applied in every build: without it
`CONFIG_INIT_ARCH_HW_AT_BOOT`, which MCUboot implies, leaves the caches off and the issue does not
show (see the `cachesDisabledAtBoot` branch). Both patches are described in
[`project/zephyr/patches.yml`](project/zephyr/patches.yml).

The application also lowers the log level of the DMA event router
(`CONFIG_DMA_ALIF_EVENT_ROUTER_LOG_LEVEL_WRN`): it logs every channel configuration at INFO level,
several lines per transfer, and the log buffer drops the test results.

### Running the demonstration

1. Select the "HP only with MCUboot" configuration (*Configure* task).
2. Build without the fix: `./project/tools/scripts/update.py --without-fixes`, then build and flash
   (*Build* and *Flash* tasks). Read the HP console.
3. Build with the fix: `./project/tools/scripts/update.py`, rebuild and flash.

### Results on an E7 DK

20 transfers per case:

| Case | Without the fix | With the fix |
| ---- | --------------- | ------------ |
| D-cache off, no maintenance by the caller | 20/20 correct | 20/20 correct |
| D-cache on, no maintenance by the caller | 20/20 corrupted: 20400 canary bytes | 20/20 correct |
| D-cache on, the caller cleans TX | 20/20 corrupted: 20400 canary bytes | 20/20 correct |
| D-cache on, the caller invalidates RX | 20/20 corrupted: 80 canary, 1020 previous TX, 19380 other | 20/20 correct |
| D-cache on, the caller cleans TX and invalidates RX | 20/20 correct | 20/20 correct |
| D-cache on, no maintenance, RX buffer unaligned (offset 5, 1000 bytes) | 20/20 corrupted: 19922 canary bytes | 20/20 correct |

No canary byte was overwritten around the RX buffer in any case. Without the fix, a caller only
gets correct data by doing the maintenance on both buffers itself, which drivers built on the SPI
API do not do (`ws2812_spi`, for instance).

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