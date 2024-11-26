#include "unity.h"
#include "gecl-mqtt-manager.h"
#include "door_handler.h"
#include "door_config.h"
#include "mqtt_custom_handler.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "gecl-rgb-led-manager.h"

static const char *TAG = "DOOR_HANDLER";

static QueueHandle_t gpio_evt_queue = NULL;
static door_state_t current_door_state = DOOR_STATE_CLOSED;
static TimerHandle_t door_open_timer = NULL;

// Forward declarations
static void door_open_timer_callback(TimerHandle_t xTimer);
static void IRAM_ATTR gpio_isr_handler(void *arg);
static void door_task(void *arg);

static bool publish_door_state_with_retry(const char *state, int max_retries)
{
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"door\": \"%s\"}", state);

    for (int i = 0; i < max_retries; i++)
    {
        if (mqtt_client_handle == NULL)
        {
            ESP_LOGE(TAG, "MQTT client handle is NULL");
            return false;
        }

        int msg_id = esp_mqtt_client_publish(mqtt_client_handle,
                                             CONFIG_MQTT_PUBLISH_DOOR_STATE_TOPIC,
                                             payload, 0, 1, 0);
        if (msg_id != -1)
        {
            ESP_LOGI(TAG, "Published message: %s", payload);
            return true;
        }

        ESP_LOGW(TAG, "Publish attempt %d failed, retrying...", i + 1);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(TAG, "Failed to publish message after %d attempts", max_retries);
    return false;
}

static void handle_door_state_change(door_state_t new_state)
{
    const char *state_str = (new_state == DOOR_STATE_OPEN) ? "open" : "closed";
    const char *led_state = (new_state == DOOR_STATE_OPEN) ? "LED_SOLID_WHITE" : "LED_OFF";

    current_door_state = new_state;
    publish_door_state_with_retry(state_str, MQTT_PUBLISH_RETRIES);
    set_rgb_led_named_color(led_state);

    if (new_state == DOOR_STATE_OPEN)
    {
        if (xTimerStart(door_open_timer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to start door open timer");
        }
    }
    else
    {
        if (xTimerStop(door_open_timer, 0) != pdPASS)
        {
            ESP_LOGE(TAG, "Failed to stop door open timer");
        }
    }
}

static void door_open_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Door STILL open.");
    publish_door_state_with_retry("open", MQTT_PUBLISH_RETRIES);
    set_rgb_led_named_color("LED_BLINK_RED");
}

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void process_door_state_change(int current_state)
{
    door_state_t new_state = current_state ? DOOR_STATE_OPEN : DOOR_STATE_CLOSED;
    ESP_LOGI(TAG, "Door %s.", current_state ? "opened" : "closed");
    handle_door_state_change(new_state);
}

static void door_task(void *arg)
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

            if (current_time - last_change_time > pdMS_TO_TICKS(DEBOUNCE_TIME_MS))
            {
                if (current_state != last_state)
                {
                    last_state = current_state;
                    last_change_time = current_time;
                    process_door_state_change(current_state);
                }
            }
        }
    }
}

static esp_err_t configure_gpio(void)
{
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE};

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "GPIO configuration failed");
        return ret;
    }

    return ESP_OK;
}

static esp_err_t create_door_timer(void)
{
    door_open_timer = xTimerCreate(
        "DoorOpenTimer",
        pdMS_TO_TICKS(DOOR_OPEN_TIMER_PERIOD_MS),
        pdTRUE,
        (void *)0,
        door_open_timer_callback);

    if (door_open_timer == NULL)
    {
        ESP_LOGE(TAG, "Failed to create door open timer");
        return ESP_FAIL;
    }

    return ESP_OK;
}

void init_door_handler(void)
{
    // Configure GPIO
    if (configure_gpio() != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to configure GPIO");
        return;
    }

    // Create GPIO event queue
    gpio_evt_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(uint32_t));
    if (gpio_evt_queue == NULL)
    {
        ESP_LOGE(TAG, "Failed to create GPIO event queue");
        return;
    }

    // Create door task
    BaseType_t task_created = xTaskCreate(
        door_task,
        "door_task",
        DOOR_TASK_STACK_SIZE,
        NULL,
        DOOR_TASK_PRIORITY,
        NULL);

    if (task_created != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create door task");
        return;
    }

    // Install ISR service and add handler
    esp_err_t isr_service = gpio_install_isr_service(0);
    if (isr_service != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to install ISR service");
        return;
    }

    esp_err_t isr_handler = gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void *)BUTTON_GPIO);
    if (isr_handler != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to add ISR handler");
        return;
    }

    // Create door timer
    if (create_door_timer() != ESP_OK)
    {
        return;
    }

    // Check initial door state
    int initial_state = gpio_get_level(BUTTON_GPIO);
    process_door_state_change(initial_state);
}
