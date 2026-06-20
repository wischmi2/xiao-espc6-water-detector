#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include "sample_credentials.h"
#include "shell.h"
#include "water_sensor.h"
#include "wifi.h"

#include <golioth/client.h>
#include <golioth/stream.h>

#define TAG "water_detector"
#define STREAM_PATH "sensor/water"

static SemaphoreHandle_t s_connected_sem;

static void on_client_event(struct golioth_client *client,
                            enum golioth_client_event event,
                            void *arg)
{
    bool is_connected = (event == GOLIOTH_CLIENT_EVENT_CONNECTED);
    if (is_connected)
    {
        xSemaphoreGive(s_connected_sem);
    }
    GLTH_LOGI(TAG, "Golioth client %s", is_connected ? "connected" : "disconnected");
}

static void on_stream_ack(struct golioth_client *client,
                          enum golioth_status status,
                          const struct golioth_coap_rsp_code *coap_rsp_code,
                          const char *path,
                          void *arg)
{
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Stream push to %s failed: %s", path, golioth_status_to_str(status));
        return;
    }

    GLTH_LOGI(TAG, "Stream push to %s acknowledged", path);
}

static void stream_water_state(struct golioth_client *client, bool is_wet, bool heartbeat)
{
    char json[64];
    int raw = water_sensor_raw_gpio();
    int len = snprintf(json,
                       sizeof(json),
                       "{\"water_detected\":%d,\"raw_gpio\":%d,\"heartbeat\":%d}",
                       is_wet ? 1 : 0,
                       raw,
                       heartbeat ? 1 : 0);
    if (len <= 0 || len >= (int) sizeof(json))
    {
        GLTH_LOGE(TAG, "Failed to format stream JSON");
        return;
    }

    enum golioth_status status = golioth_stream_set_async(client,
                                                          STREAM_PATH,
                                                          GOLIOTH_CONTENT_TYPE_JSON,
                                                          (const uint8_t *) json,
                                                          (size_t) len,
                                                          on_stream_ack,
                                                          NULL);
    if (status != GOLIOTH_OK)
    {
        GLTH_LOGW(TAG, "Failed to enqueue stream update: %s", golioth_status_to_str(status));
    }
}

void app_main(void)
{
    GLTH_LOGI(TAG, "XIAO ESP-C6 Water Detector starting");

    nvs_init();
    shell_start();

    if (!nvs_credentials_are_set())
    {
        GLTH_LOGW(TAG,
                 "WiFi and Golioth credentials are not set. "
                 "Use the shell settings commands to set them.");

        while (!nvs_credentials_are_set())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    wifi_init(nvs_read_wifi_ssid(), nvs_read_wifi_password());
    wifi_wait_for_connected();

    water_sensor_init();

    const struct golioth_client_config *config = golioth_sample_credentials_get();
    struct golioth_client *client = golioth_client_create(config);
    assert(client);

    s_connected_sem = xSemaphoreCreateBinary();
    golioth_client_register_event_callback(client, on_client_event, NULL);

    GLTH_LOGI(TAG, "Waiting for connection to Golioth...");
    xSemaphoreTake(s_connected_sem, portMAX_DELAY);

    bool last_wet = water_sensor_is_wet();
    GLTH_LOGI(TAG, "Initial water state: %s", last_wet ? "wet" : "dry");
    stream_water_state(client, last_wet, true);

    int64_t last_heartbeat_ms = esp_timer_get_time() / 1000;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(CONFIG_WATER_SENSOR_POLL_MS));

        bool is_wet = water_sensor_is_wet();
        int64_t now_ms = esp_timer_get_time() / 1000;
        bool heartbeat_due =
            (now_ms - last_heartbeat_ms) >= ((int64_t) CONFIG_WATER_SENSOR_HEARTBEAT_S * 1000);

        if (is_wet != last_wet)
        {
            GLTH_LOGI(TAG, "Water state changed: %s -> %s",
                      last_wet ? "wet" : "dry",
                      is_wet ? "wet" : "dry");
            stream_water_state(client, is_wet, false);
            last_wet = is_wet;
            last_heartbeat_ms = now_ms;
        }
        else if (heartbeat_due)
        {
            GLTH_LOGI(TAG, "Heartbeat: water state is %s", is_wet ? "wet" : "dry");
            stream_water_state(client, is_wet, true);
            last_heartbeat_ms = now_ms;
        }
    }
}
