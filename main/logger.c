#include "logger.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#define MOCK_HARDWARE 0

static const char *TAG = "logger";

#if MOCK_HARDWARE

int logger_init(void) {
  ESP_LOGI(TAG,
           "logger_init: MOCK_HARDWARE mode, writing to console instead of SD");
  return 0;
}

int logger_write(const sensor_reading_t *r) {
  // In real mode this becomes an fprintf() to the mounted SD file.
  // Keeping the exact same CSV format now means Week 2's SD swap is
  // a one-function change, not a rewrite.
  ESP_LOGI(TAG, "%lld,%.2f,%.2f,%.2f,%.2f", r->timestamp_us,
           r->soil_moisture_pct, r->temperature_c, r->humidity_pct,
           r->pressure_hpa);
  return 0;
}

#else // real hardware

static FILE *log_file = NULL;

int logger_init(void) {
  sdmmc_host_t host = SDSPI_HOST_DEFAULT();
  sdspi_slot_config_t slot_config = SDSPI_SLOT_CONFIG_DEFAULT();
  slot_config.gpio_miso = PIN_NUM_MISO;
  slot_config.gpio_mosi = PIN_NUM_MOSI;
  slot_config.gpio_sck = PIN_NUM_CLK;
  slot_config.gpio_cs = PIN_NUM_CS;
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config,
                                          &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set "
                    "format_if_mount_failed = true.");
    } else {
      ESP_LOGE(TAG,
               "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return;
  }

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);

  // Use POSIX and C standard library functions to work with files.
  // First create a file.

  // Check if destination file exists before renaming
  struct stat st;
  if (stat("/sdcard/foo.txt", &st) == 0) {
    // Delete it if it exists
    unlink("/sdcard/foo.txt");
  }

  // Open renamed file for reading
  ESP_LOGI(TAG, "Reading file");
  f = fopen("/sdcard/foo.txt", "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }
  slot_config.gpio_miso = PIN_NUM_MISO;
  slot_config.gpio_mosi = PIN_NUM_MOSI;
  slot_config.gpio_sck = PIN_NUM_CLK;
  slot_config.gpio_cs = PIN_NUM_CS;

  // Options for mounting the filesystem.
  // If format_if_mount_failed is set to true, SD card will be partitioned and
  // formatted in case when mounting fails.
  esp_vfs_fat_sdmmc_mount_config_t mount_config = {
      .format_if_mount_failed = false,
      .max_files = 5,
      .allocation_unit_size = 16 * 1024};

  // Use settings defined above to initialize SD card and mount FAT filesystem.
  // Note: esp_vfs_fat_sdmmc_mount is an all-in-one convenience function.
  // Please check its source code and implement error recovery when developing
  // production applications.
  sdmmc_card_t *card;
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config,
                                          &mount_config, &card);

  if (ret != ESP_OK) {
    if (ret == ESP_FAIL) {
      ESP_LOGE(TAG, "Failed to mount filesystem. "
                    "If you want the card to be formatted, set "
                    "format_if_mount_failed = true.");
    } else {
      ESP_LOGE(TAG,
               "Failed to initialize the card (%s). "
               "Make sure SD card lines have pull-up resistors in place.",
               esp_err_to_name(ret));
    }
    return;
  }

  // Card has been initialized, print its properties
  sdmmc_card_print_info(stdout, card);

  // First create a file.
  ESP_LOGI(TAG, "Opening file");
  log_file = fopen("/sdcard/logger.txt", "w");
  if (log_file == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  fprintf(log_file, "timestamp_us,soil_pct,temp_c,humidity_pct,pressure_hpa\n");

  return 0;
}

int logger_write(const sensor_reading_t *r) {
  if (log_file == NULL) {
    return -1;
  }
  // TODO: fprintf(log_file, "%lld,%.2f,%.2f,%.2f,%.2f\n", ...);
  ESP_LOGI(TAG, "Opening file");
  FILE *f = fopen("/sdcard/logger.txt", "w");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for writing");
    return;
  }
  fprintf(f, "%.2f,%.2f,%.2f,%.2f,%.2f\n", r->timestamp_us,
          r->soil_moisture_pct, r->temperature_c, r->humidity_pct,
          r->pressure_hpa);
  fclose(f);
  ESP_LOGI(TAG, "File written");
  // TODO: fflush()/fsync() periodically -- decide the tradeoff between
  //   write durability (flush every sample) and flash/SD wear +
  //   power draw (flush every N samples) -- worth a line in your
  //   README about why you picked what you picked.
  return 0;
}

void logger_read() {
  // Open renamed file for reading
  ESP_LOGI(TAG, "Reading file");
  f = fopen("/sdcard/foo.txt", "r");
  if (f == NULL) {
    ESP_LOGE(TAG, "Failed to open file for reading");
    return;
  }
  char line[64];
  fgets(line, sizeof(line), f);
  fclose(f);
  // strip newline
  char *pos = strchr(line, '\n');
  if (pos) {
    *pos = '\0';
  }
  ESP_LOGI(TAG, "Read from file: '%s'", line);

  return 0;
}

void close_logger() {
  // All done, unmount partition and disable SDMMC or SPI peripheral
  esp_vfs_fat_sdmmc_unmount();
  ESP_LOGI(TAG, "Card unmounted");
}

#endif // MOCK_HARDWARE