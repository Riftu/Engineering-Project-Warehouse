/**
 * @file bsp_ds3231.h
 * @brief DS3231 实时时钟驱动模块。
 * @details bsp_ds3231.c 通过 I2C 读取和设置日期时间，为追溯记录提供真实时间戳。
 *          使用 I2C1：PB6 为 SCL，PB7 为 SDA；模块 VCC、GND 分别接电源和共地。
 *          需要确认模块或外接电路提供 I2C 上拉电阻。
 */
#ifndef BSP_DS3231_H
#define BSP_DS3231_H

#include <stdint.h>
#include <stddef.h>

/* DS3231 的 7 位 I2C 地址为 0x68，HAL 接口需要左移一位。 */
#define DS3231_I2C_ADDRESS (0x68U << 1U)

/** DS3231 中保存的日历时间，年份范围为 2000~2099。 */
typedef struct
{
    uint8_t seconds;
    uint8_t minutes;
    uint8_t hours;
    uint8_t day;
    uint8_t date;
    uint8_t month;
    uint8_t year; /* 例如 26 表示 2026 年 */
} DS3231_DateTime_t;

/**
 * 初始化并验证 RTC：设备必须应答、日历寄存器必须合法，且 OSF 标志未置位。
 * OSF=1 表示振荡器曾停止，当前时间不具备审计可信度，需要重新设置时间。
 */
uint8_t DS3231_Init(void);

/** 检查 DS3231 是否能在 I2C1(PB6/PB7) 上应答。 */
uint8_t DS3231_IsReady(void);

/** 从寄存器 0x00 读取当前日期和时间；返回 1 表示成功。 */
uint8_t DS3231_ReadDateTime(DS3231_DateTime_t *dateTime);

/** 将日期和时间写入 DS3231；返回 1 表示成功。 */
uint8_t DS3231_SetDateTime(const DS3231_DateTime_t *dateTime);

/** 检查时间字段范围，避免把非法 BCD 写入芯片。 */
uint8_t DS3231_IsValidDateTime(const DS3231_DateTime_t *dateTime);

/** 将 RTC 时间格式化为 "YYYY-MM-DD HH:MM:SS"，缓冲区至少 20 字节。 */
uint8_t DS3231_FormatDateTime(const DS3231_DateTime_t *dateTime,
                              char *text,
                              size_t textSize);

/** 返回 1 表示 RTC 时间可信；OSF 置位时返回 0。 */
uint8_t DS3231_IsTimeValid(void);

#endif /* BSP_DS3231_H */
