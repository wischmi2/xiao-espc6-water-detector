#pragma once

#include <stdbool.h>

void wet_indicator_init(void);
void wet_indicator_update(bool is_wet, bool state_changed);
