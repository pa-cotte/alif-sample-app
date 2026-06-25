#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>

#define RED_LED_NODE DT_ALIAS(led0)
#define ANGLE_SENSOR_NODE DT_ALIAS(angle_sensor)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);
static const struct device *const ma780 = DEVICE_DT_GET(ANGLE_SENSOR_NODE);

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

  /*
   * The MA780 driver runs ma780_init() at POST_KERNEL: it performs several
   * SPI3 register writes, each verified by reading the value back (the
   * "write-with-check" the datasheet recommends). In dual-core operation
   * these verified writes intermittently fail after MA780_WRITE_RETRIES
   * attempts, so the device fails to become ready. This is the issue we want
   * to surface for Alif support.
   */
  if (!device_is_ready(ma780))
  {
    LOG_ERR("MA780 not ready: SPI3 verified writes failed during init (see driver errors above)");
  }
  else
  {
    LOG_INF("MA780 ready on SPI3");
  }

  while (true)
  {
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0)
    {
      LOG_ERR("Led toggle failed");
      return 0;
    }

    /*
     * Re-exercise a verified register write on demand: SENSOR_ATTR_UPPER_THRESH
     * maps to ma780_set_reg() -> ma780_write_reg(), which writes the register
     * and checks the read-back, retrying up to MA780_WRITE_RETRIES times before
     * returning -EIO. This reproduces the failing write-with-check at runtime.
     */
    struct sensor_value thr = {.val1 = 0x03, .val2 = 0};
    ret = sensor_attr_set(ma780, SENSOR_CHAN_ROTATION, SENSOR_ATTR_UPPER_THRESH, &thr);
    if (ret < 0)
    {
      LOG_ERR("MA780 verified write (threshold) failed: %d", ret);
    }

    /* Read the absolute angle */
    ret = sensor_sample_fetch(ma780);
    if (ret < 0)
    {
      LOG_ERR("MA780 sample fetch failed: %d", ret);
    }
    else
    {
      struct sensor_value angle;
      sensor_channel_get(ma780, SENSOR_CHAN_ROTATION, &angle);
      LOG_INF("MA780 angle: %d", angle.val1);
    }

    k_msleep(500);
  }
}
