#include <string.h>
#include <zephyr/arch/cache.h>
#include <zephyr/device.h>
#include <zephyr/drivers/clock_control.h>
#include <zephyr/drivers/pinctrl.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/ethernet.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/sys/util.h>

// #include "dump_soc_registers.h"
#include "eth_alif_phy.h"
#include "soc_registers.h"

LOG_MODULE_REGISTER(eth_alif, CONFIG_ETHERNET_LOG_LEVEL);

BUILD_ASSERT(ETH_BASE == DT_REG_ADDR(DT_INST(0, zephyr_eth_alif)), "ETH_BASE does not match device tree base address for zephyr_eth_alif");

#define DT_DRV_COMPAT zephyr_eth_alif

#define ETH_ALIF_PRIORITY 80
#define RX_DESC_COUNT 8
#define TX_DESC_COUNT 8
#define ETH_BUF_SIZE 1536

#define ETH_IRQ_PRIORITY 0

#define DT_ETH_NODE DT_NODELABEL(eth_alif)
#define PHY_NODE DT_PHANDLE(ETH_NODE, phy_handle)
#define PHY_ADDR 1 // DT_REG_ADDR(PHY_NODE)

BUILD_ASSERT(ETH_IRQ_PRIORITY == DT_IRQ(DT_ETH_NODE, priority), "ETH_IRQ_PRIORITY does not match device tree base IRQ priority for zephyr_eth_alif");

#define ETH_CLOCK_ID DT_CLOCKS_CELL(DT_DRV_INST(0), clkid)

/* Define all pinctrl configuration */
PINCTRL_DT_INST_DEFINE(0);

/* Device config */
struct eth_alif_config
{
  const struct pinctrl_dev_config *pinctrl;
};

/** Device data structure */
struct eth_alif_data
{
};

uint8_t mac_addr[6] = {
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 0),
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 1),
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 2),
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 3),
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 4),
    DT_PROP_BY_IDX(DT_ETH_NODE, mac_address, 5),
};

/* Area for descriptors */

static DMA_DESC dma_descs[RX_DESC_COUNT + TX_DESC_COUNT] __attribute__((section("ETH_DMA"), aligned(32)));
static uint32_t rx_buffers[RX_DESC_COUNT][ETH_BUF_SIZE >> 2] __attribute__((section("ETH_DMA"), aligned(32)));
static uint32_t tx_buffers[TX_DESC_COUNT][ETH_BUF_SIZE >> 2] __attribute__((section("ETH_DMA"), aligned(32)));

typedef struct
{
  void *descs;          // Pointer to the DMA descriptor area
  DMA_DESC *rx_descs;   // Array of Rx DMA descriptors
  DMA_DESC *tx_descs;   // Array of Tx DMA descriptors
  uint32_t rx_desc_id;  // Index of the current Rx DMA descriptor
  uint32_t tx_desc_id;  // Index of the current Tx DMA descriptor
  uint32_t tx_clean_id; // Index to reclaim TX descriptors
  uint8_t *frame_end;   // Current frame address, to support fragments
} MAC_DEV_Type;

static MAC_DEV_Type MAC_DEV;

static struct net_if *eth_iface;

static struct k_work tx_work;
static struct k_work rx_work;

static void eth_alif_iface_init(struct net_if *iface)
{
  LOG_INF("eth_alif_iface_init ...");

  net_if_set_link_addr(iface, mac_addr, sizeof(mac_addr), NET_LINK_ETHERNET);
  ethernet_init(iface);
  eth_iface = iface;

  net_if_up(iface);

  LOG_INF("eth_alif_iface_init done");
}

static void eth_alif_tx_work_handler(struct k_work *work)
{
  while (MAC_DEV.tx_clean_id != MAC_DEV.tx_desc_id)
  {
    DMA_DESC *desc = &MAC_DEV.tx_descs[MAC_DEV.tx_clean_id];

    SCB_InvalidateDCache_by_Addr((uint32_t *)desc, sizeof(DMA_DESC));

    if (desc->des3 & TDES3_OWN)
      break;

    // Safe to reclaim
    desc->des0 = 0;
    desc->des1 = 0;
    desc->des2 = 0;
    desc->des3 = (MAC_DEV.tx_clean_id == TX_DESC_COUNT - 1) ? TDES3_TER : 0;

    SCB_CleanDCache_by_Addr((uint32_t *)desc, sizeof(DMA_DESC));

    MAC_DEV.tx_clean_id = (MAC_DEV.tx_clean_id + 1) % TX_DESC_COUNT;
  }
}

static void read_rxdesc(DMA_DESC *desc, uint32_t len, uint32_t *rx_buffer)
{
  LOG_DBG("desc=%p len=%d des0=Ox%08X des1=0x%08X des2=0x%08X des3=0x%08X", desc, len, desc->des0, desc->des1, desc->des2, desc->des3);

  if (!(desc->des3 & RDES3_FIRST_DESCRIPTOR && desc->des3 & RDES3_LAST_DESCRIPTOR))
    LOG_ERR("Fragmented descriptor content");

  SCB_InvalidateDCache_by_Addr(rx_buffer, ROUND_UP(len, 32));

  struct net_pkt *pkt = net_pkt_rx_alloc_with_buffer(eth_iface, len, AF_UNSPEC, 0, K_NO_WAIT);
  if (!pkt)
  {
    LOG_ERR("net_pkt_rx_alloc_with_buffer() failed");

    return;
  }

  int ret = net_pkt_write(pkt, rx_buffer, len);
  if (ret < 0)
  {
    LOG_ERR("net_pkt_write() failed: %d", ret);
    net_pkt_unref(pkt);

    return;
  }

  ret = net_recv_data(eth_iface, pkt);
  if (ret < 0)
  {
    LOG_ERR("net_recv_data() failed: %d", ret);
    net_pkt_unref(pkt);

    return;
  }

#if CONFIG_ETH_ALIF_RX_DELAY_US > 0
  /* Add delay to help with ARP timing issues */
  k_busy_wait(CONFIG_ETH_ALIF_RX_DELAY_US);
#endif
}

static void eth_alif_rx_work_handler(struct k_work *work)
{
  bool updated = false;
  for (int count = 0; count < RX_DESC_COUNT; count++)
  {
    DMA_DESC *desc = &MAC_DEV.rx_descs[MAC_DEV.rx_desc_id];

    SCB_InvalidateDCache_by_Addr((uint32_t *)desc, sizeof(DMA_DESC));

    if (desc->des3 & RDES3_OWN)
    {
      MAC_DEV.rx_desc_id = (MAC_DEV.rx_desc_id + 1) % RX_DESC_COUNT;
      continue;
    }

    updated = true;

    uint32_t len = desc->des3 & 0x7fff;
    if (len > 0 && len <= ETH_BUF_SIZE)
      read_rxdesc(desc, len, rx_buffers[MAC_DEV.rx_desc_id]);

    desc->des0 = local_to_global(rx_buffers[MAC_DEV.rx_desc_id]);
    desc->des1 = ETH_BUF_SIZE;
    desc->des2 = 0;
    desc->des3 = RDES3_OWN | RDES3_INT_ON_COMPLETION_EN | RDES3_BUFFER1_VALID_ADDR;

    if (MAC_DEV.rx_desc_id == RX_DESC_COUNT - 1)
      desc->des3 |= RDES3_RER;

    SCB_CleanDCache_by_Addr((uint32_t *)desc, sizeof(DMA_DESC));
    SCB_CleanDCache_by_Addr((uint32_t *)rx_buffers[MAC_DEV.rx_desc_id], ETH_BUF_SIZE);

    MAC_DEV.rx_desc_id = (MAC_DEV.rx_desc_id + 1) % RX_DESC_COUNT;

    // dump_rx_dma_descs();
  }

  if (updated)
  {
    int tail = (MAC_DEV.rx_desc_id - 1 + RX_DESC_COUNT) % RX_DESC_COUNT;
    sys_write32(local_to_global(&MAC_DEV.rx_descs[tail]), ETH_DMA_CH0_RXDESC_TAIL_POINTER);
  }
}

static void eth_alif_isr(const void *arg)
{
  ARG_UNUSED(arg);

  uint32_t val = sys_read32(ETH_DMA_CH0_STATUS);

  uint32_t clear = 0;
  if (val & ETH_DMA_CH0_STATUS_RI) // Receive Interrupt
  {
    clear |= ETH_DMA_CH0_STATUS_RI;

    k_work_submit(&rx_work);
  }

  if (val & ETH_DMA_CH0_STATUS_TI) // Transmit Interrupt
  {
    clear |= ETH_DMA_CH0_STATUS_TI;

    k_work_submit(&tx_work);
  }

  if (val & ETH_DMA_CH0_STATUS_TBU)
  {
    clear |= ETH_DMA_CH0_STATUS_TBU;
  }

  if (val & ETH_DMA_CH0_STATUS_RBU)
  {
    clear |= ETH_DMA_CH0_STATUS_RBU;
  }

  if (val & ETH_DMA_CH0_STATUS_NIS)
  {
    clear |= ETH_DMA_CH0_STATUS_NIS;
  }

  if (val & ETH_DMA_CH0_STATUS_AIS)
  {
    clear |= ETH_DMA_CH0_STATUS_AIS;
  }

  if (clear)
    sys_write32(clear, ETH_DMA_CH0_STATUS);
}

static int eth_alif_send(const struct device *dev, struct net_pkt *pkt)
{
  size_t len = net_pkt_get_len(pkt);
  if (len > ETH_BUF_SIZE)
  {
    return -EMSGSIZE;
  }

  LOG_DBG("len=%d", len);

  uint32_t cur = MAC_DEV.tx_desc_id;
  DMA_DESC *desc = &MAC_DEV.tx_descs[cur];

  // wait until DMA has returned the descriptor
  SCB_InvalidateDCache_by_Addr(&MAC_DEV.tx_descs[cur], sizeof(DMA_DESC));
  if (desc->des3 & TDES3_OWN)
    return -EAGAIN; // queue is full – upper layer will retry

  // Copy payload into the Tx buffer
  if (net_pkt_read(pkt, tx_buffers[cur], len))
  {
    net_pkt_unref(pkt);
    return -EIO;
  }

  SCB_CleanDCache_by_Addr(tx_buffers[cur], ROUND_UP(len, 32));

  // Fill the descriptor
  uint32_t des3 = TDES3_FIRST_DESCRIPTOR | TDES3_LAST_DESCRIPTOR;
  if (cur == TX_DESC_COUNT - 1)
  {
    des3 |= TDES3_TER; // End-of-Ring
  }

  desc->des0 = local_to_global(tx_buffers[cur]);
  desc->des1 = 0;
  desc->des2 = (len & 0x1FFF) | TDES2_INTERRUPT_ON_COMPLETION;
  desc->des3 = des3;

  SCB_CleanDCache_by_Addr((uint32_t *)desc, sizeof(DMA_DESC));

  // Give ownership to DMA
  desc->des3 = des3 | TDES3_OWN;

  SCB_CleanDCache_by_Addr((uint32_t *)&desc->des3, sizeof(desc->des3));

  // Advance HW tail
  sys_write32(local_to_global(&MAC_DEV.tx_descs[cur]), ETH_DMA_CH0_TXDESC_TAIL_POINTER);

  // Move software index
  MAC_DEV.tx_desc_id = (cur + 1) % TX_DESC_COUNT;

  // dump_tx_eth_regs();
  // dump_tx_dma_descs();

  // If the DMA stopped, re-start it
  uint32_t val = sys_read32(ETH_DMA_CH0_TX_CONTROL);
  if (!(val & ETH_DMA_CH0_TX_CONTROL_ST))
  {
    val |= ETH_DMA_CH0_TX_CONTROL_ST;
    sys_write32(val, ETH_DMA_CH0_TX_CONTROL);
  }

  return 0;
}

static void set_mac_address()
{
  uint32_t lo = (mac_addr[3] << 24) | (mac_addr[2] << 16) | (mac_addr[1] << 8) | (mac_addr[0]);
  uint32_t hi = (mac_addr[5] << 8) | (mac_addr[4]) | ETH_MAC_ADDRESS0_HIGH_AE;
  sys_write32(lo, ETH_MAC_ADDRESS0_LOW);
  sys_write32(hi, ETH_MAC_ADDRESS0_HIGH);
}

/* Setup a single Rx DMA descriptor */
static void setup_rxdesc(uint32_t id)
{
  DMA_DESC *desc = &MAC_DEV.rx_descs[id];

  desc->des0 = local_to_global(rx_buffers[id]);
  desc->des1 = ETH_BUF_SIZE;
  desc->des2 = 0;
  desc->des3 = RDES3_OWN | RDES3_INT_ON_COMPLETION_EN | RDES3_BUFFER1_VALID_ADDR;

  if (id == RX_DESC_COUNT - 1)
    desc->des3 |= RDES3_RER;
}

/* Initialize Rx DMA descriptors */
static void init_rx_descs(void)
{
  uint32_t i;
  for (i = 0; i < RX_DESC_COUNT; i++)
    setup_rxdesc(i);

  SCB_CleanDCache_by_Addr((uint32_t *)MAC_DEV.rx_descs, RX_DESC_COUNT * sizeof(DMA_DESC));

  sys_write32(local_to_global(MAC_DEV.rx_descs), ETH_DMA_CH0_RXDESC_LIST_ADDRESS);
  sys_write32(local_to_global(&MAC_DEV.rx_descs[RX_DESC_COUNT - 1]), ETH_DMA_CH0_RXDESC_TAIL_POINTER);
  sys_write32((uint32_t)(RX_DESC_COUNT - 1), ETH_DMA_CH0_RXDESC_RING_LENGTH);

  // dump_rx_dma_descs();
}

/* Initialize Tx DMA descriptors */
static void init_tx_descs()
{
  uint32_t i;

  for (i = 0; i < TX_DESC_COUNT; i++)
  {

    MAC_DEV.tx_descs[i].des0 = 0;
    MAC_DEV.tx_descs[i].des1 = 0;
    MAC_DEV.tx_descs[i].des2 = 0;
    MAC_DEV.tx_descs[i].des3 = 0;
  }
  MAC_DEV.tx_descs[TX_DESC_COUNT - 1].des3 |= TDES3_TER;

  SCB_CleanDCache_by_Addr((uint32_t *)MAC_DEV.tx_descs, TX_DESC_COUNT * sizeof(DMA_DESC));

  sys_write32(local_to_global(MAC_DEV.tx_descs), ETH_DMA_CH0_TXDESC_LIST_ADDRESS);
  sys_write32((uint32_t)(TX_DESC_COUNT - 1), ETH_DMA_CH0_TXDESC_RING_LENGTH);

  // dump_tx_dma_descs();
}

/* Initialize DMA descriptors */
static void init_descriptors(void)
{
  MAC_DEV.tx_descs = (DMA_DESC *)dma_descs;
  MAC_DEV.rx_descs = (MAC_DEV.tx_descs + TX_DESC_COUNT);

  init_rx_descs();
  init_tx_descs();
}

/* Initialize the MAC hardware */
static int32_t mac_hw_init(void)
{
  uint32_t val;

  //
  // Alif_E7_HWRM_v2.8.pdf
  //  15.3.5.11 Power-Up Sequence
  //

  // Clear the ETH_CTRL0[PWR_DOWN_CTRL_I] bit
  val = sys_read32(CLKCTL_PER_MST_ETH_CTRL0);
  val &= ~CLKCTL_PER_MST_ETH_CTRL0_PWR_DOWN_CTRL_I;
  sys_write32(val, CLKCTL_PER_MST_ETH_CTRL0);

  // Clear the ETH_CTRL0[PWR_ISOLATE_I] bit
  val = sys_read32(CLKCTL_PER_MST_ETH_CTRL0);
  val &= ~CLKCTL_PER_MST_ETH_CTRL0_PWR_ISOLATE_I;
  sys_write32(val, CLKCTL_PER_MST_ETH_CTRL0);

  // Clear the ETH_CTRL0[PWR_CLAMP_CTRL_I] bit
  val = sys_read32(CLKCTL_PER_MST_ETH_CTRL0);
  val &= ~CLKCTL_PER_MST_ETH_CTRL0_PWR_CLAMP_CTRL_I;
  sys_write32(val, CLKCTL_PER_MST_ETH_CTRL0);

  // Logic clock for DMA controller inside the M55 subsystem
  val = sys_read32(M55HP_CFG_CLK_ENA);
  val |= M55HP_CFG_CLK_ENA_DMA_CKEN;
  sys_write32(val, M55HP_CFG_CLK_ENA);

  // Peripheral clock for ETH & DMA
  val = sys_read32(CLKCTL_PER_MST_PERIPH_CLK_ENA);
  val |= CLKCTL_PER_MST_PERIPH_CLK_ENA_ETH_CKEN | CLKCTL_PER_MST_PERIPH_CLK_ENA_DMA_CKEN;
  sys_write32(val, CLKCTL_PER_MST_PERIPH_CLK_ENA);

  // Turn on the 100 MHz/160 MHz peripheral clocks
  val = sys_read32(CGU_CLK_ENA);
  val |= CGU_CLK_ENA_CLK100M;
  val |= CGU_CLK_ENA_CLK160M;
  sys_write32(val, CGU_CLK_ENA);

  // Software de assert of DMA reset
  val = sys_read32(M55HP_CFG_DMA_CTRL);
  val &= ~M55HP_CFG_DMA_CTRL_SW_RST;
  sys_write32(val, M55HP_CFG_DMA_CTRL);
  val = sys_read32(CLKCTL_PER_MST_DMA_CTRL);
  val &= ~CLKCTL_PER_MST_DMA_CTRL_SW_RST;
  sys_write32(val, CLKCTL_PER_MST_DMA_CTRL);

  k_busy_wait(100);

  eth_alif_phy_init(PHY_ADDR);

  // dump_cgu_regs();
  // dump_m55_hp_cfg_regs();
  // dump_clkctl_per_mst_regs();

  // DMA software reset
  int timeout = 10000;
  val = sys_read32(ETH_DMA_MODE);
  val |= ETH_DMA_MODE_SWR;
  sys_write32(val, ETH_DMA_MODE);
  while ((sys_read32(ETH_DMA_MODE) & ETH_DMA_MODE_SWR) && --timeout)
  {
    k_busy_wait(100);
  }
  if (timeout == 0)
  {
    LOG_ERR("DMA software reset failed to complete");
    return -ETIMEDOUT;
  }

  // Configure MTL Tx Q0 Operating mode
  val = sys_read32(ETH_MTL_TXQ0_OPERATION_MODE);
  val &= ~ETH_MTL_TXQ0_OPERATION_MODE_TXQEN;
  val |= ETH_MTL_TXQ0_OPERATION_MODE_TXQEN | ETH_MTL_TXQ0_OPERATION_MODE_TSF;
  sys_write32(val, ETH_MTL_TXQ0_OPERATION_MODE);

  // Configure MTL Rx Q0 operating mode
  val = sys_read32(ETH_MTL_RXQ0_OPERATION_MODE);
  val |= ETH_MTL_RXQ0_OPERATION_MODE_RSF | ETH_MTL_RXQ0_OPERATION_MODE_FEP | ETH_MTL_RXQ0_OPERATION_MODE_FUP;
  sys_write32(val, ETH_MTL_RXQ0_OPERATION_MODE);

  // Configure Tx flow control
  val = sys_read32(ETH_MAC_Q0_TX_FLOW_CTRL);
  val |= 0xffff << ETH_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT | ETH_MAC_Q0_TX_FLOW_CTRL_TFE;
  sys_write32(val, ETH_MAC_Q0_TX_FLOW_CTRL);

  // Configure Rx flow control
  val = sys_read32(ETH_MAC_RX_FLOW_CTRL);
  val |= ETH_MAC_RX_FLOW_CTRL_RFE;
  sys_write32(val, ETH_MAC_RX_FLOW_CTRL);

  val = sys_read32(ETH_MAC_CONFIGURATION);
  val &= ~ETH_MAC_CONFIGURATION_JE;
  val |= (ETH_MAC_CONFIGURATION_FES | ETH_MAC_CONFIGURATION_DM | ETH_MAC_CONFIGURATION_RE | ETH_MAC_CONFIGURATION_TE);
  sys_write32(val, ETH_MAC_CONFIGURATION);

  val = sys_read32(ETH_MAC_PACKET_FILTER);
  val &= ~ETH_MAC_PACKET_FILTER_DBF;
  val |= ETH_MAC_PACKET_FILTER_PM;
  sys_write32(val, ETH_MAC_PACKET_FILTER);

  // Configure DMA error management
  val = sys_read32(ETH_DMA_CH0_STATUS);
  val |= ETH_DMA_CH0_STATUS_CDE | ETH_DMA_CH0_STATUS_FBE;
  sys_write32(val, ETH_DMA_CH0_STATUS);

  // Configure DMA transmission burst length
  val = sys_read32(ETH_DMA_CH0_TX_CONTROL);
  val |= 16 << ETH_DMA_CH0_TX_CONTROL_TXPB_SHIFT;
  sys_write32(val, ETH_DMA_CH0_TX_CONTROL);

  // Configure DMA reception burst length
  val = sys_read32(ETH_DMA_CH0_RX_CONTROL);
  // Bits 3:1 (low 3 bits)
  val |= 16 << ETH_DMA_CH0_RX_CONTROL_RXPBL_SHIFT;
  val |= ((ETH_BUF_SIZE >> 0) & ETH_DMA_CH0_RX_CONTROL_RBSZ_X_0) << ETH_DMA_CH0_RX_CONTROL_RBSZ_X_0_SHIFT;
  // Bits 14:4 (high 11 bits)
  val |= ((ETH_BUF_SIZE >> 3) & ETH_DMA_CH0_RX_CONTROL_RBSZ_13_Y) << ETH_DMA_CH0_RX_CONTROL_RBSZ_13_Y_SHIFT;
  val |= ETH_DMA_CH0_RX_CONTROL_RFP;
  sys_write32(val, ETH_DMA_CH0_RX_CONTROL);

  val = sys_read32(ETH_DMA_SYSBUS_MODE);
  val |= ETH_DMA_SYSBUS_MODE_BLEN4 | ETH_DMA_SYSBUS_MODE_BLEN8 | ETH_DMA_SYSBUS_MODE_BLEN16;
  val |= 3 << ETH_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT;
  val |= 1 << ETH_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT;
  val |= ETH_DMA_SYSBUS_MODE_ONEKBBE;
  sys_write32(val, ETH_DMA_SYSBUS_MODE);

  // Init descriptors
  init_descriptors();

  // Enable DMA Channel 0 interrupts, Rx and Tx
  val = sys_read32(ETH_DMA_CH0_INTERRUPT_ENABLE);
  val |= (ETH_DMA_CH0_INTERRUPT_ENABLE_RIE | ETH_DMA_CH0_INTERRUPT_ENABLE_NIE | ETH_DMA_CH0_INTERRUPT_ENABLE_TIE | ETH_DMA_CH0_INTERRUPT_ENABLE_AIE |
          ETH_DMA_CH0_INTERRUPT_ENABLE_RBUE);
  sys_write32(val, ETH_DMA_CH0_INTERRUPT_ENABLE);

  // Set MAC address
  set_mac_address();

  // Start transmission channel
  val = sys_read32(ETH_DMA_CH0_TX_CONTROL);
  val |= ETH_DMA_CH0_TX_CONTROL_ST;
  sys_write32(val, ETH_DMA_CH0_TX_CONTROL);

  // Start reception channel
  val = sys_read32(ETH_DMA_CH0_RX_CONTROL);
  val |= ETH_DMA_CH0_RX_CONTROL_SR;
  sys_write32(val, ETH_DMA_CH0_RX_CONTROL);

  // dump_eth_regs();

  return 0;
}

static int eth_alif_init(const struct device *dev)
{
  LOG_INF("eth_alif_init ...");
  // Configure pinmux
  const struct eth_alif_config *cfg = dev->config;
  int ret = pinctrl_apply_state(cfg->pinctrl, PINCTRL_STATE_DEFAULT);
  if (ret < 0)
  {
    LOG_ERR("Failed to apply pinctrl: %d", ret);
    return ret;
  }

  // dump_pinmux_eth_regs();

  ret = mac_hw_init();
  if (ret < 0)
  {
    return ret;
  }

  // Workqueue-based RX processing
  k_work_init(&tx_work, eth_alif_tx_work_handler);

  // Workqueue-based RX processing
  k_work_init(&rx_work, eth_alif_rx_work_handler);

  // Interrupt handler init
  IRQ_CONNECT(DT_IRQN(DT_ETH_NODE), ETH_IRQ_PRIORITY, eth_alif_isr, NULL, 0);
  irq_enable(DT_IRQN(DT_ETH_NODE));

  LOG_INF("eth_alif_init done");

  return 0;
}

struct eth_alif_config eth_alif_config = {.pinctrl = PINCTRL_DT_INST_DEV_CONFIG_GET(0)};

struct eth_alif_data eth_alif_data = {};

static enum ethernet_hw_caps eth_alif_get_capabilities(const struct device *dev)
{
  return ETHERNET_LINK_10BASE_T | ETHERNET_LINK_100BASE_T;
}

static const struct ethernet_api eth_alif_api = {
    .iface_api.init = eth_alif_iface_init,
    .send = eth_alif_send,
    .get_capabilities = eth_alif_get_capabilities,
};

ETH_NET_DEVICE_DT_INST_DEFINE(0, eth_alif_init, NULL, &eth_alif_data, &eth_alif_config, CONFIG_ETH_INIT_PRIORITY, &eth_alif_api, ETH_BUF_SIZE);
