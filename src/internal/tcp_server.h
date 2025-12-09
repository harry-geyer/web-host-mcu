#pragma once

#include <stdbool.h>

#include "lwip/pbuf.h"
#include "lwip/tcp.h"


#define WHM_TCP_SERVER_BUFFER_SIZE              2048


typedef struct whm_tcp_server
{
    struct tcp_pcb* server_pcb;
    struct tcp_pcb* client_pcb;
    uint8_t buffer_sent[WHM_TCP_SERVER_BUFFER_SIZE];
    uint8_t buffer_recv[WHM_TCP_SERVER_BUFFER_SIZE];
    int sent_len;
    int recv_len;
} whm_tcp_server_t;


int whm_tcp_server_init(whm_tcp_server_t* server);
void whm_tcp_server_deinit(whm_tcp_server_t* server;
