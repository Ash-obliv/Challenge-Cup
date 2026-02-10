#include "mpu6050.h"
#include "mpu6050_data.h"

static MPU6050_DrvTypeDef s_default_driver = {0}; // 内部静态实例
MPU6050_DrvTypeDef *g_mpu6050_driver = &s_default_driver;

void MPU6050_RegisterDriver(
    void (*read_byte)(uint8_t reg_addr, uint8_t *receive_byte),
    void (*read_bytes)(uint8_t reg_addr, uint8_t *receive_buff, uint8_t size),
    void (*write_byte)(uint8_t reg_addr, uint8_t value),
    void (*delay_ms)(uint32_t ms))
{
    g_mpu6050_driver->MPU6050_ReadByte = read_byte;
    g_mpu6050_driver->MPU6050_ReadBytes = read_bytes;
    g_mpu6050_driver->MPU6050_WriteByte = write_byte;
    g_mpu6050_driver->MPU6050_DelayMs = delay_ms;
}

void Int_MPU6050_WriteByte(uint8_t reg_addr, uint8_t send_byte) { g_mpu6050_driver->MPU6050_WriteByte(reg_addr, send_byte); }
void Int_MPU6050_ReadByte(uint8_t reg_addr, uint8_t *receive_byte) { g_mpu6050_driver->MPU6050_ReadByte(reg_addr, receive_byte); }
void Int_MPU6050_ReadBytes(uint8_t reg_addr, uint8_t *receive_buff, uint8_t size) { g_mpu6050_driver->MPU6050_ReadBytes(reg_addr, receive_buff, size); }
void Int_delay_ms(uint32_t ms) { g_mpu6050_driver->MPU6050_DelayMs(ms); }

/**
 * @description: 根据采样率设置低通滤波器
 * @param {uint16_t} rate
 * @return {*}
 */
void Int_MPU6050_Set_DLPF_CFG(uint16_t rate)
{
    /* 采样定理： 采样率 >= 2*带宽 才不会失真 ===》 带宽 <= 采样率/2 */
    uint8_t cfg = 0;
    rate = rate / 2;
    if (rate > 188)
    {
        cfg = 1;
    }
    else if (rate > 98)
    {
        cfg = 2;
    }
    else if (rate > 42)
    {
        cfg = 3;
    }
    else if (rate > 20)
    {
        cfg = 4;
    }
    else if (rate > 10)
    {
        cfg = 5;
    }
    else
    {
        cfg = 6;
    }
    Int_MPU6050_WriteByte(MPU_CFG_REG, cfg << 0);
}

/**
 * @description: 设置陀螺仪的采样率
 * @param {uint16_t} rate
 * @return {*}
 */
void Int_MPU6050_SetGyroRate(uint16_t rate)
{
    /* 采样率=输出频率/(1+分频值) ===》 分频值 = (输出频率/采样率) -1 */
    uint8_t sample_div = 0;
    /* 1. 采样率限幅:产品说明书12页，输出频率最小值=4，最大是8k，我们期望最大是1k */
    if (rate < 4)
    {
        rate = 4;
    }
    else if (rate > 1000)
    {
        rate = 1000;
    }

    /* 2. 根据期望的采样率，计算出分频值 */
    sample_div = 1000 / rate - 1;

    /* 3. 将分频值设置到寄存器中 */
    Int_MPU6050_WriteByte(MPU_SAMPLE_RATE_REG, sample_div);

    /* 4. 根据采样率去设置低通滤波器 */
    Int_MPU6050_Set_DLPF_CFG(rate);
}

/**
 * @description: 初始化
 * @return {*}
 */
void Int_MPU6050_Init(void)
{
    uint8_t dev_id = 0;

    /* 2 复位 -》延迟一会 -》 唤醒 */
    Int_MPU6050_WriteByte(MPU_PWR_MGMT1_REG, 0x80);
    Int_delay_ms(300);
    Int_MPU6050_WriteByte(MPU_PWR_MGMT1_REG, 0x00);

    /* 3 陀螺仪量程 +-2000°/s, fsr=3 */
    Int_MPU6050_WriteByte(MPU_GYRO_CFG_REG, 3 << 3);

    /* 4 加速度量程 */
    Int_MPU6050_WriteByte(MPU_ACCEL_CFG_REG, 0 << 3);

    /* 5 其他功能设置（可选）：FIFO、第二IIC、中断 */
    Int_MPU6050_WriteByte(MPU_INT_EN_REG, 0x00);    // 关闭所有中断
    Int_MPU6050_WriteByte(MPU_USER_CTRL_REG, 0x00); // 关闭第二IIC
    Int_MPU6050_WriteByte(MPU_FIFO_EN_REG, 0x00);   // 关闭FIFO

    /* 6 系统时钟源、陀螺仪采样率、低通滤波的设置 */
    /* 配置时钟源之前，确认正常工作，读一下id */
    Int_MPU6050_ReadByte(MPU_DEVICE_ID_REG, &dev_id);
    if (dev_id == MPU_IIC_ADDR)
    {
        /* 6.1 设置时钟源：陀螺仪X轴的时钟，精度更高 */
        Int_MPU6050_WriteByte(MPU_PWR_MGMT1_REG, 0x01);
        /* 6.2 设置陀螺仪采样率、低通滤波 */
        Int_MPU6050_SetGyroRate(100);
        /* 6.3 让两个传感器退出待机模式，进入正常工作状态*/
        Int_MPU6050_WriteByte(MPU_PWR_MGMT2_REG, 0x00);
    }
}

/**
 * @description: 读取陀螺仪角速度的数据，拼接两字节数据且转成带符号的类型
 * @param {short} *gx
 * @param {short} *gy
 * @param {short} *gz
 * @return {*}
 */
void Int_MPU6050_Get_Gyro(short *gx, short *gy, short *gz)
{
    uint8_t buff[6];
    Int_MPU6050_ReadBytes(MPU_GYRO_XOUTH_REG, buff, 6);

    /*
        buff[0]:角速度X轴高8位
        buff[1]:角速度X轴低8位
        buff[2]:角速度Y轴高8位
        buff[3]:角速度Y轴低8位
        buff[4]:角速度Z轴高8位
        buff[5]:角速度Z轴低8位
     */
    *gx = ((short)buff[0] << 8) | buff[1];
    *gy = ((short)buff[2] << 8) | buff[3];
    *gz = ((short)buff[4] << 8) | buff[5];
}

/**
 * @description: 读取加速度计的加速度数据，拼接两字节数据且转成带符号的类型
 * @param {short} *ax
 * @param {short} *ay
 * @param {short} *az
 * @return {*}
 */
void Int_MPU6050_Get_Accel(short *ax, short *ay, short *az)
{
    uint8_t buff[6];
    Int_MPU6050_ReadBytes(MPU_ACCEL_XOUTH_REG, buff, 6);

    /*
        buff[0]:加速度X轴高8位
        buff[1]:加速度X轴低8位
        buff[2]:加速度Y轴高8位
        buff[3]:加速度Y轴低8位
        buff[4]:加速度Z轴高8位
        buff[5]:加速度Z轴低8位
     */
    *ax = ((short)buff[0] << 8) | buff[1];
    *ay = ((short)buff[2] << 8) | buff[3];
    *az = ((short)buff[4] << 8) | buff[5];
}
