
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#include "lwip/ip4_addr.h"
#include "lwip/init.h"
#include "lwip/apps/httpd.h"

#include "http_server.h"


static const char* _whm_http_server_cgi_handler_index(int index, int num_params, char *pc_param[], char *pc_value[]);
static void* _whm_http_server_current_connection = NULL;


static tCGI _whm_http_server_cgi_handlers[] =
{
    { "/", _whm_http_server_cgi_handler_index },
    { "/index.html", _whm_http_server_cgi_handler_index },
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
    if (memcmp(uri, "/led.cgi", 8) == 0 && _whm_http_server_current_connection != connection)
    {
        _whm_http_server_current_connection = connection;
        snprintf(response_uri, response_uri_len, "/ledfail.shtml");
        *post_auto_wnd = 1;
        return ERR_OK;
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
    LWIP_ASSERT("NULL pbuf", p != NULL);
    if (_whm_http_server_current_connection == connection)
    {
    }
    pbuf_free(p);
    return ret;
}


void httpd_post_finished(void *connection, char *response_uri, u16_t response_uri_len)
{
    if (_whm_http_server_current_connection == connection)
    {
    }
    _whm_http_server_current_connection = NULL;
}


static const char* _whm_http_server_cgi_handler_index(int index, int num_params, char *pc_param[], char *pc_value[])
{
    return "/index.html";
}
