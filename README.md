# ThermalPrinterEsp32

Originally based on [Dimsy's](https://github.com/Dimsy/thermonotes/tree/master) code, but heavily modified.

`ThermalPrinterEsp32` is designed for the ESP32 family of microcontrollers and supports EPSON-compatible thermal printers.

## Features

- Print text
- Print QR codes
- Print black-and-white images
- Web-based interface for managing print jobs

The ESP32 exposes a web interface that can be accessed at `printer.local` or via the device's IP address.

## Building

Create a `secrets.h` file in the `include` directory:

```cpp
#pragma once

const char WIFI_SSID[] = <WIFI_SSID>;
const char WIFI_PASSWORD[] = <WIFI_PASSWORD>;
```

Set the desired timezone in `platformio.ini`:

```ini
build_flags =
    -D LOCATION_TIMEZONE=\"Europe/Berlin\"
```

## Filesystem Setup

The web interface requires `LittleFS`. Before uploading the firmware, you must also upload a filesystem image containing the web assets stored in the project's `data` directory.

In PlatformIO:

1. Click **Build Filesystem Image**.
2. Click **Upload Filesystem Image**.

## TODO

- [ ] Template support
- [ ] Option to add timestamps to printed output
