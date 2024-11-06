#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include "esp_system.h"  // For esp_restart()
#include "mqtt_client.h" // For esp_mqtt_client_handle_t
#include "esp_err.h"     // For esp_err_t
#include "esp_wifi.h"    // For Wi-Fi functions

void error_stop_mqtt(esp_mqtt_client_handle_t mqtt_client);
void error_stop_wifi();
void error_reload();

#endif // ERROR_HANDLER_H
