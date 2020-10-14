# esm-amp

This project implements a minimal AMP protocol stack for ESP32 SoCs. It is part of the prototype implementation of my [masters thesis](https://github.com/joschal/thesis). See section 5.2.

The project is based on sample code, provided by Espressif under CC0 license.

## Build instructions

To build this project, the Espressif SDK needs to be installed. Then, the software can be built and flashed with the usual SDK tools:

```
idf.py [fullclean] build
idf.py flash [monitor]
```

To use the resulting network of multiple ESP32, an additional WLAN router is needed, for the underling ESP-MESH layer. Its SSID and password need to be configured. This can be done manually by editing the file `sdkconfig`. But the preferred method is to use `idf.py menuconfig`
