#include <stdio.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "ap_station.h"
#include "config.h"

#define _WHM_AP_STATION_BUF_SIZE            128
#define _WHM_AP_STATION_SCAN_TIMEOUT_US     (10 * 1000 * 1000) /* 10 seconds */


typedef enum _whm_ap_station_state
{
    _WHM_AP_STATION_STATE_OFF,
    _WHM_AP_STATION_STATE_AP,
    _WHM_AP_STATION_STATE_STATION,
    _WHM_AP_STATION_STATE_SCAN,
    _WHM_AP_STATION_STATE_CONNECTING,
    _WHM_AP_STATION_STATE_CONNECTED,
    _WHM_AP_STATION_STATE_DISCONNECTED,
    _WHM_AP_STATION_STATE_COUNT,
} _whm_ap_station_state_t;


static int _whm_ap_station_reload(void);
static int _whm_ap_station_scan_result(void* userdata, const cyw43_ev_scan_result_t* result);
static int _whm_ap_station_connect(void);
static void _whm_ap_station_set_mode(bool station);


static struct
{
    _whm_ap_station_state_t state;
    bool is_station;
    uint64_t last_scan_us;
} _whm_ap_station_ctx =
{
    .state = _WHM_AP_STATION_STATE_OFF,
    .is_station = false,
    .last_scan_us = 0,
};
whm_ap_station_scan_result_t* whm_ap_station_scan_results = NULL;


int whm_ap_station_init(void)
{
    return _whm_ap_station_reload();
}


void whm_ap_station_iterate(void)
{
    uint64_t now = time_us_64();
    cyw43_arch_poll();
    switch (_whm_ap_station_ctx.state)
    {
        case _WHM_AP_STATION_STATE_SCAN:
            if (!cyw43_wifi_scan_active(&cyw43_state))
            {
                _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_STATION;
                _whm_ap_station_ctx.last_scan_us = now;
            }
            break;
        case _WHM_AP_STATION_STATE_CONNECTING:
        {
            int state = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
            if (CYW43_LINK_JOIN == state)
            {
                _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_CONNECTED;
            }
            else if (CYW43_LINK_BADAUTH == state)
            {
                _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_STATION;
            }
            else if (CYW43_LINK_NONET == state)
            {
                _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_STATION;
            }
            else if (CYW43_LINK_FAIL == state)
            {
                _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_STATION;
            }
            break;
        }
        case _WHM_AP_STATION_STATE_STATION:
            if (_whm_ap_station_ctx.last_scan_us + _WHM_AP_STATION_SCAN_TIMEOUT_US <= now)
            {
                if (!whm_ap_station_start_scan())
                {
                    /* failed to start scan, retry next */
                    _whm_ap_station_ctx.last_scan_us = now;
                }
            }
            break;
        case _WHM_AP_STATION_STATE_OFF:
            /* fall through */
        case _WHM_AP_STATION_STATE_AP:
            /* fall through */
        case _WHM_AP_STATION_STATE_CONNECTED:
            /* fall through */
        case _WHM_AP_STATION_STATE_DISCONNECTED:
            /* fall through */
        default:
            break;
    }
}


void whm_ap_station_reload(void)
{
    (void)_whm_ap_station_reload();
}


bool whm_ap_station_get_connection(char** ssid, char** password)
{
    bool ret = false;
    if (strlen(whm_conf.station.ssid))
    {
        *ssid = whm_conf.station.ssid;
        *password = whm_conf.station.password;
        ret = true;
    }
    return ret;
}


bool whm_ap_station_get_connected(void)
{
    return _WHM_AP_STATION_STATE_CONNECTED == _whm_ap_station_ctx.state;
}


bool whm_ap_station_start_scan(void)
{
    bool ret = false;
    if (NULL != whm_ap_station_scan_results)
    {
        /* free if not already */
        whm_ap_station_scan_results_free();
    }
    cyw43_wifi_scan_options_t scan_options = {0};
    if (0 == cyw43_wifi_scan(&cyw43_state, &scan_options, (void*)whm_ap_station_scan_results, _whm_ap_station_scan_result))
    {
        ret = true;
        _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_SCAN;
    }
    return ret;
}


whm_ap_station_scan_result_t* whm_ap_station_get_scan(void)
{
    return whm_ap_station_scan_results;
}


void whm_ap_station_scan_results_free(void)
{
    whm_ap_station_scan_result_t* current = whm_ap_station_scan_results;
    while (current != NULL)
    {
        whm_ap_station_scan_result_t* next = current->next;
        free(current);
        current = next;
    }
    whm_ap_station_scan_results = NULL;
}


bool whm_ap_station_scanning(void)
{
    return _WHM_AP_STATION_STATE_SCAN == _whm_ap_station_ctx.state;
}


static int _whm_ap_station_reload(void)
{
    if (!whm_config_loaded())
    {
        printf("config not yet loaded");
        return -1;
    }
    bool is_station = strnlen(whm_conf.station.ssid, WHM_CONFIG_WIRELESS_LEN);
    if (_WHM_AP_STATION_STATE_CONNECTED == _whm_ap_station_ctx.state
        && is_station)
    {
        /* disconnect if already connected */
        cyw43_arch_disable_sta_mode();
    }
    _whm_ap_station_set_mode(is_station);
    return 0;
}


static int _whm_ap_station_scan_result(void* userdata, const cyw43_ev_scan_result_t* result)
{
    whm_ap_station_scan_result_t* results = (whm_ap_station_scan_result_t*)userdata;
    if (NULL == result)
    {
        return 0;
    }
    whm_ap_station_scan_result_t* new_node = (whm_ap_station_scan_result_t*)malloc(sizeof(whm_ap_station_scan_result_t));
    if (NULL == new_node)
    {
        return -1;
    }
    (void)memcpy((void*)&new_node->result, (const void*)result, sizeof(cyw43_ev_scan_result_t));
    new_node->next = NULL;
    if (NULL == results)
    {
        results = new_node;
    }
    else
    {
        whm_ap_station_scan_result_t* res = results;
        while (NULL != res->next)
        {
            res = res->next;
        }
        res->next = new_node;
    }
    return 0;
}


static int _whm_ap_station_connect(void)
{
    int ret = cyw43_arch_wifi_connect_async(whm_conf.station.ssid, whm_conf.station.password, whm_conf.station.auth);
    if (0 == ret)
    {
        _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_CONNECTING;
    }
    return ret;
}


static void _whm_ap_station_set_mode(bool station)
{
    if (station)
    {
        cyw43_arch_disable_ap_mode();
        cyw43_arch_enable_sta_mode();
        _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_STATION;
    }
    else
    {
        cyw43_arch_disable_sta_mode();
        cyw43_arch_enable_ap_mode(whm_conf.ap.ssid, whm_conf.ap.password, CYW43_AUTH_WPA2_AES_PSK);
        _whm_ap_station_ctx.state = _WHM_AP_STATION_STATE_AP;
    }
}
