# 这个是driver组件的CMakeLists.txt文件
idf_component_register(
    SRCS "src/platform_i2c.c"  "src/platform.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_i2c  inf
)