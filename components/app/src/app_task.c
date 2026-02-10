#include "app_task.h"
#include "platform.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "my_mqtt.h"
#include <string.h>
#include <stdio.h>
#include "cJSON.h"

// ========================
// 任务配置参数
// ========================

// 数据采集任务：优先级较低（2），栈深度 4KB
#define TASK_DATA_PRIORITY 2
#define TASK_DATA_STACK_DEPTH 4096

// 下行消息转发任务：优先级较高（3），栈深度 4KB
#define TASK_DOWNLINK_PRIORITY 3
#define TASK_DOWNLINK_STACK_DEPTH 8192

// 消息处理任务（上传/下行）：优先级 3，栈深度 4KB
#define TASK_PROCESS_PRIORITY 3
#define TASK_PROCESS_STACK_DEPTH 8192

// 应用层消息队列长度（用于上传和下行消息的统一处理）
#define APP_MSG_QUEUE_LEN 8

// 下行原始消息队列长度（MQTT 收到的消息先入此队列）
#define APP_INBOUND_QUEUE_LEN 8

// JSON 消息最大长度（上传和下行共用上限）
#define APP_MSG_MAX_JSON 512

// 下行原始消息缓冲区最大长度
#define APP_INBOUND_MAX_JSON 512

// 传感器数据上传间隔（毫秒）
#define APP_UPLOAD_INTERVAL_MS 10000

// 日志标签
static const char *TAG = "APP_TASK";
static const char *MQTT_TOPIC_UP = "test/topic"; // 上行主题（可根据实际情况修改）

// ========================
// 消息类型定义
// ========================

typedef enum
{
    APP_MSG_UPLOAD = 0,   // 上报消息：由本地传感器数据构建的 JSON
    APP_MSG_DOWNLINK = 1, // 下行消息：从 MQTT 服务器接收到的原始 JSON
} app_msg_type_t;

// 应用层统一消息结构体（用于任务间通信）
typedef struct
{
    app_msg_type_t type;            // 消息类型
    size_t len;                     // payload 实际长度
    char payload[APP_MSG_MAX_JSON]; // 消息内容（JSON 字符串）
} app_msg_t;

// 下行原始消息结构体（MQTT 回调中使用，避免大结构体拷贝）
typedef struct
{
    size_t len;                         // payload 长度
    char payload[APP_INBOUND_MAX_JSON]; // 原始 JSON 字符串（来自 MQTT）
} app_inbound_msg_t;

// ========================
// 全局队列句柄（任务间通信）
// ========================

// 主消息队列：供 app_task_process 从中读取上传或下行消息进行处理
static QueueHandle_t s_app_msg_queue = NULL;

// 下行消息队列：MQTT 回调将消息放入此队列，由 app_task_downlink 转发至主队列
static QueueHandle_t s_inbound_queue = NULL;

// ========================
// 任务函数声明
// ========================

static void app_task_get_data(void *pvParameters);                                                    // 采集传感器数据并构建上传 JSON
static void app_task_downlink(void *pvParameters);                                                    // 将下行原始消息转发到主消息队列
static void app_task_process(void *pvParameters);                                                     // 处理主队列中的消息（上传 or 下行）
static void app_mqtt_data_cb(const char *topic, size_t topic_len, const char *data, size_t data_len); // MQTT 数据回调
static size_t app_build_sensor_json(const SensorData *data, char *buf, size_t len);                   // 构建传感器 JSON
static esp_err_t app_cloud_send_json(const char *json, size_t len);                                   // 通过 MQTT 发送上行数据
static void app_handle_downlink_json(const char *json, size_t len);                                   // 处理下行指令（待实现）

// ========================
// 应用任务初始化函数
// ========================
esp_err_t app_task_init(void)
{
    // 创建主消息队列（用于上传和下行消息的统一调度）
    if (!s_app_msg_queue)
    {
        s_app_msg_queue = xQueueCreate(APP_MSG_QUEUE_LEN, sizeof(app_msg_t));
    }

    // 创建下行原始消息队列（MQTT 回调专用）
    if (!s_inbound_queue)
    {
        s_inbound_queue = xQueueCreate(APP_INBOUND_QUEUE_LEN, sizeof(app_inbound_msg_t));
    }

    // 检查队列是否创建成功
    if (!s_app_msg_queue || !s_inbound_queue)
    {
        ESP_LOGE(TAG, "Failed to create queues");
        return ESP_ERR_NO_MEM;
    }

    // 注册 MQTT 数据接收回调函数
    mqtt_register_data_cb(app_mqtt_data_cb);

    // 创建三个任务，并绑定到指定 CPU 核心（APP_CPU_NUM 定义在 platform.h 中）
    xTaskCreatePinnedToCore(app_task_get_data, "app_task_get_data", TASK_DATA_STACK_DEPTH, NULL, TASK_DATA_PRIORITY, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(app_task_downlink, "app_task_downlink", TASK_DOWNLINK_STACK_DEPTH, NULL, TASK_DOWNLINK_PRIORITY, NULL, APP_CPU_NUM);
    xTaskCreatePinnedToCore(app_task_process, "app_task_process", TASK_PROCESS_STACK_DEPTH, NULL, TASK_PROCESS_PRIORITY, NULL, APP_CPU_NUM);

    return ESP_OK;
}

// ========================
// 任务 1：采集传感器数据并构建上传消息
// ========================
static void app_task_get_data(void *pvParameters)
{
    (void)pvParameters; // 避免编译警告

    SensorData data = {0}; // 存储传感器原始数据
    app_msg_t msg = {0};   // 构建好的上传消息

    for (;;)
    {
        // 从硬件平台读取传感器数据
        if (platform_get_sensor_data(&data) == ESP_OK)
        {
            // 将传感器数据格式化为 JSON 字符串
            size_t len = app_build_sensor_json(&data, msg.payload, sizeof(msg.payload));
            if (len > 0)
            {
                // 设置消息类型为上传，并填充长度
                msg.type = APP_MSG_UPLOAD;
                msg.len = len;

                // 将消息发送到主消息队列，等待处理
                xQueueSend(s_app_msg_queue, &msg, portMAX_DELAY);
            }
            else
            {
                ESP_LOGW(TAG, "Sensor JSON buffer too small");
            }
        }
        else
        {
            ESP_LOGW(TAG, "Failed to read sensor data");
        }

        // 按固定间隔休眠（默认 1 秒）
        vTaskDelay(pdMS_TO_TICKS(APP_UPLOAD_INTERVAL_MS));
    }
}

// ========================
// 任务 2：转发下行消息到主队列
// ========================
static void app_task_downlink(void *pvParameters)
{
    (void)pvParameters;

    app_inbound_msg_t in_msg = {0}; // 从下行队列接收的原始消息
    app_msg_t out_msg = {0};        // 转换后的主队列消息

    for (;;)
    {
        // 从下行队列阻塞等待新消息
        if (xQueueReceive(s_inbound_queue, &in_msg, portMAX_DELAY) == pdTRUE)
        {
            // 转换为通用 app_msg_t 格式
            out_msg.type = APP_MSG_DOWNLINK;
            out_msg.len = in_msg.len;

            // 拷贝 JSON 内容（+1 确保字符串结尾有 '\0'）
            memcpy(out_msg.payload, in_msg.payload, in_msg.len + 1);

            // 发送到主消息队列，由 process 任务统一处理
            xQueueSend(s_app_msg_queue, &out_msg, portMAX_DELAY);
        }
    }
}

// ========================
// 任务 3：统一处理上传与下行消息
// ========================
static void app_task_process(void *pvParameters)
{
    (void)pvParameters;

    app_msg_t msg = {0}; // 从主队列接收的消息

    for (;;)
    {
        // 从主消息队列阻塞等待消息
        if (xQueueReceive(s_app_msg_queue, &msg, portMAX_DELAY) != pdTRUE)
        {
            continue; // 理论上不会发生，但防御性编程
        }

        // 根据消息类型分发处理
        if (msg.type == APP_MSG_UPLOAD)
        {
            // 执行上传：通过 MQTT 发送 JSON 到云端
            app_cloud_send_json(msg.payload, msg.len);
        }
        else // APP_MSG_DOWNLINK
        {
            // 处理下行指令（如配置更新、控制命令等）
            app_handle_downlink_json(msg.payload, msg.len);
        }
    }
}

// ========================
// MQTT 数据接收回调函数
// ========================
static void app_mqtt_data_cb(const char *topic, size_t topic_len, const char *data, size_t data_len)
{
    (void)topic;
    (void)topic_len;

    // 安全检查：队列未初始化或数据无效则直接返回
    if (!s_inbound_queue || !data || data_len == 0)
    {
        return;
    }

    app_inbound_msg_t msg = {0};
    size_t copy_len = data_len;

    // 防止缓冲区溢出：截断超长消息
    if (copy_len >= sizeof(msg.payload))
    {
        copy_len = sizeof(msg.payload) - 1;
    }

    // 拷贝数据并确保字符串以 '\0' 结尾
    memcpy(msg.payload, data, copy_len);
    msg.payload[copy_len] = '\0';
    msg.len = copy_len;

    // 尝试将消息放入下行队列（非阻塞发送，避免在中断上下文中卡死）
    xQueueSend(s_inbound_queue, &msg, 0);
}

// ========================
// 构建传感器数据的 JSON 字符串
// ========================
static size_t app_build_sensor_json(const SensorData *data, char *buf, size_t len)
{
    // 参数合法性检查
    if (!data || !buf || len == 0)
    {
        return 0;
    }

    cJSON *root = cJSON_CreateObject();
    if (!root)
    {
        return 0;
    }

    cJSON *data_obj = cJSON_CreateObject();
    if (!data_obj)
    {
        cJSON_Delete(root);
        return 0;
    }

    cJSON_AddStringToObject(root, "type", "upload");
    cJSON_AddNumberToObject(root, "ver", 1);
    cJSON_AddNumberToObject(root, "ts", (double)xTaskGetTickCount());

    cJSON_AddNumberToObject(data_obj, "ax", (int)data->mpu_ax);
    cJSON_AddNumberToObject(data_obj, "ay", (int)data->mpu_ay);
    cJSON_AddNumberToObject(data_obj, "az", (int)data->mpu_az);
    cJSON_AddNumberToObject(data_obj, "gx", (int)data->mpu_gx);
    cJSON_AddNumberToObject(data_obj, "gy", (int)data->mpu_gy);
    cJSON_AddNumberToObject(data_obj, "gz", (int)data->mpu_gz);

    cJSON_AddItemToObject(root, "data", data_obj);

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json)
    {
        return 0;
    }

    size_t json_len = strlen(json);
    if (json_len >= len)
    {
        cJSON_free(json);
        return 0;
    }

    memcpy(buf, json, json_len + 1);
    cJSON_free(json);
    return json_len;
}

// ========================
// 通过 MQTT 发送上行 JSON 数据
// ========================
static esp_err_t app_cloud_send_json(const char *json, size_t len)
{
    // 参数检查
    if (!json || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 检查 MQTT 是否已连接，避免发送失败
    if (!mqtt_is_connected())
    {
        ESP_LOGW(TAG, "MQTT not connected, upload skipped");
        return ESP_ERR_INVALID_STATE;
    }

    // 调用封装好的 MQTT 发布接口（QoS=1，不保留）
    return mqtt_app_publish(MQTT_TOPIC_UP, json, len, 0, false);
}

// ========================
// 处理下行 JSON 指令（用户需在此实现具体逻辑）
// ========================
static void app_handle_downlink_json(const char *json, size_t len)
{
    // 参数检查
    if (!json || len == 0)
    {
        return;
    }

    // 当前仅打印日志，实际应用中应解析 JSON 并执行相应操作
    // 例如：OTA 触发、参数配置、LED 控制等
    ESP_LOGI(TAG, "Downlink JSON: %.*s", (int)len, json);
}