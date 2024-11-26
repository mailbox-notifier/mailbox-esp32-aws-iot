#ifndef DOOR_HANDLER_H
#define DOOR_HANDLER_H

#include "esp_err.h"

typedef enum
{
    DOOR_STATE_CLOSED,
    DOOR_STATE_OPEN
} door_state_t;

void init_door_handler(void);

#endif // DOOR_HANDLER_H
