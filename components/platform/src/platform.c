#include "platform.h"
#include "mpu6050.h" // 假设有这个头文件，包含 MPU6050 相关函数声明 

esp_err_t platform_get_sensor_data(SensorData *data)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 假设有函数 Int_MPU6050_Get_Accel 和 Int_MPU6050_Get_Gyro 用于获取传感器数据
    short ax, ay, az;
    Int_MPU6050_Get_Accel(&ax, &ay, &az);

    // 这里只使用 ax 来模拟丙酮的数据，其他数据暂时不使用
    data->mpu_ax = ax;
    return ESP_OK;
}

