#include <stdio.h>
#include <stdint.h>
#include <errno.h>

#include "pico/time.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcp_server.h"


#define _WHM_DHCP_SERVER_PACKET_MIN_SIZE            (240 + 3)
#define _WHM_DHCP_SERVER_MAC_LEN                    6
#define _WHM_DHCP_SERVER_BASE_IP                    16
#define _WHM_DHCP_SERVER_PORT                       67
#define _WHM_DHCP_SERVER_CLIENT_PORT                68
#define _WHM_DHCP_SERVER_DEFAULT_LEASE_TIME_S       (24 * 60 * 60) // in seconds


typedef enum _whm_dhcp_server_packet_type
{
    _WHM_DHCP_SERVER_PACKET_TYPE_DISCOVER   = 1,
    _WHM_DHCP_SERVER_PACKET_TYPE_OFFER      = 2,
    _WHM_DHCP_SERVER_PACKET_TYPE_REQUEST    = 3,
    _WHM_DHCP_SERVER_PACKET_TYPE_DECLINE    = 4,
    _WHM_DHCP_SERVER_PACKET_TYPE_ACK        = 5,
    _WHM_DHCP_SERVER_PACKET_TYPE_NACK       = 6,
    _WHM_DHCP_SERVER_PACKET_TYPE_RELEASE    = 7,
    _WHM_DHCP_SERVER_PACKET_TYPE_INFORM     = 8,
} _whm_dhcp_server_packet_type_t;


typedef enum _whm_dhcp_server_opt
{
    _WHM_DHCP_SERVER_OPT_PAD                = 0,
    _WHM_DHCP_SERVER_OPT_SUBNET_MASK        = 1,
    _WHM_DHCP_SERVER_OPT_ROUTER             = 3,
    _WHM_DHCP_SERVER_OPT_DNS                = 6,
    _WHM_DHCP_SERVER_OPT_HOST_NAME          = 12,
    _WHM_DHCP_SERVER_OPT_REQUESTED_IP       = 50,
    _WHM_DHCP_SERVER_OPT_IP_LEASE_TIME      = 51,
    _WHM_DHCP_SERVER_OPT_MSG_TYPE           = 53,
    _WHM_DHCP_SERVER_OPT_SERVER_ID          = 54,
    _WHM_DHCP_SERVER_OPT_PARAM_REQUEST_LIST = 55,
    _WHM_DHCP_SERVER_OPT_MAX_MSG_SIZE       = 57,
    _WHM_DHCP_SERVER_OPT_VENDOR_CLASS_ID    = 60,
    _WHM_DHCP_SERVER_OPT_CLIENT_ID          = 61,
    _WHM_DHCP_SERVER_OPT_END                = 255,
} _whm_dhcp_server_opt_t;


typedef struct _whm_dhcp_server_msg
{
    uint8_t op; // message opcode
    uint8_t htype; // hardware address type
    uint8_t hlen; // hardware address length
    uint8_t hops;
    uint32_t xid; // transaction id, chosen by client
    uint16_t secs; // client seconds elapsed
    uint16_t flags;
    uint8_t ciaddr[4]; // client IP address
    uint8_t yiaddr[4]; // your IP address
    uint8_t siaddr[4]; // next server IP address
    uint8_t giaddr[4]; // relay agent IP address
    uint8_t chaddr[16]; // client hardware address
    uint8_t sname[64]; // server host name
    uint8_t file[128]; // boot file name
    uint8_t options[312]; // optional parameters, variable, starts with magic
} _whm_dhcp_server_msg_t;


static void _dhcp_server_process(void* userdata, struct udp_pcb* upcb, struct pbuf* p, const ip_addr_t* src_addr, uint16_t src_port);
static uint8_t* _whm_dhcp_server_opt_find(uint8_t* opt, uint8_t cmd);
static void _whm_dhcp_server_opt_write_n(uint8_t** opt, uint8_t cmd, size_t n, const void* data);
static void _whm_dhcp_server_opt_write_u32(uint8_t** opt, uint8_t cmd, uint32_t val);
static void _whm_dhcp_server_opt_write_u8(uint8_t** opt, uint8_t cmd, uint8_t val);
static int _whm_dhcp_server_dhcp_socket_sendto(struct udp_pcb** udp, struct netif* nif, const void* buf, size_t len, uint32_t ip, uint16_t port);


int whm_dhcp_server_init(whm_dhcp_server_t* server)
{
    server->nm.addr = PP_HTONL(CYW43_DEFAULT_IP_MASK);
    server->ip.addr = PP_HTONL(CYW43_DEFAULT_IP_AP_ADDRESS);
    memset(server->lease, 0, WHM_DHCP_SERVER_MAX_IP * sizeof(whm_dhcp_server_lease_t));
    server->udp = udp_new();
    if (!server->udp)
    {
        printf("Unable to allocate memory for udp.");
        return -ENOMEM;
    }
    udp_recv(server->udp, _dhcp_server_process, (void*)server);
    udp_bind(server->udp, IP_ANY_TYPE, _WHM_DHCP_SERVER_PORT);
    return 0;
}


void whm_dhcp_server_deinit(whm_dhcp_server_t* server)
{
    if (server->udp)
    {
        udp_remove(server->udp);
        server->udp = NULL;
    }
}


static void _dhcp_server_process(void* userdata, struct udp_pcb* upcb, struct pbuf* p, const ip_addr_t* src_addr, uint16_t src_port)
{
    whm_dhcp_server_t* server = userdata;
    (void)upcb;
    (void)src_addr;
    (void)src_port;
    if (_WHM_DHCP_SERVER_PACKET_MIN_SIZE > p->tot_len)
    {
        /* packet too small to be a DHCP packet */
        goto exit;
    }

    _whm_dhcp_server_msg_t dhcp_msg;
    size_t len = pbuf_copy_partial(p, &dhcp_msg, sizeof(dhcp_msg), 0);
    if (_WHM_DHCP_SERVER_PACKET_MIN_SIZE > len)
    {
        /* copied section too small to be a DHCP packet */
        goto exit;
    }
    dhcp_msg.op = _WHM_DHCP_SERVER_PACKET_TYPE_OFFER;
    memcpy(&dhcp_msg.yiaddr, &ip4_addr_get_u32(ip_2_ip4(&server->ip)), 4);
    uint8_t *opt = (uint8_t *)&dhcp_msg.options;
    opt += 4;
    uint8_t *msgtype = _whm_dhcp_server_opt_find(opt, _WHM_DHCP_SERVER_OPT_MSG_TYPE);
    if (!msgtype)
    {
        /* no message type in the DHCP packet */
        goto exit;
    }

    switch (msgtype[2])
    {
        case _WHM_DHCP_SERVER_PACKET_TYPE_DISCOVER:
        {
            int yi = WHM_DHCP_SERVER_MAX_IP;
            for (int i = 0; i < WHM_DHCP_SERVER_MAX_IP; ++i)
            {
                if (memcmp(server->lease[i].mac, dhcp_msg.chaddr, _WHM_DHCP_SERVER_MAC_LEN) == 0)
                {
                    /* already assigned, IP address with existing lease,
                     * reassign */
                    yi = i;
                    break;
                }
                if (yi == WHM_DHCP_SERVER_MAX_IP)
                {
                    /* not assigned, find free spot */
                    if (memcmp(server->lease[i].mac, "\x00\x00\x00\x00\x00\x00", _WHM_DHCP_SERVER_MAC_LEN) == 0)
                    {
                        yi = i;
                    }
                    uint32_t expiry = server->lease[i].expiry << 16 | 0xffff;
                    if ((int32_t)(expiry - cyw43_hal_ticks_ms()) < 0)
                    {
                        /* lease expired */
                        memset(server->lease[i].mac, 0, _WHM_DHCP_SERVER_MAC_LEN);
                        yi = i;
                    }
                }
            }
            if (yi == WHM_DHCP_SERVER_MAX_IP)
            {
                /* no lease space left */
                goto exit;
            }
            dhcp_msg.yiaddr[3] = _WHM_DHCP_SERVER_BASE_IP + yi;
            _whm_dhcp_server_opt_write_u8(&opt, _WHM_DHCP_SERVER_OPT_MSG_TYPE, _WHM_DHCP_SERVER_PACKET_TYPE_OFFER);
            break;
        }
        case _WHM_DHCP_SERVER_PACKET_TYPE_REQUEST:
        {
            uint8_t *o = _whm_dhcp_server_opt_find(opt, _WHM_DHCP_SERVER_OPT_REQUESTED_IP);
            if (!o)
            {
                goto exit;
            }
            if (0 != memcmp(o + 2, &ip4_addr_get_u32(ip_2_ip4(&server->ip)), 3))
            {
                goto exit;
            }
            uint8_t yi = o[5] - _WHM_DHCP_SERVER_BASE_IP;
            if (yi >= WHM_DHCP_SERVER_MAX_IP)
            {
                goto exit;
            }
            if (0 == memcmp(server->lease[yi].mac, dhcp_msg.chaddr, _WHM_DHCP_SERVER_MAC_LEN))
            {
            }
            else if (0 == memcmp(server->lease[yi].mac, "\x00\x00\x00\x00\x00\x00", _WHM_DHCP_SERVER_MAC_LEN))
            {
                memcpy(server->lease[yi].mac, dhcp_msg.chaddr, _WHM_DHCP_SERVER_MAC_LEN);
            }
            else
            {
                /* IP already in use */
                goto exit;
            }
            server->lease[yi].expiry = (cyw43_hal_ticks_ms() + _WHM_DHCP_SERVER_DEFAULT_LEASE_TIME_S * 1000) >> 16;
            dhcp_msg.yiaddr[3] = _WHM_DHCP_SERVER_BASE_IP + yi;
            _whm_dhcp_server_opt_write_u8(&opt, _WHM_DHCP_SERVER_OPT_MSG_TYPE, _WHM_DHCP_SERVER_PACKET_TYPE_ACK);
            printf(
                "DHCPS: client connected: MAC=%02x:%02x:%02x:%02x:%02x:%02x IP=%u.%u.%u.%u\n",
                dhcp_msg.chaddr[0], dhcp_msg.chaddr[1], dhcp_msg.chaddr[2], dhcp_msg.chaddr[3], dhcp_msg.chaddr[4], dhcp_msg.chaddr[5],
                dhcp_msg.yiaddr[0], dhcp_msg.yiaddr[1], dhcp_msg.yiaddr[2], dhcp_msg.yiaddr[3]
            );
            break;
        }
        default:
            goto exit;
    }
    _whm_dhcp_server_opt_write_n(&opt, _WHM_DHCP_SERVER_OPT_SERVER_ID, 4, &ip4_addr_get_u32(ip_2_ip4(&server->ip)));
    _whm_dhcp_server_opt_write_n(&opt, _WHM_DHCP_SERVER_OPT_SUBNET_MASK, 4, &ip4_addr_get_u32(ip_2_ip4(&server->nm)));
    _whm_dhcp_server_opt_write_n(&opt, _WHM_DHCP_SERVER_OPT_ROUTER, 4, &ip4_addr_get_u32(ip_2_ip4(&server->ip)));
    _whm_dhcp_server_opt_write_n(&opt, _WHM_DHCP_SERVER_OPT_DNS, 4, &ip4_addr_get_u32(ip_2_ip4(&server->ip)));
    _whm_dhcp_server_opt_write_u32(&opt, _WHM_DHCP_SERVER_OPT_IP_LEASE_TIME, _WHM_DHCP_SERVER_DEFAULT_LEASE_TIME_S);
    *opt++ = _WHM_DHCP_SERVER_OPT_END;
    struct netif *nif = ip_current_input_netif();
    _whm_dhcp_server_dhcp_socket_sendto(&server->udp, nif, &dhcp_msg, opt - (uint8_t *)&dhcp_msg, 0xffffffff, _WHM_DHCP_SERVER_CLIENT_PORT);

exit:
    pbuf_free(p);
}


static uint8_t* _whm_dhcp_server_opt_find(uint8_t* opt, uint8_t cmd)
{
    for (int i = 0; i < 308 && opt[i] != _WHM_DHCP_SERVER_OPT_END;)
    {
        if (opt[i] == cmd)
        {
            return &opt[i];
        }
        i += 2 + opt[i + 1];
    }
    return NULL;
}


static void _whm_dhcp_server_opt_write_n(uint8_t** opt, uint8_t cmd, size_t n, const void* data)
{
    uint8_t* o = *opt;
    *o++ = cmd;
    *o++ = n;
    memcpy(o, data, n);
    *opt = o + n;
}


static void _whm_dhcp_server_opt_write_u32(uint8_t** opt, uint8_t cmd, uint32_t val)
{
    uint8_t *o = *opt;
    *o++ = cmd;
    *o++ = 4;
    *o++ = val >> 24;
    *o++ = val >> 16;
    *o++ = val >> 8;
    *o++ = val;
    *opt = o;
}


static void _whm_dhcp_server_opt_write_u8(uint8_t** opt, uint8_t cmd, uint8_t val)
{
    uint8_t* o = *opt;
    *o++ = cmd;
    *o++ = 1;
    *o++ = val;
    *opt = o;
}


static int _whm_dhcp_server_dhcp_socket_sendto(struct udp_pcb** udp, struct netif* nif, const void* buf, size_t len, uint32_t ip, uint16_t port)
{
    if (len > 0xffff)
    {
        len = 0xffff;
    }

    struct pbuf *p = pbuf_alloc(PBUF_TRANSPORT, len, PBUF_RAM);
    if (p == NULL)
    {
        return -ENOMEM;
    }

    memcpy(p->payload, buf, len);

    ip_addr_t dest;
    IP4_ADDR(ip_2_ip4(&dest), ip >> 24 & 0xff, ip >> 16 & 0xff, ip >> 8 & 0xff, ip & 0xff);
    err_t err;
    if (nif != NULL)
    {
        err = udp_sendto_if(*udp, p, &dest, port, nif);
    }
    else
    {
        err = udp_sendto(*udp, p, &dest, port);
    }

    pbuf_free(p);

    if (err != ERR_OK) {
        return err;
    }

    return len;
}
