
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
static const char* _whm_http_server_cgi_handler_app(int index, int num_params, char *pc_param[], char *pc_value[]);
static const char* _whm_http_server_cgi_handler_styles(int index, int num_params, char *pc_param[], char *pc_value[]);
static err_t _whm_http_server_rest_get_handler_config(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_meas(struct fs_file *file, const char* name);
static err_t _whm_http_server_rest_get_handler_status(struct fs_file *file, const char* name);
static void _whm_http_server_meas_finish(void* userdata, bool success, uint32_t rh_e3, int32_t t_e3);
static err_t _whm_http_server_rest_post_handler_config_begin(const char* http_request, uint16_t http_request_len, int content_len, char* response_uri, uint16_t response_uri_len, uint8_t* post_auto_wnd);
static err_t _whm_http_server_rest_post_handler_config_recv(struct pbuf* p);
static err_t _whm_http_server_rest_post_handler_config_finish(char* response_uri, uint16_t response_uri_len);
static _whm_http_server_rest_get_handler_t* _whm_http_server_rest_get_handler_find(const char* uri);
static _whm_http_server_rest_post_handler_t* _whm_http_server_rest_post_handler_find(const char* uri);


static char _whm_http_server_config_buffer[_WHM_HTTP_SERVER_CONFIG_BUFFER_SIZE];
static char _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE];
static _whm_http_server_rest_t _whm_http_server_current_rest_req = _WHM_HTTP_SERVER_REST_GET;
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
    {"/app.js", _whm_http_server_cgi_handler_app},
    {"/styles.css", _whm_http_server_cgi_handler_styles},
};


static _whm_http_server_rest_get_handler_t _whm_http_server_rest_get_handlers[] =
{
    {"/api/config" , _whm_http_server_rest_get_handler_config},
    {"/api/meas" , _whm_http_server_rest_get_handler_meas},
    {"/api/status" , _whm_http_server_rest_get_handler_status},
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
    _whm_http_server_current_rest_req = _WHM_HTTP_SERVER_REST_POST;
    if (_whm_http_server_current_connection != connection)
    {
        _whm_http_server_current_connection = connection;
        _whm_http_server_rest_post_handler_t* h = _whm_http_server_rest_post_handler_find(uri);
        _whm_http_server_current_post = h;
        if (NULL != h)
        {
            h->begin_handler(http_request, http_request_len, content_len, response_uri, response_uri_len, post_auto_wnd);
        }
    }
    return ERR_VAL;
}


char* httpd_param_value(struct pbuf* p, const char* param_name, char* value_buf, size_t value_buf_len)
{
    size_t param_len = strlen(param_name);
    u16_t param_pos = pbuf_memfind(p, param_name, param_len, 0);
    if (param_pos != 0xFFFF)
    {
        u16_t param_value_pos = param_pos + param_len;
        u16_t param_value_len = 0;
        u16_t tmp = pbuf_memfind(p, "&", 1, param_value_pos);
        if (tmp != 0xFFFF)
        {
            param_value_len = tmp - param_value_pos;
        }
        else
        {
            param_value_len = p->tot_len - param_value_pos;
        }
        if (param_value_len > 0 && param_value_len < value_buf_len)
        {
            char *result = (char *)pbuf_get_contiguous(p, value_buf, value_buf_len, param_value_len, param_value_pos);
            if (result)
            {
                result[param_value_len] = 0;
                return result;
            }
        }
    }
    return NULL;
}


err_t httpd_post_receive_data(void *connection, struct pbuf *p)
{
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
    _whm_http_server_current_rest_req = _WHM_HTTP_SERVER_REST_GET;
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
    switch (_whm_http_server_current_rest_req)
    {
        case _WHM_HTTP_SERVER_REST_GET:
        {
            _whm_http_server_rest_get_handler_t* h = _whm_http_server_rest_get_handler_find(name);
            ret = (NULL != h && ERR_OK == h->handler(file, name));
            break;
        }
        case _WHM_HTTP_SERVER_REST_POST:
        {
            _whm_http_server_rest_post_handler_t* h = _whm_http_server_rest_post_handler_find(name);
            if (NULL != h)
            {
                file->data = _whm_http_server_response_buffer;
                file->len = strnlen(_whm_http_server_response_buffer, _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
                file->index = file->len;
                file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
            }
            break;
        }
        default:
            break;
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
__WHM_HTTP_SERVER_CGI_HANDLER_DEFAULT(app, "/app.js")
__WHM_HTTP_SERVER_CGI_HANDLER_DEFAULT(styles, "/styles.css")


static err_t _whm_http_server_rest_get_handler_config(struct fs_file *file, const char* name)
{
    const char* config = whm_config_get_string();
    file->data = config;
    file->len = strlen(config);
    file->index = file->len;
    file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
    return ERR_OK;
}


static err_t _whm_http_server_rest_get_handler_meas(struct fs_file *file, const char* name)
{
    whm_htu31d_get(NULL, _whm_http_server_meas_finish);
    while (!_whm_http_server_meas.done)
    {
        tight_loop_contents();
        whm_htu31d_iterate();
    }
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


static void _whm_http_server_meas_finish(void* userdata, bool success, uint32_t rh_e3, int32_t t_e3)
{
    _whm_http_server_meas.done = true;
    _whm_http_server_meas.success = success;
    _whm_http_server_meas.rel_hum = rh_e3;
    _whm_http_server_meas.temperature = t_e3;
}


static err_t _whm_http_server_rest_post_handler_config_begin(const char* http_request, uint16_t http_request_len, int content_len, char* response_uri, uint16_t response_uri_len, uint8_t* post_auto_wnd)
{
    _whm_http_server_config_buffer[0] = '\0';
    *post_auto_wnd = 1;
    return ERR_OK;
}


static err_t _whm_http_server_rest_post_handler_config_recv(struct pbuf* p)
{
    int ret = ERR_VAL;
    if (p && p->len < _WHM_HTTP_SERVER_CONFIG_BUFFER_SIZE)
    {
        memcpy(_whm_http_server_config_buffer, p->payload, p->len);
        _whm_http_server_config_buffer[p->len] = '\0';
        ret = ERR_OK;
    }
    return ret;
}


static err_t _whm_http_server_rest_post_handler_config_finish(char* response_uri, uint16_t response_uri_len)
{
    strncpy(_whm_http_server_response_buffer, "{\"status\":\"ok\"}", _WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1);
    _whm_http_server_response_buffer[_WHM_HTTP_SERVER_RESPONSE_BUFFER_SIZE-1] = '\0';
    return ERR_OK;
}
