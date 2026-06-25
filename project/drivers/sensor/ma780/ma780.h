// Confidential - Copyright PA.COTTE: All rights reserved

#ifndef ZEPHYR_DRIVERS_SENSOR_MA780_H_
#define ZEPHYR_DRIVERS_SENSOR_MA780_H_

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/types.h>

/* MA780 communication commands */
#define MA780_READ_ANGLE (0x00)
#define MA780_WRITE_REG (0x80)
#define MA780_READ_REG (0x40)

/* Registers */
#define MA780_REG_ZERO_LSB (0x00)
#define MA780_REG_ZERO_MSB (0x01)
#define MA780_REG_TRIM (0x03)
#define MA780_REG_THR (0x08)
#define MA780_REG_DIRECTION (0x09)
#define MA780_REG_REF (0x0A)
#define MA780_REG_ASC (0x0B)
#define MA780_REG_RES_CFG (0x16)
#define MA780_REG_STATUS (0x1A)

/* MA780_REG_TRIM definitions */
#define MA780_TRIM_EN_X (0x01)
#define MA780_TRIM_EN_Y (0x02)

/* MA780_REG_RES_CFG definitions */
#define MA780_RES_0_TURN (0x00)
#define MA780_RES_16_TURN (0x08)
#define MA780_RES_64_TURN (0x10)
#define MA780_RES_256_TURN (0x18)
#define MA780_RES_MASK (MA780_RES_256_TURN)

/* MA780_REG_DIRECTION definitions */
#define MA780_DIR_CW (0 << 7)
#define MA780_DIR_CCW (1 << 7)

/* MA780_REG_ASC definitions */
#define MA780_ASC_AUTACT (1 << 5)
#define MA780_ASC_ASC (1 << 6)
#define MA780_ASC_ASCR (1 << 7)

/* MA780_REG_STATUS definitions */
#define MA780_STATUS_ERRNVM (1 << 1)
#define MA780_STATUS_ERRMEM (1 << 2)
#define MA780_STATUS_ERRPAR (1 << 3)

/* MA780_REG_THR definitions */
#define MA780_THR_ONE_DEG (0x03)

/* Angle definitions for 64 turns */
#define MA780_ANGLE_MASK (0x03FF) // 10 bits for angle
#define MA780_ANGLE_RESOLUTION (0x0400)
#define MA780_TURN_MASK (0xFC00) // 6 bits for turn
#define MA780_ANGLE_DEGREE (360)
#define MA780_MAX_TURNS (64)
#define MA780_TURNS_MARGIN (5)

struct ma780_config
{
    struct spi_dt_spec bus;
#if defined(CONFIG_MA780_TRIGGER)
    struct gpio_dt_spec interrupt;
    uint8_t int_config;
#endif
};

struct ma780_data
{
    /* Absolute angle value in 1/3 of degrees */
    uint16_t angle;

#if defined(CONFIG_MA780_TRIGGER)
    const struct device *dev;
    struct gpio_callback gpio_cb;
    struct k_mutex trigger_mutex;

    sensor_trigger_handler_t inact_handler;
    const struct sensor_trigger *inact_trigger;
    sensor_trigger_handler_t act_handler;
    const struct sensor_trigger *act_trigger;
    sensor_trigger_handler_t drdy_handler;
    const struct sensor_trigger *drdy_trigger;

#if defined(CONFIG_MA780_TRIGGER_OWN_THREAD)
    K_KERNEL_STACK_MEMBER(thread_stack, CONFIG_MA780_THREAD_STACK_SIZE);
    struct k_sem gpio_sem;
    struct k_thread thread;
#elif defined(CONFIG_MA780_TRIGGER_GLOBAL_THREAD)
    struct k_work work;
#endif
#endif /* CONFIG_MA780_TRIGGER */
};

int ma780_read_raw_angle(const struct device *dev, uint16_t *rawAngle);
int ma780_set_reg(const struct device *dev, uint8_t register_value, uint8_t register_address);
int ma780_get_reg(const struct device *dev, uint8_t *read_buf, uint8_t register_address);
int ma780_read_raw_angle(const struct device *dev, uint16_t *rawAngle);
uint16_t ma780_raw_to_angle(uint16_t rawAngle);
uint16_t ma780_raw_to_turn(int16_t rawAngle);
uint16_t ma780_raw_to_absolute(uint16_t rawAngle);

#ifdef CONFIG_MA780_TRIGGER
int ma780_set_reg_mask(const struct device *dev, uint8_t register_value, uint8_t register_address, uint8_t mask);
int ma780_get_status(const struct device *dev, uint8_t *status);
int ma780_interrupt_activity_enable(const struct device *dev);
int ma780_trigger_set(const struct device *dev, const struct sensor_trigger *trig, sensor_trigger_handler_t handler);
int ma780_init_interrupt(const struct device *dev);
#endif /* CONFIG_MA780_TRIGGER */

#endif /* ZEPHYR_DRIVERS_SENSOR_MA780_H_ */
