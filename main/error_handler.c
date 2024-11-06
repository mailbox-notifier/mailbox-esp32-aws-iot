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

static const char *TAG = "ERROR_HANDLER";

void error_stop_mqtt(esp_mqtt_client_handle_t mqtt_client)
{
    esp_err_t ret;

    ret = esp_mqtt_client_stop(mqtt_client);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "MQTT client stopped successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to stop MQTT client: %s", esp_err_to_name(ret));
    }

    // Optionally destroy the MQTT client to free up resources
    ret = esp_mqtt_client_destroy(mqtt_client);
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "MQTT client destroyed successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to destroy MQTT client: %s", esp_err_to_name(ret));
    }
}

void error_stop_wifi()
{
    esp_err_t ret;

    // Stop Wi-Fi
    ret = esp_wifi_stop();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Wi-Fi stopped successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to stop Wi-Fi: %s", esp_err_to_name(ret));
    }

    // Optionally deinitialize Wi-Fi
    ret = esp_wifi_deinit();
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Wi-Fi deinitialized successfully.");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to deinitialize Wi-Fi: %s", esp_err_to_name(ret));
    }
}

void error_reload(esp_mqtt_client_handle_t mqtt_client_handle)
{
    // Stop the MQTT client
    if (mqtt_client_handle != NULL)
    {
        error_stop_mqtt(mqtt_client_handle);
    }

    // Stop the Wi-Fi
    error_stop_wifi();

    // Restart the ESP32
    esp_restart();
}
