## 项目简介

这是一个基于 ESP32-S3 的物联网示例工程，集成了 MPU6050 传感器、OLED 显示、Wi-Fi 配网与联网流程，并在联网后启动 MQTT 客户端与本地 HTTP 服务。工程以组件化方式组织，适合用作端侧设备的快速原型。

## 主要功能

- I2C 平台初始化与设备驱动注册
- MPU6050 初始化与数据采集（示例已预留）
- OLED 状态显示（传感器、网络连接状态等）
- Wi-Fi AP+STA 配网流程，联网后自动关闭配网 AP
- 网络管理回调，支持 Wi-Fi/4G 传输类型区分
- MQTT 客户端初始化（连接参数在源码内配置）
- 应用任务初始化入口（`app_task_init()`）

## 目录结构

```
.
├─ main/                 # 应用入口与 CMake
│  ├─ 01_project.c       # app_main 逻辑
│  └─ CMakeLists.txt
├─ components/           # 自定义组件
│  ├─ app/               # 任务与业务逻辑
│  ├─ inf/               # 传感器/外设接口
│  ├─ net/               # 网络与协议封装
│  ├─ platform/          # 硬件平台抽象
│  └─ tool/              # 通用工具
├─ CMakeLists.txt
├─ sdkconfig             # 编译配置
└─ build/                # 构建产物
```

## 硬件与外设

- 主控：ESP32-S3
- 传感器：MPU6050（I2C）
- 显示：OLED（I2C）

## 依赖与环境

- ESP-IDF v5.x（建议与本地环境保持一致）
- 已正确设置 `IDF_PATH`

## 快速开始

1. 配置并进入 ESP-IDF 环境
2. 在项目根目录执行：

```bash
idf.py build
idf.py -p <PORT> flash
idf.py -p <PORT> monitor
```

## MQTT 配置说明

MQTT 连接参数位于主程序中，可按需修改：

- Broker：`mqtt://47.92.152.245:1883`
- Client ID：`test/topic`
- 用户名/密码：`esp32` / `esp32`

如需启用 TLS，可在源码中替换为证书指针。

## 运行流程概览

1. I2C 初始化与驱动注册
2. Wi-Fi AP+STA 初始化，用于配网
3. MPU6050 与 OLED 初始化，OLED 显示状态
4. 网络连接后触发回调，显示网络类型与 IP
5. 启动 MQTT 客户端并停止配网 AP
6. 启动应用任务

## 备注

- 传感器检测函数目前为示例注释，可按需要开启。
- 网络接入方式由 `net_manager` 决定，当前支持 Wi-Fi/4G 的区分显示。
