#include "water_sensor.h"

#include "driver/gpio.h"
#include "sdkconfig.h"

void water_sensor_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << CONFIG_WATER_SENSOR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);
}

bool water_sensor_is_wet(void)
{
    return gpio_get_level(CONFIG_WATER_SENSOR_GPIO) == 1;
}

int water_sensor_raw_gpio(void)
{
    return gpio_get_level(CONFIG_WATER_SENSOR_GPIO);
}
