#ifndef LED_HANDLER_H
#define LED_HANDLER_H

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

extern QueueHandle_t led_event_queue;

void init_led_handler();
void led_handling_task(void *pvParameter);
QueueHandle_t get_led_state_queue(void);

#define LED_ON 1

#endif  // LED_HANDLER_H
