// Confidential - Copyright PA.COTTE: All rights reserved

/*
 * Alif E7 Series Registers Map
 */

#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/sys/util.h>

#ifdef CONFIG_RTSS_HP
#include "M55_HP_map.h"
#endif
#ifdef CONFIG_RTSS_HE
#include "M55_HE_map.h"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  /*
   * Alif_E7_HWRM_v2.8.pdf
   *  8.3.2.2 Clock Generation Unit (CGU) Registers Map
   */

#define CGU_OSC_CTRL (CGU_BASE + 0X0)       // Oscillator Control Register
#define CGU_OSC_CTRL_SYS_XTAL_SEL BIT(0)    // Select oscillator clock source
#define CGU_OSC_CTRL_PERIPH_XTAL_SEL BIT(4) // Select oscillator clock source for HFOSC_CLK
#define CGU_OSC_CTRL_CLKMON_ENA BIT(16)     // Enable 38.4 MHz crystal-oscillator clock monitor
#define CGU_OSC_XTAL_DEAD BIT(20)           // 38.4 MHz crystal-oscillator clock monitor status
#define CGU_PLL_LOCK_CTRL CGU_BASE + 0X4    // PLL Lock Control Register
#define CGU_PLL_LOCK_PLL_LOCK BIT(0)        // PLL lock control
#define CGU_PLL_LOCK_PLL_CALIB BIT(4)       // PLL calibration status
// #define CGU_PLL_CLK_SEL                         CGU_BASE + 0X8        // PLL Clock Select Register
#define CGU_PLL_CLK_SEL_SYSREF BIT(0)         // Select the source for SYST_REFCLK
#define CGU_PLL_CLK_SEL_SYS BIT(4)            // Select the source for CPUPLL_CLK and SYSPLL_CLK
#define CGU_PLL_CLK_SEL_ES0 BIT(16)           // Select the source for RTSS_HP_CLK
#define CGU_PLL_CLK_SEL_ES1 BIT(20)           // Select the source for RTSS_HE_CLK
#define CGU_ESCLK_SEL (CGU_BASE + 0X10)       // Clock Select Register for M55-HP and M55-HE
#define CGU_ESCLK_SEL_ES0_PLL GENMASK(1, 0)   // Select PLL clock frequency for RTSS_HP_CLK
#define CGU_ESCLK_SEL_ES1_PLL GENMASK(5, 4)   // Select PLL clock frequency for RTSS_HE_CLK
#define CGU_ESCLK_SEL_ES0_OSC GENMASK(9, 8)   // Select oscillator clock frequency for RTSS_HP_CLK
#define CGU_ESCLK_SEL_ES1_OSC GENMASK(13, 12) // Select oscillator clock frequency for RTSS_HE_CLK
#define CGU_CLK_ENA (CGU_BASE + 0X14)         // Clock Enable Register
#define CGU_CLK_ENA_SYSPLL BIT(0)    // Enable SYSPLL_CLK
#define CGU_CLK_ENA_CPUPLL BIT(4)    // Enable CPUPLL_CLK
#define CGU_CLK_ENA_ES0 BIT(12)      // Enable RTSS_HP_CLK
#define CGU_CLK_ENA_ES1 BIT(13)      // Enable RTSS_HE_CLK
#define CGU_CLK_ENA_CLK160M BIT(20)  // Enable 160M_CLK
#define CGU_CLK_ENA_CLK100M BIT(21)  // Enable 100M_CLK
#define CGU_CLK_ENA_CLK20M BIT(22)   // Enable USB_CLK and 10M_CLK
#define CGU_CLK_ENA_CLK38P4M BIT(23) // Enable HFOSC_CLK
#define CGU_CLK_ENA_CVM BIT(24)      // Enable SRAM0 clock
#define CGU_CLK_ENA_OCVM BIT(28)     // Enable SRAM1 clock
#define CGU_IRQ (CGU_BASE + 0x20)    // CGU Interrupt Status Register

  /*
   * Alif_E7_HWRM_v2.8.pdf
   *  8.3.9.2 M55HP_CFG Registers Map (M55 High Performance Configuration)
   */

#define M55HP_CFG_DMA_CTRL (M55_CFG_BASE + 0X0)   // DMA1 Boot Control Register
#define M55HP_CFG_DMA_CTRL_BOOT_MANAGER BIT(0)    // Security state of the DMA manager thread
#define M55HP_CFG_DMA_CTRL_SW_RST BIT(16)         // Software reset for DMA1
#define M55HP_CFG_DMA_IRQ (M55_CFG_BASE + 0X4)    // DMA1 Boot IRQ Non-Secure Register
#define M55HP_CFG_DMA_PERIPH (M55_CFG_BASE + 0X8) // DMA1 Boot Peripheral Non-Secure Register
#define M55HP_CFG_DMA_SEL (M55_CFG_BASE + 0XC)    // DMA1 Select Register
#define M55HP_CFG_CLK_ENA (M55_CFG_BASE + 0X10)   // Peripheral Clock Enable Register
#define M55HP_CFG_CLK_ENA_NPU_CKEN BIT(0)         // Enable clock for DMA2 and EVTRTR2
#define M55HP_CFG_CLK_ENA_DMA_CKEN BIT(4)         // Enable clock for NPU-HE

  /*
   * Alif_E7_HWRM_v2.8.pdf
   *  8.3.4.2 CLKCTL_PER_MST Registers Map (Peripheral Clock Master)
   */

#define CLKCTL_PER_MST_CAMERA_PIXCLK_CTRL (CLKCTL_PER_MST_BASE + 0X0) // CPI Pixel Clock Control Register
#define CLKCTL_PER_MST_CDC200_PIXCLK_CTRL (CLKCTL_PER_MST_BASE + 0X4) // CDC Pixel Clock Control Register
#define CLKCTL_PER_MST_CSI_PIXCLK_CTRL (CLKCTL_PER_MST_BASE + 0X8)    // CSI Pixel Clock Control Register
#define CLKCTL_PER_MST_PERIPH_CLK_ENA (CLKCTL_PER_MST_BASE + 0XC)     // Peripheral Clock Enable Register
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_CPI_CKEN BIT(0)                 // Enable clock for CPI
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_DPI_CKEN BIT(1)                 // Enable clock for DPI controller (CDC)
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_DMA_CKEN BIT(4)                 // Enable clock for DMA0
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_GPU_CKEN BIT(8)                 // Enable clock for GPU2D
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_ETH_CKEN BIT(12)                // Enable clock for ETH
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_SDC_CKEN BIT(16)                // Enable clock for SDMMC
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_USB_CKEN BIT(20)                // Enable clock for USB
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_CSI_CKEN BIT(24)                // Enable clock for CSI
#define CLKCTL_PER_MST_PERIPH_CLK_ENA_DSI_CKEN BIT(28)                // Enable clock for DSI
#define CLKCTL_PER_MST_DPHY_PLL_CTRL0 (CLKCTL_PER_MST_BASE + 0X10)    // MIPI-DPHY PLL Control Register 0
#define CLKCTL_PER_MST_DPHY_PLL_CTRL1 (CLKCTL_PER_MST_BASE + 0X14)    // MIPI-DPHY PLL Control Register 1
#define CLKCTL_PER_MST_DPHY_PLL_CTRL2 (CLKCTL_PER_MST_BASE + 0X18)    // MIPI-DPHY PLL Control Register 2
#define CLKCTL_PER_MST_DPHY_PLL_STAT0 (CLKCTL_PER_MST_BASE + 0X20)    // MIPI-DPHY PLL Status Register 0
#define CLKCTL_PER_MST_DPHY_PLL_STAT1 (CLKCTL_PER_MST_BASE + 0X24)    // MIPI-DPHY PLL Status Register 1
#define CLKCTL_PER_MST_TX_DPHY_CTRL0 (CLKCTL_PER_MST_BASE + 0X30)     // MIPI-DPHY TX Control Register 0
#define CLKCTL_PER_MST_TX_DPHY_CTRL1 (CLKCTL_PER_MST_BASE + 0X34)     // MIPI-DPHY TX Control Register 1
#define CLKCTL_PER_MST_RX_DPHY_CTRL0 (CLKCTL_PER_MST_BASE + 0X38)     // MIPI-DPHY RX Control Register 0
#define CLKCTL_PER_MST_RX_DPHY_CTRL1 (CLKCTL_PER_MST_BASE + 0X3C)     // MIPI-DPHY RX Control Register 1
#define CLKCTL_PER_MST_MIPI_CKEN (CLKCTL_PER_MST_BASE + 0X40)         // MIPI-DPHY Clock Enable Register
#define CLKCTL_PER_MST_DSI_CTRL (CLKCTL_PER_MST_BASE + 0X44)          // DSI Control Register
#define CLKCTL_PER_MST_DMA_CTRL (CLKCTL_PER_MST_BASE + 0X70)          // DMA0 Boot Control Register
#define CLKCTL_PER_MST_DMA_CTRL_BOOT_MANAGER BIT(0)                   // Controls the security state of the DMA manager thread
#define CLKCTL_PER_MST_DMA_CTRL_SW_RST BIT(16)                        // Software reset for DMA0
#define CLKCTL_PER_MST_DMA_IRQ (CLKCTL_PER_MST_BASE + 0X74)           // DMA0 Boot IRQ Non-Secure Register
#define CLKCTL_PER_MST_DMA_PERIPH (CLKCTL_PER_MST_BASE + 0X78)        // DMA0 Boot Peripheral Non-Secure Register
#define CLKCTL_PER_MST_DMA_GLITCH_FLT (CLKCTL_PER_MST_BASE + 0X7C)    // DMA0 Glitch Filter Register
#define CLKCTL_PER_MST_ETH_CTRL0 (CLKCTL_PER_MST_BASE + 0X80)         // ETH Control Register
#define CLKCTL_PER_MST_ETH_CTRL0_PWR_DOWN_CTRL_I BIT(0)               // ETH power down control
#define CLKCTL_PER_MST_ETH_CTRL0_PWR_ISOLATE_I BIT(1)                 // Isolation cells enable
#define CLKCTL_PER_MST_ETH_CTRL0_PWR_CLAMP_CTRL_I BIT(2)              // ETH always-ON logic reset control
#define CLKCTL_PER_MST_ETH_CTRL0_SBD_FLOWCTRL_I BIT(3)                // ETH sideband flow control
#define CLKCTL_PER_MST_ETH_CTRL0_RMII_CLKSEL BIT(4)                   // Select RMII clock source
#define CLKCTL_PER_MST_ETH_STAT0 (CLKCTL_PER_MST_BASE + 0X84)         // ETH Status Register
#define CLKCTL_PER_MST_ETH_STAT0_SBD_INTR_O BIT(0)                    // Value of the ETH_SBD_IRQ interrupt
#define CLKCTL_PER_MST_ETH_STAT0_SBD_PWR_DOWN_ACK_O BIT(3)            // ETH power-down sequence acknowledge
#define CLKCTL_PER_MST_ETH_STAT0_MAC_SPEED_O GENMASK(5, 4)            // Indicates the MAC speed mode selected
#define CLKCTL_PER_MST_ETH_PTP_TMST0 (CLKCTL_PER_MST_BASE + 0X88)     // ETH Timestamp Register 0
#define CLKCTL_PER_MST_ETH_PTP_TMST1 (CLKCTL_PER_MST_BASE + 0X8C)     // ETH Timestamp Register 1
#define CLKCTL_PER_MST_SDC_CTRL0 (CLKCTL_PER_MST_BASE + 0X90)         // SDMMC Control Register
#define CLKCTL_PER_MST_SDC_STAT0 (CLKCTL_PER_MST_BASE + 0X94)         // SDMMC Status Register 0
#define CLKCTL_PER_MST_SDC_STAT1 (CLKCTL_PER_MST_BASE + 0X98)         // SDMMC Status Register 1
#define CLKCTL_PER_MST_USB_GPIO0 (CLKCTL_PER_MST_BASE + 0XA0)         // USB GPIO Register
#define CLKCTL_PER_MST_USB_STAT0 (CLKCTL_PER_MST_BASE + 0XA4)         // USB Status Register
#define CLKCTL_PER_MST_USB_CTRL1 (CLKCTL_PER_MST_BASE + 0XA8)         // USB Control Register 1
#define CLKCTL_PER_MST_USB_CTRL2 (CLKCTL_PER_MST_BASE + 0XAC)         // USB Control Register 2

  /*
   * Alif_E7_HWRM_v2.8.pdf
   *  8.5.3 PINMUX Registers Guide
   */

#define PINMUX_PN_I(n, i) (PINMUX_BASE + (0x20 * (n)) + (4 * (i))) // Control Register for Port n, Pin i
#define PINMUX_PN_I_PINMUX GENMASK(2, 0)                           // Pin multiplexing control field
#define PINMUX_PN_I_PINMUX_SHIFT 0
#define PINMUX_PN_I_REN BIT(16)       // Receiver enable
#define PINMUX_PN_I_SMT BIT(17)       // Schmitt trigger (hysteresis) enable
#define PINMUX_PN_I_SR BIT(18)        // Slew rate
#define PINMUX_PN_I_P GENMASK(20, 19) // Driver disabled state control
#define PINMUX_PN_I_P_SHIFT 19
#define PINMUX_PN_I_E GENMASK(22, 21) // Output drive strength
#define PINMUX_PN_I_E_SHIFT 21
#define PINMUX_PN_I_DRV BIT(23) // Driver type

  /*
   * Alif_E7_HWRM_v2.8.pdf
   *  15.3.6.3 ETH Registers Description
   */

#define ETH_MAC_CONFIGURATION (ETH_BASE + 0X0)     // MAC Configuration Register
#define ETH_MAC_CONFIGURATION_RE BIT(0)            // Receiver Enable
#define ETH_MAC_CONFIGURATION_TE BIT(1)            // Transmitter Enable
#define ETH_MAC_CONFIGURATION_PRELEN GENMASK(3, 2) // Preamble Length for Transmit packets
#define ETH_MAC_CONFIGURATION_DC BIT(4)            // Deferral Check
#define ETH_MAC_CONFIGURATION_BL GENMASK(6, 5)     // Back-Off Limit
#define ETH_MAC_CONFIGURATION_DR BIT(8)            // Disable Retry
#define ETH_MAC_CONFIGURATION_DCRS BIT(9)          // Disable Carrier Sense During Transmission
#define ETH_MAC_CONFIGURATION_DO BIT(10)           // Disable Receive Own
#define ETH_MAC_CONFIGURATION_ECRSFD BIT(11)       // Enable Carrier Sense Before Transmission in Full-Duplex Mode
#define ETH_MAC_CONFIGURATION_LM BIT(12)           // Loopback Mode
#define ETH_MAC_CONFIGURATION_DM BIT(13)           // Duplex Mode
#define ETH_MAC_CONFIGURATION_FES BIT(14)          // Speed
#define ETH_MAC_CONFIGURATION_PS BIT(15)           // Port Select
#define ETH_MAC_CONFIGURATION_JE BIT(16)           // Jumbo Packet Enable
#define ETH_MAC_CONFIGURATION_JDE BIT(17)          // Jabber Disable
#define ETH_MAC_CONFIGURATION_WD BIT(19)           // Watchdog Disable
#define ETH_MAC_CONFIGURATION_ACS BIT(20)          // Automatic Pad or CRC Stripping
#define ETH_MAC_CONFIGURATION_CST BIT(21)          // CRC stripping for Type packets
#define ETH_MAC_CONFIGURATION_S2KP BIT(22)         // IEEE 802.3as Support for 2KB Packets
#define ETH_MAC_CONFIGURATION_GPSLCE BIT(23)       // Giant Packet Size Limit Control Enable
#define ETH_MAC_CONFIGURATION_IPG GENMASK(26, 24)  // Inter-Packet Gap
#define ETH_MAC_CONFIGURATION_IPC BIT(27)          // Checksum Offload
#define ETH_MAC_CONFIGURATION_ARPEN BIT(31)        // ARP Offload Enable
#define ETH_MAC_EXT_CONFIGURATION (ETH_BASE + 0X4) // MAC Extended Configuration Register
#define ETH_MAC_PACKET_FILTER (ETH_BASE + 0X8)     // MAC Packet Filter Register
#define ETH_MAC_PACKET_FILTER_PR BIT(0)            // Promiscuous Mode
#define ETH_MAC_PACKET_FILTER_DAIF BIT(3)          // DA Inverse Filtering
#define ETH_MAC_PACKET_FILTER_PM BIT(4)            // Pass All Multicast
#define ETH_MAC_PACKET_FILTER_DBF BIT(5)           // Disable Broadcast Packets
#define ETH_MAC_PACKET_FILTER_PCF GENMASK(7, 6)    // Pass Control Packets
#define ETH_MAC_PACKET_FILTER_VTFE BIT(16)         // VLAN Tag Filter Enable
#define ETH_MAC_PACKET_FILTER_RA BIT(31)           // Receive All
#define ETH_MAC_WATCHDOG_TIMEOUT (ETH_BASE + 0XC)  // Watchdog Timeout Register
#define ETH_MAC_VLAN_TAG (ETH_BASE + 0X50)         // VLAN Tag Register
#define ETH_MAC_Q0_TX_FLOW_CTRL (ETH_BASE + 0X70)  // Flow Control Register
#define ETH_MAC_Q0_TX_FLOW_CTRL_FCB_BPA BIT(0)     // Flow Control Busy or Backpressure Activate
#define ETH_MAC_Q0_TX_FLOW_CTRL_TFE BIT(1)         // Transmit Flow Control Enable
#define ETH_MAC_Q0_TX_FLOW_CTRL_PLT GENMASK(6, 4)  // Pause Low Threshold
#define ETH_MAC_Q0_TX_FLOW_CTRL_DZPQ BIT(7)        // Disable Zero-Quanta Pause
#define ETH_MAC_Q0_TX_FLOW_CTRL_PT GENMASK(31, 16) // Pause Time
#define ETH_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT 16
#define ETH_MAC_RX_FLOW_CTRL (ETH_BASE + 0X90)       // Receive Flow Control Register
#define ETH_MAC_RX_FLOW_CTRL_RFE BIT(0)              // Receive Flow Control Enable
#define ETH_MAC_RX_FLOW_CTRL_UP BIT(1)               // Unicast Pause Packet Detect
#define ETH_MAC_INTERRUPT_ENABLE (ETH_BASE + 0XB4)   // Interrupt Enable Register
#define ETH_MAC_INTERRUPT_STATUS (ETH_BASE + 0XB0)   // Interrupt Status Register
#define ETH_MAC_RX_TX_STATUS (ETH_BASE + 0XB8)       // Receive Transmit Status Register
#define ETH_MAC_PMT_CONTROL_STATUS (ETH_BASE + 0XC0) // PMT Control and Status Register
#define ETH_RWK_FILTER_BYTE_MASK (ETH_BASE + 0XC4)   // Remote Wakeup Filter Byte Mask Register
#define ETH_RWK_FILTER_COMMAND (ETH_BASE + 0XC4)     // Remote Wakeup Filter Command Register
#define ETH_RWK_FILTER_OFFSET (ETH_BASE + 0XC4)      // Remote Wakeup Filter Offset Register
#define ETH_RWK_FILTER_CRC (ETH_BASE + 0XC4)         // Remote Wakeup Filter CRC-16 Register
#define ETH_MAC_VERSION (ETH_BASE + 0x110)           // Module Version Register
#define ETH_MAC_DEBUG (ETH_BASE + 0x114)             // Debug Register
#define ETH_MAC_HW_FEATURE0 (ETH_BASE + 0x11C)       // ETH Hardware Feature Register 0
#define ETH_MAC_HW_FEATURE0_MIISEL BIT(0)            // 10 or 100 Mbps Support
#define ETH_MAC_HW_FEATURE0_GMIISEL BIT(1)           // 1000 Mbps Support
#define ETH_MAC_HW_FEATURE0_HDSEL BIT(2)             // Half-duplex Support
#define ETH_MAC_HW_FEATURE0_PCSSEL BIT(3)            // PCS Registers (TBI, SGMII, or RTBI PHY interface)
#define ETH_MAC_HW_FEATURE1 (ETH_BASE + 0x120)       // ETH Hardware Feature Register 1
#define ETH_MAC_HW_FEATURE2 (ETH_BASE + 0x124)       // ETH Hardware Feature Register 2
#define ETH_MAC_HW_FEATURE3 (ETH_BASE + 0x128)       // ETH Hardware Feature Register 3
#define ETH_MAC_MDIO_ADDRESS (ETH_BASE + 0x200)      // MDIO Address Register
#define ETH_MAC_MDIO_ADDRESS_GB BIT(0)               // RMII Busy
#define ETH_MAC_MDIO_ADDRESS_C45E BIT(1)             // Clause 45 PHY Enable
#define ETH_MAC_MDIO_ADDRESS_GOC_O BIT(2)            // Operation Command 0
#define ETH_MAC_MDIO_ADDRESS_GOC_1 BIT(3)            // Operation Command 1
#define ETH_MAC_MDIO_ADDRESS_GOC_SHIFT 2
#define ETH_MAC_MDIO_ADDRESS_GOC_READ 0x3
#define ETH_MAC_MDIO_ADDRESS_GOC_WRITE 0x1
#define ETH_MAC_MDIO_ADDRESS_SKAP BIT(4)       // Skip Address Packet
#define ETH_MAC_MDIO_ADDRESS_CR GENMASK(11, 8) // CSR Clock (CLK_CSR) Range
#define ETH_MAC_MDIO_ADDRESS_CR_SHIFT 8
#define ETH_MAC_MDIO_ADDRESS_CR_150_250 0X4
#define ETH_MAC_MDIO_ADDRESS_NTC GENMASK(14, 12) // Number of Trailing Clocks
#define ETH_MAC_MDIO_ADDRESS_RDA GENMASK(20, 16) // Register or Device Address
#define ETH_MAC_MDIO_ADDRESS_RDA_SHIFT 16
#define ETH_MAC_MDIO_ADDRESS_PA GENMASK(25, 21) // Physical Layer Address
#define ETH_MAC_MDIO_ADDRESS_PA_SHIFT 21
#define ETH_MAC_MDIO_ADDRESS_BTB BIT(26)                             // Back to Back Transactions
#define ETH_MAC_MDIO_ADDRESS_PSE BIT(27)                             // Preamble Suppression Enable
#define ETH_MAC_MDIO_DATA (ETH_BASE + 0x204)                         // MDIO Data Register
#define ETH_MAC_GPIO_CONTROL (ETH_BASE + 0x208)                      // GPIO Control Register
#define ETH_MAC_GPIO_STATUS (ETH_BASE + 0x20C)                       // GPIO Status Register
#define ETH_MAC_ARP_ADDRESS (ETH_BASE + 0x210)                       // ARP Address Register
#define ETH_MAC_CSR_SW_CTRL (ETH_BASE + 0x230)                       // CSR Software Control Register
#define ETH_MAC_ADDRESS0_HIGH (ETH_BASE + 0x300)                     // MAC Address 0 High Register
#define ETH_MAC_ADDRESS0_HIGH_AE BIT(31)                             // Address Enable
#define ETH_MAC_ADDRESS0_LOW (ETH_BASE + 0x304)                      // MAC Address 0 Low Register
#define ETH_MAC_TIMESTAMP_CONTROL (ETH_BASE + 0xB00)                 // Timestamp Control Register
#define ETH_MAC_SUB_SECOND_INCREMENT (ETH_BASE + 0xB04)              // Sub-second Increment Register
#define ETH_MAC_SYSTEM_TIME_SECONDS (ETH_BASE + 0xB08)               // System Time Seconds Register
#define ETH_MAC_SYSTEM_TIME_NANOSECONDS (ETH_BASE + 0xB0C)           // System Time Nanoseconds Register
#define ETH_MAC_SYSTEM_TIME_SECONDS_UPDATE (ETH_BASE + 0xB10)        // System Time Seconds Update Register
#define ETH_MAC_SYSTEM_TIME_NANOSECONDS_UPDATE (ETH_BASE + 0xB14)    // System Time Nanoseconds Update Register
#define ETH_MAC_TIMESTAMP_ADDEND (ETH_BASE + 0xB18)                  // Timestamp Addend Register
#define ETH_MAC_TIMESTAMP_STATUS (ETH_BASE + 0xB20)                  // Timestamp Status Register
#define ETH_MAC_TX_TIMESTAMP_STATUS_NANOSECONDS (ETH_BASE + 0xB30)   // Transmit Timestamp Status Nanoseconds Register
#define ETH_MAC_TX_TIMESTAMP_STATUS_SECONDS (ETH_BASE + 0xB34)       // Transmit Timestamp Status Seconds Register
#define ETH_MAC_TIMESTAMP_INGRESS_CORR_NANOSECOND (ETH_BASE + 0xB58) // Timestamp Ingress Correction Nanoseconds Register
#define ETH_MAC_TIMESTAMP_EGRESS_CORR_NANOSECOND (ETH_BASE + 0xB5C)  // Timestamp Egress Correction Nanoseconds Register
#define ETH_MAC_TIMESTAMP_INGRESS_LATENCY (ETH_BASE + 0xB68)         // Ingress MAC latency Register
#define ETH_MAC_TIMESTAMP_EGRESS_LATENCY (ETH_BASE + 0xB6C)          // Egress MAC latency Register
#define ETH_MAC_PPS_CONTROL (ETH_BASE + 0xB70)                       // PPS Control Register
#define ETH_MAC_PPS0_TARGET_TIME_SECONDS (ETH_BASE + 0xB80)          // PPS0 Target Time Seconds Register
#define ETH_MAC_PPS0_TARGET_TIME_NANOSECONDS (ETH_BASE + 0xB84)      // PPS0 Target Time Nanoseconds Register
#define ETH_MTL_OPERATION_MODE (ETH_BASE + 0xC00)                    // Operation Mode Register
#define ETH_MTL_DBG_STS (ETH_BASE + 0xC10)                           // FIFO Debug Data Register
#define ETH_MTL_DBG_CTL (ETH_BASE + 0xC08)                           // FIFO Debug Access Control and Status Register
#define ETH_MTL_INTERRUPT_STATUS (ETH_BASE + 0xC20)                  // MTL Interrupt Status Register
#define ETH_MTL_TXQ0_OPERATION_MODE (ETH_BASE + 0xD00)               // Queue 0 Transmit Operation Mode Register
#define ETH_MTL_TXQ0_OPERATION_MODE_FTQ BIT(0)                       // Flush Transmit Queue
#define ETH_MTL_TXQ0_OPERATION_MODE_TSF BIT(1)                       // Transmit Store and Forward
#define ETH_MTL_TXQ0_OPERATION_MODE_TTC GENMASK(6, 4)                // Transmit Threshold Control
#define ETH_MTL_TXQ0_OPERATION_MODE_TXQEN GENMASK(3, 2)              // Transmit Queue Enable Mask
#define ETH_MTL_TXQ0_UNDERFLOW (ETH_BASE + 0xD04)                    // Queue 0 Underflow Counter Register
#define ETH_MTL_TXQ0_DEBUG (ETH_BASE + 0xD08)                        // Queue 0 Transmit Debug Register
#define ETH_MTL_Q0_INTERRUPT_CONTROL_STATUS (ETH_BASE + 0xD2C)       // Queue 0 Interrupt Enable and Status Register
#define ETH_MTL_RXQ0_OPERATION_MODE (ETH_BASE + 0xD30)               // Queue 0 Receive Operation Mode Register
#define ETH_MTL_RXQ0_OPERATION_MODE_RTC GENMASK(1, 0)                // Receive Queue Threshold Control
#define ETH_MTL_RXQ0_OPERATION_MODE_FUP BIT(3)                       // Forward Undersized Good Packets
#define ETH_MTL_RXQ0_OPERATION_MODE_FEP BIT(4)                       // Forward Error Packets
#define ETH_MTL_RXQ0_OPERATION_MODE_RSF BIT(5)                       // Receive Queue Store and Forward
#define ETH_MTL_RXQ0_OPERATION_MODE_DIS_TCP_EF BIT(6)                // Disable Dropping of TCP/IP Checksum Error Packets
#define ETH_MTL_RXQ0_MISSED_PACKET_OVERFLOW_CNT (ETH_BASE + 0xD34)   // Queue 0 Missed Packet and Overflow Counter Register
#define ETH_MTL_RXQ0_DEBUG (ETH_BASE + 0xD38)                        // Queue 0 Receive Debug Register
#define ETH_DMA_MODE (ETH_BASE + 0x1000)                             // Bus Mode Register
#define ETH_DMA_MODE_SWR BIT(0)                                      // Software Reset
#define ETH_DMA_MODE_DSPW BIT(8)                                     // Descriptor Posted Write
#define ETH_DMA_MODE_INTM GENMASK(17, 16)                            // Interrupt Mode
#define ETH_DMA_SYSBUS_MODE (ETH_BASE + 0x1004)                      // System Bus Mode Register
#define ETH_DMA_SYSBUS_MODE_FB BIT(0)                                // Fixed Burst Length
#define ETH_DMA_SYSBUS_MODE_BLEN4 BIT(1)                             // AXI Burst Length 4
#define ETH_DMA_SYSBUS_MODE_BLEN8 BIT(2)                             // AXI Burst Length 8
#define ETH_DMA_SYSBUS_MODE_BLEN16 BIT(3)                            // AXI Burst Length 16
#define ETH_DMA_SYSBUS_MODE_BLEN32 BIT(4)                            // AXI Burst Length 32
#define ETH_DMA_SYSBUS_MODE_BLEN64 BIT(5)                            // AXI Burst Length 64
#define ETH_DMA_SYSBUS_MODE_BLEN128 BIT(6)                           // AXI Burst Length 128
#define ETH_DMA_SYSBUS_MODE_BLEN256 BIT(7)                           // AXI Burst Length 256
#define ETH_DMA_SYSBUS_MODE_AALE BIT(10)                             // Automatic AXI LPI enable
#define ETH_DMA_SYSBUS_MODE_AAL BIT(12)                              // Address-Aligned Beats
#define ETH_DMA_SYSBUS_MODE_ONEKBBE BIT(13)                          // 1KB Boundary Crossing Enable for the AXI Master
#define ETH_DMA_SYSBUS_MODE_RD_OSR_LMT GENMASK(17, 16)               // AXI Maximum Read Outstanding Request Limit
#define ETH_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT 16
#define ETH_DMA_SYSBUS_MODE_WR_OSR_LMT GENMASK(25, 24) // AXI Maximum Write Outstanding Request Limit
#define ETH_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT 24
#define ETH_DMA_SYSBUS_MODE_LPI_XIT_PKT BIT(30)      // Unlock on Magic Packet or Remote Wake-Up Packet
#define ETH_DMA_SYSBUS_MODE_EN_LPI BIT(31)           // Enable Low Power Interface (LPI)
#define ETH_DMA_INTERRUPT_STATUS (ETH_BASE + 0x1008) // DMA, MTL, and MAC Interrupt Status Register
#define ETH_DMA_DEBUG_STATUS0 (ETH_BASE + 0x100C)    // Debug Status 0 Register
#define ETH_DMA_DEBUG_STATUS0_AXWHSTS BIT(0)         // AXI Master Write Channel Status
#define ETH_DMA_DEBUG_STATUS0_AXRHSTS BIT(1)         // AXI Master Read Channel Status
#define ETH_DMA_DEBUG_STATUS0_RPS0 GENMASK(11, 8)    // DMA Channel 0 Receive Process State
#define ETH_DMA_DEBUG_STATUS0_RPS0_SHIFT 8
#define ETH_DMA_DEBUG_STATUS0_TPS0 GENMASK(15, 12) // DMA Channel 0 Transmit Process State
#define ETH_DMA_DEBUG_STATUS0_TPS0_SHIFT 12
#define ETH_AXI_LPI_ENTRY_INTERVAL (ETH_BASE + 0x1040) // AXI LPI Entry Interval Register
#define ETH_DMA_CH0_CONTROL (ETH_BASE + 0x1100)        // DMA Channel 0 Control Register
#define ETH_DMA_CH0_TX_CONTROL (ETH_BASE + 0x1104)     // DMA Channel 0 Transmit Control Register
#define ETH_DMA_CH0_TX_CONTROL_ST BIT(0)               // Start or Stop Transmission Command
#define ETH_DMA_CH0_TX_CONTROL_OSF BIT(4)              // Operate on Second Packet
#define ETH_DMA_CH0_TX_CONTROL_TSE BIT(12)             // TCP Segmentation Enabled
#define ETH_DMA_CH0_TX_CONTROL_IPBL BIT(15)            // Ignore PBL Requirement
#define ETH_DMA_CH0_TX_CONTROL_TXPB GENMASK(21, 16)    // Transmit Programmable Burst Length
#define ETH_DMA_CH0_TX_CONTROL_TXPB_SHIFT 16
#define ETH_DMA_CH0_RX_CONTROL (ETH_BASE + 0x1108)    // DMA Channel 0 Receive Control Register
#define ETH_DMA_CH0_RX_CONTROL_SR BIT(0)              // Start or Stop Receive
#define ETH_DMA_CH0_RX_CONTROL_RBSZ_X_0 GENMASK(3, 1) // Receive Buffer Size Low
#define ETH_DMA_CH0_RX_CONTROL_RBSZ_X_0_SHIFT 1
#define ETH_DMA_CH0_RX_CONTROL_RBSZ_13_Y GENMASK(14, 4) // Receive Buffer Size High
#define ETH_DMA_CH0_RX_CONTROL_RBSZ_13_Y_SHIFT 4
#define ETH_DMA_CH0_RX_CONTROL_RXPBL GENMASK(21, 16) // Receive Programmable Burst Length
#define ETH_DMA_CH0_RX_CONTROL_RXPBL_SHIFT 16
#define ETH_DMA_CH0_RX_CONTROL_RFP BIT(31)                          // Rx Packet Flush
#define ETH_DMA_CH0_TXDESC_LIST_ADDRESS (ETH_BASE + 0x1114)         // DMA Channel 0 Transmit Descriptor List Address Register
#define ETH_DMA_CH0_RXDESC_LIST_ADDRESS (ETH_BASE + 0x111C)         // DMA Channel 0 Receive Descriptor List Address Register
#define ETH_DMA_CH0_TXDESC_TAIL_POINTER (ETH_BASE + 0x1120)         // DMA Channel 0 Transmit Descriptor Tail Pointer Register
#define ETH_DMA_CH0_RXDESC_TAIL_POINTER (ETH_BASE + 0x1128)         // DMA Channel 0 Receive Descriptor Tail Pointer Register
#define ETH_DMA_CH0_TXDESC_RING_LENGTH (ETH_BASE + 0x112C)          // DMA Channel 0 Transmit Descriptor Ring Length Register
#define ETH_DMA_CH0_RXDESC_RING_LENGTH (ETH_BASE + 0x1130)          // DMA Channel 0 Receive Descriptor Ring Length Register
#define ETH_DMA_CH0_INTERRUPT_ENABLE (ETH_BASE + 0x1134)            // DMA Channel 0 Interrupt Enable Register
#define ETH_DMA_CH0_INTERRUPT_ENABLE_TIE BIT(0)                     // Transmit Interrupt Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_TXSE BIT(1)                    // Transmit Stopped Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_TBUE BIT(2)                    // Transmit Buffer Unavailable Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_RIE BIT(6)                     // Receive Interrupt Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_RBUE BIT(7)                    // Receive Buffer Unavailable Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_RSE BIT(8)                     // Receive Stopped Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_RWTE BIT(9)                    // Receive Watchdog Timeout Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_ETIE BIT(10)                   // Early Transmit Interrupt Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_ERIE BIT(11)                   // Early Receive Interrupt Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_FBEE BIT(12)                   // Fatal Bus Error Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_CDEE BIT(13)                   // Context Descriptor Error Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_AIE BIT(14)                    // Abnormal Interrupt Summary Enable
#define ETH_DMA_CH0_INTERRUPT_ENABLE_NIE BIT(15)                    // Normal Interrupt Summary Enable
#define ETH_DMA_CH0_RX_INTERRUPT_WATCHDOG_TIMER (ETH_BASE + 0x1138) // DMA Channel 0 Receive Interrupt Watchdog Timer Register
#define ETH_DMA_CH0_CURRENT_APP_TXDESC (ETH_BASE + 0x1144)          // DMA Channel 0 Current Application Transmit Descriptor Register
#define ETH_DMA_CH0_CURRENT_APP_RXDESC (ETH_BASE + 0x114C)          // DMA Channel 0 Current Application Receive Descriptor Register
#define ETH_DMA_CH0_CURRENT_APP_TXBUFFER (ETH_BASE + 0x1154)        // DMA Channel 0 Current Application Transmit Buffer Address Register
#define ETH_DMA_CH0_CURRENT_APP_RXBUFFER (ETH_BASE + 0x115C)        // DMA Channel 0 Current Application Receive Buffer Address Register
#define ETH_DMA_CH0_STATUS (ETH_BASE + 0x1160)                      // DMA Channel 0 Status Register
#define ETH_DMA_CH0_STATUS_TI BIT(0)                                // Transmit Interrupt
#define ETH_DMA_CH0_STATUS_TPS BIT(1)                               // Transmit Process Stopped
#define ETH_DMA_CH0_STATUS_TBU BIT(2)                               // Transmit Buffer Unavailable
#define ETH_DMA_CH0_STATUS_RI BIT(6)                                // Receive Interrupt
#define ETH_DMA_CH0_STATUS_RBU BIT(7)                               // Receive Buffer Unavailable
#define ETH_DMA_CH0_STATUS_RPS BIT(8)                               // Receive Process Stopped
#define ETH_DMA_CH0_STATUS_RWT BIT(9)                               // Receive Watchdog Timeout
#define ETH_DMA_CH0_STATUS_ETI BIT(10)                              // Early Transmit Interrupt
#define ETH_DMA_CH0_STATUS_ERI BIT(11)                              // Early Receive Interrupt
#define ETH_DMA_CH0_STATUS_FBE BIT(12)                              // Fatal Bus Error
#define ETH_DMA_CH0_STATUS_CDE BIT(13)                              // Context Descriptor Error
#define ETH_DMA_CH0_STATUS_AIS BIT(14)                              // Abnormal Interrupt Summary
#define ETH_DMA_CH0_STATUS_NIS BIT(15)                              // Normal Interrupt Summary
#define ETH_DMA_CH0_STATUS_TEB GENMASK(18, 16)                      // Tx DMA Error Bits
#define ETH_DMA_CH0_STATUS_REB GENMASK(21, 19)                      // Rx DMA Error Bits
#define ETH_DMA_CH0_MISS_FRAME_CNT (ETH_BASE + 0x1164)              // DMA Channel 0 Dropped Packet Counter Register
#define ETH_DMA_CH0_RX_ERI_CNT (ETH_BASE + 0x1168)                  // DMA Channel 0 Receive ERI Counter Register

  /*
   * DMA Descriptors for enhanced layouts
   * Derived from Linux drivers (alif_linux)
   *  drivers/net/ethernet/stmicro/stmmac/dwmac4_descs.h
   */

  /*
   * local CPU-visible to global DMA-visible address
   */
  static inline uint32_t local_to_global(const volatile void *local_addr)
  {
    //
    // Map local TCM address to global address space
    // Keep other memories (SRAM0/1, MRAM, OctalSPI) address unchanged
    //

    uint32_t addr = (uint32_t)local_addr;
    if ((addr >= DTCM_BASE) && (addr < (DTCM_BASE + DTCM_REGION_SIZE)))
      return (addr & (DTCM_ALIAS_BIT - 1)) + DTCM_GLOBAL_BASE;
    else if ((addr < (ITCM_BASE + ITCM_REGION_SIZE)))
      return (addr & (ITCM_ALIAS_BIT - 1)) + ITCM_GLOBAL_BASE;
    else
      return (addr);
  }

  /*
   * global DMA-visible to local CPU-visible address
   */
  static inline void *global_to_local(uint32_t addr)
  {
    if ((addr >= DTCM_GLOBAL_BASE) && (addr < (DTCM_GLOBAL_BASE + DTCM_SIZE)))
      return (void *)(addr - DTCM_GLOBAL_BASE + DTCM_BASE);
    else if ((addr >= ITCM_GLOBAL_BASE) && (addr < (ITCM_GLOBAL_BASE + ITCM_SIZE)))
      return (void *)(addr - ITCM_GLOBAL_BASE + ITCM_BASE);
    else
      return ((void *)addr);
  }

  /* Rx/Tx DMA Descriptor */
  typedef struct
  {
      uint32_t des0;
      uint32_t des1;
      uint32_t des2;
      uint32_t des3;
  } DMA_DESC;

  /*
   * Descriptor Definitions
   *  Derived from Azure RTOS (alif_ensemble-Azure-RTOS)
   *  NETX/driver/Inc/mac_hw.h
   */

#define RDES3_BUFFER1_VALID_ADDR BIT(24)
#define RDES3_BUFFER2_VALID_ADDR BIT(25)
#define RDES3_INT_ON_COMPLETION_EN BIT(30)
#define RDES3_OWN BIT(31)
#define RDES3_RER BIT(15) /* End-of-ring */
#define RDES3_LAST_DESCRIPTOR BIT(28)
#define RDES3_FIRST_DESCRIPTOR BIT(29)

#define TDES2_INTERRUPT_ON_COMPLETION BIT(31)

#define TDES3_OWN BIT(31)
#define TDES3_FIRST_DESCRIPTOR BIT(29) /* FD */
#define TDES3_LAST_DESCRIPTOR BIT(28)  /* LD */
#define TDES3_CONTEXT BIT(23)          /* CTXT – must stay 0 */
#define TDES3_TER BIT(2)               /* End-of-ring */

#ifdef __cplusplus
}
#endif
