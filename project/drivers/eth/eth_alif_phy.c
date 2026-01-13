/*
 * Native Zephyr Ethernet Driver for Alif E7 ETH PHY
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "eth_alif_phy.h"
#include "soc_registers.h"

LOG_MODULE_DECLARE(eth_alif);

/*
 * IEEE 802.3 Clause 22 (MII Registers)
 */

#define PHY_BCR 0x0              // Basic Control Register (BCR)
#define PHY_RESET BIT(15)        // Soft reset
#define PHY_LOOPBACK BIT(14)     // Enable loopback mode
#define PHY_SPEED_SELECT BIT(13) // 1 = 100Mbps, 0 = 10Mbps
#define PHY_AN_ENABLE BIT(12)    // Enable auto-negotiation
#define PHY_POWER_DOWN BIT(11)   // Power down
#define PHY_ISOLATE BIT(10)      // Electrically isolate PHY from MII
#define PHY_RESTART_AN BIT(9)    // Restart auto-negotiation
#define PHY_DUPLEX_MODE BIT(8)   // 1 = Full duplex, 0 = Half duplex
#define PHY_COL_TEST BIT(7)      // Collision test mode
#define PHY_BSR 0x01             // Basic Status Register (BSR)
#define PHY_LINK_STATUS BIT(2)   // Link status (1 = link up)
#define PHY_ID1 0x02             // PHY Identifier 1 (Manufacturer ID)
#define PHY_ID2 0x03             // PHY Identifier 2 (Model number + revision)
#define PHY_AN_ADV 0x04          // Auto-Negotiation Advertisement

#define MDIO_ADDR_REG (*(volatile uint32_t *)(ETH_MAC_MDIO_ADDRESS))
#define MDIO_DATA_REG (*(volatile uint32_t *)(ETH_MAC_MDIO_DATA))

static int eth_alif_mdio_read(uint8_t phy_addr, uint8_t reg_addr, uint16_t *data)
{
  uint32_t val = (phy_addr << ETH_MAC_MDIO_ADDRESS_PA_SHIFT) | (reg_addr << ETH_MAC_MDIO_ADDRESS_RDA_SHIFT) |
                 (ETH_MAC_MDIO_ADDRESS_GOC_READ << ETH_MAC_MDIO_ADDRESS_GOC_SHIFT) |
                 (ETH_MAC_MDIO_ADDRESS_CR_150_250 << ETH_MAC_MDIO_ADDRESS_CR_SHIFT) | ETH_MAC_MDIO_ADDRESS_GB;

  sys_write32(val, ETH_MAC_MDIO_ADDRESS);

  int timeout = 10;
  while ((sys_read32(ETH_MAC_MDIO_ADDRESS) & ETH_MAC_MDIO_ADDRESS_GB) && timeout--)
  {
    k_msleep(1);
  }

  if (timeout <= 0)
  {
    return -EIO;
  }

  *data = (uint16_t)sys_read32(ETH_MAC_MDIO_DATA);

  return 0;
}

static int eth_alif_mdio_write(uint8_t phy_addr, uint8_t reg_addr, uint16_t data)
{
  sys_write32(data, ETH_MAC_MDIO_DATA);

  uint32_t val = (phy_addr << ETH_MAC_MDIO_ADDRESS_PA_SHIFT) | (reg_addr << ETH_MAC_MDIO_ADDRESS_RDA_SHIFT) |
                 (ETH_MAC_MDIO_ADDRESS_GOC_WRITE << ETH_MAC_MDIO_ADDRESS_GOC_SHIFT) |
                 (ETH_MAC_MDIO_ADDRESS_CR_150_250 << ETH_MAC_MDIO_ADDRESS_CR_SHIFT) | ETH_MAC_MDIO_ADDRESS_GB;

  sys_write32(val, ETH_MAC_MDIO_ADDRESS);

  int timeout = 10;
  while ((sys_read32(ETH_MAC_MDIO_ADDRESS) & ETH_MAC_MDIO_ADDRESS_GB) && timeout--)
  {
    k_msleep(1);
  }

  return (timeout > 0) ? 0 : -EIO;
}

int eth_alif_phy_init(uint8_t phy_addr)
{
  uint16_t id1, id2;
  int ret;

  // Read PHY ID
  ret = eth_alif_mdio_read(phy_addr, PHY_ID1, &id1);
  if (ret < 0)
    return ret;

  ret = eth_alif_mdio_read(phy_addr, PHY_ID2, &id2);
  if (ret < 0)
    return ret;

  uint32_t phy_id = ((uint32_t)id1 << 16) | id2;
  LOG_INF("PHY ID: 0x%08X", phy_id);

  // Reset PHY
  ret = eth_alif_mdio_write(phy_addr, PHY_BCR, PHY_RESET);
  if (ret < 0)
    return ret;

  // Wait for reset complete
  for (int i = 0; i < 10; i++)
  {
    uint16_t ctrl;
    eth_alif_mdio_read(phy_addr, PHY_BCR, &ctrl);
    if (!(ctrl & PHY_RESET))
      break;

    k_msleep(10);
  }

  // Enable auto-negotiation
  ret = eth_alif_mdio_write(phy_addr, PHY_BCR, PHY_AN_ENABLE | PHY_RESTART_AN);
  if (ret < 0)
    return ret;

  LOG_INF("PHY reset and auto-negotiation initiated");

  // Read link status
  for (int i = 0; i < 50; i++)
  {
    uint16_t bsr;
    eth_alif_mdio_read(phy_addr, PHY_BSR, &bsr);
    if (bsr & PHY_LINK_STATUS)
    {
      LOG_INF("Link is up");
      break;
    }
    k_msleep(100);
  }

  return 0;
}
