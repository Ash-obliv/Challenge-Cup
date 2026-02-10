#ifndef __PLATFORM_I2C_H__
#define __PLATFORM_I2C_H__

void platform_i2c_init(void);
void platform_i2c_mpu6050_is_present(void);
void platform_driver_register(void);
void platform_i2c_oled_is_present(void);

#endif /* __PLATFORM_I2C_H__ */
