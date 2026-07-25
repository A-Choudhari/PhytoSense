#pragma once
#include "sensors.h"

// Mounts the SD card over SPI and opens the log file for appending.
// Returns 0 on success.
int logger_init(void);

// Appends one reading as a CSV line. Returns 0 on success.
int logger_write(const sensor_reading_t *reading);
