// Confidential - Copyright PA.COTTE: All rights reserved

#define DT_DRV_COMPAT mps_ma780

#include <string.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/byteorder.h>

#include "ma780.h"

LOG_MODULE_REGISTER(MA780, CONFIG_SENSOR_LOG_LEVEL);

static int ma780_reg_access(const struct device *dev, uint8_t cmd, uint8_t reg_addr, void *data, size_t length)
{
  const struct ma780_config *cfg = dev->config;
  uint8_t access[2] = {cmd | reg_addr, 0};
  uint16_t angle = 0;
  struct spi_buf buffer[2] = {{.buf = access, .len = 2}, {.buf = data, .len = length}};
  const struct spi_buf_set tx = {.buffers = buffer, .count = 1};
  const struct spi_buf_set rx = {.buffers = buffer, .count = 2};

  switch (cmd)
  {
    case MA780_READ_ANGLE: {
      /* For the angle command, just send twice null bytes */
      buffer[0].buf = &angle;
      int ret = spi_transceive_dt(&cfg->bus, NULL, &tx);
      /* Reorder the 16 bits angle value */
      *(uint16_t *)data = sys_be16_to_cpu(angle);
      return ret;
    }

    case MA780_READ_REG: {
      /* Pass only the 8 bits register address for read operation */
      return spi_transceive_dt(&cfg->bus, &tx, &rx);
    }

    case MA780_WRITE_REG: {
      /* Pass the 8 bits register value for write operation */
      access[1] = ((uint8_t *)data)[0];
      return spi_transceive_dt(&cfg->bus, &tx, &rx);
    }
  }

  return -ENOTSUP;
}

/* Number of attempts for a verified register write, so retry before giving up. */
#define MA780_WRITE_RETRIES (3)

/**
 * @brief Write a register and verify the sensor acknowledged the value.
 *
 * The write command's second SPI frame returns the stored register value. The datasheet
 * recommends checking it matches the requested value, since a write can silently fail
 * during an ASC active/idle transition.
 */
static int ma780_write_reg(const struct device *dev, uint8_t register_address, uint8_t register_value)
{
  uint8_t ack[2] = {0};
  int ret = 0;

  for (int attempt = 0; attempt < MA780_WRITE_RETRIES; attempt++)
  {
    /* ack[0] holds the value to write (read by ma780_reg_access), then the rx phase
     * overwrites the buffer with [angle(15:8)][register value read back]. */
    ack[0] = register_value;
    ret = ma780_reg_access(dev, MA780_WRITE_REG, register_address, ack, sizeof(ack));
    if (ret)
    {
      return ret;
    }

    if (ack[1] == register_value)
    {
      return 0;
    }
  }

  LOG_ERR("reg 0x%.2X write not acknowledged after %d attempts: wrote 0x%.2X, read 0x%.2X", register_address, MA780_WRITE_RETRIES, register_value,
          ack[1]);
  return -EIO;
}

int ma780_set_reg(const struct device *dev, uint8_t register_value, uint8_t register_address)
{
  return ma780_write_reg(dev, register_address, register_value);
}

int ma780_set_reg_mask(const struct device *dev, uint8_t register_value, uint8_t register_address, uint8_t mask)
{
  int ret = 0;
  uint8_t valueTable[2] = {0};

  /* Read the current value of the register */
  ret = ma780_reg_access(dev, MA780_READ_REG, register_address, valueTable, sizeof(valueTable));
  if (ret)
  {
    return ret;
  }

  /* Apply the mask and data, then write back with acknowledge verification */
  valueTable[1] &= ~mask;
  valueTable[1] |= register_value;

  return ma780_write_reg(dev, register_address, valueTable[1]);
}

int ma780_get_reg(const struct device *dev, uint8_t *read_buf, uint8_t register_address)
{
  uint8_t valueTable[2] = {0};
  int ret = ma780_reg_access(dev, MA780_READ_REG, register_address, valueTable, 2);
  if (ret)
  {
    return ret;
  }

  /* Return only the second byte (the register value) */
  *read_buf = valueTable[1];

  return 0;
}

int ma780_get_status(const struct device *dev, uint8_t *status)
{
  return ma780_get_reg(dev, status, MA780_REG_STATUS);
}

int ma780_read_raw_angle(const struct device *dev, uint16_t *rawAngle)
{
  return ma780_reg_access(dev, MA780_READ_ANGLE, MA780_READ_ANGLE, rawAngle, 2);
}

uint16_t ma780_raw_to_angle(uint16_t rawAngle)
{
  // Convert the raw angle to degrees
  return (rawAngle & MA780_ANGLE_MASK) * MA780_ANGLE_DEGREE / MA780_ANGLE_RESOLUTION; // Assuming 10 bits angle resolution
}

uint16_t ma780_raw_to_turn(int16_t rawAngle)
{
  // Convert the raw angle to turns
  return (rawAngle & MA780_TURN_MASK) >> 10; // Assuming 10 bits angle resolution
}

uint16_t ma780_raw_to_absolute(uint16_t rawAngle)
{
  int turns = ma780_raw_to_turn(rawAngle);

  // Convert the raw angle and turns to absolute value
  if (turns > (MA780_MAX_TURNS - MA780_TURNS_MARGIN))
  {
    turns = turns - MA780_MAX_TURNS; // Handle negative turns by wrapping around
  }

  return ((rawAngle & MA780_ANGLE_MASK) + ((turns + MA780_TURNS_MARGIN) * MA780_ANGLE_RESOLUTION)); // Assuming 10 bits angle resolution
}

static int ma780_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
  struct ma780_data *data = dev->data;

  switch (chan)
  {
    case SENSOR_CHAN_ROTATION: /* Acceleration on the X axis, in m/s^2. */
    case SENSOR_CHAN_MAX:
      val->val1 = data->angle;
      val->val2 = 0;
      break;
    default:
      return -ENOTSUP;
  }

  return 0;
}

static int ma780_attr_set(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, const struct sensor_value *val)
{
  switch (attr)
  {
    case SENSOR_ATTR_UPPER_THRESH:
      return ma780_set_reg(dev, (uint8_t)val->val1, MA780_REG_THR);
    default:
      /* Do nothing */
      break;
  }

  return 0;
}

static int ma780_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
  struct ma780_data *data = dev->data;
  int16_t rawAngle = 0;
  int ret = 0;

  __ASSERT_NO_MSG((chan == SENSOR_CHAN_ALL) || (chan == SENSOR_CHAN_ROTATION));

  ret = ma780_read_raw_angle(dev, &rawAngle);
  if (ret)
  {
    LOG_ERR("Failed to read raw angle: %d", ret);
    return ret;
  }

  data->angle = ma780_raw_to_absolute(rawAngle);

  return 0;
}

static const struct sensor_driver_api ma780_api_funcs = {
    .attr_set = ma780_attr_set,
    .sample_fetch = ma780_sample_fetch,
    .channel_get = ma780_channel_get,
#ifdef CONFIG_MA780_TRIGGER
    .trigger_set = ma780_trigger_set,
#endif
};

/**
 * @brief Initializes component.
 *
 * @return  0 - the initialization was successful and the device is present;
 *         -1 - an error occurred.
 */
static int ma780_init(const struct device *dev)
{
  const struct ma780_config *config = dev->config;
  int ret = 0;
  uint16_t angle = 0;

  /* Check SPI bus */
  if (!spi_is_ready_dt(&config->bus))
  {
    LOG_ERR("spi device not ready: %s", config->bus.bus->name);
    return -EINVAL;
  }

  /* The MA780 powers up in ASC mode by default (register 11 = 0xE0), where the front end
   * cycles between active and idle and register writes may silently fail during a
   * transition. Force idle mode (ASCR=1, ASC=0) before configuring so the writes below
   * are reliable; ASC mode is re-enabled as the very last step. */
  ret = ma780_set_reg(dev, MA780_ASC_ASCR, MA780_REG_ASC);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_ASC, ret);
    return ret;
  }

  /* Enable the trimming on X and Y axis */
  ret = ma780_set_reg(dev, MA780_TRIM_EN_X | MA780_TRIM_EN_Y, MA780_REG_TRIM);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_TRIM, ret);
    return ret;
  }

  /* Configure the sensor with 64 turns: 10 bits angle definition and 6 bits index definition */
  ret = ma780_set_reg_mask(dev, MA780_RES_64_TURN, MA780_REG_RES_CFG, MA780_RES_MASK);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_RES_CFG, ret);
    return ret;
  }

  /* Configure the rotation direction */
  ret = ma780_set_reg(dev, MA780_DIR_CCW, MA780_REG_DIRECTION);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_DIRECTION, ret);
    return ret;
  }

  /* Read the angle value */
  ret = ma780_read_raw_angle(dev, &angle);
  if (ret)
  {
    LOG_ERR("Read angle failed: error %d", ret);
    return ret;
  }
  LOG_INF("read ANGLE: %d / TURN: %d", ma780_raw_to_angle(angle), ma780_raw_to_turn(angle));

#if defined(CONFIG_MA780_TRIGGER)

  /* Configure the threshold angle for ND trigger */
  ret = ma780_set_reg(dev, MA780_THR_ONE_DEG, MA780_REG_THR);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_THR, ret);
    return ret;
  }
  /* Enable the new data interrupt */
  if (config->interrupt.port)
  {
    if (ma780_init_interrupt(dev) < 0)
    {
      LOG_ERR("Failed to initialize interrupt!");
      return -EIO;
    }
  }

#endif

  /* Enable ASC mode now that every other register is configured */
  ret = ma780_set_reg(dev, MA780_ASC_AUTACT | MA780_ASC_ASC | MA780_ASC_ASCR, MA780_REG_ASC);
  if (ret)
  {
    LOG_ERR("spi device %s write reg 0x%.2X failed: error %d", dev->name, MA780_REG_ASC, ret);
    return ret;
  }

  return 0;
}

#define MA780_DEFINE(inst)                                                                                                                           \
  static struct ma780_data ma780_data_##inst;                                                                                                        \
                                                                                                                                                     \
  static const struct ma780_config ma780_config_##inst = {                                                                                           \
      .bus = SPI_DT_SPEC_INST_GET(inst, SPI_WORD_SET(8) | SPI_TRANSFER_MSB, 0),                                                                      \
      IF_ENABLED(CONFIG_MA780_TRIGGER, (.interrupt = GPIO_DT_SPEC_INST_GET_OR(inst, int_gpios, {0}), ))};                                            \
                                                                                                                                                     \
  SENSOR_DEVICE_DT_INST_DEFINE(inst, ma780_init, NULL, &ma780_data_##inst, &ma780_config_##inst, POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY,           \
                               &ma780_api_funcs);

DT_INST_FOREACH_STATUS_OKAY(MA780_DEFINE)
