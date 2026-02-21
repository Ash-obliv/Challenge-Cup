#ifndef __PLATFORM_H__
#define __PLATFORM_H__

#include "esp_err.h"

typedef struct 
{
    short   mpu_ax;                  // 这里最后一定是会用丙酮的数据 这里就用这个去模拟丙酮的数据
}SensorData;   // 封装要上报的数据结构

esp_err_t platform_get_sensor_data(SensorData *data);



#endif // __PLATFORM_H__