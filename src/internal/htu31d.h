#pragma once

#include <stdint.h>
#include <stdbool.h>

#define WHM_HTU31D_MAX_CONV_TIME_US             13000U


typedef void (* whm_htu31d_callback_t)(void* userdata, bool success, uint32_t rh_e3, int32_t t_e3);

void whm_htu31d_init(void);
void whm_htu31d_deinit(void);
void whm_htu31d_iterate(void);
/* e3 represents x1000, so temperature in milli-celcius, relative humidity
 * in per-millicent */
bool whm_htu31d_get(void* userdata, whm_htu31d_callback_t callback);
