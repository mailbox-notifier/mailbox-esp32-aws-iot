#ifndef MQTT_CUSTOM_HANDLER_H
#define MQTT_CUSTOM_HANDLER_H

#include "mqtt_client.h"       // For esp_mqtt_event_handle_t
#include "freertos/FreeRTOS.h" // For FreeRTOS types
#include "freertos/task.h"     // For TaskHandle_t
#include "gecl-ota-manager.h"  // For ota_config_t
#include "gecl-wifi-manager.h" // For wifi_active()

extern esp_mqtt_client_handle_t mqtt_client_handle;

// Function prototypes
void custom_handle_mqtt_event_connected(esp_mqtt_event_handle_t event);
void custom_handle_mqtt_event_disconnected(esp_mqtt_event_handle_t event);
bool extract_ota_url_from_event(esp_mqtt_event_handle_t event, char *local_mac_address, char *ota_url);
void custom_handle_mqtt_event_ota(esp_mqtt_event_handle_t event, char *my_mac_address);
void custom_handle_mqtt_event_data(esp_mqtt_event_handle_t event);
void custom_handle_mqtt_event_error(esp_mqtt_event_handle_t event);
void init_custom_mqtt();

#endif // MQTT_CUSTOM_HANDLER_H