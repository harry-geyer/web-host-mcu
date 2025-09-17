#pragma once


typedef struct whm_http_server
{
} whm_http_server_t;

int whm_http_server_init(whm_http_server_t* server);
void whm_http_server_deinit(whm_http_server_t* server);
