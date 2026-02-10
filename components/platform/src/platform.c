#include "platform.h"
#include "mpu6050.h" // 假设有这个头文件，包含 MPU6050 相关函数声明 

esp_err_t platform_get_sensor_data(SensorData *data)
{
    if (data == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    // 假设有函数 Int_MPU6050_Get_Accel 和 Int_MPU6050_Get_Gyro 用于获取传感器数据
    Int_MPU6050_Get_Accel(&data->mpu_ax, &data->mpu_ay, &data->mpu_az);
    Int_MPU6050_Get_Gyro(&data->mpu_gx, &data->mpu_gy, &data->mpu_gz);

    return ESP_OK;
}

