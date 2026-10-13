#include "bsp_ds3231.h"
#include "i2c.h"
#include <stdio.h>

#define DS3231_REG_TIME 0x00U
#define DS3231_REG_STATUS 0x0FU
#define DS3231_STATUS_OSF 0x80U

static uint8_t Ds3231_BcdToBin(uint8_t value)
{
    return (uint8_t)(((value >> 4U) * 10U) + (value & 0x0FU));
}

static uint8_t Ds3231_BinToBcd(uint8_t value)
{
    return (uint8_t)(((value / 10U) << 4U) | (value % 10U));
}

static uint8_t Ds3231_DaysInMonth(uint8_t year, uint8_t month)
{
    static const uint8_t days[12] =
        {31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U};

    if ((month < 1U) || (month > 12U))
    {
        return 0U;
    }
    if ((month == 2U) && ((year % 4U) == 0U))
    {
        return 29U; /* DS3231 本项目限定 2000~2099，year%4 足以判断闰年。 */
    }
    return days[month - 1U];
}

static uint8_t Ds3231_ReadStatus(uint8_t *status)
{
    if (status == NULL)
    {
        return 0U;
    }
    return (HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDRESS,
        DS3231_REG_STATUS, I2C_MEMADD_SIZE_8BIT, status, 1U, 200U) == HAL_OK)
        ? 1U : 0U;
}

uint8_t DS3231_IsReady(void)
{
    return (HAL_I2C_IsDeviceReady(&hi2c1, DS3231_I2C_ADDRESS, 2U, 100U) == HAL_OK)
         ? 1U : 0U;
}

uint8_t DS3231_IsTimeValid(void)
{
    uint8_t status;

    if (Ds3231_ReadStatus(&status) == 0U)
    {
        return 0U;
    }
    return ((status & DS3231_STATUS_OSF) == 0U) ? 1U : 0U;
}

uint8_t DS3231_IsValidDateTime(const DS3231_DateTime_t *dateTime)
{
    if (dateTime == NULL || dateTime->seconds > 59U || dateTime->minutes > 59U ||
        dateTime->hours > 23U || dateTime->day < 1U || dateTime->day > 7U ||
        dateTime->month < 1U || dateTime->month > 12U ||
        dateTime->date < 1U ||
        dateTime->date > Ds3231_DaysInMonth(dateTime->year, dateTime->month))
    {
        return 0U;
    }
    return 1U;
}

uint8_t DS3231_ReadDateTime(DS3231_DateTime_t *dateTime)
{
    uint8_t data[7];
    uint8_t hour;

    if (dateTime == NULL || HAL_I2C_Mem_Read(&hi2c1, DS3231_I2C_ADDRESS,
        DS3231_REG_TIME, I2C_MEMADD_SIZE_8BIT, data, sizeof(data), 200U) != HAL_OK)
    {
        return 0U;
    }

    dateTime->seconds = Ds3231_BcdToBin(data[0] & 0x7FU);
    dateTime->minutes = Ds3231_BcdToBin(data[1] & 0x7FU);
    /* 同时兼容芯片原先被设置为 12 小时制或 24 小时制的情况。 */
    if ((data[2] & 0x40U) != 0U)
    {
        hour = Ds3231_BcdToBin(data[2] & 0x1FU);
        if (hour == 12U)
        {
            hour = 0U;
        }
        if ((data[2] & 0x20U) != 0U)
        {
            hour = (uint8_t)(hour + 12U);
        }
        dateTime->hours = hour;
    }
    else
    {
        dateTime->hours = Ds3231_BcdToBin(data[2] & 0x3FU);
    }
    dateTime->day = Ds3231_BcdToBin(data[3] & 0x07U);
    dateTime->date = Ds3231_BcdToBin(data[4] & 0x3FU);
    dateTime->month = Ds3231_BcdToBin(data[5] & 0x1FU);
    dateTime->year = Ds3231_BcdToBin(data[6]);

    return DS3231_IsValidDateTime(dateTime);
}

uint8_t DS3231_SetDateTime(const DS3231_DateTime_t *dateTime)
{
    uint8_t data[7];
    uint8_t status;

    if (DS3231_IsValidDateTime(dateTime) == 0U)
    {
        return 0U;
    }

    data[0] = Ds3231_BinToBcd(dateTime->seconds);
    data[1] = Ds3231_BinToBcd(dateTime->minutes);
    data[2] = Ds3231_BinToBcd(dateTime->hours);
    data[3] = Ds3231_BinToBcd(dateTime->day);
    data[4] = Ds3231_BinToBcd(dateTime->date);
    data[5] = Ds3231_BinToBcd(dateTime->month);
    data[6] = Ds3231_BinToBcd(dateTime->year);

    if (HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDRESS, DS3231_REG_TIME,
        I2C_MEMADD_SIZE_8BIT, data, sizeof(data), 200U) != HAL_OK)
    {
        return 0U;
    }

    /* 设置了可信时间后清除 Oscillator Stop Flag，其他状态位保持不变。 */
    if (Ds3231_ReadStatus(&status) == 0U)
    {
        return 0U;
    }
    status &= (uint8_t)~DS3231_STATUS_OSF;
    return (HAL_I2C_Mem_Write(&hi2c1, DS3231_I2C_ADDRESS, DS3231_REG_STATUS,
        I2C_MEMADD_SIZE_8BIT, &status, 1U, 200U) == HAL_OK) ? 1U : 0U;
}

uint8_t DS3231_Init(void)
{
    DS3231_DateTime_t dateTime;

    if ((DS3231_IsReady() == 0U) ||
        (DS3231_IsTimeValid() == 0U) ||
        (DS3231_ReadDateTime(&dateTime) == 0U))
    {
        return 0U;
    }
    return 1U;
}

uint8_t DS3231_FormatDateTime(const DS3231_DateTime_t *dateTime,
                              char *text,
                              size_t textSize)
{
    int length;

    if ((text == NULL) || (textSize < 20U) ||
        (DS3231_IsValidDateTime(dateTime) == 0U))
    {
        return 0U;
    }
    length = snprintf(text, textSize,
                      "20%02u-%02u-%02u %02u:%02u:%02u",
                      dateTime->year, dateTime->month, dateTime->date,
                      dateTime->hours, dateTime->minutes, dateTime->seconds);
    return (length == 19) ? 1U : 0U;
}
