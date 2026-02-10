#include <stdio.h>
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "platform_i2c.h"
#include "mpu6050.h"
#include "OLED.h"
#include "wifi.h"
#include "http_server.h"
#include "my_mqtt.h"
#include "net_manager.h"
#include "app_task.h"

static const char *TAG = "main";
short ax, ay, az;

#define MQTT_URI "mqtt://47.92.152.245:1883"
#define MQTT_CLIENT_ID "test/topic"
#define MQTT_USERNAME "esp32"
#define MQTT_PASSWORD "esp32"
#define MQTT_CA_CERT NULL

static bool s_mqtt_started = false;

static void net_connected_cb(net_transport_t transport, const char *ip)
{
    if (transport == NET_TRANSPORT_CELLULAR)
    {
        OLED_ClearArea(0, 20, 128, 10);
        OLED_Printf(0, 20, OLED_6X8, "4G OK");
    }
    else
    {
        OLED_ClearArea(0, 20, 128, 10);
        OLED_Printf(0, 20, OLED_6X8, "WiFi OK");
    }
    OLED_Update();
    ESP_LOGI(TAG, "Network connected (%s), IP: %s",(transport == NET_TRANSPORT_CELLULAR) ? "4G" : "Wi-Fi",ip ? ip : "");

    if (!s_mqtt_started)
    {
        if (mqtt_app_init(MQTT_URI, MQTT_CLIENT_ID, MQTT_USERNAME, MQTT_PASSWORD, MQTT_CA_CERT) == ESP_OK)
        {
            s_mqtt_started = true;
            ESP_LOGI(TAG, "MQTT client initialized");
        }
    }

    // 到这里说明当前就是已经可以上网 为了功耗考虑 可以关闭配网 AP
    wifi_stop_provisioning_ap();

    OLED_ClearArea(0, 10, 128, 10);
    OLED_Printf(0, 10, OLED_6X8, "net success ap stop");
    OLED_Update();
}

void app_main(void)
{
    // 1. 初始化I2C平台
    platform_i2c_init();

    // 2. 初始化 Wi-Fi（AP+STA 模式） 这个是硬件的初始化配置
    ESP_ERROR_CHECK(wifi_init_apsta_for_provisioning());

    // // 5. 检测MPU6050是否存在
    // platform_i2c_mpu6050_is_present();
    // platform_i2c_oled_is_present();

    // 3. 注册MPU6050驱动函数
    platform_driver_register();

    // 4. 初始化MPU6050传感器
    Int_MPU6050_Init();
    OLED_Init();
    OLED_Printf(0, 0, OLED_6X8, "sensor init!");
    OLED_Update();

    net_register_connected_cb(net_connected_cb);

    app_task_init();
}
