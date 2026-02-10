#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "esp_err.h"

typedef struct 
{
    short   mpu_ax;
    short   mpu_ay;
    short   mpu_az;
    short   mpu_gx;
    short   mpu_gy;
    short   mpu_gz;
}SensorData;   // 封装要上报的数据结构

esp_err_t platform_get_sensor_data(SensorData *data);



#endif // __PLATFORM_H__