#pragma once

#include "context.h"

typedef struct MousePosition {
        float mousex;
        float mousey;
} MousePosition_t;

MousePosition_t get_mouse_position(context_t *context);
