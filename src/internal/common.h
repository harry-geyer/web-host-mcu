#pragma once

#include <stdint.h>


int whm_main_loop_iterate(void* userdata, int (* cb)(void* userdata), uint64_t timeout_us);
