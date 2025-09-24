#include <stdint.h>
#include <string.h>
#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/sync.h"
#include "pico/sync.h"
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
}


static int _whm_config_commit2(uint8_t* dst, uint8_t* src, uint32_t size);


static char _whm_config_json[_WHM_CONFIG_JSON_BUFFER_LEN];
static whm_config_t _whm_config = _WHM_CONFIG_DEFAULT;
static json_t _whm_config_json_pool[_WHM_CONFIG_JSON_MAX_FIELDS];


int whm_config_init(void)
{
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
        memcpy(_whm_config.name, namefield, len);
        _whm_config.name[len] = '\0';
    }
    const char* blinking_ms_str = json_getPropertyValue(parent, "name");
    if (blinking_ms_str)
    {
        char* p = NULL;
        uint32_t blinking_ms = strtoul(blinking_ms_str, &p, 10);
        if (*p == '\0') {
            printf("invalid blinking_ms\n");
        } else {
            _whm_config.blinking_ms = blinking_ms;
        }
    }
    return 0;
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
    memset(&_whm_config, 0, sizeof(whm_config_t));
    static const whm_config_t _default_config = _WHM_CONFIG_DEFAULT;
    memcpy(&_whm_config, &_default_config, sizeof(whm_config_t));
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
