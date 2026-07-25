#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

#include "sensors.h"
#include "logger.h"

static const char *TAG = "main";

// Holds a handful of readings in flight between the sampling task and
// the logging task, so a slow SD write never blocks/delays the next
// sample. This queue is the actual "RTOS" part of the project worth
// talking about in an interview -- not just "I used FreeRTOS tasks."
static QueueHandle_t reading_queue;

#define QUEUE_LENGTH        8
#define SAMPLE_PERIOD_MS    (30 * 1000)   // 30s for now; will move to
                                           // minutes once deep sleep is
                                           // wired in (Week 2).

// Producer: reads all sensors on a fixed period, pushes onto the queue.
static void sampling_task(void *arg)
{
    sensors_init();

    sensor_reading_t reading;
    for (;;) {
        if (sensors_read(&reading) == 0) {
            if (xQueueSend(reading_queue, &reading, pdMS_TO_TICKS(1000)) != pdTRUE) {
                // Queue was full -- logging task is falling behind.
                // Worth watching for in Week 2 once real SD write
                // latency is in the picture.
                ESP_LOGW(TAG, "reading_queue full, dropping sample");
            }
        } else {
            ESP_LOGW(TAG, "sensor read failed");
        }
        vTaskDelay(pdMS_TO_TICKS(SAMPLE_PERIOD_MS));
    }
}

// Consumer: blocks on the queue, writes each reading as it arrives.
static void logging_task(void *arg)
{
    logger_init();

    sensor_reading_t reading;
    for (;;) {
        if (xQueueReceive(reading_queue, &reading, portMAX_DELAY) == pdTRUE) {
            logger_write(&reading);
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "plant logger starting");

    reading_queue = xQueueCreate(QUEUE_LENGTH, sizeof(sensor_reading_t));
    if (reading_queue == NULL) {
        ESP_LOGE(TAG, "failed to create reading_queue");
        return;
    }

    // Sampling task gets a slightly higher priority than logging --
    // we never want a slow SD write to delay taking the next reading.
    xTaskCreate(sampling_task, "sampling_task", 4096, NULL, 6, NULL);
    xTaskCreate(logging_task,  "logging_task",  4096, NULL, 5, NULL);
}
