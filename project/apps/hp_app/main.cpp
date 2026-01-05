#include <zephyr/types.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/cache.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>
#include <stdlib.h>
#include "npu_handler.h"

#include <cmsis_compiler.h>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

#include <ethosu_driver.h>

// LED
#define RED_LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

// Accéléromètre
static const struct device *sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dux12_body));

// Buffer de données aligné
__aligned(16) static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
static size_t feat_index = 0;

// Callback pour Edge Impulse
static int get_feature_callback(size_t offset, size_t length, float *out_ptr)
{
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

static ei_impulse_result_t g_result;

auto main() -> int
{

  printk("=== ATTEMPTING INFERENCE ===\n");

  printf("Starting EI realtime classifier ...\n");

  if (!device_is_ready(sensor))
  {
    printf("Accelerometer NOT READY\n");
    return 0;
  }
  else
  {
    printf("Accelerometer is ready\n");
  }

  if (gpio_is_ready_dt(&led))
  {
    gpio_pin_configure_dt(&led, GPIO_OUTPUT);
  }

  const struct device *ethosu = DEVICE_DT_GET(DT_NODELABEL(ethosu0));
  if (!device_is_ready(ethosu))
  {
    printf("Ethos-U device not ready\n");
  }
  else
  {
    printf("Ethos-U device is ready\n");
  }

  const size_t frame_size = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE;

  // Test malloc

  // void *p = malloc(16);
  // printf("features %p size=%zu\n", (void *)features, sizeof(features));
  // printf("malloc ptr=%p\n", p);

  // int ret = npu_init();
  // if (ret != 0)
  // {
  //   printk("Erreur: Impossible d'initialiser le NPU\n");
  //   return ret;
  // }
  // printk("NPU initialisé avec succès !\n");

  while (true)
  {
    gpio_pin_toggle_dt(&led);

    struct sensor_value accel[3];
    sensor_sample_fetch_chan(sensor, SENSOR_CHAN_ACCEL_XYZ);
    sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);

    features[feat_index++] = sensor_value_to_double(&accel[0]);
    features[feat_index++] = sensor_value_to_double(&accel[1]);
    features[feat_index++] = sensor_value_to_double(&accel[2]);

    if (feat_index >= frame_size)
    {
      feat_index = 0;

      signal_t signal;
      signal.total_length = frame_size;
      signal.get_data = get_feature_callback;

      sys_cache_data_flush_all();

      ei_printf("\n=== start inference ===\n");

      EI_IMPULSE_ERROR ei_status = run_classifier(&signal, &g_result, false);

      sys_cache_data_invd_all();
      ei_printf("\n=== end inference ===\n");

      if (ei_status != EI_IMPULSE_OK)
      {
        printf("EI classifier error: %d\n", ei_status);
      }
      else
      {
        printf("\n=== PREDICTION ===\n");
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
        {
          printf("%s : %.3f\n", g_result.classification[i].label, g_result.classification[i].value);
        }

        // === Trouver la classe dominante ===
        float best_value = -1.0f;
        const char *best_label = "unknown";

        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
        {
          if (g_result.classification[i].value > best_value)
          {
            best_value = g_result.classification[i].value;
            best_label = g_result.classification[i].label;
          }
        }

        printf("**Detected state : %s (%.3f)\n", best_label, best_value);
        ei_free(g_result.classification);
      }
    }
    k_msleep(EI_CLASSIFIER_INTERVAL_MS);
  }
  return 0;
}
