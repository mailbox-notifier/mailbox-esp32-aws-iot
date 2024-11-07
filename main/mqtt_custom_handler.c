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
#include "gecl-misc-util-manager.h"
#include "mbedtls/debug.h" // Add this to include mbedtls debug functions
#include "nvs_flash.h"
#include "mqtt_custom_handler.h"

static const char *TAG = "MQTT_CUSTOM_HANDLER";

esp_mqtt_client_handle_t mqtt_client_handle = NULL;

TaskHandle_t ota_task_handle = NULL;

extern const uint8_t certificate[];
extern const uint8_t private_key[];
extern const uint8_t root_ca[];
const uint8_t *cert = certificate;
const uint8_t *key = private_key;
const uint8_t *ca = root_ca;

char mac_address[18];

void record_local_mac_address(char *mac_str)
{
    uint8_t mac[6];
    esp_err_t ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (ret == ESP_OK)
    {
        snprintf(mac_str, 18, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    else
    {
        snprintf(mac_str, 18, "ERROR");
    }
}

void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_CONNECTED");
    int msg_id;

    record_local_mac_address(mac_address);
    ESP_LOGW(TAG, "Burned-In MAC Address: %s", mac_address);

    msg_id = esp_mqtt_client_subscribe(client, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_MAILBOX_TOPIC, 0);
    ESP_LOGI(TAG, "Subscribed to topic %s, msg_id=%d", CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_MAILBOX_TOPIC, msg_id);
}

void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_DISCONNECTED");
    if (ota_task_handle != NULL)
    {
        vTaskDelete(ota_task_handle);
        ota_task_handle = NULL;
    }
    // Reconnect logic
    int retry_count = 0;
    const int max_retries = 5;
    const int retry_delay_ms = 5000;
    esp_err_t err;
    esp_mqtt_client_handle_t client = event->client;

    // Check if the network is connected before attempting reconnection
    if (wifi_active())
    {
        do
        {
            ESP_LOGI(TAG, "Attempting to reconnect, retry %d/%d", retry_count + 1, max_retries);
            err = esp_mqtt_client_reconnect(client);
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "Failed to reconnect MQTT client, retrying in %d seconds...", retry_delay_ms / 1000);
                vTaskDelay(pdMS_TO_TICKS(retry_delay_ms)); // Delay for 5 seconds
                retry_count++;
            }
        } while (err != ESP_OK && retry_count < max_retries);

        if (err != ESP_OK)
        {
            ESP_LOGE(TAG, "Failed to reconnect MQTT client after %d retries. Restarting", retry_count);
            error_reload(mqtt_client_handle);
        }
    }
    else
    {
        ESP_LOGE(TAG, "Network not connected, skipping MQTT reconnection");
        error_reload(mqtt_client_handle);
    }
}

bool extract_ota_url_from_event(esp_mqtt_event_handle_t event, char *local_mac_address, char *ota_url)
{
    bool success = false;
    cJSON *root = cJSON_Parse(event->data);
    cJSON *host_key = cJSON_GetObjectItem(root, local_mac_address);
    const char *host_key_value = cJSON_GetStringValue(host_key);

    if (!host_key || !host_key_value)
    {
        ESP_LOGW(TAG, "'%s' MAC address key not found in JSON", local_mac_address);
    }
    else
    {
        size_t url_len = strlen(host_key_value);
        strncpy(ota_url, host_key_value, url_len);
        ota_url[url_len] = '\0'; // Manually set the null terminator
        success = true;
    }

    cJSON_Delete(root); // Free JSON object
    return success;
}

void custom_handle_mqtt_event_ota(esp_mqtt_event_handle_t event, char *my_mac_address)
{
    if (ota_task_handle != NULL)
    {
        eTaskState task_state = eTaskGetState(ota_task_handle);
        if (task_state != eDeleted)
        {
            char log_message[256]; // Adjust the size according to your needs
            snprintf(log_message, sizeof(log_message),
                     "OTA task is already running or not yet cleaned up, skipping OTA update. task_state=%d",
                     task_state);

            ESP_LOGW(TAG, "%s", log_message);
            return;
        }
        else
        {
            // Clean up task handle if it has been deleted
            ota_task_handle = NULL;
        }
    }
    else
    {
        ESP_LOGI(TAG, "OTA task handle is NULL");
    }

    // Parse the message and get any URL associated with our MAC address
    assert(event->data != NULL);
    assert(event->data_len > 0);

    ota_config_t ota_config;
    ota_config.mqtt_client = event->client;

    if (!extract_ota_url_from_event(event, my_mac_address, ota_config.url))
    {
        ESP_LOGW(TAG, "OTA URL not found in event data");
        return;
    }

    // Pass the allocated URL string to the OTA task
    if (xTaskCreate(&ota_task, "ota_task", 8192, (void *)&ota_config, 5, &ota_task_handle) != pdPASS)
    {
        error_reload(mqtt_client_handle);
    }
}

void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event)
{

    ESP_LOGW(TAG, "Received topic %.*s", event->topic_len, event->topic);

    if (strncmp(event->topic, CONFIG_MQTT_SUBSCRIBE_OTA_UPDATE_MAILBOX_TOPIC, event->topic_len) == 0)
    {
        // Use the global mac_address variable to pass the MAC address to the OTA function
        custom_handle_mqtt_event_ota(event, mac_address);
    }
    else
    {
        ESP_LOGE(TAG, "Un-Handled topic %.*s", event->topic_len, event->topic);
    }
}

void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event)
{
    ESP_LOGI(TAG, "Custom handler: MQTT_EVENT_ERROR");
    if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS)
    {
        ESP_LOGI(TAG, "Last ESP error code: 0x%x", event->error_handle->esp_tls_last_esp_err);
        ESP_LOGI(TAG, "Last TLS stack error code: 0x%x", event->error_handle->esp_tls_stack_err);
        ESP_LOGI(TAG, "Last TLS library error code: 0x%x", event->error_handle->esp_tls_cert_verify_flags);
    }
    else if (event->error_handle->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED)
    {
        ESP_LOGI(TAG, "Connection refused error: 0x%x", event->error_handle->connect_return_code);
    }
    else
    {
        ESP_LOGI(TAG, "Unknown error type: 0x%x", event->error_handle->error_type);
    }
    error_reload(mqtt_client_handle);
}

void init_custom_mqtt()
{
    mqtt_set_event_connected_handler(custom_handle_mqtt_event_connected);
    mqtt_set_event_disconnected_handler(custom_handle_mqtt_event_disconnected);
    mqtt_set_event_data_handler(custom_handle_mqtt_event_data);
    mqtt_set_event_error_handler(custom_handle_mqtt_event_error);

    mqtt_config_t config = {.certificate = cert,
                            .private_key = key,
                            .root_ca = ca,
                            .broker_uri = CONFIG_AWS_IOT_ENDPOINT};

    mqtt_client_handle = init_mqtt(&config);

    if (mqtt_client_handle == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        error_reload(mqtt_client_handle);
    }
}