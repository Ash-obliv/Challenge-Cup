#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_log.h"
#include "wifi.h"

static const char *TAG = "http_server";

/* ================== 主页：选择 4G 或 Wi-Fi ================== */
static const char *index_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <meta charset='UTF-8'>"
    "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "  <title>设备配网</title>"
    "  <style>"
    "    body { font-family: Arial, sans-serif; text-align: center; padding: 30px; background: #f5f5f5; }"
    "    h1 { color: #333; margin-bottom: 40px; }"
    "    .btn {"
    "      display: block; width: 80%; padding: 15px; margin: 15px auto;"
    "      font-size: 18px; border: none; border-radius: 8px; cursor: pointer;"
    "    }"
    "    .btn-4g { background: #ff6b6b; color: white; }"
    "    .btn-wifi { background: #4ecdc4; color: white; }"
    "  </style>"
    "</head>"
    "<body>"
    "  <h1>请选择联网方式</h1>"
    "  <button class='btn btn-4g' onclick=\"window.location.href='/g4_setup'\">使用 4G 上网</button>"
    "  <button class='btn btn-wifi' onclick=\"window.location.href='/wifi_setup'\">使用 Wi-Fi 联网</button>"
    "</body>"
    "</html>";

static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================== 4G 配网页面（占位） ================== */
static const char *g4_setup_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <meta charset='UTF-8'>"
    "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "  <title>4G 配网</title>"
    "  <style>"
    "    body { font-family: Arial, sans-serif; text-align: center; padding: 40px; background: #f0f0f0; }"
    "    h2 { color: #d32f2f; }"
    "    .note { background: #fff8e1; padding: 20px; border-radius: 8px; margin: 20px 0; }"
    "    .btn-back {"
    "      display: inline-block; margin-top: 20px; padding: 10px 20px;"
    "      background: #90caf9; color: #0d47a1; text-decoration: none; border-radius: 5px;"
    "    }"
    "  </style>"
    "</head>"
    "<body>"
    "  <h2>4G 联网配置</h2>"
    "  <div class='note'>"
    "    <p>当前版本暂未集成 4G 模块功能。</p>"
    "  </div>"
    "  <a href='/' class='btn-back'>返回首页</a>"
    "</body>"
    "</html>";

static esp_err_t g4_setup_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    httpd_resp_send(req, g4_setup_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================== Wi-Fi 配网页（JS 内联，最可靠） ================== */
static const char *wifi_setup_html =
    "<!DOCTYPE html>"
    "<html>"
    "<head>"
    "  <meta charset='UTF-8'>"
    "  <meta name='viewport' content='width=device-width, initial-scale=1.0'>"
    "  <title>Wi-Fi 配网</title>"
    "  <style>"
    "    body { font-family: Arial, sans-serif; padding: 20px; background: #fff; }"
    "    h2 { text-align: center; color: #333; }"
    "    .btn { padding: 10px 20px; margin: 10px 5px; font-size: 16px; cursor: pointer; }"
    "    .scan-btn { background: #4ecdc4; color: white; border: none; border-radius: 5px; }"
    "    .connect-btn { background: #51cf66; color: white; border: none; border-radius: 5px; }"
    "    input { width: 90%; padding: 10px; margin: 10px 0; border: 1px solid #ccc; border-radius: 4px; }"
    "    .ap-list { margin-top: 20px; }"
    "    .ap-item { padding: 12px; margin: 8px 0; background: #f9f9f9; border-left: 4px solid #4ecdc4; cursor: pointer; }"
    "    .ap-item:hover { background: #e0f7fa; }"
    "    .rssi { float: right; color: #888; font-size: 14px; }"
    "    #status { margin-top: 10px; padding: 10px; font-size: 14px; color: #555; }"
    "  </style>"
    "</head>"
    "<body>"
    "  <h2>Wi-Fi 配网</h2>"
    /* 关键修改：去掉 onclick 属性，只用 id 来绑定事件 */
    "  <button class='btn scan-btn' id='scanBtn' type='button'>扫描附近 Wi-Fi</button>"
    "  <div id='status'></div>"
    "  <div id='apList' class='ap-list'></div>"
    ""
    "  <form id='wifiForm'>"
    "    <input type='text' id='ssid' name='ssid' placeholder='Wi-Fi 名称 (SSID)' required><br>"
    "    <input type='password' id='password' name='password' placeholder='Wi-Fi 密码' required><br>"
    "    <button type='submit' class='btn connect-btn'>连接 Wi-Fi</button>"
    "  </form>"
    ""
    /* 关键修改：JS 直接内联，不依赖外部脚本加载 */
    "  <script>"
    "    var scanBtn = document.getElementById('scanBtn');"
    "    var apList = document.getElementById('apList');"
    "    var statusDiv = document.getElementById('status');"
    "    var wifiForm = document.getElementById('wifiForm');"
    ""
    "    function setStatus(msg) {"
    "      statusDiv.innerText = msg;"
    "    }"
    ""
    "    function scanWiFi() {"
    "      setStatus('正在扫描，请稍候...');"
    "      apList.innerHTML = '';"
    "      var xhr = new XMLHttpRequest();"
    "      xhr.open('GET', '/scan', true);"
    "      xhr.timeout = 15000;"  /* 扫描可能需要几秒，设15秒超时 */
    "      xhr.onload = function() {"
    "        if (xhr.status !== 200) {"
    "          setStatus('扫描失败: HTTP ' + xhr.status);"
    "          return;"
    "        }"
    "        var aps;"
    "        try { aps = JSON.parse(xhr.responseText); } catch(e) {"
    "          setStatus('JSON 解析错误');"
    "          return;"
    "        }"
    "        if (aps.length === 0) {"
    "          setStatus('未发现可用 Wi-Fi');"
    "          return;"
    "        }"
    "        setStatus('发现 ' + aps.length + ' 个网络:');"
    "        var html = '';"
    "        for (var i = 0; i < aps.length; i++) {"
    "          var ap = aps[i];"
    "          if (!ap.ssid || ap.ssid.trim() === '') continue;"
    "          var rssi = ap.rssi;"
    "          var bars = '弱';"
    "          if (rssi > -60) bars = '强';"
    "          else if (rssi > -75) bars = '中';"
    "          html += '<div class=\"ap-item\" data-ssid=\"' + ap.ssid + '\">' +"
    "                  ap.ssid + ' <span class=\"rssi\">' + bars + ' (' + rssi + ' dBm)</span>' +"
    "                  '</div>';"
    "        }"
    "        apList.innerHTML = html;"
    /* 给每个 AP 项绑定点击事件 */
    "        var items = apList.querySelectorAll('.ap-item');"
    "        for (var j = 0; j < items.length; j++) {"
    "          items[j].addEventListener('click', function() {"
    "            var s = this.getAttribute('data-ssid');"
    "            document.getElementById('ssid').value = s;"
    "            document.getElementById('password').focus();"
    "            setStatus('已选择: ' + s);"
    "          });"
    "        }"
    "      };"
    "      xhr.onerror = function() {"
    "        setStatus('网络错误，请检查是否连接到 ESP32-AP');"
    "      };"
    "      xhr.ontimeout = function() {"
    "        setStatus('扫描超时，请重试');"
    "      };"
    "      xhr.send();"
    "    }"
    ""
    "    function connectWiFi(e) {"
    "      e.preventDefault();"
    "      var ssid = document.getElementById('ssid').value.trim();"
    "      var pass = document.getElementById('password').value;"
    "      if (!ssid) { setStatus('请输入 SSID'); return; }"
    "      setStatus('正在连接 ' + ssid + ' ...');"
    "      var body = 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(pass);"
    "      var xhr = new XMLHttpRequest();"
    "      xhr.open('POST', '/connect', true);"
    "      xhr.setRequestHeader('Content-Type', 'application/x-www-form-urlencoded');"
    "      xhr.onload = function() {"
    "        if (xhr.status !== 200) {"
    "          setStatus('请求失败: HTTP ' + xhr.status);"
    "          return;"
    "        }"
    "        var data;"
    "        try { data = JSON.parse(xhr.responseText); } catch(e2) {"
    "          setStatus('响应解析失败');"
    "          return;"
    "        }"
    "        if (data && data.ok) {"
    "          setStatus('已发起连接，正在等待...');"
    "          pollStatus();"
    "        } else {"
    "          setStatus(data && data.message ? data.message : '连接失败');"
    "        }"
    "      };"
    "      xhr.onerror = function() { setStatus('网络错误'); };"
    "      xhr.send(body);"
    "    }"
    ""
    "    function pollStatus() {"
    "      var count = 0;"
    "      var timer = setInterval(function() {"
    "        count++;"
    "        if (count > 20) { clearInterval(timer); setStatus('连接超时'); return; }"
    "        var xhr = new XMLHttpRequest();"
    "        xhr.open('GET', '/status', true);"
    "        xhr.timeout = 3000;"
    "        xhr.onload = function() {"
    "          if (xhr.status !== 200) return;"
    "          var s;"
    "          try { s = JSON.parse(xhr.responseText); } catch(e3) { return; }"
    "          if (s && s.connected) {"
    "            clearInterval(timer);"
    "            setStatus('连接成功! IP: ' + (s.ip || ''));"
    "          }"
    "        };"
    "        xhr.onerror = function() {};"
    "        xhr.send();"
    "      }, 1500);"
    "    }"
    ""
    /* 绑定事件 */
    "    scanBtn.addEventListener('click', scanWiFi);"
    "    wifiForm.addEventListener('submit', connectWiFi);"
    "  </script>"
    "</body>"
    "</html>";

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