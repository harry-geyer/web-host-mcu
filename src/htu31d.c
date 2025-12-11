#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#include "hardware/i2c.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"

#include "htu31d.h"
#include "pinmap.h"
#include "util.h"


#define _WHM_HTU31D_I2C_SCL_FREQ_HZ                 (400U * 1000U)

#define _WHM_HTU31D_I2C_ADDR_GND                    0U
#define _WHM_HTU31D_I2C_ADDR_VDD                    1U
#define __WHM_HTU31D_I2C_ADDR(_v)                   (0x40U | _v)
#define _WHM_HTU31D_I2C_ADDR                        __WHM_HTU31D_I2C_ADDR(_WHM_HTU31D_I2C_ADDR_GND)

#define _WHM_HTU31D_I2C_WRITE_TIMEOUT_US(_bytes)   (2000U * (_bytes))
#define _WHM_HTU31D_I2C_READ_TIMEOUT_US(_bytes)    (2000U * (_bytes))

#define _WHM_HTU31D_RH_OSR_0_020_PERC               0x0U
#define _WHM_HTU31D_RH_OSR_0_014_PERC               0x1U
#define _WHM_HTU31D_RH_OSR_0_010_PERC               0x2U
#define _WHM_HTU31D_RH_OSR_0_007_PERC               0x3U

#define _WHM_HTU31D_RH_OSR_0_020_PERC_CONV_TIME_US  1000U
#define _WHM_HTU31D_RH_OSR_0_014_PERC_CONV_TIME_US  2000U
#define _WHM_HTU31D_RH_OSR_0_010_PERC_CONV_TIME_US  3900U
#define _WHM_HTU31D_RH_OSR_0_007_PERC_CONV_TIME_US  7800U

#define _WHM_HTU31D_T_OSR_0_040_C                   0x0U
#define _WHM_HTU31D_T_OSR_0_025_C                   0x1U
#define _WHM_HTU31D_T_OSR_0_016_C                   0x2U
#define _WHM_HTU31D_T_OSR_0_012_C                   0x3U

#define _WHM_HTU31D_T_OSR_0_040_C_CONV_TIME_US      1600U
#define _WHM_HTU31D_T_OSR_0_025_C_CONV_TIME_US      3100U
#define _WHM_HTU31D_T_OSR_0_016_C_CONV_TIME_US      6100U
#define _WHM_HTU31D_T_OSR_0_012_C_CONV_TIME_US      12100U

#define _WHM_HTU31D_CONV_TIME_STATIC                10000U

#define _WHM_HTU31D_RH_OSR                          _WHM_HTU31D_RH_OSR_0_020_PERC
#define _WHM_HTU31D_T_OSR                           _WHM_HTU31D_T_OSR_0_040_C

#define _WHM_HTU31D_CMD_CONVERSION(_rh, _t)         (0x40U | ((0x3U & _rh) << 3U) | ((0x3U & _t) << 1U))
#define _WHM_HTU31D_CMD_READ_T_RH                   0x00U
#define _WHM_HTU31D_CMD_READ_RH                     0x10U
#define _WHM_HTU31D_CMD_RESET                       0x1EU
#define _WHM_HTU31D_CMD_HEATER_ON                   0x04U
#define _WHM_HTU31D_CMD_HEATER_OFF                  0x02U
#define _WHM_HTU31D_CMD_READ_SERIAL                 0x0AU
#define _WHM_HTU31D_CMD_READ_DIAGNOSTIC             0x08U


static void _whm_htu31d_do_read(void);
static bool _whm_htu31d_command(const uint8_t command, bool nostop);
static bool _whm_htu31d_read_rh_t(uint16_t* rh, uint16_t* t, bool nostop);
static uint8_t _whm_htu31d_crc8(uint8_t* data, uint8_t length);
static uint32_t _whm_htu31d_conv_rel_hum(uint16_t raw);
static int32_t _whm_htu31d_conv_temperature(uint16_t raw);


static const uint16_t _whm_htu31d_rel_hum_conv_time[] =
{
    _WHM_HTU31D_RH_OSR_0_020_PERC_CONV_TIME_US,
    _WHM_HTU31D_RH_OSR_0_014_PERC_CONV_TIME_US,
    _WHM_HTU31D_RH_OSR_0_010_PERC_CONV_TIME_US,
    _WHM_HTU31D_RH_OSR_0_007_PERC_CONV_TIME_US,
};
static const uint16_t _whm_htu31d_temperature_conv_time[] =
{
    _WHM_HTU31D_T_OSR_0_040_C_CONV_TIME_US,
    _WHM_HTU31D_T_OSR_0_025_C_CONV_TIME_US,
    _WHM_HTU31D_T_OSR_0_016_C_CONV_TIME_US,
    _WHM_HTU31D_T_OSR_0_012_C_CONV_TIME_US,
};
#define _WHM_HTU31D_GET_CONV_TIME(_rh, _t)          WHM_MAX(_whm_htu31d_rel_hum_conv_time[_rh], _whm_htu31d_temperature_conv_time[_t])

static void* _whm_htu31d_callback = NULL;
static void* _whm_htu31d_userdata = NULL;
static bool _whm_htu31d_collecting = 0;
static uint64_t _whm_htu31d_conversion_time = 0;
#define _WHM_HTU31D_CONVERSION_READY(_t)            (_whm_htu31d_conversion_time <= _t)


void whm_htu31d_init(void)
{
    i2c_init(I2C_INSTANCE(WHM_HTU31D_I2C_UNIT), _WHM_HTU31D_I2C_SCL_FREQ_HZ);
    gpio_init(WHM_HTU31D_SDA_PIN);
    gpio_init(WHM_HTU31D_SCL_PIN);
    gpio_set_drive_strength(WHM_HTU31D_SDA_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_drive_strength(WHM_HTU31D_SCL_PIN, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_function(WHM_HTU31D_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(WHM_HTU31D_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(WHM_HTU31D_SDA_PIN);
    gpio_pull_up(WHM_HTU31D_SCL_PIN);
    gpio_init(WHM_HTU31D_RESET_PIN);
    gpio_set_dir(WHM_HTU31D_RESET_PIN, true);
    gpio_put(WHM_HTU31D_RESET_PIN, 1);
}


void whm_htu31d_deinit(void)
{
    i2c_deinit(I2C_INSTANCE(WHM_HTU31D_I2C_UNIT));
    _whm_htu31d_collecting = false;
    _whm_htu31d_callback = NULL;
    _whm_htu31d_userdata = NULL;
}


void whm_htu31d_iterate(void)
{
    uint64_t now = time_us_64();
    if (_whm_htu31d_collecting && _WHM_HTU31D_CONVERSION_READY(now))
    {
        _whm_htu31d_do_read();
        _whm_htu31d_collecting = false;
        _whm_htu31d_callback = NULL;
        _whm_htu31d_userdata = NULL;
    }
}


bool whm_htu31d_get(void* userdata, whm_htu31d_callback_t callback)
{
    if (_whm_htu31d_collecting)
    {
        /* already collecting */
        return false;
    }
    uint8_t conv_command = _WHM_HTU31D_CMD_CONVERSION(_WHM_HTU31D_RH_OSR, _WHM_HTU31D_T_OSR);
    if (!_whm_htu31d_command(conv_command, false))
    {
        /* failed to execute conversion command */
        return false;
    }
    _whm_htu31d_collecting = true;
    _whm_htu31d_conversion_time = time_us_64()
        + _WHM_HTU31D_GET_CONV_TIME(_WHM_HTU31D_RH_OSR, _WHM_HTU31D_T_OSR) + _WHM_HTU31D_CONV_TIME_STATIC;
    _whm_htu31d_userdata = userdata;
    _whm_htu31d_callback = (void*)callback;
    return true;
}


static void _whm_htu31d_do_read(void)
{
    uint16_t rh_raw = 0;
    uint16_t t_raw = 0;
    bool success = _whm_htu31d_command(_WHM_HTU31D_CMD_READ_T_RH, true)
        && _whm_htu31d_read_rh_t(&rh_raw, &t_raw, false);
    if (_whm_htu31d_callback)
    {
        uint32_t rh_e3 = _whm_htu31d_conv_rel_hum(rh_raw);
        int32_t t_e3 = _whm_htu31d_conv_temperature(t_raw);
        ((whm_htu31d_callback_t)_whm_htu31d_callback)(_whm_htu31d_userdata, success, rh_e3, t_e3);
    }
}


static bool _whm_htu31d_command(const uint8_t command, bool nostop)
{
    int ret = i2c_write_timeout_us(
        I2C_INSTANCE(WHM_HTU31D_I2C_UNIT),
        _WHM_HTU31D_I2C_ADDR,
        &command,
        1U,
        nostop,
        _WHM_HTU31D_I2C_WRITE_TIMEOUT_US(1U)
    );
    return ret == 1;
}


static bool _whm_htu31d_read_rh_t(uint16_t* rh, uint16_t* t, bool nostop)
{
    if (!rh || !t)
    {
        /* null pointer */
        return false;
    }
    uint8_t buf[6];

    int ret = i2c_read_timeout_us(
        I2C_INSTANCE(WHM_HTU31D_I2C_UNIT),
        _WHM_HTU31D_I2C_ADDR,
        buf,
        6U,
        nostop,
        _WHM_HTU31D_I2C_READ_TIMEOUT_US(6U)
    );
    if (ret != 6)
    {
        /* failed reading i2c */
        return false;
    }
    uint8_t t_crc8 = _whm_htu31d_crc8(buf, 2);
    if (t_crc8 != buf[2])
    {
        /* bad CRC8 */
        return false;
    }
    *t = (buf[0] << 8) | buf[1];
    uint8_t rh_crc8 = _whm_htu31d_crc8(buf + 3, 2);
    if (rh_crc8 != buf[5])
    {
        /* bad CRC8 */
        return false;
    }
    *rh = (buf[3] << 8) | buf[4];
    return true;
}


static uint8_t _whm_htu31d_crc8(uint8_t *data, uint8_t length)
{
    uint32_t polynom = 0x98800000UL;
    uint32_t msb     = 0x80000000UL;
    uint32_t mask    = 0xFF800000UL;
    uint32_t result  = 0;

    if (length == 1)
    {
        result = (uint32_t)data[0] << 8;
    }
    else if (length == 2)
    {
        result = ((uint32_t)data[0] << 16) | ((uint32_t)data[1] << 8);
    }
    else if (length == 3)
    {
        result = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8);
    }
    else
    {
        return 0xFF;
    }
    while (msb != 0x80)
    {
        if (result & msb)
        {
            result = ((result ^ polynom) & mask) | (result & ~mask);
        }
        msb >>= 1;
        mask >>= 1;
        polynom >>= 1;
    }

    return (uint8_t)(result & 0xFF);
}



static uint32_t _whm_htu31d_conv_rel_hum(uint16_t raw)
{
    uint64_t rel_hum = 1000ULL * 100ULL * (uint64_t)raw;
    rel_hum /= UINT16_MAX;
    return (uint32_t)rel_hum;
}


static int32_t _whm_htu31d_conv_temperature(uint16_t raw)
{
    int64_t temperature = 1000LL * 165LL * (uint64_t)raw;
    temperature /= UINT16_MAX;
    temperature -= 1000 * 40;
    return (int32_t)temperature;
}
