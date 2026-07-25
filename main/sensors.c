#include "sensors.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdlib.h>

// ---------------------------------------------------------------------
// MOCK_HARDWARE = 1: no board needed, generates plausible fake readings
// so the rest of the pipeline (queue, logging, TinyML) can be built and
// tested now. Flip to 0 once the ESP32 + sensors are wired up, and fill
// in the TODOs below with real ADC/I2C driver calls.
// ---------------------------------------------------------------------
#define MOCK_HARDWARE 1

static const char *TAG = "sensors";

#if MOCK_HARDWARE

void sensors_init(void)
{
    ESP_LOGI(TAG, "sensors_init: MOCK_HARDWARE mode, no peripherals configured");
    srand((unsigned int)esp_timer_get_time());
}

int sensors_read(sensor_reading_t *out)
{
    // Plausible-looking but fake values, with a little jitter so
    // logged data isn't perfectly flat.
    out->timestamp_us     = esp_timer_get_time();
    out->soil_moisture_pct = 35.0f + (rand() % 200) / 10.0f;   // ~35-55%
    out->temperature_c     = 21.0f + (rand() % 40) / 10.0f;    // ~21-25 C
    out->humidity_pct      = 40.0f + (rand() % 200) / 10.0f;   // ~40-60%
    out->pressure_hpa      = 1010.0f + (rand() % 50) / 10.0f;  // ~1010-1015
    return 0;
}

#else // real hardware

// TODO(Week 1): replace with real driver includes:
// #include "driver/adc.h"
// #include "driver/i2c.h"
// #include "bme280.h" (or whichever BME280 driver you settle on)

void sensors_init(void)
{
    // TODO: configure ADC1 channel for the capacitive soil moisture sensor
    //   adc1_config_width(ADC_WIDTH_BIT_12);
    //   adc1_config_channel_atten(ADC1_CHANNEL_X, ADC_ATTEN_DB_11);
    //
    // TODO: configure I2C master for the BME280
    //   i2c_config_t conf = { ... };
    //   i2c_param_config(I2C_NUM_0, &conf);
    //   i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    //
    // TODO: BME280 init sequence (reset, read calibration registers,
    //   set oversampling/mode registers) -- this is the part worth
    //   writing yourself rather than pulling a library wholesale, since
    //   walking the datasheet register map is the actual learning here.
    ESP_LOGW(TAG, "sensors_init: real hardware path not yet implemented");
}

int sensors_read(sensor_reading_t *out)
{
    out->timestamp_us = esp_timer_get_time();

    // TODO: adc1_get_raw(ADC1_CHANNEL_X) -> convert raw counts to a
    //   moisture percentage. You'll need to calibrate: read the sensor
    //   fully dry and fully submerged in water, and linearly map the
    //   raw ADC range between those two points to 0-100%.
    out->soil_moisture_pct = 0.0f;

    // TODO: I2C read of BME280 data registers (0xF7-0xFE), then apply
    //   the compensation formulas from the datasheet using the
    //   calibration constants read during init.
    out->temperature_c = 0.0f;
    out->humidity_pct = 0.0f;
    out->pressure_hpa = 0.0f;

    return -1; // not implemented yet
}

#endif // MOCK_HARDWARE
