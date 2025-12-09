#include <stdio.h>
#include <stdint.h>

#include "pico/time.h"
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "lwip/pbuf.h"
#include "lwip/tcp.h"

#include "dhcp_server.h"
#include "http_server.h"
#include "htu31d.h"
#include "ap_station.h"
#include "config.h"
#include "util.h"


int main(int argc, char **argv)
{
    stdio_init_all();

    whm_htu31d_init();

    int gpio_toggle = 1;
    if (cyw43_arch_init())
    {
        printf("Wi-Fi init failed\n");
        return -1;
    }
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, gpio_toggle);

    printf("----start----\n");
    printf("Version : %s\n", FIRMWARE_SHA1);

    whm_config_init();

    int ret = whm_ap_station_init();
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

    bool done = false;
    uint64_t loop_time = 0;
    while (!done)
    {
        while (time_us_64() - loop_time < WHM_MS_TO_US(whm_conf.blinking_ms))
        {
            tight_loop_contents();
            whm_ap_station_iterate();
            whm_htu31d_iterate();
        }
        loop_time = time_us_64();
        gpio_toggle = (gpio_toggle + 1) % 2;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, gpio_toggle);
    }

    whm_http_server_deinit(&http_server);
    whm_dhcp_server_deinit(&dhcp_server);
    whm_htu31d_deinit();
    return 0;
}
