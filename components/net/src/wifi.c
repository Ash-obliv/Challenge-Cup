#include "wifi.h"
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net_manager.h"
#include "OLED.h"
#include "http_server.h"

static const char *TAG = "wifi";

// 全局 netif 句柄（必须全局或静态，避免重复创建）
static esp_netif_t *esp_netif_sta = NULL;
static esp_netif_t *esp_netif_ap = NULL;

// 连接重试计数（仅用于日志，不影响逻辑，因为不保存配置）
static int s_retry_count = 0;
#define MAX_RETRY 5

// STA 连接状态与 IP
static bool s_sta_connected = false;
static ip4_addr_t s_sta_ip = {0};
static wifi_connected_cb_t s_connected_cb = NULL;

/* ======================== Wi-Fi 事件处理函数 ======================== */

/**
 * @brief 处理 Wi-Fi 和 IP 事件
 * - STA 启动时自动连接（由 connect_to_target 触发）
 * - 断开时重试（最多 MAX_RETRY 次）
 * - 获取 IP 时表示连接成功
 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // 此事件在 esp_wifi_connect() 后由系统触发
        ESP_LOGI(TAG, "STA interface started (waiting for user config)");
        // 注意：实际连接由 connect_to_target() 中的 esp_wifi_connect() 发起
        // 这里一般不需要再调 connect()

        // 当发起信号的时候 说明当前可以进行配网环节 创建以后的AP热点 供用户连接 配置Wi-Fi信息
        ESP_ERROR_CHECK(wifi_start_provisioning_ap());

        // 6. 启动 HTTP 服务器 用于配网的网页服务
        ESP_ERROR_CHECK(start_webserver());

        OLED_ClearArea(0, 10, 128, 10);
        OLED_Printf(0, 10, OLED_6X8, "net setting ap started");
        OLED_Update();

        s_sta_connected = false;
        s_sta_ip.addr = 0;
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        s_sta_connected = false;
        s_sta_ip.addr = 0;
        net_notify_disconnected(NET_TRANSPORT_WIFI);
        s_retry_count++;
        if (s_retry_count <= MAX_RETRY)
        {
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying... (%d/%d)", s_retry_count, MAX_RETRY);
            esp_wifi_connect(); // 重试连接
        }
        else
        {
            ESP_LOGE(TAG, "Failed to connect after %d attempts.", MAX_RETRY);
            // 注意：因为不保存配置，这里不做恢复 AP 的操作
            // 实际产品中可考虑重启进入配网模式
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected! Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_count = 0; // 重置重试计数
        s_sta_connected = true;
        s_sta_ip.addr = event->ip_info.ip.addr;
        char ip_str[16] = {0};
        ip4addr_ntoa_r(&s_sta_ip, ip_str, sizeof(ip_str));
        if (s_connected_cb)
        {
            s_connected_cb(ip_str);
        }
        net_notify_connected(NET_TRANSPORT_WIFI, ip_str);
    }
}

/* ======================== 对外 API 实现 ======================== */

// 初始化ESP32关于网络的底层硬件部分 和软件部分，为后续的Wi-Fi功能做准备
esp_err_t wifi_init_apsta_for_provisioning(void)
{
    // 1. 初始化 NVS（Wi-Fi 配置会用到，即使我们不用保存）
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. 初始化 LwIP 网络协议栈
    ESP_ERROR_CHECK(esp_netif_init());

    // 3. 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 4. 创建 STA 和 AP 网络接口（netif）
    //    必须都创建，才能使用 WIFI_MODE_APSTA
    esp_netif_sta = esp_netif_create_default_wifi_sta();
    assert(esp_netif_sta != NULL);

    esp_netif_ap = esp_netif_create_default_wifi_ap();
    assert(esp_netif_ap != NULL);

    // 5. 初始化 Wi-Fi 驱动
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 6. 注册事件回调（监听所有 Wi-Fi 事件和 IP 获取事件）
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, NULL));

    // 7. 设置为 AP+STA 模式（但此时 AP 和 STA 都未配置）
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    // 8. 启动 Wi-Fi（进入 idle 状态，等待配置）
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi initialized in AP+STA mode (ready for provisioning)");
    return ESP_OK;
}

esp_err_t wifi_start_provisioning_ap(void)
{
    // 配置 SoftAP 参数
    wifi_config_t ap_config = {
        .ap = {
            .ssid = "ESP32-AP",
            .ssid_len = 0, // 自动计算长度
            .channel = 6,
            .password = "", // 无密码
            .max_connection = 2,
            .authmode = WIFI_AUTH_OPEN, // 必须与 password="" 匹配
        },
    };

    // 设置 AP 配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    // 配置固定 IP 地址（方便用户访问）
    esp_netif_ip_info_t ip_info = {0};
    IP4_ADDR(&ip_info.ip, 192, 168, 100, 1);
    IP4_ADDR(&ip_info.gw, 192, 168, 100, 1);
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    // 应用静态 IP（先停 DHCP Server）
    ESP_ERROR_CHECK(esp_netif_dhcps_stop(esp_netif_ap));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif_ap, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(esp_netif_ap));

    ESP_LOGI(TAG, "Provisioning AP started: SSID=ESP32-AP, IP=192.168.100.1");
    return ESP_OK;
}

esp_err_t wifi_connect_to_target(const char *ssid, const char *password)
{
    if (!ssid || !password)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 构造 STA 配置
    wifi_config_t sta_config = {0};
    strncpy((char *)sta_config.sta.ssid, ssid, sizeof(sta_config.sta.ssid) - 1);
    strncpy((char *)sta_config.sta.password, password, sizeof(sta_config.sta.password) - 1);
    sta_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK; // 兼容 WPA/WPA2
    sta_config.sta.pmf_cfg.capable = true;
    sta_config.sta.pmf_cfg.required = false;

    // 设置 STA 配置
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    if (err == ESP_ERR_WIFI_STATE)
    {
        esp_wifi_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));
        err = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
    }
    if (err != ESP_OK)
    {
        return err;
    }

    // 发起连接（异步）
    err = esp_wifi_connect();
    if (err != ESP_OK)
    {
        return err;
    }

    ESP_LOGI(TAG, "Connecting to target Wi-Fi: %s", ssid);
    return ESP_OK;
}

esp_err_t wifi_stop_provisioning_ap(void)
{
    // 方法：清空 AP 配置（最安全）
    wifi_config_t empty_ap = {0};
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &empty_ap));

    // 可选：停止 DHCP Server（非必须，set_config 会处理）
    esp_netif_dhcps_stop(esp_netif_ap);

    ESP_LOGI(TAG, "Provisioning AP stopped.");
    return ESP_OK;
}

bool wifi_is_connected(void)
{
    return s_sta_connected;
}

esp_err_t wifi_get_ip_str(char *buf, size_t len)
{
    if (!buf || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_sta_connected)
    {
        return ESP_ERR_INVALID_STATE;
    }

    ip4addr_ntoa_r(&s_sta_ip, buf, len);
    return ESP_OK;
}

// 注意：调用此函数前需确保 Wi-Fi 已初始化且 STA 模式已启动
// 这个函数的作用就是扫描附近的Wi-Fi网络，并将结果以JSON格式存储在buf中
esp_err_t wifi_scan_to_json(char *buf, size_t len)
{
    if (!buf || len == 0)
    {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_scan_config_t scan_conf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false};

    esp_err_t err = esp_wifi_scan_start(&scan_conf, true);
    if (err != ESP_OK)
    {
        return err;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0)
    {
        snprintf(buf, len, "[]");
        return ESP_OK;
    }

    wifi_ap_record_t *ap_list = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (!ap_list)
    {
        return ESP_ERR_NO_MEM;
    }

    uint16_t got_count = ap_count;
    esp_wifi_scan_get_ap_records(&got_count, ap_list);

    size_t used = 0;
    used += snprintf(buf + used, len - used, "[");

    int added = 0;
    for (int i = 0; i < got_count; i++)
    {
        if (strlen((char *)ap_list[i].ssid) == 0)
        {
            continue;
        }

        if (added > 0)
        {
            if (used + 1 >= len)
            {
                break;
            }
            buf[used++] = ',';
            buf[used] = '\0';
        }

        int n = snprintf(buf + used, len - used,
                         "{\"ssid\":\"%s\",\"rssi\":%d}",
                         (char *)ap_list[i].ssid,
                         ap_list[i].rssi);
        if (n < 0 || (size_t)n >= len - used)
        {
            break;
        }
        used += (size_t)n;
        added++;
    }

    if (used + 2 <= len)
    {
        snprintf(buf + used, len - used, "]");
    }
    else
    {
        buf[len - 2] = ']';
        buf[len - 1] = '\0';
    }

    free(ap_list);
    return ESP_OK;
}

void wifi_register_connected_cb(wifi_connected_cb_t cb)
{
    s_connected_cb = cb;
}
