#include "wet_indicator.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define LEDC_DUTY_RESOLUTION LEDC_TIMER_10_BIT
#define LEDC_MAX_DUTY        ((1U << 10) - 1U)
#define LEDC_HALF_DUTY       (LEDC_MAX_DUTY / 2U)

static bool s_buzzer_ready;
static int64_t s_last_repeat_beep_ms;

static void led_set(bool is_wet)
{
#if CONFIG_WATER_INDICATOR_LED_ENABLE
    gpio_set_level(CONFIG_WATER_INDICATOR_LED_GPIO, is_wet ? 0 : 1);
#else
    (void) is_wet;
#endif
}

#if CONFIG_WATER_INDICATOR_BUZZER_GPIO >= 0
static void buzzer_tone(bool on)
{
    if (!s_buzzer_ready)
    {
        return;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, on ? LEDC_HALF_DUTY : 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

static void buzzer_beep(void)
{
    if (!s_buzzer_ready)
    {
        return;
    }

    buzzer_tone(true);
    vTaskDelay(pdMS_TO_TICKS(CONFIG_WATER_INDICATOR_BUZZER_BEEP_MS));
    buzzer_tone(false);
}
#else
static void buzzer_beep(void) {}
#endif

void wet_indicator_init(void)
{
    s_last_repeat_beep_ms = 0;
    s_buzzer_ready = false;

#if CONFIG_WATER_INDICATOR_LED_ENABLE
    gpio_config_t led_cfg = {
        .pin_bit_mask = 1ULL << CONFIG_WATER_INDICATOR_LED_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_cfg);
    gpio_set_level(CONFIG_WATER_INDICATOR_LED_GPIO, 1);
#endif

#if CONFIG_WATER_INDICATOR_BUZZER_GPIO >= 0
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_DUTY_RESOLUTION,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = CONFIG_WATER_INDICATOR_BUZZER_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .gpio_num = CONFIG_WATER_INDICATOR_BUZZER_GPIO,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
    s_buzzer_ready = true;
#endif
}

void wet_indicator_update(bool is_wet, bool state_changed)
{
    led_set(is_wet);

    if (state_changed && is_wet)
    {
        buzzer_beep();
        s_last_repeat_beep_ms = esp_timer_get_time() / 1000;
        return;
    }

#if CONFIG_WATER_INDICATOR_BUZZER_GPIO >= 0 && CONFIG_WATER_INDICATOR_BUZZER_REPEAT_S > 0
    if (is_wet && s_buzzer_ready)
    {
        int64_t now_ms = esp_timer_get_time() / 1000;
        int64_t repeat_ms = (int64_t) CONFIG_WATER_INDICATOR_BUZZER_REPEAT_S * 1000;

        if ((now_ms - s_last_repeat_beep_ms) >= repeat_ms)
        {
            buzzer_beep();
            s_last_repeat_beep_ms = now_ms;
        }
    }
#endif
}
