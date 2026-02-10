#include "my_mqtt.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <inttypes.h>
#include "OLED.h"

static const char *TAG = "MY_MQTT";

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_is_connected = false;
static mqtt_data_cb_t s_data_cb = NULL;

// MQTT 事件处理函数
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id)
    {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT connected");
        OLED_ClearArea(0, 20, 128, 10);
        OLED_Printf(0, 20, OLED_6X8, "MQTT Connected");
        OLED_Update();
        s_is_connected = true;
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT disconnected");
        OLED_ClearArea(0, 20, 128, 10);
        OLED_Printf(0, 20, OLED_6X8, "MQTT Disconnected");
        OLED_Update();
        s_is_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "Subscribed to topic: %s", event->topic);
        break;

    case MQTT_EVENT_DATA: // 这个就是当前的设备就是订阅了某个主题 然后服务器发过来了数据
        ESP_LOGI(TAG, "Received data on topic: %.*s", event->topic_len, event->topic);
        ESP_LOG_BUFFER_HEXDUMP(TAG, event->data, event->data_len, ESP_LOG_INFO);
        if (s_data_cb)
        {
            s_data_cb(event->topic, (size_t)event->topic_len, event->data, (size_t)event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error");

        s_is_connected = false;
        break;

    default:
        ESP_LOGD(TAG, "Other event id:%" PRId32, event_id);
        break;
    }
}

esp_err_t mqtt_app_init(
    const char *uri,
    const char *client_id,
    const char *username,
    const char *password,
    const char *ca_cert)
{
    if (!uri)
    {
        ESP_LOGE(TAG, "URI is required");
        return ESP_ERR_INVALID_ARG;
    }

    esp_mqtt_client_config_t config = {
        .broker.address.uri = uri,
        .credentials.client_id = client_id,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .session.keepalive = 30,
        .network.reconnect_timeout_ms = 5000,
        .network.disable_auto_reconnect = false,
        .buffer.size = 2048,
    };

    // 启用 TLS 验证（如果提供了 CA 证书且是 mqtts）
    if (ca_cert && strncmp(uri, "mqtts://", 8) == 0)
    {
        config.broker.verification.certificate = ca_cert;
        config.broker.verification.skip_cert_common_name_check = false;
    }

    s_mqtt_client = esp_mqtt_client_init(&config);
    if (!s_mqtt_client)
    {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start MQTT client: %d", err);
        esp_mqtt_client_destroy(s_mqtt_client);
        s_mqtt_client = NULL;
        return err;
    }

    return ESP_OK;
}

esp_err_t mqtt_app_publish(const char *topic, const char *payload, size_t len, int qos, bool retain)
{
    if (!s_mqtt_client || !topic || !payload)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_is_connected)
    {
        ESP_LOGW(TAG, "MQTT not connected, dropping publish");
        return ESP_ERR_INVALID_STATE;
    }

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, payload, len, qos, retain);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to publish message");
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Published message ID: %d", msg_id);
    return ESP_OK;
}

esp_err_t mqtt_app_subscribe(const char *topic, int qos)
{
    if (!s_mqtt_client || !topic)
    {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_is_connected)
    {
        ESP_LOGW(TAG, "MQTT not connected, cannot subscribe");
        return ESP_ERR_INVALID_STATE;
    }

    // 直接调用底层函数，避免 _Generic 宏问题
    int msg_id = esp_mqtt_client_subscribe_single(s_mqtt_client, topic, qos);
    if (msg_id < 0)
    {
        ESP_LOGE(TAG, "Failed to subscribe to topic: %s", topic);
        return ESP_FAIL;
    }
    ESP_LOGD(TAG, "Subscribed to topic: %s, msg_id=%d", topic, msg_id);
    return ESP_OK;
}

bool mqtt_is_connected(void)
{
    return s_is_connected;
}

void mqtt_register_data_cb(mqtt_data_cb_t cb)
{
    s_data_cb = cb;
}