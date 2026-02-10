// wifi.h
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "esp_err.h"
#include <stdbool.h>

#include "lwip/ip4_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Wi-Fi 为 AP+STA 模式（用于配网）
 * - 创建 STA 和 AP netif
 * - 注册事件回调
 * - 启动 Wi-Fi 驱动（但未配置具体网络）
 * 
 * 调用一次即可，通常在 app_main() 开头调用。
 */
esp_err_t wifi_init_apsta_for_provisioning(void);

/**
 * @brief 启动配网用的 SoftAP
 * - SSID: "ESP32-AP"
 * - 无密码（WIFI_AUTH_OPEN）
 * - 固定 IP: 192.168.100.1
 * 
 * 用户通过手机连接此 AP 并访问网页进行配网。
 */
esp_err_t wifi_start_provisioning_ap(void);

/**
 * @brief 连接到用户指定的目标 Wi-Fi（不保存到 NVS）
 * - 仅用于本次运行
 * - 连接成功后建议关闭 AP 以省电
 * 
 * @param ssid 目标 Wi-Fi 名称
 * @param password 目标 Wi-Fi 密码
 */
esp_err_t wifi_connect_to_target(const char *ssid, const char *password);

/**
 * @brief 关闭 SoftAP（保留 STA 连接）
 * - 清空 AP 配置，停止 DHCP Server
 * - 适用于连接成功后节省功耗
 */
esp_err_t wifi_stop_provisioning_ap(void);

/**
 * @brief 查询当前 STA 连接状态
 *
 * @return true 已连接并获取 IP；false 未连接
 */
bool wifi_is_connected(void);

/**
 * @brief 获取 STA IP 字符串
 *
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 * @return ESP_OK 成功；ESP_ERR_INVALID_STATE 未连接
 */
esp_err_t wifi_get_ip_str(char *buf, size_t len);

/**
 * @brief 扫描附近 Wi-Fi 并输出 JSON 字符串
 *
 * @param buf 输出缓冲区
 * @param len 缓冲区长度
 * @return ESP_OK 成功；其他为错误码
 */
esp_err_t wifi_scan_to_json(char *buf, size_t len);

typedef void (*wifi_connected_cb_t)(const char *ip);

/**
 * @brief 注册 STA 连接成功回调（获取到 IP 时触发）
 *
 * @param cb 回调函数，传入 IP 字符串
 */
void wifi_register_connected_cb(wifi_connected_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif // WIFI_MANAGER_H