# 这个是net组件的CMakeLists.txt文件
idf_component_register(
    SRCS "src/wifi.c" "src/http_server.c" "src/my_mqtt.c" "src/net_manager.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_wifi nvs_flash esp_http_server lwip esp_netif mqtt inf
)