#include <stdio.h>
#include <stdint.h>

#include "pico/time.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcp_server.h"
#include "http_server.h"


#define WHM_AP_SSID             "Web-Host MCU"
#define WHM_AP_PASSWORD         "host52%files"


static int _init_ap(const char* ssid, const char* password);
static int _init_dhcp(ip_addr_t* gateway);


static bool _done = false;


int main(int argc, char **argv)
{
    stdio_init_all();

    int gpio_toggle = 1;
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, gpio_toggle);

    printf("----start----\n");
    printf("Version : %s\n", FIRMWARE_SHA1);

    int ret = _init_ap(WHM_AP_SSID, WHM_AP_PASSWORD);
    if (ret)
    {
        printf("Failed to initialise access point\n");
        return ret;
    }
    whm_dhcp_server_t dhcp_server;
    ret = whm_dhcp_server_init(&dhcp_server);
    if (ret)
    {
        printf("Failed to initialise dhcp server\n");
        return ret;
    }

    whm_http_server_t http_server;
    ret = whm_http_server_init(&http_server);
    if (ret)
    {
        printf("Failed to initialise tcp server\n");
        return ret;
    }

    uint64_t loop_time = 0;
    while (!_done)
    {
        while (time_us_64() - loop_time < 250000)
        {
            tight_loop_contents();
        }
        loop_time = time_us_64();
        gpio_toggle = (gpio_toggle + 1) % 2;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, gpio_toggle);
    }

    whm_http_server_deinit(&http_server);
    whm_dhcp_server_deinit(&dhcp_server);
    return 0;
}


static int _init_ap(const char* ssid, const char* password)
{
    cyw43_arch_enable_ap_mode(ssid, password, CYW43_AUTH_WPA2_AES_PSK);
    return 0;
}
