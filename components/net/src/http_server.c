#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi.h"
#include "http.h"

static const char *TAG = "http_server";


static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


static esp_err_t g4_setup_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, g4_setup_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}



static esp_err_t wifi_setup_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, wifi_setup_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t log_handler(httpd_req_t *req)
{
    char buf[64] = {0};
    int len = httpd_req_get_url_query_len(req);
    if (len > 0 && len < (int)sizeof(buf))
    {
        if (httpd_req_get_url_query_str(req, buf, sizeof(buf)) == ESP_OK)
        {
            char msg[48] = {0};
            if (httpd_query_key_value(buf, "m", msg, sizeof(msg)) == ESP_OK)
            {
                ESP_LOGI(TAG, "web_log: %s", msg);
            }
        }
    }

    httpd_resp_set_type(req, "text/plain");
    httpd_resp_sendstr(req, "ok");
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    char ip_buf[16] = {0};
    bool connected = wifi_is_connected();
    if (connected)
    {
        wifi_get_ip_str(ip_buf, sizeof(ip_buf));
    }

    char resp[64];
    if (connected)
    {
        snprintf(resp, sizeof(resp), "{\"connected\":true,\"ip\":\"%s\"}", ip_buf);
    }
    else
    {
        snprintf(resp, sizeof(resp), "{\"connected\":false}");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

/* ================== /scan 接口 ================== */
static esp_err_t scan_handler(httpd_req_t *req)
{
    char *json = calloc(1, 2048);
    if (!json)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi scan...");
    esp_err_t err = wifi_scan_to_json(json, 2048);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        free(json);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Scan result: %s", json);
    httpd_resp_set_type(req, "application/json");
    /* 关键：添加 CORS 头，防止浏览器拦截 XHR 响应 */
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_sendstr(req, json);
    free(json);
    return ESP_OK;
}

static esp_err_t connect_handler(httpd_req_t *req)
{
    char buf[200];
    int ret;

    if (req->content_len > sizeof(buf) - 1)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }

    ret = httpd_req_recv(req, buf, req->content_len);
    if (ret <= 0)
    {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Bad Request");
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    char ssid[33] = {0};
    char password[65] = {0};

    char *ssid_ptr = strstr(buf, "ssid=");
    char *pass_ptr = strstr(buf, "&password=");

    if (ssid_ptr && pass_ptr)
    {
        ssid_ptr += 5;
        *pass_ptr = '\0';
        strncpy(ssid, ssid_ptr, sizeof(ssid) - 1);

        pass_ptr += 10;
        strncpy(password, pass_ptr, sizeof(password) - 1);

        esp_err_t err = wifi_connect_to_target(ssid, password);
        if (err == ESP_OK)
        {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"ok\":true,\"message\":\"connecting\"}");
        }
        else
        {
            httpd_resp_set_type(req, "application/json");
            httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"connect failed\"}");
        }
    }
    else
    {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_sendstr(req, "{\"ok\":false,\"message\":\"bad request\"}");
    }

    return ESP_OK;
}

/* ================== 启动服务器 ================== */
esp_err_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.stack_size = 8192;
    config.max_uri_handlers = 10;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t wifi_setup_uri = {
        .uri = "/wifi_setup",
        .method = HTTP_GET,
        .handler = wifi_setup_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &wifi_setup_uri);

    httpd_uri_t status_uri = {
        .uri = "/status",
        .method = HTTP_GET,
        .handler = status_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &status_uri);

    httpd_uri_t g4_setup_uri = {
        .uri = "/g4_setup",
        .method = HTTP_GET,
        .handler = g4_setup_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &g4_setup_uri);

    httpd_uri_t scan_uri = {
        .uri = "/scan",
        .method = HTTP_GET,
        .handler = scan_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &scan_uri);

    httpd_uri_t log_uri = {
        .uri = "/log",
        .method = HTTP_GET,
        .handler = log_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &log_uri);

    httpd_uri_t connect_uri = {
        .uri = "/connect",
        .method = HTTP_POST,
        .handler = connect_handler,
        .user_ctx = NULL};
    httpd_register_uri_handler(server, &connect_uri);

    ESP_LOGI(TAG, "HTTP server started on http://192.168.100.1");
    return ESP_OK;
}