#ifndef __MPU6050_H__
#define __MPU6050_H__

#include <stdint.h>

// 这个是MPU6050的驱动注册 一共会要用到几个函数 在这里定义成为函数指针的结构体
typedef struct
{
    void (*MPU6050_WriteByte)(uint8_t reg_addr, uint8_t send_byte);
    void (*MPU6050_ReadByte)(uint8_t reg_addr, uint8_t *receive_byte);
    void (*MPU6050_ReadBytes)(uint8_t reg_addr, uint8_t *buf, uint8_t len);
    void (*MPU6050_DelayMs)(uint32_t ms);
} MPU6050_DrvTypeDef;

void MPU6050_RegisterDriver(
    void (*read_byte)(uint8_t reg_addr, uint8_t *receive_byte),
    void (*read_bytes)(uint8_t reg_addr, uint8_t *receive_buff, uint8_t size),
    void (*write_byte)(uint8_t reg_addr, uint8_t value),
    void (*delay_ms)(uint32_t ms)
);

void Int_MPU6050_Init(void);
void Int_MPU6050_Get_Gyro(short *gx, short *gy, short *gz);
void Int_MPU6050_Get_Accel(short *ax, short *ay, short *az);




#endif /* __MPU6050_H__ */