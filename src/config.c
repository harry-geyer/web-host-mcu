#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/divider.h"

#include "tiny-json.h"

#include "config.h"
#include "flash_layout.h"


#define _WHM_CONFIG_JSON_BUFFER_LEN                 1024
#define _WHM_CONFIG_JSON_MAX_FIELDS                 32
#define _WHM_CONFIG_DEFAULT                                             \
{                                                                       \
    .name = "Web-Host MCU",                                             \
    .blinking_ms = 250,                                                 \
    .ap =                                                               \
    {                                                                   \
        .ssid = "Web-Host MCU",                                         \
        .password = "host52%files",                                     \
    },                                                                  \
    .station =                                                          \
    {                                                                   \
        .ssid = "",                                                     \
        .password = "",                                                 \
        .auth = 0,                                                      \
    },                                                                  \
}


static int _whm_config_commit2(uint8_t* dst, uint8_t* src, uint32_t size);
static bool _whm_config_get_auth(const char* auth_str, uint32_t* auth);


whm_config_t whm_conf = _WHM_CONFIG_DEFAULT;
static char _whm_config_json[_WHM_CONFIG_JSON_BUFFER_LEN];
static json_t _whm_config_json_pool[_WHM_CONFIG_JSON_MAX_FIELDS];
static bool _whm_config_loaded = false;


int whm_config_init(void)
{
    _whm_config_loaded = true;
    const char* config_raw = (const char*)PERSIST_RAW_DATA;
    memcpy(_whm_config_json, config_raw, sizeof(_WHM_CONFIG_JSON_BUFFER_LEN));

    char* config = whm_config_get_string();
    json_t const* parent = json_create((char*)config, _whm_config_json_pool, _WHM_CONFIG_JSON_MAX_FIELDS);
    if (!parent)
    {
        /* no config available */
        return -1;
    }
    const char* namefield = json_getPropertyValue(parent, "name");
    if (namefield)
    {
        unsigned len = strlen(namefield);
        memcpy(whm_conf.name, namefield, len);
        whm_conf.name[len] = '\0';
    }
    const char* blinking_ms_str = json_getPropertyValue(parent, "name");
    if (blinking_ms_str)
    {
        char* p = NULL;
        uint32_t blinking_ms = strtoul(blinking_ms_str, &p, 10);
        if (*p == '\0')
        {
            printf("invalid blinking_ms\n");
        }
        else
        {
            whm_conf.blinking_ms = blinking_ms;
        }
    }
    json_t const* ap_obj = json_getProperty(parent, "ap");
    if (ap_obj)
    {
        const char* ap_ssid = json_getPropertyValue(ap_obj, "ssid");
        if (ap_ssid)
        {
            strncpy(whm_conf.ap.ssid, ap_ssid, WHM_CONFIG_WIRELESS_LEN-1);
        }
        const char* ap_password = json_getPropertyValue(ap_obj, "password");
        if (ap_password)
        {
            strncpy(whm_conf.ap.password, ap_password, WHM_CONFIG_WIRELESS_LEN-1);
        }
    }
    json_t const* station_obj = json_getProperty(parent, "station");
    if (station_obj)
    {
        const char* station_ssid = json_getPropertyValue(station_obj, "ssid");
        if (station_ssid)
        {
            strncpy(whm_conf.station.ssid, station_ssid, WHM_CONFIG_WIRELESS_LEN-1);
        }
        const char* station_password = json_getPropertyValue(station_obj, "password");
        if (station_password)
        {
            strncpy(whm_conf.station.password, station_password, WHM_CONFIG_WIRELESS_LEN-1);
        }
        const char* station_auth = json_getPropertyValue(station_obj, "auth");
        uint32_t auth = 0;
        if (_whm_config_get_auth(station_auth, &auth))
        {
            whm_conf.station.auth = auth;
        }
        else
        {
            printf("invalid auth\n");
        }
    }
    return 0;
}


bool whm_config_loaded(void)
{
    return _whm_config_loaded;
}


int whm_config_set_string(char* config_str, unsigned len)
{
    int ret = -1;
    if (len < _WHM_CONFIG_JSON_BUFFER_LEN)
    {
        memcpy(_whm_config_json, config_str, len);
        memset(&_whm_config_json[len], 0, _WHM_CONFIG_JSON_BUFFER_LEN - len);
        ret = 0;
    }
    return ret;
}


char* whm_config_get_string(void)
{
    return _whm_config_json;
}


void whm_config_wipe(void)
{
    memset(&whm_conf, 0, sizeof(whm_config_t));
    static const whm_config_t _default_config = _WHM_CONFIG_DEFAULT;
    memcpy(&whm_conf, &_default_config, sizeof(whm_config_t));
}


int whm_config_save(void)
{
    critical_section_t crit_sec;
    critical_section_init(&crit_sec);
    critical_section_enter_blocking(&crit_sec);
    flash_range_erase(PERSIST_CONFIG_SECTOR, FLASH_SECTOR_SIZE);
    int ret = _whm_config_commit2(PERSIST_RAW_DATA, (uint8_t*)_whm_config_json, _WHM_CONFIG_JSON_BUFFER_LEN);
    critical_section_exit(&crit_sec);
    critical_section_deinit(&crit_sec);
    return ret;
}


static int _whm_config_commit2(uint8_t* dst, uint8_t* src, uint32_t size)
{
    static uint8_t  _persist_page[FLASH_PAGE_SIZE];

    divmod_result_t uresult = hw_divider_divmod_u32(size, FLASH_PAGE_SIZE);
    uint32_t num_full_pages = to_quotient_u32(uresult);
    uint32_t last_page_len = to_remainder_u32(uresult);
    uint32_t offset = 0;
    for (uint32_t page = 0; page < num_full_pages; page++)
    {
        flash_range_program((uintptr_t)dst - XIP_BASE + offset, src + offset, FLASH_PAGE_SIZE);
        offset += FLASH_PAGE_SIZE;
    }
    memcpy(_persist_page, src + offset, last_page_len);
    memset(_persist_page + last_page_len, 0xFF, FLASH_PAGE_SIZE - last_page_len);
    flash_range_program((uintptr_t)dst - XIP_BASE + offset, _persist_page, FLASH_PAGE_SIZE);
    return memcmp(dst, src, size) == 0;
}


static bool _whm_config_get_auth(const char* auth_str, uint32_t* auth)
{
    if (!auth_str || !auth)
    {
        return false;
    }
    if (0 == strcmp(auth_str, "OPEN"))
    {
        *auth = CYW43_AUTH_OPEN;
    }
    else if (0 == strcmp(auth_str, "WPA_TKIP"))
    {
        *auth = CYW43_AUTH_WPA_TKIP_PSK;
    }
    else if (0 == strcmp(auth_str, "WPA2_AES"))
    {
        *auth = CYW43_AUTH_WPA2_AES_PSK;
    }
    else if (0 == strcmp(auth_str, "WPA2_MIXED"))
    {
        *auth = CYW43_AUTH_WPA2_MIXED_PSK;
    }
    else if (0 == strcmp(auth_str, "WPA3_SAE_AES"))
    {
        *auth = CYW43_AUTH_WPA3_SAE_AES_PSK;
    }
    else if (0 == strcmp(auth_str, "WPA3_WPA2_AES"))
    {
        *auth = CYW43_AUTH_WPA3_WPA2_AES_PSK;
    }
    else
    {
        return false;
    }
    return true;
}
