/* Override ethosu_address_remap to map local CPU addresses (ITCM/DTCM)
 * to the physical/global addresses exposed on the AXI bus for the NPU.
 *
 * The values below are taken from the generated devicetree in the build
 * and correspond to the `zephyr,memory-region` global_base shown in
 * `build/hp/zephyr/dts.cmake` / `zephyr.dts.pre`.
 */

#include <stdint.h>

uint64_t __attribute__((weak)) ethosu_address_remap(uint64_t address, int index)
{
    (void)index;

    /* ITCM on this platform: CPU addr 0x00000000 -> NPU visible 0x50000000 */
    const uint64_t ITCM_CPU_BASE = 0x00000000ull;
    const uint64_t ITCM_SIZE = 0x40000ull; /* 256 KiB */
    const uint64_t ITCM_GLOBAL = 0x50000000ull;

    /* DTCM on this platform: CPU addr 0x20000000 -> NPU visible 0x50800000 */
    const uint64_t DTCM_CPU_BASE = 0x20000000ull;
    const uint64_t DTCM_SIZE = 0x100000ull; /* 1 MiB */
    const uint64_t DTCM_GLOBAL = 0x50800000ull;

    if (address >= ITCM_CPU_BASE && address < (ITCM_CPU_BASE + ITCM_SIZE))
    {
        return (address - ITCM_CPU_BASE) + ITCM_GLOBAL;
    }

    if (address >= DTCM_CPU_BASE && address < (DTCM_CPU_BASE + DTCM_SIZE))
    {
        return (address - DTCM_CPU_BASE) + DTCM_GLOBAL;
    }

    /* Default: no remap */
    return address;
}
