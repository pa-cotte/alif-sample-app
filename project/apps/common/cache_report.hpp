#pragma once

// Include after LOG_MODULE_REGISTER(): the report logs through the module of the including file.

#include <cmsis_core.h>
#include <cstring>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/** Log the state of the CPU caches and the time of a memcpy between two buffers of the given region. */
template <size_t TSize>
void reportCaches(const char *region, uint8_t (&src)[TSize], uint8_t (&dst)[TSize])
{
  constexpr int ITERATIONS = 1000;
  const uint32_t ccr = SCB->CCR;

  LOG_INF("SCB->CCR = 0x%08x: I-cache %s, D-cache %s", ccr, (ccr & SCB_CCR_IC_Msk) ? "on" : "OFF",
          (ccr & SCB_CCR_DC_Msk) ? "on" : "OFF");

  memset(src, 0x5a, TSize);
  const int64_t start = k_uptime_ticks();
  for (int i = 0; i < ITERATIONS; i++)
  {
    memcpy(dst, src, TSize);
    // Keep the compiler from merging the identical copies.
    __asm__ volatile("" ::: "memory");
  }
  const uint64_t tenthsOfUs = k_ticks_to_us_floor64(k_uptime_ticks() - start) * 10U / ITERATIONS;

  LOG_INF("memcpy of %u bytes in %s: %u.%u us per copy", (unsigned)TSize, region, (unsigned)(tenthsOfUs / 10U),
          (unsigned)(tenthsOfUs % 10U));
}
