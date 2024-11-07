#include "unity.h"
#include "gecl-mqtt-manager.h"
#include "door_handler.h"
#include "mqtt_custom_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#define BUTTON_GPIO GPIO_NUM_21
#define DEBOUNCE_TIME_MS 500

static const char *TAG = "BUTTON_HANDLER";

static QueueHandle_t gpio_evt_queue = NULL;
static esp_timer_handle_t door_timer;
static bool door_open = false;

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
                    }
                    else
                    {
                        // Door is closed
                        ESP_LOGI(TAG, "Door closed.");
                        door_open = false;
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
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    // Create a queue to handle GPIO events
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));

    // Start the door task
    xTaskCreate(door_task, "door_task", 2048, NULL, 10, NULL);

    // Install ISR service and add handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);

    // Initial door state check
    int initial_state = gpio_get_level(BUTTON_GPIO);
    if (initial_state)
    {
        ESP_LOGI(TAG, "Door is initially open.");
        door_open = true;
    }
    else
    {
        ESP_LOGI(TAG, "Door is initially closed.");
        door_open = false;
    }
}
