#include <stdio.h>
#include <stdint.h>

#include "pico/time.h"
#include "pico/stdlib.h"

#include "pico/cyw43_arch.h"


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

    uint64_t loop_time = 0;
    while (1)
    {
        while (time_us_64() - loop_time < 250000)
        {
            tight_loop_contents();
        }
        loop_time = time_us_64();
        gpio_toggle = (gpio_toggle + 1) % 2;
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, gpio_toggle);
    }
    return 0;
}
