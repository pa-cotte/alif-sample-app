/*
 * Native Zephyr Ethernet Driver for Alif E7 ETH PHY
 */

#pragma once

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C"
{
#endif

  int eth_alif_phy_init(uint8_t phy_addr);

#ifdef __cplusplus
}
#endif
