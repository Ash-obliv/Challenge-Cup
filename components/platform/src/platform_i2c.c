#include "platform_i2c.h"
#include "driver/i2c_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include <stdbool.h>
#include "mpu6050.h"
#include "OLED.h"

static const char *TAG = "platform_i2c";

#define I2C_PORT I2C_NUM_0
#define I2C_SCL_PIN GPIO_NUM_41
#define I2C_SDA_PIN GPIO_NUM_42

#define DEV_MPU_ADDR 0x68 // MPU6050的I2C地址
#define MPU6050_WHO_AM_I_REG 0x75

#define DEV_OLED_ADDR 0x3C // OLED的I2C地址

static i2c_master_bus_handle_t bus_handle;      // 这个是I2C总线的句柄
static i2c_master_dev_handle_t mpu_dev_handle;  // 这个是MPU6050设备的句柄
static i2c_master_dev_handle_t oled_dev_handle; // 这个是OLED设备的句柄

void platform_i2c_init(void)
{
    // 1.完善总线的配置结构体
    i2c_master_bus_config_t i2c_bus_config = {0};
    i2c_bus_config.clk_source = I2C_CLK_SRC_DEFAULT; // 时钟源
    i2c_bus_config.i2c_port = I2C_PORT;              // I2C端口号
    i2c_bus_config.scl_io_num = I2C_SCL_PIN;         // SCL引脚
    i2c_bus_config.sda_io_num = I2C_SDA_PIN;         // SDA引脚
    i2c_bus_config.glitch_ignore_cnt = 7;
    i2c_bus_config.flags.enable_internal_pullup = true;

    // 2.将配置结构体传递给I2C驱动，创建I2C总线
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &bus_handle));

    // 3. 创建一个设备的配置
    i2c_device_config_t dev_config = {0};
    dev_config.device_address = DEV_MPU_ADDR;
    dev_config.scl_speed_hz = 100000;            // 100KHz
    dev_config.dev_addr_length = I2C_ADDR_BIT_7; // 7位地址

    // 4. 配置完成后 创建设备句柄
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &mpu_dev_handle));

    // 5. 在总线添加OLED设备
    dev_config.device_address = DEV_OLED_ADDR;
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_config, &oled_dev_handle));
}

void platform_i2c_mpu6050_is_present(void)
{
    uint8_t reg = MPU6050_WHO_AM_I_REG;
    uint8_t whoami = 0;

    esp_err_t err = i2c_master_transmit_receive(mpu_dev_handle,
                                                &reg, 1,
                                                &whoami, 1,
                                                1000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "MPU6050 not detected");
        return;
    }

    ESP_LOGI(TAG, "MPU6050 WHO_AM_I: 0x%02X", whoami);
}

void platform_i2c_oled_is_present(void)
{
    uint8_t test_data = 0x00;
    esp_err_t err = i2c_master_transmit(oled_dev_handle, &test_data, 1, 1000);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "OLED not detected");
        return ;
    }

    ESP_LOGI(TAG, "OLED detected successfully");
}

/***********************************************************  MPU6050驱动 *********************************************************/
void Int_MPU6050_WriteByteFunc(uint8_t reg_addr, uint8_t send_byte)
{
    uint8_t buff[2] = {reg_addr, send_byte};
    i2c_master_transmit(mpu_dev_handle, buff, 2, 1000);
}

void Int_MPU6050_ReadByteFunc(uint8_t reg_addr, uint8_t *receive_byte)
{
    i2c_master_transmit_receive(mpu_dev_handle, &reg_addr, 1, receive_byte, 1, 1000);
}

void Int_MPU6050_ReadBytesFunc(uint8_t reg_addr, uint8_t *receive_buff, uint8_t size)
{
    i2c_master_transmit_receive(mpu_dev_handle, &reg_addr, 1, receive_buff, size, 1000);
}

void Int_MPU6050_DelayMsFunc(uint32_t ms)
{
    esp_rom_delay_us(ms * 1000);
}
/***********************************************************  MPU6050驱动 *********************************************************/

/***********************************************************  OLED驱动 *********************************************************/
void OLED_WriteCommandFunc(uint8_t Command)
{
    uint8_t buff[2] = {0x00, Command};
    esp_err_t ret = i2c_master_transmit(oled_dev_handle, buff, 2, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE("I2C_WRITE", "Failed to add device: %s", esp_err_to_name(ret));
    }
}

// IIC OLED写入多个字节的数据
void OLED_WriteDataFunc(uint8_t *Data, uint8_t Count)
{
    uint8_t buff[Count + 1];
    buff[0] = 0x40;
    memcpy(&buff[1], Data, Count);
    esp_err_t ret = i2c_master_transmit(oled_dev_handle, buff, Count + 1, 100);
    if (ret != ESP_OK)
    {
        ESP_LOGE("I2C_WRITE", "Failed to add device: %s", esp_err_to_name(ret));
    }
}
/***********************************************************  OLED驱动 *********************************************************/

void platform_driver_register(void)
{
    MPU6050_RegisterDriver(Int_MPU6050_ReadByteFunc, Int_MPU6050_ReadBytesFunc,
                           Int_MPU6050_WriteByteFunc, Int_MPU6050_DelayMsFunc);
    OLED_RegisterDriver(OLED_WriteCommandFunc, OLED_WriteDataFunc);
}
