// http_server.h
#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 启动配网用的 HTTP 服务器
 * - 监听 80 端口
 * - 根路径返回简单 HTML
 */
esp_err_t start_webserver(void);

#ifdef __cplusplus
}
#endif

#endif // HTTP_SERVER_H