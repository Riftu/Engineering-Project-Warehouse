/**
 * @file app_tasks.c
 * @brief 实验室试剂追溯系统的 FreeRTOS 任务与第一版业务流程。
 * @details RfidTask 产生刷卡事件，TraceTask 驱动业务状态机，SensorTask 按通知
 *          采集传感器快照，StorageTask 将完整记录写入 W25Q64。
 */
#include "app_tasks.h"
#include "app_types.h"
#include "app_config.h"
#include "cmsis_os2.h"
#include "bsp_KeyLedBuzzer.h"
#include "bsp_dht11.h"
#include "bsp_ds3231.h"
#include "bsp_hc_sr04.h"
#include "bsp_hx711.h"
#include "bsp_rc522.h"
#include "bsp_servo.h"
#include "bsp_w25qxx.h"
#include "main.h"
#include "iwdg.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>

#define BOTTLE_DETECT_DISTANCE_CM 15U
#define BOTTLE_CONFIRM_COUNT 3U
#define WEIGHT_CHANGE_DEADBAND_G 1.0f
#define SCALE_SETTLE_TIME_MS 800U
#define WEIGHT_READ_RETRY_COUNT 3U
#define SENSOR_RESULT_TIMEOUT_MS 7000U
#define STORAGE_CACHE_VALID_MARK 0xA5U
#define STORAGE_CACHE_SENT_MARK 0x00U
#define STORAGE_CACHE_FORMAT_VERSION 2U

extern osMessageQueueId_t RfidEventQueueHandle;
extern osMessageQueueId_t SensorDataQueueHandle;
extern osMessageQueueId_t StorageQueueHandle;
extern osMessageQueueId_t MqttPublishQueueHandle;
extern osMessageQueueId_t DeviceCommandQueueHandle;
extern osMessageQueueId_t DeviceCommandResultQueueHandle;
extern osMutexId_t SpiMutexHandle;
extern osMutexId_t RtcMutexHandle;
extern osSemaphoreId_t SensorReadySemHandle;

static void App_Print(const char *text)
{
    /* 串口助手按 CRLF 换行；必须一次完整发送，不能拆成两次。 */
    HAL_UART_Transmit(&huart1, (uint8_t *)text, (uint16_t)strlen(text), 1000U);
}

typedef struct
{
    uint8_t marker;
    uint8_t version;
    uint8_t reserved[2];
    StorageRecord_t record;
} StorageCacheSlot_t;

static uint8_t storageCacheReady;
static uint8_t storageCacheScanned;
static uint32_t storageCacheNextSlot;
static uint32_t storageCacheScanSlot;
static volatile uint32_t taskHeartbeatTick[APP_TASK_COUNT];
static volatile uint32_t taskHeartbeatSeenMask;
static volatile uint16_t systemAlarmFlags;
static uint8_t rtcInitialized;

void App_Heartbeat(AppTaskId_t taskId)
{
    if ((uint32_t)taskId < (uint32_t)APP_TASK_COUNT)
    {
        taskHeartbeatTick[(uint32_t)taskId] = HAL_GetTick();
        taskHeartbeatSeenMask |= (1UL << (uint32_t)taskId);
    }
}

uint32_t App_HeartbeatMask(void)
{
    return taskHeartbeatSeenMask;
}

uint16_t App_SystemAlarmFlags(void)
{
    return systemAlarmFlags;
}

uint32_t App_HealthCheck(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t failedMask = 0U;
    uint32_t task;

    if (now < APP_HEARTBEAT_STARTUP_GRACE_MS)
        return 0U;

    for (task = 0U; task < (uint32_t)APP_TASK_HEALTH; ++task)
    {
        if (((taskHeartbeatSeenMask & (1UL << task)) == 0U) ||
            ((uint32_t)(now - taskHeartbeatTick[task]) > APP_HEARTBEAT_TIMEOUT_MS))
        {
            failedMask |= (1UL << task);
        }
    }
    return failedMask;
}

static uint16_t App_EvaluateEnvironmentAlarms(float temperature, float humidity)
{
    uint16_t alarms = 0U;

    if (temperature < APP_TEMPERATURE_MIN_C)
        alarms |= APP_ALARM_TEMPERATURE_LOW;
    else if (temperature > APP_TEMPERATURE_MAX_C)
        alarms |= APP_ALARM_TEMPERATURE_HIGH;

    if (humidity < APP_HUMIDITY_MIN_PERCENT)
        alarms |= APP_ALARM_HUMIDITY_LOW;
    else if (humidity > APP_HUMIDITY_MAX_PERCENT)
        alarms |= APP_ALARM_HUMIDITY_HIGH;

    return alarms;
}

static uint32_t StorageCache_Address(uint32_t slot)
{
    return STORAGE_CACHE_BASE_ADDRESS + slot * STORAGE_CACHE_SLOT_SIZE;
}

void StorageCache_Init(void)
{
    StorageCacheSlot_t slotData;
    uint32_t slot;

    storageCacheReady = 0U;
    storageCacheScanned = 0U;
    storageCacheNextSlot = 0U;
    storageCacheScanSlot = 0U;

    (void)osMutexAcquire(SpiMutexHandle, osWaitForever);
    W25QXX_Init();
    for (slot = 0U; slot < STORAGE_CACHE_SLOT_COUNT; ++slot)
    {
        memset(&slotData, 0, sizeof(slotData));
        (void)W25QXX_Read(StorageCache_Address(slot),
                           (uint8_t *)&slotData, sizeof(slotData));
        if ((slotData.marker == STORAGE_CACHE_VALID_MARK) &&
            (slotData.version == STORAGE_CACHE_FORMAT_VERSION) &&
            (slotData.record.sequence != 0U))
        {
            storageCacheNextSlot = slot + 1U;
        }
    }
    (void)osMutexRelease(SpiMutexHandle);

    if (storageCacheNextSlot >= STORAGE_CACHE_SLOT_COUNT)
        storageCacheNextSlot = 0U;
    storageCacheReady = 1U;
}

uint8_t StorageCache_Save(const StorageRecord_t *record)
{
    StorageCacheSlot_t slotData;
    uint32_t address;

    if ((storageCacheReady == 0U) || (record == NULL))
        return 0U;

    address = StorageCache_Address(storageCacheNextSlot);
    (void)osMutexAcquire(SpiMutexHandle, osWaitForever);
    /* 每个槽位独占一个4KB扇区，只擦即将覆盖的槽位。 */
    if (W25QXX_EraseSector(address) == 0U)
    {
        (void)osMutexRelease(SpiMutexHandle);
        return 0U;
    }
    memset(&slotData, 0xFF, sizeof(slotData));
    slotData.marker = STORAGE_CACHE_VALID_MARK;
    slotData.version = STORAGE_CACHE_FORMAT_VERSION;
    slotData.record = *record;
    if (W25QXX_PageProgram(address, (const uint8_t *)&slotData,
                           (uint16_t)sizeof(slotData)) == 0U)
    {
        (void)osMutexRelease(SpiMutexHandle);
        return 0U;
    }
    (void)osMutexRelease(SpiMutexHandle);

    storageCacheNextSlot = (storageCacheNextSlot + 1U) % STORAGE_CACHE_SLOT_COUNT;
    /* 新记录到达后允许 MQTT 任务重新扫描 Flash。 */
    storageCacheScanSlot = 0U;
    storageCacheScanned = 0U;
    return 1U;
}

uint8_t StorageCache_GetPending(StorageRecord_t *record)
{
    StorageCacheSlot_t slotData;
    uint32_t slot;

    if ((storageCacheReady == 0U) || (record == NULL) ||
        (storageCacheScanned != 0U))
        return 0U;

    (void)osMutexAcquire(SpiMutexHandle, osWaitForever);
    for (slot = storageCacheScanSlot; slot < STORAGE_CACHE_SLOT_COUNT; ++slot)
    {
        memset(&slotData, 0, sizeof(slotData));
        (void)W25QXX_Read(StorageCache_Address(slot),
                          (uint8_t *)&slotData, sizeof(slotData));
        if ((slotData.marker == STORAGE_CACHE_VALID_MARK) &&
            (slotData.version == STORAGE_CACHE_FORMAT_VERSION) &&
            (slotData.record.sequence != 0U))
        {
            *record = slotData.record;
            storageCacheScanSlot = slot + 1U;
            (void)osMutexRelease(SpiMutexHandle);
            return 1U;
        }
    }
    (void)osMutexRelease(SpiMutexHandle);
    storageCacheScanned = 1U;
    return 0U;
}

uint8_t StorageCache_MarkSent(uint32_t sequence)
{
    StorageCacheSlot_t slotData;
    uint32_t slot;
    uint8_t marker = STORAGE_CACHE_SENT_MARK;

    if ((storageCacheReady == 0U) || (sequence == 0U))
        return 0U;
    (void)osMutexAcquire(SpiMutexHandle, osWaitForever);
    for (slot = 0U; slot < STORAGE_CACHE_SLOT_COUNT; ++slot)
    {
        memset(&slotData, 0, sizeof(slotData));
        (void)W25QXX_Read(StorageCache_Address(slot),
                          (uint8_t *)&slotData, sizeof(slotData));
        if ((slotData.marker == STORAGE_CACHE_VALID_MARK) &&
            (slotData.version == STORAGE_CACHE_FORMAT_VERSION) &&
            (slotData.record.sequence == sequence))
        {
            if (W25QXX_PageProgram(StorageCache_Address(slot), &marker, 1U) != 0U)
            {
                (void)osMutexRelease(SpiMutexHandle);
                return 1U;
            }
            break;
        }
    }
    (void)osMutexRelease(SpiMutexHandle);
    return 0U;
}

static void App_FormatUid(char *text, size_t textSize, const uint8_t uid[4])
{
    (void)snprintf(text, textSize, "%02X%02X%02X%02X", uid[0], uid[1], uid[2], uid[3]);
}

uint8_t App_ReadTimestamp(char timestamp[20])
{
    DS3231_DateTime_t dateTime;
    uint8_t success = 0U;

    if (timestamp == NULL)
    {
        return 0U;
    }
    timestamp[0] = '\0';
    if ((RtcMutexHandle == NULL) ||
        (osMutexAcquire(RtcMutexHandle, 500U) != osOK))
    {
        return 0U;
    }

    if (rtcInitialized == 0U)
    {
        rtcInitialized = DS3231_Init();
    }
    if ((rtcInitialized != 0U) &&
        (DS3231_ReadDateTime(&dateTime) != 0U) &&
        (DS3231_FormatDateTime(&dateTime, timestamp, 20U) != 0U))
    {
        success = 1U;
    }
    else
    {
        /* 下次调用重新初始化，允许 I2C 瞬时故障恢复。 */
        rtcInitialized = 0U;
    }
    (void)osMutexRelease(RtcMutexHandle);
    return success;
}

static void App_PrintRecord(const StorageRecord_t *record)
{
    char message[240];
    int32_t temperature10 = (int32_t)(record->sensorData.temperature * 10.0f);
    int32_t humidity10 = (int32_t)(record->sensorData.humidity * 10.0f);
    int32_t weightBefore10 = (int32_t)(record->weightBefore * 10.0f);
    int32_t weightAfter10 = (int32_t)(record->weightAfter * 10.0f);
    int32_t weightDelta10 = (int32_t)(record->weightDelta * 10.0f);

    (void)snprintf(message, sizeof(message),
                   "[记录] operator=%s reagent=%s time=%s "
                   "before=%ld.%ldg after=%ld.%ldg used=%ld.%ldg "
                   "temperature=%ld.%ldC humidity=%ld.%ld%% distance=%ucm\r\n",
                   record->operatorId, record->reagentId, record->sensorData.timestamp,
                   (long)(weightBefore10 / 10L),
                   (long)(weightBefore10 >= 0 ? weightBefore10 % 10L : -(weightBefore10 % 10L)),
                   (long)(weightAfter10 / 10L),
                   (long)(weightAfter10 >= 0 ? weightAfter10 % 10L : -(weightAfter10 % 10L)),
                   (long)(weightDelta10 / 10L),
                   (long)(weightDelta10 >= 0 ? weightDelta10 % 10L : -(weightDelta10 % 10L)),
                   (long)(temperature10 / 10L), (long)(temperature10 % 10L),
                   (long)(humidity10 / 10L), (long)(humidity10 % 10L),
                   record->sensorData.distanceCm);
    App_Print(message);
}

static void App_PrintWeight(const char *label, float weight)
{
    char message[96];
    int32_t weight10 = (int32_t)(weight * 10.0f);

    (void)snprintf(message, sizeof(message),
                   "[流程] %s: %ld.%ldg\r\n", label,
                   (long)(weight10 / 10L),
                   (long)(weight10 >= 0 ? weight10 % 10L : -(weight10 % 10L)));
    App_Print(message);
}

void AppTask_TestDs3231Once(void)
{
    static uint8_t tested = 0U;
    char timestamp[20];
    char message[80];

    if (tested != 0U) return;
    tested = 1U;
    if (App_ReadTimestamp(timestamp) == 0U)
    {
        App_Print("[故障] DS3231 初始化失败或时间无效\r\n");
        return;
    }
    (void)snprintf(message, sizeof(message),
                   "[系统] DS3231 初始化成功，当前时间: %s\r\n",
                   timestamp);
    App_Print(message);
}

void AppTask_RunUsart1(void *argument)
{
    (void)argument;
    App_Print("\r\n[系统] 试剂追溯第一版启动成功\r\n");
    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
    {
        systemAlarmFlags |= APP_ALARM_IWDG_RESET;
        App_Print("[故障] 检测到 IWDG 看门狗复位\r\n");
    }
    __HAL_RCC_CLEAR_RESET_FLAGS();
    AppTask_TestDs3231Once();
    for (;;)
    {
        App_Heartbeat(APP_TASK_USART1);
        osDelay(1000U);
    }
}

void AppTask_RunSensor(void *argument)
{
    SensorData_t data;
    DHT11_Data_t dht;
    uint32_t distance;
    float weight;
    uint8_t attempt;
    (void)argument;
    HCSR04_Init();
    DHT11_Init();
    HX711_Init();
    App_Print("[系统] 传感器任务初始化成功\r\n");

    for (;;)
    {
        App_Heartbeat(APP_TASK_SENSOR);
        /* 二值信号量：TraceTask 确认试剂卡后才允许采集完整快照。 */
        if (osSemaphoreAcquire(SensorReadySemHandle, 1000U) != osOK)
        {
            continue;
        }
        memset(&data, 0, sizeof(data));
        if (App_ReadTimestamp(data.timestamp) == 0U)
        {
            App_Print("[流程] DS3231 读取失败，本次采集取消\r\n");
            StatusIndicator_Notify(STATUS_INDICATOR_ERROR);
            continue;
        }

        /* 放置或放回瓶子后先等待机械振动衰减，再进行真正的稳定性判断。 */
        osDelay(SCALE_SETTLE_TIME_MS);
        for (attempt = 0U; attempt < WEIGHT_READ_RETRY_COUNT; ++attempt)
        {
            if (HX711_ReadStableWeightGram(&weight) != 0U)
            {
                break;
            }
            App_Print("[流程] HX711 重量异常，请勿触碰称台，正在重新测量\r\n");
            osDelay(200U);
        }
        if (attempt >= WEIGHT_READ_RETRY_COUNT)
        {
            App_Print("[流程] HX711 连续三次读取失败，本次采集取消\r\n");
            continue;
        }
        data.weight = weight;
        App_Print("[流程] HX711 重量稳定读取成功\r\n");
        if (DHT11_Read(&dht) == 0U)
        {
            App_Print("[流程] DHT11 温湿度读取失败\r\n");
            continue;
        }
        data.temperature = (float)dht.temperatureInteger + (float)dht.temperatureDecimal / 10.0f;
        data.humidity = (float)dht.humidityInteger + (float)dht.humidityDecimal / 10.0f;
        data.alarmFlags = App_EvaluateEnvironmentAlarms(data.temperature, data.humidity);
        if (data.alarmFlags != 0U)
        {
            App_Print("[报警] 温湿度超过保存范围\r\n");
            StatusIndicator_Notify(STATUS_INDICATOR_ERROR);
        }
        App_Print("[流程] DHT11 温湿度读取成功\r\n");
        if (HCSR04_ReadMedianDistanceCm(&distance) == 0U)
        {
            App_Print("[流程] HC-SR04 距离复核失败\r\n");
            continue;
        }
        data.distanceCm = (uint16_t)distance;
        data.bottlePresent = (distance <= BOTTLE_DETECT_DISTANCE_CM) ? 1U : 0U;
        /* 消息队列：将完整传感器快照安全交给 TraceTask。 */
        if (osMessageQueuePut(SensorDataQueueHandle, &data, 0U, 0U) == osOK)
            App_Print("[流程] 传感器数据入队成功\r\n");
    }
}

void AppTask_RunRfid(void *argument)
{
    uint8_t uid[4], lastUid[4] = {0U};
    uint8_t hasLastUid = 0U;
    RfidEvent_t event;
    RC522_CardType_t cardType;
    RC522_Status_t status;
    (void)argument;
    /* SPI 互斥锁：统一保护 RC522/W25Q64 的 SPI 访问。 */
    (void)osMutexAcquire(SpiMutexHandle, osWaitForever);
    status = (RC522_Init() != 0U) ? RC522_STATUS_OK : RC522_STATUS_ERROR;
    (void)osMutexRelease(SpiMutexHandle);
    App_Print((status == RC522_STATUS_OK) ? "[系统] RC522 初始化成功\r\n" : "[系统] RC522 初始化失败\r\n");

    for (;;)
    {
        App_Heartbeat(APP_TASK_RFID);
        if (osMutexAcquire(SpiMutexHandle, 100U) == osOK)
        {
            status = RC522_ReadCard(uid, &cardType);
            (void)osMutexRelease(SpiMutexHandle);
        }
        else status = RC522_STATUS_ERROR;
        if (status == RC522_STATUS_NO_CARD) hasLastUid = 0U;
        else if ((status == RC522_STATUS_OK) && ((hasLastUid == 0U) || (memcmp(uid, lastUid, 4U) != 0)))
        {
            memcpy(lastUid, uid, 4U);
            hasLastUid = 1U;
            memset(&event, 0, sizeof(event));
            App_FormatUid(event.cardId, sizeof(event.cardId), uid);
            event.cardType = (uint8_t)cardType;
            if (App_ReadTimestamp(event.timestamp) == 0U)
            {
                App_Print("[流程] DS3231 读取失败，本次刷卡事件忽略\r\n");
                osDelay(100U);
                continue;
            }
            /* RFID 事件队列：读卡任务不直接控制舵机或业务状态。 */
            if (osMessageQueuePut(RfidEventQueueHandle, &event, 0U, 0U) == osOK)
            {
                App_Print((cardType == RC522_CARD_OPERATOR) ? "[流程] 操作员卡识别成功\r\n" :
                          (cardType == RC522_CARD_REAGENT) ? "[流程] 试剂瓶卡识别成功\r\n" :
                          "[流程] 未登记卡已忽略\r\n");
            }
        }
        osDelay(100U);
    }
}

void AppTask_RunTrace(void *argument)
{
    typedef enum
    {
        TRACE_WAIT_OPERATOR = 0,
        TRACE_WAIT_BOTTLE_BEFORE,
        TRACE_WAIT_REAGENT_BEFORE,
        TRACE_WAIT_SENSOR_BEFORE,
        TRACE_WAIT_BOTTLE_REMOVED,
        TRACE_WAIT_BOTTLE_RETURNED,
        TRACE_WAIT_REAGENT_AFTER,
        TRACE_WAIT_SENSOR_AFTER
    } TraceState_t;
    TraceState_t state = TRACE_WAIT_OPERATOR;
    RfidEvent_t event;
    SensorData_t sensor;
    StorageRecord_t record;
    char operatorId[16] = {0};
    char reagentId[16] = {0};
    float weightBefore = 0.0f;
    uint32_t distance;
    uint8_t detectedCount = 0U;
    DeviceCommand_t command;
    (void)argument;
    if (Servo_Init() != 0U) { Servo_Close(); App_Print("[系统] 舵机初始化成功\r\n"); }
    App_Print("[流程] 等待操作员刷员工卡\r\n");

    for (;;)
    {
        App_Heartbeat(APP_TASK_TRACE);
        /* 控制命令队列：舵机始终由 TraceTask 操作，避免跨任务竞争。 */
        if (osMessageQueueGet(DeviceCommandQueueHandle, &command, NULL, 0U) == osOK)
        {
            if (command == DEVICE_COMMAND_OPEN_DOOR)
            {
                Servo_Open();
                App_Print("[控制] MQTT 远程开门执行成功\r\n");
                (void)osMessageQueuePut(DeviceCommandResultQueueHandle,
                                        &command, 0U, 0U);
            }
            else if (command == DEVICE_COMMAND_CLOSE_DOOR)
            {
                Servo_Close();
                App_Print("[控制] MQTT 远程关门执行成功\r\n");
                (void)osMessageQueuePut(DeviceCommandResultQueueHandle,
                                        &command, 0U, 0U);
            }
        }

        if (state == TRACE_WAIT_OPERATOR)
        {
            if ((osMessageQueueGet(RfidEventQueueHandle, &event, NULL, 100U) == osOK) &&
                (event.cardType == (uint8_t)RC522_CARD_OPERATOR))
            {
                strcpy(operatorId, event.cardId);
                Servo_Open();
                App_Print("[流程] 操作员身份匹配成功，舵机开门成功\r\n");
                App_Print("[流程] 请将试剂瓶放到称重台\r\n");
                detectedCount = 0U;
                state = TRACE_WAIT_BOTTLE_BEFORE;
            }
        }
        else if (state == TRACE_WAIT_BOTTLE_BEFORE)
        {
            if ((HCSR04_ReadMedianDistanceCm(&distance) != 0U) && (distance <= BOTTLE_DETECT_DISTANCE_CM)) ++detectedCount;
            else detectedCount = 0U;
            if (detectedCount >= BOTTLE_CONFIRM_COUNT)
            {
                App_Print("[流程] 已检测到试剂瓶，请刷试剂瓶卡\r\n");
                state = TRACE_WAIT_REAGENT_BEFORE;
            }
            osDelay(100U);
        }
        else if (state == TRACE_WAIT_REAGENT_BEFORE)
        {
            if ((osMessageQueueGet(RfidEventQueueHandle, &event, NULL, 100U) == osOK) &&
                (event.cardType == (uint8_t)RC522_CARD_REAGENT))
            {
                strcpy(reagentId, event.cardId);
                App_Print("[流程] 试剂瓶身份确认成功，开始领用前称重\r\n");
                /* 第一次释放信号量：请求 SensorTask 采集领用前快照。 */
                (void)osSemaphoreRelease(SensorReadySemHandle);
                state = TRACE_WAIT_SENSOR_BEFORE;
            }
        }
        else if (state == TRACE_WAIT_SENSOR_BEFORE)
        {
            App_Heartbeat(APP_TASK_TRACE);
            if (osMessageQueueGet(SensorDataQueueHandle, &sensor, NULL,
                                  SENSOR_RESULT_TIMEOUT_MS) != osOK)
            {
                App_Print("[流程] 领用前称重超时，请重新刷试剂瓶卡\r\n");
                state = TRACE_WAIT_REAGENT_BEFORE;
            }
            else if (sensor.bottlePresent == 0U)
            {
                App_Print("[流程] 领用前复核未检测到试剂瓶，请重新放置\r\n");
                detectedCount = 0U;
                state = TRACE_WAIT_BOTTLE_BEFORE;
            }
            else
            {
                weightBefore = sensor.weight;
                App_PrintWeight("weight before", weightBefore);
                App_Print("[流程] 领用前重量记录成功，请取走试剂瓶并使用\r\n");
                detectedCount = 0U;
                state = TRACE_WAIT_BOTTLE_REMOVED;
            }
        }
        else if (state == TRACE_WAIT_BOTTLE_REMOVED)
        {
            /* 无瓶时可能测到远处背景，也可能没有回波而超时，两者都表示瓶子已取走。 */
            if ((HCSR04_ReadMedianDistanceCm(&distance) == 0U) ||
                (distance > BOTTLE_DETECT_DISTANCE_CM))
                ++detectedCount;
            else
                detectedCount = 0U;

            if (detectedCount >= BOTTLE_CONFIRM_COUNT)
            {
                App_Print("[流程] 已检测到试剂瓶取走，使用后请放回称重台\r\n");
                detectedCount = 0U;
                state = TRACE_WAIT_BOTTLE_RETURNED;
            }
            osDelay(100U);
        }
        else if (state == TRACE_WAIT_BOTTLE_RETURNED)
        {
            if ((HCSR04_ReadMedianDistanceCm(&distance) != 0U) &&
                (distance <= BOTTLE_DETECT_DISTANCE_CM))
                ++detectedCount;
            else
                detectedCount = 0U;

            if (detectedCount >= BOTTLE_CONFIRM_COUNT)
            {
                App_Print("[流程] 已检测到试剂瓶放回，请再次刷同一张试剂瓶卡\r\n");
                state = TRACE_WAIT_REAGENT_AFTER;
            }
            osDelay(100U);
        }
        else if (state == TRACE_WAIT_REAGENT_AFTER)
        {
            if ((osMessageQueueGet(RfidEventQueueHandle, &event, NULL, 100U) == osOK) &&
                (event.cardType == (uint8_t)RC522_CARD_REAGENT))
            {
                if (strcmp(event.cardId, reagentId) != 0)
                {
                    App_Print("[流程] 试剂瓶卡不一致，请放回原试剂瓶并重新刷卡\r\n");
                }
                else
                {
                    App_Print("[流程] 试剂瓶复核成功，开始领用后称重\r\n");
                    /* 第二次释放信号量：请求 SensorTask 采集领用后快照。 */
                    (void)osSemaphoreRelease(SensorReadySemHandle);
                    state = TRACE_WAIT_SENSOR_AFTER;
                }
            }
        }
        else if (state == TRACE_WAIT_SENSOR_AFTER)
        {
            App_Heartbeat(APP_TASK_TRACE);
            if (osMessageQueueGet(SensorDataQueueHandle, &sensor, NULL,
                                  SENSOR_RESULT_TIMEOUT_MS) != osOK)
            {
                App_Print("[流程] 领用后称重超时，请重新刷试剂瓶卡\r\n");
                state = TRACE_WAIT_REAGENT_AFTER;
            }
            else if (sensor.bottlePresent == 0U)
            {
                App_Print("[流程] 领用后复核未检测到试剂瓶，请重新放回\r\n");
                detectedCount = 0U;
                state = TRACE_WAIT_BOTTLE_RETURNED;
            }
            else
            {
                memset(&record, 0, sizeof(record));
                record.sensorData = sensor;
                record.weightBefore = weightBefore;
                record.weightAfter = sensor.weight;
                record.weightDelta = record.weightBefore - record.weightAfter;
                record.alarmFlags = sensor.alarmFlags;
                if (record.weightAfter < APP_LOW_STOCK_THRESHOLD_G)
                {
                    record.alarmFlags |= APP_ALARM_LOW_STOCK;
                    App_Print("[报警] 试剂剩余量低于库存阈值\r\n");
                    StatusIndicator_Notify(STATUS_INDICATOR_ERROR);
                }
                if ((record.weightDelta > -WEIGHT_CHANGE_DEADBAND_G) &&
                    (record.weightDelta < WEIGHT_CHANGE_DEADBAND_G))
                    record.weightDelta = 0.0f;
                App_PrintWeight("weight after", record.weightAfter);
                App_PrintWeight("weight used", record.weightDelta);
                record.sequence = HAL_GetTick();
                strcpy(record.operatorId, operatorId);
                strcpy(record.reagentId, reagentId);
                App_PrintRecord(&record);

                /* 存储队列：TraceTask 不等待 Flash 擦写。 */
                if (osMessageQueuePut(StorageQueueHandle, &record, 0U, 0U) == osOK)
                {
                    App_Print("[流程] 完整领用记录生成成功\r\n");
                }
                else
                {
                    App_Print("[流程] W25Q64 存储队列已满，记录未提交\r\n");
                }

                Servo_Close();
                App_Print("[流程] 舵机关门成功，本次领用流程完成\r\n");
                App_Print("[流程] 等待下一位操作员刷员工卡\r\n");
                state = TRACE_WAIT_OPERATOR;
            }
        }
    }
}

void AppTask_RunStorage(void *argument)
{
    StorageRecord_t record;
    (void)argument;
    StorageCache_Init();
    App_Print("[系统] W25Q64 存储任务初始化成功\r\n");
    for (;;)
    {
        App_Heartbeat(APP_TASK_STORAGE);
        if (osMessageQueueGet(StorageQueueHandle, &record, NULL, 1000U) == osOK)
        {
            if (StorageCache_Save(&record) != 0U)
            {
                App_Print("[流程] W25Q64 离线记录保存成功\r\n");
                /* 必须先落盘再上传，断电时记录才不会停留在 RAM。 */
                if (osMessageQueuePut(MqttPublishQueueHandle, &record, 0U, 0U) == osOK)
                    App_Print("[流程] MQTT 上传记录入队成功\r\n");
                else
                    App_Print("[流程] MQTT 队列已满，稍后从 Flash 补发\r\n");
            }
            else
            {
                App_Print("[流程] W25Q64 离线记录保存失败\r\n");
                StatusIndicator_Notify(STATUS_INDICATOR_ERROR);
            }
        }
    }
}

void AppTask_RunHealth(void *argument)
{
    uint32_t lastCheck = 0U;
    uint32_t failedMask = 0U;
    uint32_t lastFailedMask = 0U;
    char message[96];
    (void)argument;
    StatusIndicator_Init();
    for (;;)
    {
        App_Heartbeat(APP_TASK_HEALTH);
        StatusIndicator_Process();

        if ((uint32_t)(HAL_GetTick() - lastCheck) >= APP_HEARTBEAT_CHECK_PERIOD_MS)
        {
            lastCheck = HAL_GetTick();
            failedMask = App_HealthCheck();
            if ((failedMask != 0U) && (failedMask != lastFailedMask))
            {
                (void)snprintf(message, sizeof(message),
                               "[FAULT] software watchdog timeout mask=0x%08lX\r\n",
                               (unsigned long)failedMask);
                App_Print(message);
                StatusIndicator_Notify(STATUS_INDICATOR_ERROR);
            }
            else if ((failedMask == 0U) && (lastFailedMask != 0U))
            {
                App_Print("[恢复] 所有任务心跳恢复正常\r\n");
            }
            lastFailedMask = failedMask;
        }

        /* 任一关键任务持续失去心跳时停止喂狗，由 IWDG 完成整机恢复。 */
        if (failedMask == 0U)
            HAL_IWDG_Refresh(&hiwdg);
        osDelay(20U);
    }
}
