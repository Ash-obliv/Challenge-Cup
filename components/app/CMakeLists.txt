# 这个是app组件的CMakeLists.txt文件
idf_component_register(
    SRCS "src/app_task.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES platform net tool
)