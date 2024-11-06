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
#include "mbedtls/debug.h" // Add this to include mbedtls debug functions
#include "nvs_flash.h"
#include "mqtt_custom_handler.h"
#include "gecl-rgb-led-manager.h"
#include "unity.h"
#include "iot_button.h"
#include "sdkconfig.h"

static const char *TAG = "MAILBOX";

static void button_press_up_cb(void *arg, void *data)
{
    TEST_ASSERT_EQUAL_HEX(BUTTON_PRESS_UP, iot_button_get_event(arg));
    ESP_LOGI(TAG, "BUTTON_PRESS_UP[%" PRIu32 "]", iot_button_get_ticks_time((button_handle_t)arg));
}

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

    button_config_t cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = CONFIG_BUTTON_LONG_PRESS_TIME_MS,
        .short_press_time = CONFIG_BUTTON_SHORT_PRESS_TIME_MS,
        .gpio_button_config = {
            .gpio_num = 4,
            .active_level = 1,
        },
    };

    button_handle_t g_btn = iot_button_create(&cfg);

    iot_button_register_cb(g_btn, BUTTON_PRESS_UP, button_press_up_cb, NULL);

    ESP_LOGI(TAG, "Entering infinite loop");
    // Infinite loop to prevent exiting app_main
    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000)); // Delay to allow other tasks to run
    }
}
