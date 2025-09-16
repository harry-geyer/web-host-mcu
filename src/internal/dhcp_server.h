#pragma once

#include "lwip/ip_addr.h"


#define WHM_DHCP_SERVER_MAX_IP          8


typedef struct whm_dhcp_server_lease {
    uint8_t mac[6];
    uint16_t expiry;
} whm_dhcp_server_lease_t;


typedef struct whm_dhcp_server
{
    ip_addr_t ip;
    ip_addr_t nm;
    whm_dhcp_server_lease_t lease[WHM_DHCP_SERVER_MAX_IP];
    struct udp_pcb *udp;
} whm_dhcp_server_t;


int whm_dhcp_server_init(whm_dhcp_server_t* server);
void whm_dhcp_server_deinit(whm_dhcp_server_t* server);
