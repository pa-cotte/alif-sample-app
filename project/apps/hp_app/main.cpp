#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/gpio.h>
#include <stdio.h>

#include "edge-impulse-sdk/classifier/ei_run_classifier.h"

// LED (facultatif, pour voir que ça tourne)
#define RED_LED_NODE DT_ALIAS(led0)
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(RED_LED_NODE, gpios);

// Accéléromètre
static const struct device *sensor = DEVICE_DT_GET(DT_NODELABEL(lis2dux12_body));

// Buffer brut : 100 échantillons * 3 axes = 300 valeurs
static float features[EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE] = {0};
static size_t feat_index = 0;

// Callback utilisé par Edge Impulse pour lire les données

static int get_feature_callback(size_t offset, size_t length, float *out_ptr)
{
  memcpy(out_ptr, features + offset, length * sizeof(float));
  return 0;
}

// On met le résultat en global pour ne pas le mettre sur la pile
static ei_impulse_result_t g_result;

auto main() -> int
{

  printf("Starting EI realtime classifier...\n");

  // --- Vérif capteur ---
  if (!device_is_ready(sensor))
  {
    printf("Accelerometer NOT READY\n");
    return 0;
  }

  // --- LED ---
  if (gpio_is_ready_dt(&led))
  {
    gpio_pin_configure_dt(&led, GPIO_OUTPUT);
  }

  // Une prédiction = 100 échantillons @100 Hz -> 1 seconde de données
  const size_t frame_size = EI_CLASSIFIER_DSP_INPUT_FRAME_SIZE; // 300

  while (true)
  {

    gpio_pin_toggle_dt(&led);

    struct sensor_value accel[3];
    int rc = sensor_sample_fetch_chan(sensor, SENSOR_CHAN_ACCEL_XYZ);

    if (rc != 0)
    {
      printf("sensor_sample_fetch_chan error: %d\n", rc);
      k_msleep(10);
      continue;
    }

    rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);

    if (rc != 0)
    {
      printf("sensor_channel_get error: %d\n", rc);
      k_msleep(10);
      continue;
    }

    float ax = sensor_value_to_double(&accel[0]);
    float ay = sensor_value_to_double(&accel[1]);
    float az = sensor_value_to_double(&accel[2]);

    // printf("%.3f,%.3f,%.3f\n", ax, ay, az);

    features[feat_index++] = ax;
    features[feat_index++] = ay;
    features[feat_index++] = az;

    // Quand la fenêtre est pleine, on lance le modèle
    if (feat_index >= frame_size)
    {

      // On repars au début pour la prochaine fenêtre
      feat_index = 0;

      // Création du signal attendu par EI
      signal_t signal;
      signal.total_length = frame_size;
      signal.get_data = get_feature_callback;

      // Lancer le modèle
      EI_IMPULSE_ERROR ei_status = run_classifier(&signal, &g_result, false);

      if (ei_status != EI_IMPULSE_OK)
      {
        printf("EI classifier error: %d\n", ei_status);
      }
      else
      {
        printf("\n=== PREDICTION ===\n");

        // Affichage brut
        for (size_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++)
        {
          printf("%s : %.3f\n",
                 g_result.classification[i].label,
                 g_result.classification[i].value);
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

        printf("**Detected state : %s  \n", best_label, best_value);
      }
    }

    // Attendre 10 ms pour respecter 100 Hz
    k_msleep(EI_CLASSIFIER_INTERVAL_MS);
  }

  return 0;
}
