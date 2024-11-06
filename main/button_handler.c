#include "unity.h"
#include "iot_button.h"
#include "esp_log.h"

static const char *TAG = "BUTTON_HANDLER";

static void button_press_down_cb(void *arg, void *data)
{
    ESP_LOGI(TAG, "CLOSED");
}

static void button_press_up_cb(void *arg, void *data)
{
    // ESP_LOGI(TAG, "BUTTON_PRESS_UP[%" PRIu32 "]", iot_button_get_ticks_time((button_handle_t)arg));
    ESP_LOGI(TAG, "OPEN");
}

void init_button(void)
{
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

    iot_button_register_cb(g_btn, BUTTON_PRESS_DOWN, button_press_down_cb, NULL);

    iot_button_register_cb(g_btn, BUTTON_PRESS_UP, button_press_up_cb, NULL);
}
