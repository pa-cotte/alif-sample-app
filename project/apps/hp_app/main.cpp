#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>

#define RED_LED_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

LOG_MODULE_REGISTER(app);

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

  while (true)
  {
    LOG_DBG("Blink from HP!");
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0)
    {
      LOG_ERR("Led toggle failed");
      return 0;
    }

    k_msleep(500);
  }
}