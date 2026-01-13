
// Confidential - Copyright PA.COTTE: All rights reserved

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

#define SRAM2_BASE (0x50000000UL)
#define SRAM2_SIZE 0x00040000 /* 256K */
#define SRAM3_BASE (0x50800000UL)
#define SRAM3_SIZE 0x00100000 /* 1M */

#define DTCM_BASE (0x20000000UL)
#define DTCM_GLOBAL_BASE (SRAM3_BASE)
#define DTCM_REGION_SIZE (0x02000000UL)
#define DTCM_ALIAS_BIT (0x01000000UL)
#define DTCM_SIZE (SRAM3_SIZE) /* 1MB */

#define ITCM_BASE (0x00000000UL)
#define ITCM_GLOBAL_BASE (SRAM2_BASE)
#define ITCM_REGION_SIZE (0x02000000UL)
#define ITCM_ALIAS_BIT (0x01000000UL)
#define ITCM_SIZE (SRAM2_SIZE) /* 256K */

#define CGU_BASE (0x1A602000UL)
#define CLKCTL_PER_MST_BASE (0x4903F000UL)
#define CLKCTL_PER_SLV_BASE (0x4902F000UL)
#define M55_CFG_BASE (0x400F0000UL)
#define PINMUX_BASE (0x1A603000UL)
#define ETH_BASE (0x48100000UL)

#ifdef __cplusplus
}
#endif