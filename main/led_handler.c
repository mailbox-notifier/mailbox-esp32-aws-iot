#include "led_handler.h"

#include <inttypes.h>
#include <stdlib.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "LED_HANDLER";

led_strip_handle_t led_strip;

/* LED strip initialization with the GPIO and pixels number */
led_strip_config_t strip_config = {
    .strip_gpio_num = CONFIG_LED_STRIP_GPIO_PIN, // The GPIO connected to the LED strip's data line
    .max_leds = CONFIG_MAX_LED_COUNT,            // Set in led_handler_init()
    .led_pixel_format = LED_PIXEL_FORMAT_GRB,    // Pixel format of your LED strip
    .led_model = LED_MODEL_WS2812,               // LED strip model
    .flags.invert_out = false,                   // Invert output signal if needed
};

led_strip_rmt_config_t led_strip_rmt_config = {
    .clk_src = RMT_CLK_SRC_DEFAULT,    // Clock source
    .resolution_hz = 10 * 1000 * 1000, // 10MHz
    .flags.with_dma = false,           // DMA feature
};

QueueHandle_t led_state_queue;

QueueHandle_t get_led_state_queue(void) { return led_state_queue; }

void init_led_handler()
{
    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &led_strip_rmt_config, &led_strip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error initializing LED strip: %s", esp_err_to_name(ret));
        return;
    }

    // Create queue for LED handling events
    led_state_queue = xQueueCreate(10, sizeof(uint32_t));

    ESP_LOGI(TAG, "Initializing LED handler with %" PRIu32 " LEDs", strip_config.max_leds);
    // Turn off the LED strip initially
    ret = led_strip_clear(led_strip);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Error clearing LED strip: %s", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "LED strip cleared at initialization.");
    }
    xTaskCreate(&led_handling_task, "led_handling_task", 8192, NULL, 5, NULL);
}

void turn_all_leds_off()
{
    for (int i = 0; i < strip_config.max_leds; i++)
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, 0, 0, 0));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void turn_all_leds_on(uint8_t brightness_r, uint8_t brightness_g, uint8_t brightness_b)
{
    for (int i = 0; i < strip_config.max_leds; i++)
    {
        ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, i, brightness_r, brightness_g, brightness_b));
    }
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_handling_task(void *pvParameter)
{
    uint32_t led_event;
    bool led_on = false;

    assert(led_state_queue != NULL);
    led_strip_clear(led_strip);

    while (1)
    {
        // Wait indefinitely for an event
        if (xQueueReceive(led_state_queue, &led_event, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "Received event: %" PRIu32, led_event);
            if (led_event == 1 && !led_on)
            {
                led_on = true;
                ESP_LOGI(TAG, "Turning on the LED strip.");
                // Light the LED strip
                turn_all_leds_on(
                    CONFIG_LED_BRIGHTNESS_RED,
                    CONFIG_LED_BRIGHTNESS_GREEN,
                    CONFIG_LED_BRIGHTNESS_BLUE);
            }
            else if (led_event == 0 && led_on)
            {
                led_on = false;
                ESP_LOGI(TAG, "Turning off the LED strip.");
                // Turn off the LED strip
                turn_all_leds_off();
            }
        }
    }
}
