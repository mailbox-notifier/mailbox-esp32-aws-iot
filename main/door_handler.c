#include "unity.h"
#include "gecl-mqtt-manager.h"
#include "door_handler.h"
#include "mqtt_custom_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h" // Added this header
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "gecl-rgb-led-manager.h"

#define BUTTON_GPIO GPIO_NUM_21
#define DEBOUNCE_TIME_MS 500

static const char *TAG = "BUTTON_HANDLER";

static QueueHandle_t gpio_evt_queue = NULL;
static bool door_open = false;
static TimerHandle_t door_open_timer = NULL;

void publish_door_state(char *state)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"door\": \"%s\" }", state);

    if (mqtt_client_handle == NULL)
    {
        ESP_LOGE(TAG, "MQTT client handle is NULL");
        return;
    }

    // Publish to MQTT topic
    int msg_id = esp_mqtt_client_publish(mqtt_client_handle, CONFIG_MQTT_PUBLISH_DOOR_STATE_TOPIC, payload, 0, 1, 0);
    if (msg_id == -1)
    {
        ESP_LOGE("MQTT", "Failed to publish message");
    }
    else
    {
        ESP_LOGI("MQTT", "Published message: %s", payload);
    }
}

void door_open_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Door STILL open.");
    // The door is still open, send the message
    publish_door_state("open");
    set_rgb_led_named_color("LED_BLINK_RED");
}

void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void door_task(void *arg)
{
    uint32_t io_num;
    int last_state = gpio_get_level(BUTTON_GPIO);
    TickType_t last_change_time = xTaskGetTickCount();

    while (1)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            int current_state = gpio_get_level(io_num);
            TickType_t current_time = xTaskGetTickCount();

            // Debounce logic
            if (current_time - last_change_time > pdMS_TO_TICKS(DEBOUNCE_TIME_MS))
            {
                if (current_state != last_state)
                {
                    last_state = current_state;
                    last_change_time = current_time;

                    if (current_state)
                    {
                        // Door is open
                        ESP_LOGI(TAG, "Door opened.");
                        door_open = true;
                        publish_door_state("open");
                        set_rgb_led_named_color("LED_SOLID_WHITE");

                        // Start the door open timer
                        if (xTimerStart(door_open_timer, 0) != pdPASS)
                        {
                            ESP_LOGE(TAG, "Failed to start door open timer");
                        }
                    }
                    else
                    {
                        // Door is closed
                        ESP_LOGI(TAG, "Door closed.");
                        door_open = false;
                        publish_door_state("closed");
                        set_rgb_led_named_color("LED_OFF");

                        // Stop the door open timer
                        if (xTimerStop(door_open_timer, 0) != pdPASS)
                        {
                            ESP_LOGE(TAG, "Failed to stop door open timer");
                        }
                    }
                }
            }
        }
    }
}

void init_door_handler(void)
{
    // Configure the GPIO pin
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << BUTTON_GPIO);
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE; // Enable internal pull-up
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create a queue to handle GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start the door task
    xTaskCreate(door_task, "door_task", 2048, NULL, 10, NULL);

    // Install ISR service and add handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    // Create the door open timer (period of 5 minutes)
    door_open_timer = xTimerCreate(
        "DoorOpenTimer",           // Timer name
        pdMS_TO_TICKS(300000),     // Period in ticks (5 minutes)
        pdTRUE,                    // Auto-reload (repeats)
        (void *)0,                 // Timer ID (optional)
        door_open_timer_callback); // Callback function

    if (door_open_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create door open timer");
    }

    // Initial door state check
    int initial_state = gpio_get_level(BUTTON_GPIO);
    if (initial_state)
    {
        ESP_LOGI(TAG, "Door is initially open.");
        door_open = true;
        publish_door_state("open");
        set_rgb_led_named_color("LED_SOLID_WHITE");

        // Start the timer since the door is open
        if (xTimerStart(door_open_timer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to start door open timer");
        }
    }
    else
    {
        ESP_LOGI(TAG, "Door is initially closed.");
        door_open = false;
        publish_door_state("closed");
        set_rgb_led_named_color("LED_OFF");

        // Ensure the timer is stopped
        if (xTimerStop(door_open_timer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to stop door open timer");
        }
    }
}
