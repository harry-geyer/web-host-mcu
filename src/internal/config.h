#pragma once

#include <stdint.h>


#define WHM_CONFIG_NAME_LEN                 63


typedef struct whm_config
{
    char name[WHM_CONFIG_NAME_LEN + 1];
    uint16_t blinking_ms;
} whm_config_t;


int whm_config_init(void);
int whm_config_set_string(char* config_str, unsigned len);
char* whm_config_get_string(void);
void whm_config_wipe(void);
int whm_config_save(void);
