/**
 * @file app_types.h
 * @brief 任务间公共数据类型模块。
 * @details app_types.c/.h 定义 RFID 刷卡事件、传感器采集结果、Flash 离线记录等结构体。
 *          这些结构体分别作为 RfidEventQueue、SensorDataQueue、StorageQueue 的队列元素。
 *          不直接操作任何引脚或硬件模块。
 */
#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>

/* 追溯记录中的报警位，可同时出现多个报警。 */
#define APP_ALARM_LOW_STOCK       0x0001U
#define APP_ALARM_TEMPERATURE_LOW 0x0002U
#define APP_ALARM_TEMPERATURE_HIGH 0x0004U
#define APP_ALARM_HUMIDITY_LOW    0x0008U
#define APP_ALARM_HUMIDITY_HIGH   0x0010U
#define APP_ALARM_TASK_HEARTBEAT  0x0020U
#define APP_ALARM_IWDG_RESET      0x0040U

typedef enum
{
    DEVICE_COMMAND_NONE = 0U,
    DEVICE_COMMAND_OPEN_DOOR,
    DEVICE_COMMAND_CLOSE_DOOR
} DeviceCommand_t;

/* 一次 RFID 刷卡事件 */
typedef struct
{
    char cardId[16];       /* 卡号字符串 */
    uint8_t cardType;      /* 1=操作员卡，2=试剂卡 */
    char timestamp[20];    /* DS3231 提供的 YYYY-MM-DD HH:MM:SS */
} RfidEvent_t;

/* 一次传感器采集结果 */
typedef struct
{
    float temperature;     /* DHT11 温度 */
    float humidity;        /* DHT11 湿度 */
    float weight;          /* HX711 本次采样重量，由业务状态决定是领用前或领用后 */
    uint16_t distanceCm;   /* HC-SR04 距离 */
    uint8_t bottlePresent; /* 超声波判断是否放入试剂瓶 */
    uint16_t alarmFlags;   /* 温湿度等传感器报警位 */
    char timestamp[20];    /* DS3231 提供的 YYYY-MM-DD HH:MM:SS */
} SensorData_t;

/* 需要写入 W25QXX 的离线记录 */
typedef struct
{
    SensorData_t sensorData;
    uint32_t sequence;     /* 数据序号 */
    char operatorId[16];   /* 操作员卡 UID */
    char reagentId[16];    /* 试剂瓶卡 UID */
    float weightBefore;    /* 领用前的试剂瓶重量，单位 g */
    float weightAfter;     /* 领用后放回时的试剂瓶重量，单位 g */
    float weightDelta;     /* 取用量：领用前重量 - 领用后重量，单位 g */
    uint16_t alarmFlags;   /* 完整记录报警位，包括低库存 */
} StorageRecord_t;

#endif /* APP_TYPES_H */

