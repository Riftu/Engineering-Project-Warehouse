/**
 * @file app_tasks.h
 * @brief 应用层任务模块。
 * @details app_tasks.c 存放传感器采集、RFID 读卡、追溯记录、离线存储和健康监测的任务循环。
 *          各硬件模块已经接入业务状态机，并通过队列、信号量和互斥锁协作。
 *          由 freertos.c 的 CubeMX 任务入口调用。
 */
#ifndef APP_TASKS_H
#define APP_TASKS_H

#include <stdint.h>
#include "app_types.h"
#include "app_config.h"

/* W25Q64 离线记录缓存：固定区域最多保存 32 条记录。 */
#define STORAGE_CACHE_BASE_ADDRESS 0x00010000UL
#define STORAGE_CACHE_SLOT_SIZE 4096U
#define STORAGE_CACHE_SLOT_COUNT 32U

typedef enum
{
    APP_TASK_USART1 = 0U,
    APP_TASK_MQTT,
    APP_TASK_SENSOR,
    APP_TASK_RFID,
    APP_TASK_TRACE,
    APP_TASK_STORAGE,
    APP_TASK_HEALTH,
    APP_TASK_COUNT
} AppTaskId_t;

/** 任务循环报到，用于软件看门狗记录最后运行时间。 */
void App_Heartbeat(AppTaskId_t taskId);

/** 返回当前软件看门狗检测到的异常任务位图。 */
uint32_t App_HealthCheck(void);

/** 返回任务最近一次报到的位图，供 MQTT 心跳上报。 */
uint32_t App_HeartbeatMask(void);

/** 返回启动以来检测到的系统故障报警位（例如 IWDG 复位）。 */
uint16_t App_SystemAlarmFlags(void);

/**
 * 互斥读取 DS3231 并输出标准审计时间。成功返回 1；失败时输出空字符串。
 * 所有任务必须通过该接口取时间，不允许直接并发访问 I2C1。
 */
uint8_t App_ReadTimestamp(char timestamp[20]);

void AppTask_RunUsart1(void *argument);
void AppTask_RunSensor(void *argument);
void AppTask_RunRfid(void *argument);
void AppTask_RunTrace(void *argument);
void AppTask_RunStorage(void *argument);
void AppTask_RunHealth(void *argument);

/** 初始化离线缓存并扫描上次断电后仍未上传的记录。 */
void StorageCache_Init(void);

/** 取出一条尚未上传的 Flash 记录；没有记录时返回 0。 */
uint8_t StorageCache_GetPending(StorageRecord_t *record);

/** MQTT 发布成功后，按 sequence 将对应 Flash 槽位标记为已上传。 */
uint8_t StorageCache_MarkSent(uint32_t sequence);

/** 将一条新记录追加到 W25Q64 离线缓存。 */
uint8_t StorageCache_Save(const StorageRecord_t *record);

/** 启动时通过 USART1 输出 DS3231 当前真实时间。 */
void AppTask_TestDs3231Once(void);

#endif /* APP_TASKS_H */
