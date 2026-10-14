/*
 * USB CDC-ECM sample for the Alif E7 DK (RTSS-HE), showing two issues of the DWC3 device controller
 * driver (udc_dwc3) and the patches that fix them:
 *
 *  - udc-dwc3-report-reset-after-connect-done.patch: the stack reads the bus speed before the
 *    controller has negotiated it, so on a full-speed link it keeps the high-speed configuration
 *    and announces 512-byte bulk endpoints instead of 64.
 *  - udc-dwc3-bulk-in-zlp.patch: the driver never sends the zero-length packet a bulk IN transfer
 *    needs when its length is a multiple of the endpoint size, so such Ethernet frames never end
 *    on the bus.
 *
 * The board exposes an Ethernet interface over USB (192.0.2.1/24). A Linux host pings it with
 * tools/scripts/usb_ecm_check.py, around the frame sizes that need a zero-length packet. See the
 * README for the full procedure.
 */

#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/net_if.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/usb/usbd_msg.h>

/*
 * sample_usbd.h (samples/subsys/usb/common) has no extern "C" guard and pulls in C++-incompatible
 * headers, so the helper it provides is declared here with C linkage. It is implemented in the
 * C-compiled sample_usbd_init.c.
 */
extern "C"
{
  struct usbd_context *sample_usbd_init_device(usbd_msg_cb_t msg_cb);
}

LOG_MODULE_REGISTER(app);

#define GREEN_LED_NODE DT_ALIAS(led0)

/* On while the host has the link up (CDC-ECM carrier on). */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);

/* Bulk endpoint size the CDC-ECM class uses at each speed (USB 2.0, 5.8.3). */
static constexpr unsigned int FULL_SPEED_BULK_MPS = 64U;
static constexpr unsigned int HIGH_SPEED_BULK_MPS = 512U;

static auto bulkMps(enum usbd_speed speed) -> unsigned int
{
  return speed == USBD_SPEED_HS ? HIGH_SPEED_BULK_MPS : FULL_SPEED_BULK_MPS;
}

static auto speedName(enum usbd_speed speed) -> const char *
{
  switch (speed)
  {
    case USBD_SPEED_FS:
      return "full speed";
    case USBD_SPEED_HS:
      return "high speed";
    default:
      return "unknown speed";
  }
}

/*
 * Called from the USB device thread. The speed reported on a bus reset is the one the stack has
 * chosen its descriptors and endpoint sizes from: without the connect-done patch, it is high speed
 * even when the host only talks full speed (compare with `lsusb -t` on the host).
 */
static void onUsbMessage(struct usbd_context *const context, const struct usbd_msg *const message)
{
  switch (message->type)
  {
    case USBD_MSG_RESET: {
      const enum usbd_speed speed = usbd_bus_speed(context);
      LOG_INF("Bus reset: the stack uses its %s configuration, bulk endpoints of %u bytes", speedName(speed), bulkMps(speed));
      break;
    }
    case USBD_MSG_CONFIGURATION:
      LOG_INF("Configured by the host (configuration %d)", message->status);
      break;
    case USBD_MSG_SUSPEND:
      LOG_INF("Suspended by the host");
      break;
    case USBD_MSG_RESUME:
      LOG_INF("Resumed by the host");
      break;
    case USBD_MSG_UDC_ERROR:
    case USBD_MSG_STACK_ERROR:
      LOG_ERR("USB %s: %d", usbd_msg_type_string(message->type), message->status);
      break;
    default:
      break;
  }
}

auto main() -> int
{
  if (!gpio_is_ready_dt(&led) || gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE) < 0)
  {
    LOG_ERR("Led not ready");
    return 0;
  }

  struct usbd_context *usbd = sample_usbd_init_device(onUsbMessage);
  if (usbd == nullptr)
  {
    LOG_ERR("Failed to initialise the USB device stack");
    return 0;
  }

  if (int err = usbd_enable(usbd); err != 0)
  {
    LOG_ERR("Failed to enable the USB device stack (%d)", err);
    return 0;
  }

  struct net_if *iface = net_if_get_first_by_type(&NET_L2_GET_NAME(ETHERNET));
  if (iface == nullptr)
  {
    LOG_ERR("No CDC-ECM interface");
    return 0;
  }

  LOG_INF("USB CDC-ECM device ready: plug the board into a Linux host and reach it at %s", CONFIG_NET_CONFIG_MY_IPV4_ADDR);

  bool carrier = false;
  while (true)
  {
    if (const bool up = net_if_is_carrier_ok(iface); up != carrier)
    {
      carrier = up;
      LOG_INF("Link %s", carrier ? "up: the host has activated the interface" : "down");
      (void)gpio_pin_set_dt(&led, carrier ? 1 : 0);
    }

    k_msleep(100);
  }
}
