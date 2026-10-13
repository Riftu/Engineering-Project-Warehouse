/**
 * @file app_config.h
 * @brief 应用全局配置模块。
 * @details app_config.c/.h 将统一存放设备 ID、MQTT Broker 地址、发布订阅主题、
 *          离线缓存数量、超时值等不随任务变化的配置，避免散落在各个 .c 文件中。
 *          库存和环境报警阈值、任务心跳监测周期等统一配置在这里。
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* 设备身份和环境报警阈值。重量单位为克，温度单位为摄氏度。 */
#define APP_DEVICE_ID "stm32_001"
#define APP_LOW_STOCK_THRESHOLD_G 50.0f
#define APP_TEMPERATURE_MIN_C 0.0f
#define APP_TEMPERATURE_MAX_C 30.0f
#define APP_HUMIDITY_MIN_PERCENT 20.0f
#define APP_HUMIDITY_MAX_PERCENT 80.0f

/* 软件看门狗：HealthTask 超过该时间未收到任务心跳就记录故障。 */
#define APP_HEARTBEAT_CHECK_PERIOD_MS 1000U
#define APP_HEARTBEAT_TIMEOUT_MS 15000U
#define APP_HEARTBEAT_STARTUP_GRACE_MS 10000U

#endif /* APP_CONFIG_H */
