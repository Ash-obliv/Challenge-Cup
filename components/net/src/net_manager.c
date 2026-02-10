#include "net_manager.h"

#define NET_CB_MAX 4

static net_connected_cb_t s_connected_cbs[NET_CB_MAX] = {0};
static net_disconnected_cb_t s_disconnected_cbs[NET_CB_MAX] = {0};

esp_err_t net_register_connected_cb(net_connected_cb_t cb)
{
    if (!cb)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_connected_cbs[i] == cb)
        {
            return ESP_OK;
        }
    }

    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_connected_cbs[i] == NULL)
        {
            s_connected_cbs[i] = cb;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

esp_err_t net_register_disconnected_cb(net_disconnected_cb_t cb)
{
    if (!cb)
    {
        return ESP_ERR_INVALID_ARG;
    }

    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_disconnected_cbs[i] == cb)
        {
            return ESP_OK;
        }
    }

    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_disconnected_cbs[i] == NULL)
        {
            s_disconnected_cbs[i] = cb;
            return ESP_OK;
        }
    }

    return ESP_ERR_NO_MEM;
}

void net_notify_connected(net_transport_t transport, const char *ip)
{
    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_connected_cbs[i])
        {
            s_connected_cbs[i](transport, ip);
        }
    }
}

void net_notify_disconnected(net_transport_t transport)
{
    for (int i = 0; i < NET_CB_MAX; i++)
    {
        if (s_disconnected_cbs[i])
        {
            s_disconnected_cbs[i](transport);
        }
    }
}
