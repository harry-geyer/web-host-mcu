
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/ip4_addr.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/tcpbase.h"

#include "http_server.h"
#include "config.h"
#include "util.h"
#include "htu31d.h"
#include "ap_station.h"
#include "common.h"


#define _WHM_HTTP_SERVER_CONFIG_BUFFER_SIZE                 1024
#define _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE               1024


typedef enum _whm_http_server_rest
{
    _WHM_HTTP_SERVER_REST_GET,
    _WHM_HTTP_SERVER_REST_POST,
} _whm_http_server_rest_t;


typedef struct _whm_http_server_rest_get_handler
{
    const char *path;
    err_t (* handler)(struct fs_file *file, const char* name);
} _whm_http_server_rest_get_handler_t;


typedef struct _whm_http_server_rest_post_handler
{
    const char *path;
    err_t (* begin_handler)(const char* http_request, uint16_t http_request_len, int content_len, char* response_uri, uint16_t response_uri_len, uint8_t* post_auto_wnd);
    err_t (* recv_handler)(struct pbuf *p);
    err_t (* finish_handler)(char* response_uri, uint16_t response_uri_len);
} _whm_http_server_rest_post_handler_t;


static const char* _whm_http_server_cgi_handler_index(int index, int num_params, char *pc_param[], char *pc_value[]);
static err_t _whm_http_server_rest_get_handler_config(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_meas(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_status(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_wifi_scan_start(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_wifi_scan_get(struct fs_file *file, const char* name);
static void _whm_http_server_meas_finish(void* userdata, bool success, uint32_t rh_e3, int32_t t_e3);
static err_t _whm_http_server_rest_post_handler_config_begin(const char* http_request, uint16_t http_request_len, int content_len, char* response_uri, uint16_t response_uri_len, uint8_t* post_auto_wnd);
static err_t _whm_http_server_rest_post_handler_config_recv(struct pbuf* p);
static err_t _whm_http_server_rest_post_handler_config_finish(char* response_uri, uint16_t response_uri_len);
static _whm_http_server_rest_get_handler_t* _whm_http_server_rest_get_handler_find(const char* uri);
static _whm_http_server_rest_post_handler_t* _whm_http_server_rest_post_handler_find(const char* uri);
static int _whm_http_server_gen_mac(char* buf, unsigned buflen, const uint8_t* bssid, unsigned bssid_len);
static const char* _whm_http_server_gen_auth(uint8_t auth);


static char _whm_http_server_config_buffer[_WHM_HTTP_SERVER_CONFIG_BUFFER_SIZE];
static char* _whm_http_server_config_pos = _whm_http_server_config_buffer;
static char _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE];
static int _whm_http_server_current_rest_req = 0;
static void* _whm_http_server_current_connection = NULL;
static _whm_http_server_rest_post_handler_t* _whm_http_server_current_post = NULL;
static struct {
    bool done;
    bool success;
    uint32_t rel_hum;
    int32_t temperature;
} _whm_http_server_meas =
{
    .done = false,
    .success = false,
    .rel_hum = 0,
    .temperature = 0,
};


static tCGI _whm_http_server_cgi_handlers[] =
{
    {"/", _whm_http_server_cgi_handler_index},
    {"/index.html", _whm_http_server_cgi_handler_index},
};


static _whm_http_server_rest_get_handler_t _whm_http_server_rest_get_handlers[] =
{
    {"/api/config" , _whm_http_server_rest_get_handler_config},
    {"/api/meas" , _whm_http_server_rest_get_handler_meas},
    {"/api/status" , _whm_http_server_rest_get_handler_status},
    {"/api/wifi-scan-start" , _whm_http_server_rest_get_handler_wifi_scan_start},
    {"/api/wifi-scan-get" , _whm_http_server_rest_get_handler_wifi_scan_get},
};


static _whm_http_server_rest_post_handler_t _whm_http_server_rest_post_handlers[] =
{
    {
        .path = "/api/config",
        .begin_handler = _whm_http_server_rest_post_handler_config_begin,
        .recv_handler = _whm_http_server_rest_post_handler_config_recv,
        .finish_handler = _whm_http_server_rest_post_handler_config_finish,
    },
};


int whm_http_server_init(whm_http_server_t* server)
{
    cyw43_arch_lwip_begin();
    httpd_init();
    http_set_cgi_handlers(_whm_http_server_cgi_handlers, LWIP_ARRAYSIZE(_whm_http_server_cgi_handlers));
    cyw43_arch_lwip_end();
    return 0;
}

void whm_http_server_deinit(whm_http_server_t* server)
{
}


err_t httpd_post_begin(void* connection, const char* uri, const char* http_request,
        uint16_t http_request_len, int content_len, char* response_uri,
        uint16_t response_uri_len, uint8_t* post_auto_wnd)
{
    printf("POST BEGIN\n");
    _whm_http_server_current_rest_req++;
    err_t ret = ERR_VAL;
    if (_whm_http_server_current_connection != connection)
    {
        printf("NEW CONNECTION\n");
        _whm_http_server_current_connection = connection;
        _whm_http_server_rest_post_handler_t* h = _whm_http_server_rest_post_handler_find(uri);
        _whm_http_server_current_post = h;
        if (NULL != h)
        {
            printf("HAS HANDLER\n");
            ret = h->begin_handler(http_request, http_request_len, content_len, response_uri, response_uri_len, post_auto_wnd);
        }
    }
    return ret;
}


err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
    printf("POST RECV\n");
    err_t ret = ERR_VAL;
    if (_whm_http_server_current_connection == connection
        && p && p->len)
    {
        _whm_http_server_rest_post_handler_t* h = _whm_http_server_current_post;
        if (NULL != h)
        {
            h->recv_handler(p);
        }
        ret = ERR_OK;
        printf("POST RECV OK\n");
    }
    pbuf_free(p);
    return ret;
}


void httpd_post_finished(void* connection, char* response_uri, uint16_t response_uri_len)
{
    if (_whm_http_server_current_connection == connection)
    {
        _whm_http_server_rest_post_handler_t* h = _whm_http_server_current_post;
        if (NULL != h)
        {
            h->finish_handler(response_uri, response_uri_len);
        }
    }
    _whm_http_server_current_connection = NULL;
    _whm_http_server_current_post = NULL;
    printf("POST FIN\n");
}


const char* httpd_headers(struct fs_file* file, const char* uri)
{
    _whm_http_server_rest_get_handler_t* handler = _whm_http_server_rest_get_handler_find(uri);
    if (NULL != handler)
    {
        return "Content-Type: application/json\r\n"
               "Cache-Control: no-cache\r\n";
    }
    return NULL;
}


int fs_open_custom(struct fs_file* file, const char* name)
{
    int ret = 0;
    if (0 < _whm_http_server_current_rest_req)
    {
        _whm_http_server_current_rest_req--;
        printf("FS SERVING POST: %s\n", name);
        _whm_http_server_rest_post_handler_t* h = _whm_http_server_rest_post_handler_find(name);
        if (NULL != h)
        {
            file->data = _whm_http_server_response_buffer;
            file->len = strnlen(_whm_http_server_response_buffer, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
            file->index = file->len;
            file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            ret = 1;
        }
    }
    else
    {
        _whm_http_server_current_rest_req = 0;
        printf("FS SERVING GET: %s\n", name);
        _whm_http_server_rest_get_handler_t* h = _whm_http_server_rest_get_handler_find(name);
        ret = (NULL != h && ERR_OK == h->handler(file, name));
    }
    return ret;
}


void fs_close_custom(struct fs_file *file)
{
}


static _whm_http_server_rest_get_handler_t* _whm_http_server_rest_get_handler_find(const char* uri)
{
    for (size_t i = 0; i < LWIP_ARRAYSIZE(_whm_http_server_rest_get_handlers); i++)
    {
        if (strcmp(uri, _whm_http_server_rest_get_handlers[i].path) == 0)
        {
            return &_whm_http_server_rest_get_handlers[i];
        }
    }
    return NULL;
}


static _whm_http_server_rest_post_handler_t* _whm_http_server_rest_post_handler_find(const char* uri)
{
    for (size_t i = 0; i < LWIP_ARRAYSIZE(_whm_http_server_rest_post_handlers); i++)
    {
        if (strcmp(uri, _whm_http_server_rest_post_handlers[i].path) == 0)
        {
            return &_whm_http_server_rest_post_handlers[i];
        }
    }
    return NULL;
}


#define __WHM_HTTP_SERVER_CGI_HANDLER_DEFAULT(_name, _path)                                                                 \
static const char* _whm_http_server_cgi_handler_ ## _name (int index, int num_params, char *pc_param[], char *pc_value[])\
{                                                                                                                           \
    return _path;                                                                                                           \
}
__WHM_HTTP_SERVER_CGI_HANDLER_DEFAULT(index, "/index.html")


static err_t _whm_http_server_rest_get_handler_config(struct fs_file *file, const char* name)
{
    const char* config = whm_config_get_string();
    file->data = config;
    file->len = strlen(config);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static int _whm_http_server_meas_get_htu31d(void* userdata)
{
    return _whm_http_server_meas.done ? 1 : 0;
}


static err_t _whm_http_server_rest_get_handler_meas(struct fs_file *file, const char* name)
{
    whm_htu31d_get(NULL, _whm_http_server_meas_finish);
    whm_main_loop_iterate(NULL, _whm_http_server_meas_get_htu31d, WHM_HTU31D_MAX_CONV_TIME_US);
    _whm_http_server_meas.done = false;
    int len = 0;
    if (_whm_http_server_meas.success)
    {
        len = snprintf(
            _whm_http_server_response_buffer,
            _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1,
            "["
                "{"
                    "\"name\":\"relative_humidity\","
                    "\"value\":%"PRIu32".%03"PRIu32","
                    "\"unit\":\"%%\""
                "},{"
                    "\"name\":\"temperature\","
                    "\"value\"%"PRId32".%03"PRIu32","
                    "\"unit\":\"celcius\""
                "}"
            "]",
            _whm_http_server_meas.rel_hum / 1000U, _whm_http_server_meas.rel_hum % 1000U,
            _whm_http_server_meas.temperature / 1000, WHM_ABS32(_whm_http_server_meas.temperature) % 1000U
        );
        _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1] = '\0';
    }
    else
    {
        strncpy(
            _whm_http_server_response_buffer,
            "{\"error\":\"failed to get measurements\"}",
            _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1
        );
        _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1] = '\0';
        len = strnlen(_whm_http_server_response_buffer, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
    }
    file->data = _whm_http_server_response_buffer;
    file->len = len;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static err_t _whm_http_server_rest_get_handler_status(struct fs_file *file, const char* name)
{
    bool is_connected = whm_ap_station_get_connected();
    unsigned len = snprintf(
        _whm_http_server_response_buffer,
        _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE,
        "{\"network\":{\"connected\":%s}}",
        is_connected ? "true" : "false"
    );
    file->data = _whm_http_server_response_buffer;
    file->len = len;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static err_t _whm_http_server_rest_get_handler_wifi_scan_start(struct fs_file *file, const char* name)
{
    bool started = whm_ap_station_start_scan();
    unsigned len = snprintf(
        _whm_http_server_response_buffer,
        _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE,
        "{\"status\":\"ok\",\"scan\":\"%s\"}",
        started ? "started" : "failed"
    );
    file->data = _whm_http_server_response_buffer;
    file->len = len;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static err_t _whm_http_server_rest_get_handler_wifi_scan_get(struct fs_file *file, const char* name)
{
    whm_ap_station_scan_result_t* results = whm_ap_station_get_scan();
    unsigned len = 0;
    if (NULL == results)
    {
        strncpy(
            _whm_http_server_response_buffer,
            "{\"status\":\"error\"}",
            _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE
        );
        len = strnlen(_whm_http_server_response_buffer, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
    }
    else
    {
        strncpy(_whm_http_server_response_buffer, "{\"status\":\"ok\",\"stations\":[", _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE);
        len = strnlen(_whm_http_server_response_buffer, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
        char* p = &_whm_http_server_response_buffer[len];
        whm_ap_station_scan_result_t* c = results;
        while (c)
        {
            const cyw43_ev_scan_result_t* r = &c->result;
            char mac_address[18];
            _whm_http_server_gen_mac(mac_address, 18, r->bssid, sizeof(r->bssid));
            const char* auth = _whm_http_server_gen_auth(r->auth_mode);
            len += snprintf(p, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE - len,
                "{\"ssid\":\"%.*s\",\"mac\":\"%s\",\"channel\":%"PRIu16",\"auth\":\"%s\",\"rssi\":%"PRId16"},",
                r->ssid_len, r->ssid, mac_address, r->channel, auth, r->rssi
            );
        }
        _whm_http_server_response_buffer[len-1] = ']';
        _whm_http_server_response_buffer[len] = '}';
        whm_ap_station_scan_results_free();
    }
    file->data = _whm_http_server_response_buffer;
    file->len = len;
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static void _whm_http_server_meas_finish(void* userdata, bool success, uint32_t rh_e3, int32_t t_e3)
{
    _whm_http_server_meas.done = true;
    _whm_http_server_meas.success = success;
    _whm_http_server_meas.rel_hum = rh_e3;
    _whm_http_server_meas.temperature = t_e3;
}


static err_t _whm_http_server_rest_post_handler_config_begin(const char* http_request, uint16_t http_request_len, int content_len, char* response_uri, uint16_t response_uri_len, uint8_t* post_auto_wnd)
{
    printf("BEGIN CONFIG POST\n");
    _whm_http_server_config_buffer[0] = '\0';
    *post_auto_wnd = 1;
    return ERR_OK;
}


static err_t _whm_http_server_rest_post_handler_config_recv(struct pbuf* p)
{
    printf("RECV CONFIG POST\n");
    int ret = ERR_VAL;
    int rem_size = _whm_http_server_config_pos - _whm_http_server_config_buffer + _WHM_HTTP_SERVER_CONFIG_BUFFER_SIZE;
    if (p && p->len < rem_size)
    {
        memcpy(_whm_http_server_config_pos, p->payload, p->len);
        _whm_http_server_config_pos[p->len] = '\0';
        _whm_http_server_config_pos += p->len;
        ret = ERR_OK;
        printf("RECV CONFIG POST OK\n");
        printf("RECV: %.*s\n", p->len, (char*)p->payload);
    }
    return ret;
}


static err_t _whm_http_server_rest_post_handler_config_finish(char* response_uri, uint16_t response_uri_len)
{
    printf("FINISH CONFIG POST\n");
    int len = _whm_http_server_config_pos - _whm_http_server_config_buffer;
    _whm_http_server_config_pos = _whm_http_server_config_buffer;
    err_t ret = ERR_OK;
    int a = whm_config_set_string(_whm_http_server_config_buffer, len);
    int b = whm_config_save();
    if (0 == a && 0 == b)
    {
        strncpy(_whm_http_server_response_buffer, "{\"status\":\"ok\"}", _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
    }
    else
    {
        strncpy(_whm_http_server_response_buffer, "{\"status\":\"error\",\"error\":\"config invalid\"}", _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
        ret = ERR_ARG;
    }
    printf("a = %d\n", a);
    printf("b = %d\n", b);
    _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1] = '\0';
    strncpy(response_uri, "/api/config", response_uri_len);
    return ret;
}


static int _whm_http_server_gen_mac(char* buf, unsigned buflen, const uint8_t* bssid, unsigned bssid_len)
{
    int len = 0;
    char* p = buf;
    for (unsigned i = 0; i < bssid_len; i++)
    {
        if (buf + buflen <= p)
        {
            break;
        }
        len += snprintf(p, buf+buflen - p, "%02"PRIx8":", bssid[i]);
        p = buf + len;
    }
    buf[--len] = '\0';
    return len;
}


static const char* _whm_http_server_gen_auth(uint8_t auth)
{
    /* Unfortunately these auths aren't CYW43_AUTH_, but made up of some
     * different values
        OPEN  = 0x0
        WEP  |= 0x1
        WPA  |= 0x2
        WPA2 |= 0x4
    */
    switch(auth)
    {
        case 0x0:
            return "OPEN";
        case 0x1:
            return "WEP";
        case 0x2:
            return "WPA";
        case 0x4:
            return "WPA2";
        case 0x6:
            return "WPA2_MIXED";
        default:
            break;
    }
    return "UNKNOWN";
}
