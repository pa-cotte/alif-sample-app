#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>

#define GREEN_LED_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(GREEN_LED_NODE, gpios);

LOG_MODULE_REGISTER(app);

#define DEVICE_NAME     CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN (sizeof(DEVICE_NAME) - 1)

static const struct bt_data ad[] = {
  BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
  BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void start_adv(void)
{
  int err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_2, ad, ARRAY_SIZE(ad), NULL, 0);
  if (err) {
    LOG_ERR("Advertising failed to start: %d", err);
  } else {
    LOG_INF("Advertising as \"%s\"", DEVICE_NAME);
  }
}

static void on_connected(struct bt_conn *conn, uint8_t err)
{
  if (err) {
    LOG_ERR("Connection failed (err 0x%02x)", err);
    return;
  }
  LOG_INF("Connected");
}

static void on_disconnected(struct bt_conn *conn, uint8_t reason)
{
  LOG_INF("Disconnected (reason 0x%02x)", reason);
  start_adv();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
  .connected    = on_connected,
  .disconnected = on_disconnected,
};

auto main() -> int
{
  int ret;

  if (!gpio_is_ready_dt(&led)) {
    LOG_ERR("Led not ready");
    return 0;
  }

  ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);
  if (ret < 0) {
    LOG_ERR("Led config failed");
    return 0;
  }

  ret = bt_enable(NULL);
  if (ret) {
    LOG_ERR("bt_enable failed: %d", ret);
    return 0;
  }

  start_adv();

  while (true) {
    LOG_INF("Blink from HE!");
    ret = gpio_pin_toggle_dt(&led);
    if (ret < 0) {
      LOG_ERR("Led toggle failed");
      return 0;
    }
    k_msleep(500);
  }
}
