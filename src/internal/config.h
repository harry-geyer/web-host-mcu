#pragma once

#include <stdint.h>


#define WHM_CONFIG_NAME_LEN                 63
#define WHM_CONFIG_WIRELESS_LEN             128


typedef struct whm_config
{
    char name[WHM_CONFIG_NAME_LEN + 1];
    uint16_t blinking_ms;
    struct
    {
        char ssid[WHM_CONFIG_WIRELESS_LEN];
        char password[WHM_CONFIG_WIRELESS_LEN];
    } ap;
    struct
    {
        char ssid[WHM_CONFIG_WIRELESS_LEN];
        char password[WHM_CONFIG_WIRELESS_LEN];
        uint32_t auth;
    } station;
} whm_config_t;


extern whm_config_t whm_conf;


int whm_config_init(void);
bool whm_config_loaded(void);
int whm_config_set_string(char* config_str, unsigned len);
char* whm_config_get_string(void);
void whm_config_wipe(void);
int whm_config_save(void);
