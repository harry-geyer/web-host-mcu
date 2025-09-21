
#include "whm_config.h"


#define _WHM_CONFIG_JSON_MAX_FIELDS                  32
#define _WHM_CONFIG_DEFAULT                                             \
{                                                                       \
    .name = "Web-Host MCU",                                             \
    .blinking_ms = 250,                                                 \
}


static whm_config_t _whm_config = _WHM_CONFIG_DEFAULT;
static json_t _whm_config_json_pool[_WHM_CONFIG_JSON_MAX_FIELDS];


int whm_config_init(void)
{
    const char* config_raw = (const char*)PERSIST_RAW_DATA;
    json_t const* parent = json_create(config_raw, _whm_config_json_pool, _WHM_CONFIG_JSON_MAX_FIELDS);
    if (!parent)
    {
        /* no config available */
        return 0;
    }
    json_t const* namefield = json_getProperty(parent, "name");
    if (namefield)
    {
        memcpy(
    }
}


int whm_config_set_string(char* config_str, unsigned len)
{
}


const char* whm_config_get_string(void)
{
    return (const char*)PERSIST_RAW_DATA;
}


void whm_config_wipe(void)
{
    memset(&_whm_config, 0, sizeof(whm_config_t));
    static const whm_config_t _default_config = _WHM_CONFIG_DEFAULT;
    memcpy(&_whm_config, &default_config, sizeof(whm_config_t));
}


void whm_config_save(void)
{
    critical_section_t crit_sec;
    critical_section_init(&crit_sec);
    critical_section_enter_blocking(&crit_sec);
    flash_range_erase(PERSIST_CONFIG_SECTOR, FLASH_SECTOR_SIZE);
    bool ret = _whm_config_commit2(PERSIST_RAW_DATA,
    bool ret = (_rp2350_persist_commit2(PERSIST_RAW_DATA, (uint8_t*)persist_data, sizeof(persist_storage_t)) &&
        (_rp2350_persist_commit2(PERSIST_RAW_MEASUREMENTS, (uint8_t*)persist_measurements, sizeof(persist_measurements_storage_t))));
    critical_section_exit(&crit_sec);
    critical_section_deinit(&crit_sec);
}


static bool _whm_config_commit2(uint8_t* dst, uint8_t* src, uint32_t size)
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
