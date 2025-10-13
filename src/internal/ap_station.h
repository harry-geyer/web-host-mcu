#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "pico/cyw43_arch.h"


#define WHM_AP_STATION_SCAN_RESULT_BUF_LEN              128;


struct whm_ap_station_scan_result
{
    const cyw43_ev_scan_result_t result;
    struct whm_ap_station_scan_result* next;
};

typedef struct whm_ap_station_scan_result whm_ap_station_scan_result_t;


extern whm_ap_station_scan_result_t* whm_ap_station_scan_results;


int whm_ap_station_init(void);
void whm_ap_station_iterate(void);
void whm_ap_station_reload(void);
bool whm_ap_station_get_connection(char** ssid, char** password);
bool whm_ap_station_get_connected(void);
bool whm_ap_station_start_scan(void);
whm_ap_station_scan_result_t* whm_ap_station_get_scan(void);
void whm_ap_station_scan_results_free(void);
bool whm_ap_station_scanning(void);
