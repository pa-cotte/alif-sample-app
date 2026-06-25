// Confidential - Copyright PA.COTTE: All rights reserved

#define DT_DRV_COMPAT mps_ma780

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include "ma780.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(MA780, CONFIG_SENSOR_LOG_LEVEL);

static void ma780_thread_cb(const struct device *dev)
{
  struct ma780_data *drv_data = dev->data;
  uint16_t angle = 0;
  int ret = 0;

  /* Read the angle value (and clear interrupt) */
  ret = ma780_read_raw_angle(dev, &angle);
  if (ret)
  {
    LOG_ERR("Read angle failed: error %d", ret);
  }
  LOG_DBG("read ANGLE: %d degres / %d tours / abs %d", ma780_raw_to_angle(angle), ma780_raw_to_turn(angle), ma780_raw_to_absolute(angle));

  k_mutex_lock(&drv_data->trigger_mutex, K_FOREVER);
  if (drv_data->act_handler != NULL)
  {
    drv_data->act_handler(dev, drv_data->act_trigger);
  }
  k_mutex_unlock(&drv_data->trigger_mutex);
}

static void ma780_gpio_callback(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
  struct ma780_data *drv_data = CONTAINER_OF(cb, struct ma780_data, gpio_cb);

#if defined(CONFIG_MA780_TRIGGER_OWN_THREAD)
  k_sem_give(&drv_data->gpio_sem);
#elif defined(CONFIG_MA780_TRIGGER_GLOBAL_THREAD)
  k_work_submit(&drv_data->work);
#endif
}

#if defined(CONFIG_MA780_TRIGGER_OWN_THREAD)
static void ma780_thread(void *p1, void *p2, void *p3)
{
  ARG_UNUSED(p2);
  ARG_UNUSED(p3);

  struct ma780_data *drv_data = p1;

  while (true)
  {
    k_sem_take(&drv_data->gpio_sem, K_FOREVER);
    ma780_thread_cb(drv_data->dev);
  }
}
#elif defined(CONFIG_MA780_TRIGGER_GLOBAL_THREAD)
static void ma780_work_cb(struct k_work *work)
{
  struct ma780_data *drv_data = CONTAINER_OF(work, struct ma780_data, work);

  ma780_thread_cb(drv_data->dev);
}
#endif

int ma780_trigger_set(const struct device *dev, const struct sensor_trigger *trig, sensor_trigger_handler_t handler)
{
  struct ma780_data *drv_data = dev->data;
  const struct ma780_config *config = dev->config;

  if (!config->interrupt.port)
  {
    return -ENOTSUP;
  }

  switch (trig->type)
  {
    case SENSOR_TRIG_DATA_READY:
      k_mutex_lock(&drv_data->trigger_mutex, K_FOREVER);
      drv_data->act_handler = handler;
      drv_data->act_trigger = trig;
      k_mutex_unlock(&drv_data->trigger_mutex);
      break;
    default:
      LOG_ERR("Unsupported sensor trigger");
      return -ENOTSUP;
  }

  return 0;
}

int ma780_init_interrupt(const struct device *dev)
{
  const struct ma780_config *cfg = dev->config;
  struct ma780_data *drv_data = dev->data;
  int ret;

  k_mutex_init(&drv_data->trigger_mutex);

  if (!gpio_is_ready_dt(&cfg->interrupt))
  {
    LOG_ERR("GPIO port %s not ready", cfg->interrupt.port->name);
    return -ENODEV;
  }

  ret = gpio_pin_configure_dt(&cfg->interrupt, GPIO_INPUT);
  if (ret < 0)
  {
    return ret;
  }

  gpio_init_callback(&drv_data->gpio_cb, ma780_gpio_callback, BIT(cfg->interrupt.pin));

  ret = gpio_add_callback(cfg->interrupt.port, &drv_data->gpio_cb);
  if (ret < 0)
  {
    return ret;
  }

  drv_data->dev = dev;

#if defined(CONFIG_MA780_TRIGGER_OWN_THREAD)
  k_sem_init(&drv_data->gpio_sem, 0, K_SEM_MAX_LIMIT);

  k_thread_create(&drv_data->thread, drv_data->thread_stack, CONFIG_MA780_THREAD_STACK_SIZE, ma780_thread, drv_data, NULL, NULL,
                  K_PRIO_COOP(CONFIG_MA780_THREAD_PRIORITY), 0, K_NO_WAIT);
#elif defined(CONFIG_MA780_TRIGGER_GLOBAL_THREAD)
  drv_data->work.handler = ma780_work_cb;
#endif

  ret = gpio_pin_interrupt_configure_dt(&cfg->interrupt, GPIO_INT_EDGE_TO_ACTIVE);
  if (ret < 0)
  {
    return ret;
  }

  return 0;
}
