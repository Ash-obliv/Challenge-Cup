#ifndef NET_MANAGER_H
#define NET_MANAGER_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    NET_TRANSPORT_WIFI = 0,
    NET_TRANSPORT_CELLULAR = 1,
} net_transport_t;

typedef void (*net_connected_cb_t)(net_transport_t transport, const char *ip);
typedef void (*net_disconnected_cb_t)(net_transport_t transport);

esp_err_t net_register_connected_cb(net_connected_cb_t cb);
esp_err_t net_register_disconnected_cb(net_disconnected_cb_t cb);

void net_notify_connected(net_transport_t transport, const char *ip);
void net_notify_disconnected(net_transport_t transport);

#ifdef __cplusplus
}
#endif

#endif // NET_MANAGER_H
