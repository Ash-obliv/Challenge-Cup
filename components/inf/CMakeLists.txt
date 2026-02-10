# 这个是本项目硬件接口层inf的CMakeLists.txt文件
idf_component_register(
    SRCS "src/mpu6050.c"  "src/OLED.c"  "src/OLED_Data.c"
    INCLUDE_DIRS "include"
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-missing-braces)
