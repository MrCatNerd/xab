#pragma once

// don't ask why include is after enum lol
#include "context.h"

enum WM_CLASS identify_wm(context_t *context);
void setup_background(context_t *context);
void update_background(context_t *context);
