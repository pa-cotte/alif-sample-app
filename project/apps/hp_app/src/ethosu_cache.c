/* Override cache maintenance functions used by ethosu driver
 * This file provides concrete implementations of the weak
 * functions `ethosu_flush_dcache` and `ethosu_invalidate_dcache`
 * so the HAL driver will perform correct cache maintenance on Zephyr.
 */

#include <zephyr/cache.h>
#include <stdint.h>
#include <stddef.h>

void ethosu_flush_dcache(uint32_t *p, size_t bytes)
{
    if (p == NULL) {
        sys_cache_data_flush_all();
    } else {
        sys_cache_data_flush_range((void *)p, bytes);
    }
}

void ethosu_invalidate_dcache(uint32_t *p, size_t bytes)
{
    if (p == NULL) {
        sys_cache_data_invd_all();
    } else {
        sys_cache_data_invd_range((void *)p, bytes);
    }
}
