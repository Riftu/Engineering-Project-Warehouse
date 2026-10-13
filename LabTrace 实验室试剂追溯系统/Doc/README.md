# LabTrace 实验室试剂追溯系统

基于 STM32F103、ESP8266 与 Qt 的实验室试剂数据采集和追溯工程。系统包含嵌入式设备固件、Qt 服务端及客户端，服务端通过 MQTT 接收设备数据、写入 SQLite，并通过 TCP 向客户端提供实时数据、控制和历史查询。

## 目录

- `../Core/stm32_001/`：STM32F103C8T6 固件工程，使用 STM32CubeMX、HAL 和 FreeRTOS。
- `../Core/server/`：Qt 服务端，使用 MQTT、TCP、WebSocket 和 SQL 模块。
- `../Core/client/`：Qt 桌面客户端，使用 MQTT、TCP 与 Charts 模块。
- `三方通信协议说明.md`：设备、服务端和客户端之间的数据流、主题及消息格式。
- `pin_plan_final.html`：硬件引脚规划资料。

## 通信流程

```text
STM32 + ESP8266 --MQTT--> Broker --MQTT--> Qt 服务端 --TCP--> Qt 客户端
                                      服务端使用 SQLite 保存遥测数据
```

服务端和设备的 MQTT Broker、客户端连接的服务端 IP/端口应按实际部署环境配置。协议说明中的公共 Broker 与测试地址仅用于开发测试，不适合生产环境。

## 构建入口

- 固件：使用 Keil MDK 打开 `../Core/stm32_001/MDK-ARM/` 下的 `.uvprojx` 工程；通过 `.ioc` 在 CubeMX 中查看配置。目标 MCU 为 STM32F103C8T6，系统时钟配置为 72 MHz。
- 服务端：使用 Qt Creator 打开 `../Core/server/server.pro`。
- 客户端：使用 Qt Creator 打开 `../Core/client/projectclient.pro`。

Qt 工程依赖 MQTT、SQL、Network 等模块，并需要匹配的 Qt 开发环境。设备端还需要配置 Wi-Fi、MQTT Broker 和板级外设连接。

## 文档

- [三方通信协议说明](三方通信协议说明.md)
- [硬件引脚规划](pin_plan_final.html)

## 文档索引

- [pin_plan_final.html](pin_plan_final.html)
- [三方通信协议说明.md](%E4%B8%89%E6%96%B9%E9%80%9A%E4%BF%A1%E5%8D%8F%E8%AE%AE%E8%AF%B4%E6%98%8E.md)
