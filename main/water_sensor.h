#pragma once

#include <stdbool.h>

void water_sensor_init(void);
bool water_sensor_is_wet(void);
int water_sensor_raw_gpio(void);
