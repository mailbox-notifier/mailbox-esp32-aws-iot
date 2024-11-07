#include "cJSON.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h" // Include FreeRTOS timers header
#include "gecl-mqtt-manager.h"
#include "gecl-nvs-manager.h"
#include "gecl-ota-manager.h"
#include "gecl-time-sync-manager.h"
#include "gecl-wifi-manager.h"
#include "gecl-rgb-led-manager.h"
#include "mbedtls/debug.h" // Add this to include mbedtls debug functions
#include "nvs_flash.h"
#include "mqtt_custom_handler.h"
#include "unity.h"
#include "sdkconfig.h"
#include "door_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "MAILBOX";

void app_main(void)
{
    ESP_LOGI(TAG, "Init NVS");
    init_nvs();

    ESP_LOGI(TAG, "Init WiFi");
    init_wifi();

    ESP_LOGI(TAG, "Init Time Sync");
    init_time_sync();

    ESP_LOGI(TAG, "Init MQTT");
    init_custom_mqtt();

    ESP_LOGI(TAG, "Init RGB LED");
    init_rgb_led();

    ESP_LOGI(TAG, "Init door handler");
    init_door_handler();

    ESP_LOGI(TAG, "Entering infinite loop");
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow other tasks to run
    }
}
