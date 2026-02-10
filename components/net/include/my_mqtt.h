// my_mqtt.h
#ifndef __MY_MQTT_H__
#define __MY_MQTT_H__

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化并连接 MQTT 客户端
 *
 * @param uri        MQTT 服务器 URI，例如:
 *                   - "mqtt://192.168.1.100:1883"
 *                   - "mqtts://your.domain.com:8883"
 * @param client_id  客户端 ID（建议唯一）
 * @param username   用户名（可为 NULL）
 * @param password   密码（可为 NULL）
 * @param ca_cert    服务器 CA 证书（PEM 字符串，仅用于 mqtts；若为 NULL 则跳过验证）
 * @return esp_err_t ESP_OK 表示成功启动连接（异步）
 */
esp_err_t mqtt_app_init(
    const char* uri,
    const char* client_id,
    const char* username,
    const char* password,
    const char* ca_cert
);

/**
 * @brief 发布消息到指定主题
 *
 * @param topic     主题名（如 "/sensor/mpu6050"）
 * @param payload   消息内容
 * @param len       消息长度
 * @param qos       QoS 等级：0, 1, 2
 * @param retain    是否保留消息
 * @return esp_err_t
 */
esp_err_t mqtt_app_publish(const char* topic, const char* payload, size_t len, int qos, bool retain);

/**
 * @brief 订阅主题
 *
 * @param topic     主题名
 * @param qos       QoS 等级
 * @return esp_err_t
 */
esp_err_t mqtt_app_subscribe(const char* topic, int qos);

/**
 * @brief 获取当前连接状态
 *
 * @return true 已连接，false 未连接
 */
bool mqtt_is_connected(void);

typedef void (*mqtt_data_cb_t)(const char *topic, size_t topic_len, const char *data, size_t data_len);

/**
 * @brief Register a callback for incoming MQTT data.
 *
 * Note: topic and data buffers are not null-terminated and are only valid
 * during the callback. Copy if you need them later.
 */
void mqtt_register_data_cb(mqtt_data_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif // __MY_MQTT_H__