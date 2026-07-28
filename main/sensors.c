#include "sensors.h"
#include "bme280.h"
#include "driver/gpio.h"
#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include <esp_adc/adc_oneshot.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdlib.h>
#include <string.h>

#define MOCK_HARDWARE 0
#define ADC_PIN                                                                \
  ADC_CHANNEL_7 // Channel 7 - Check ESP32 Pinout for the GPIO Number
#define ADC_UNIT ADC_UNIT_1          // ADC1
#define ADC_BITWIDTH ADC_BITWIDTH_12 // 12-bit resolution (0-4095)
#define ADC_ATTEN ADC_ATTEN_DB_12    // ~3.3V full-scale voltage
#define SOIL_ADC_DRY 2800
#define SOIL_ADC_WET 1200
#define SDA_PIN GPIO_NUM_21
#define SCL_PIN GPIO_NUM_22

#define I2C_MASTER_ACK 0
#define I2C_MASTER_NACK 1

static const char *TAG = "sensors";
static const char *TAG_BME280 = "BME280";
int adc_value;
adc_oneshot_unit_handle_t adc_handle;
struct bme280_t bme280;
s32 com_rslt;

#if MOCK_HARDWARE

void sensors_init(void) {
  ESP_LOGI(TAG, "sensors_init: MOCK_HARDWARE mode, no peripherals configured");
  srand((unsigned int)esp_timer_get_time());
}

int sensors_read(sensor_reading_t *out) {
  // Plausible-looking but fake values, with a little jitter so
  // logged data isn't perfectly flat.
  out->timestamp_us = esp_timer_get_time();
  out->soil_moisture_pct = 35.0f + (rand() % 200) / 10.0f; // ~35-55%
  out->temperature_c = 21.0f + (rand() % 40) / 10.0f;      // ~21-25 C
  out->humidity_pct = 40.0f + (rand() % 200) / 10.0f;      // ~40-60%
  out->pressure_hpa = 1010.0f + (rand() % 50) / 10.0f;     // ~1010-1015
  return 0;
}

#else // real hardware

s8 BME280_I2C_bus_write(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt) {
  s32 iError = BME280_INIT_VALUE;

  esp_err_t espRc;
  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);

  i2c_master_write_byte(cmd, reg_addr, true);
  i2c_master_write(cmd, reg_data, cnt, true);
  i2c_master_stop(cmd);

  espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
  if (espRc == ESP_OK) {
    iError = SUCCESS;
  } else {
    iError = FAIL;
  }
  i2c_cmd_link_delete(cmd);

  return (s8)iError;
}

s8 BME280_I2C_bus_read(u8 dev_addr, u8 reg_addr, u8 *reg_data, u8 cnt) {
  s32 iError = BME280_INIT_VALUE;
  esp_err_t espRc;

  i2c_cmd_handle_t cmd = i2c_cmd_link_create();

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_WRITE, true);
  i2c_master_write_byte(cmd, reg_addr, true);

  i2c_master_start(cmd);
  i2c_master_write_byte(cmd, (dev_addr << 1) | I2C_MASTER_READ, true);

  if (cnt > 1) {
    i2c_master_read(cmd, reg_data, cnt - 1, I2C_MASTER_ACK);
  }
  i2c_master_read_byte(cmd, reg_data + cnt - 1, I2C_MASTER_NACK);
  i2c_master_stop(cmd);

  espRc = i2c_master_cmd_begin(I2C_NUM_0, cmd, pdMS_TO_TICKS(10));
  if (espRc == ESP_OK) {
    iError = SUCCESS;
  } else {
    iError = FAIL;
  }

  i2c_cmd_link_delete(cmd);

  return (s8)iError;
}

void sensors_init(void) {

  // Initialize ADC Oneshot Mode Driver on the ADC Unit
  adc_oneshot_unit_init_cfg_t init_config = {
      .unit_id = ADC_UNIT,
      .clk_src = ADC_RTC_CLK_SRC_DEFAULT,
  };
  ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc_handle));

  // Configure ADC channel
  adc_oneshot_chan_cfg_t config = {
      .bitwidth = ADC_BITWIDTH,
      .atten = ADC_ATTEN,
  };
  ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_PIN, &config));

  i2c_config_t i2c_config = {.mode = I2C_MODE_MASTER,
                             .sda_io_num = SDA_PIN,
                             .scl_io_num = SCL_PIN,
                             .sda_pullup_en = GPIO_PULLUP_ENABLE,
                             .scl_pullup_en = GPIO_PULLUP_ENABLE,
                             .master.clk_speed = 400000};
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0, &i2c_config));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0, I2C_MODE_MASTER, 0, 0, 0));

  bme280 = (struct bme280_t){.bus_write = BME280_I2C_bus_write,
                             .bus_read = BME280_I2C_bus_read,
                             .dev_addr = BME280_I2C_ADDRESS1,
                             .delay_msec = BME280_delay_msek};
  com_rslt = bme280_init(&bme280);
  com_rslt += bme280_set_oversamp_pressure(BME280_OVERSAMP_16X);
  com_rslt += bme280_set_oversamp_temperature(BME280_OVERSAMP_2X);
  com_rslt += bme280_set_oversamp_humidity(BME280_OVERSAMP_1X);
  com_rslt += bme280_set_standby_durn(BME280_STANDBY_TIME_1_MS);
  com_rslt += bme280_set_filter(BME280_FILTER_COEFF_16);
  com_rslt += bme280_set_power_mode(BME280_NORMAL_MODE);
  if (com_rslt != SUCCESS) {
    ESP_LOGE(TAG, "BME280 init/config failed: %ld", (long)com_rslt);
  } else {
    ESP_LOGI(TAG, "sensors_init complete");
  }
}

static float soil_raw_to_percent(int raw) {
  float pct = 100.0f * (float)(SOIL_ADC_DRY - raw) /
              (float)(SOIL_ADC_DRY - SOIL_ADC_WET);
  if (pct < 0.0f)
    pct = 0.0f;
  if (pct > 100.0f)
    pct = 100.0f;
  return pct;
}

void get_bme280_readings(sensor_reading_t *out) {

  s32 v_uncomp_pressure_s32;
  s32 v_uncomp_temperature_s32;
  s32 v_uncomp_humidity_s32;
  bme280_read_uncomp_pressure_temperature_humidity(&v_uncomp_pressure_s32,
                                                   &v_uncomp_temperature_s32,
                                                   &v_uncomp_humidity_s32);

  ESP_LOGI(TAG_BME280, "%.2f degC / %.2f hPa / %.2f %%",
           bme280_compensate_temperature_double(v_uncomp_temperature_s32),
           bme280_compensate_pressure_double(v_uncomp_pressure_s32) /
               100, // Pa -> hPa
           bme280_compensate_humidity_double(v_uncomp_humidity_s32));

  out->temperature_c =
      bme280_compensate_temperature_double(v_uncomp_temperature_s32);
  out->pressure_hpa =
      bme280_compensate_pressure_double(v_uncomp_pressure_s32) / 100;
  out->humidity_pct = bme280_compensate_humidity_double(v_uncomp_humidity_s32);
}

int sensors_read(sensor_reading_t *out) {
  out->timestamp_us = esp_timer_get_time();
  esp_err_t err = adc_oneshot_read(adc_handle, ADC_PIN, &adc_value);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "ADC read failed: %s", esp_err_to_name(err));
    return -1;
  }

  ESP_LOGI("ADC Value", "%d", adc_value);
  out->soil_moisture_pct = soil_raw_to_percent(adc_value);

  get_bme280_readings(out);

  return 0;
}

#endif
