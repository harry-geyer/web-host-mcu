# Web-Host-MCU

This project is a web-configurable IoT sensor that provides an wireless
access point, DHCP server, TCP/HTTP server to allow users to connect to
configure.

The MCU chosen for this project was the RP2350 because:

- the provided SDK (pico-sdk) has integrated lwip for the network stack,
- it is cheap and easy to get going with,
- has enough memory and flash for hosting webpages and network stack,
- I prefer this SDK over others, such as the ESP-IDF.

The sensor includes a driver for a HTU31D for measuring temperature and
humidity.

There is a separate (3rd stage in this case) bootloader, this is with
the idea that both application and bootloader can be updated OTA (over
the air), but this is not yet implemented.

## Building

Dependancies:

- cmake
- make
- gcc-arm-none-eabi
- pico-sdk (see its own installation instructions, PICO_SDK_PATH needs
  to be exported)
- python3 (just for developing really)
- python3-venv
- terser
- cleancss
- webpack-cli

To build:

    $ mkdir build
    $ cd build/
    $ cmake .. && make -j

## Flashing

Follow normal Pico procedure flashing (typically holding the boot button
when plugging in power), mount and copy over `firmware.uf2` to the
storage device. The MCU will reboot into the new firmware, the LED will
start flashing.

After building, there will be several `.bin` and `.uf2` files. As
touched on before, there is a separate bootloader, and the main program
is called `application`. These have been combined into a `firmware.bin`
and `firmware.uf2`.

## Using

With the device freshly flashed, it will start a wireless access point
with the SSID "Web-Host MCU", connect to with another device
(phone/laptop) and the password "host52%files".

Now with your browser of choice visit (http://192.168.4.1)[http://192.168.4.1].
You will be able to configure the device and the connection details so
the device can connect to the network and post the measurements it
collects.

## Developing

There is an included cmake rule `fake_host` this is for hosting the
website without the use of the MCU. It just hosts it with python3 fastapi.
This is useful when developing the hosted webpage as you do not need to
continually flash the MCU, but of course the backend of the API is
completely different and would need to be implemented on both the fake_host
and the real embedded application.

Both the real, embedded webserver and the development webserver will
host a single minified HTML file which reduces the number of requests to
the server.

License: see License file.
