#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#define RED_LED_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

LOG_MODULE_REGISTER(app);

#include "../common/cache_report.hpp"

// Each core copies in its own region: both are cacheable, and the two images never share them.
static uint8_t g_copySrc[4096] __attribute__((section(".alif_sram1")));
static uint8_t g_copyDst[4096] __attribute__((section(".alif_sram1")));

auto main() -> int
{
  int ret;

  if (!gpio_is_ready_dt(&led))
  {
    LOG_ERR("Led not ready\n");
    return 0;
  }

  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0)
  {
    LOG_ERR("Led config failed\n");
    return 0;
  }

  for (unsigned blink = 0;; blink++)
  {
    // At boot, then every 5 s for a console opened late.
    if (blink % 10 == 0)
    {
      reportCaches("SRAM1", g_copySrc, g_copyDst);
    }
    LOG_INF("Blink from HP!");
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0)
    {
      LOG_ERR("Led toggle failed");
      return 0;
    }

    k_msleep(500);
  }
}