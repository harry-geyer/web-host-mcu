#include <stdint.h>

#include "pico/time.h"

#include "ap_station.h"
#include "htu31d.h"


int whm_main_loop_iterate(void* userdata, int (* cb)(void* userdata), uint64_t timeout_us)
{
    uint64_t end_time = time_us_64() + timeout_us;
    while (time_us_64() <= end_time)
    {
        tight_loop_contents();
        whm_ap_station_iterate();
        whm_htu31d_iterate();
        int ret = cb(userdata);
        if (ret)
        {
            return ret;
        }
    }
    return 0;
}

