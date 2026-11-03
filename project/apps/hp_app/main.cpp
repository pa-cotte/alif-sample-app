#include <cmsis_core.h>
#include <cstring>
#include <zephyr/cache.h>
#include <zephyr/device.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(app);

namespace
{

constexpr size_t LEN = 1024;
constexpr uint8_t CANARY = 0xC5;
constexpr int TRANSFERS = 20;
constexpr size_t UNALIGNED_OFFSET = 5;
constexpr size_t UNALIGNED_LEN = 1000;

// Cacheable SRAM1: the DTCM, where Zephyr puts .bss on this board, is never cached and hides the defect.
// Aligned on d-cache lines so that the maintenance done by the caller in some cases touches nothing else.
uint8_t g_tx[LEN] __aligned(32) __attribute__((section(".alif_sram1")));
// Room for canaries around the unaligned receive buffer.
uint8_t g_rx[LEN + 64] __aligned(32) __attribute__((section(".alif_sram1")));

const struct device *const g_spi = DEVICE_DT_GET(DT_NODELABEL(spi1));
// The driver compares the config by address: keep a single instance.
const struct spi_config g_config = {
    .frequency = 4000000,
    .operation = SPI_OP_MODE_MASTER | SPI_TRANSFER_MSB | SPI_WORD_SET(8) | SPI_MODE_LOOP,
    .slave = 0,
};

struct Case
{
  const char *what;
  bool dcache;
  bool callerCleansTx;
  bool callerInvalidatesRx;
  size_t rxOffset;
  size_t len;
};

constexpr Case CASES[] = {
    {"D-cache off, no maintenance by the caller", false, false, false, 0, LEN},
    {"D-cache on,  no maintenance by the caller", true, false, false, 0, LEN},
    {"D-cache on,  caller cleans TX", true, true, false, 0, LEN},
    {"D-cache on,  caller invalidates RX", true, false, true, 0, LEN},
    {"D-cache on,  caller cleans TX and invalidates RX", true, true, true, 0, LEN},
    {"D-cache on,  no maintenance, unaligned RX buffer", true, false, false, UNALIGNED_OFFSET, UNALIGNED_LEN},
};

/** Run TRANSFERS loopback transfers and log how the received bytes differ from the sent ones. */
void runCase(const Case &c)
{
  static uint8_t seed;
  uint8_t *rx = g_rx + c.rxOffset;
  int corrupted = 0;
  size_t staleRx = 0;
  size_t staleTx = 0;
  size_t other = 0;
  size_t canariesHit = 0;

  if (c.dcache)
  {
    sys_cache_data_enable();
  }
  else
  {
    sys_cache_data_disable();
  }

  for (int t = 0; t < TRANSFERS; t++)
  {
    const uint8_t previous = seed++;
    for (size_t i = 0; i < c.len; i++)
    {
      g_tx[i] = static_cast<uint8_t>(i * 7U + seed);
    }
    memset(g_rx, CANARY, sizeof(g_rx));
    if (c.callerCleansTx)
    {
      sys_cache_data_flush_range(g_tx, c.len);
    }
    if (c.callerInvalidatesRx)
    {
      sys_cache_data_flush_and_invd_range(rx, c.len);
    }

    const struct spi_buf txBuf = {.buf = g_tx, .len = c.len};
    const struct spi_buf rxBuf = {.buf = rx, .len = c.len};
    const struct spi_buf_set txSet = {.buffers = &txBuf, .count = 1};
    const struct spi_buf_set rxSet = {.buffers = &rxBuf, .count = 1};
    const int ret = spi_transceive(g_spi, &g_config, &txSet, &rxSet);
    if (ret != 0)
    {
      LOG_ERR("%s: spi_transceive failed: %d", c.what, ret);
      return;
    }
    if (c.callerInvalidatesRx)
    {
      sys_cache_data_invd_range(rx, c.len);
    }

    size_t bad = 0;
    for (size_t i = 0; i < c.len; i++)
    {
      if (rx[i] == g_tx[i])
      {
        continue;
      }
      bad++;
      if (rx[i] == CANARY)
      {
        staleRx++;
      }
      else if (rx[i] == static_cast<uint8_t>(i * 7U + previous))
      {
        staleTx++;
      }
      else
      {
        other++;
      }
    }
    size_t hits = 0;
    for (size_t i = 0; i < sizeof(g_rx); i++)
    {
      if ((i < c.rxOffset || i >= c.rxOffset + c.len) && g_rx[i] != CANARY)
      {
        hits++;
      }
    }
    canariesHit += hits;
    corrupted += (bad != 0 || hits != 0) ? 1 : 0;
  }

  if (corrupted == 0)
  {
    LOG_INF("%-50s: OK, %d/%d transfers correct", c.what, TRANSFERS, TRANSFERS);
  }
  else
  {
    LOG_ERR("%-50s: %d/%d transfers corrupted; bad bytes: %u still the CPU's canary (RX read from the cache), %u from "
            "the previous TX (TX read stale SRAM), %u other; %u canary bytes overwritten around the buffer",
            c.what, corrupted, TRANSFERS, (unsigned)staleRx, (unsigned)staleTx, (unsigned)other, (unsigned)canariesHit);
  }
}

} // namespace

auto main() -> int
{
  if (!device_is_ready(g_spi))
  {
    LOG_ERR("spi1 not ready");
    return 0;
  }

  while (true)
  {
    LOG_INF("spi1 loopback through DMA, %u-byte transfers, buffers in SRAM1 (tx %p, rx %p)", (unsigned)LEN, g_tx, g_rx);
    for (const Case &c : CASES)
    {
      runCase(c);
    }
    // Leave the D-cache on, as configured, between two runs.
    sys_cache_data_enable();
    k_sleep(K_SECONDS(10));
  }
}
