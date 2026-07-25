#include "logger.h"
#include "esp_log.h"

// Same mock/real split as sensors.c -- see that file for rationale.
#define MOCK_HARDWARE 1

static const char *TAG = "logger";

#if MOCK_HARDWARE

int logger_init(void)
{
    ESP_LOGI(TAG, "logger_init: MOCK_HARDWARE mode, writing to console instead of SD");
    return 0;
}

int logger_write(const sensor_reading_t *r)
{
    // In real mode this becomes an fprintf() to the mounted SD file.
    // Keeping the exact same CSV format now means Week 2's SD swap is
    // a one-function change, not a rewrite.
    ESP_LOGI(TAG, "%lld,%.2f,%.2f,%.2f,%.2f",
             r->timestamp_us, r->soil_moisture_pct, r->temperature_c,
             r->humidity_pct, r->pressure_hpa);
    return 0;
}

#else // real hardware

// TODO(Week 2): replace with real driver includes:
// #include "driver/sdspi_host.h"
// #include "esp_vfs_fat.h"
// #include "sdmmc_cmd.h"

static FILE *log_file = NULL;

int logger_init(void)
{
    // TODO: configure SPI bus for the SD card module (MOSI/MISO/CLK/CS
    //   pins), mount FAT filesystem via esp_vfs_fat_sdspi_mount(), then
    //   fopen("/sdcard/log.csv", "a") into log_file.
    //
    // TODO: if this is a fresh card / empty file, write a CSV header
    //   line first: "timestamp_us,soil_pct,temp_c,humidity_pct,pressure_hpa"
    ESP_LOGW(TAG, "logger_init: real hardware path not yet implemented");
    return -1;
}

int logger_write(const sensor_reading_t *r)
{
    if (log_file == NULL) {
        return -1;
    }
    // TODO: fprintf(log_file, "%lld,%.2f,%.2f,%.2f,%.2f\n", ...);
    // TODO: fflush()/fsync() periodically -- decide the tradeoff between
    //   write durability (flush every sample) and flash/SD wear +
    //   power draw (flush every N samples) -- worth a line in your
    //   README about why you picked what you picked.
    return 0;
}

#endif // MOCK_HARDWARE
