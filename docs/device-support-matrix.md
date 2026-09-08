# Device Support Matrix

## Build Targets

| Checker target | PlatformIO environments | MCU | Devices |
| --- | --- | --- | --- |
| `default` | `default`, `release_xteink_c3` | ESP32-C3 | X3, X4 |
| `x4pro` | `x4pro`, `release_x4pro` | ESP32-S3 | X4 Pro |

## Release Artifacts

`make package` writes the release images and `manifest.json` to `dist/`.
Packaging checks target features before it copies an image.

| Artifact | Devices |
| --- | --- |
| `papyrix-xteink-c3.bin` | X3, X4 |
| `papyrix-x4pro.bin` | X4 Pro |

The manifest contains the version, environment, MCU, flash offset, SHA-256,
profile schema, board values, and panel values.
The panel list identifies supported controllers.

## Hardware Services

| Device | Battery | RTC | Front light | USB detection |
| --- | --- | --- | --- | --- |
| X3 | BQ27220 | DS3231 | None | Native USB and battery charge state |
| X4 | ADC | None | None | Native USB and GPIO20 |
| X4 Pro | CW2017 and GPIO21 | BM8563 | GPIO8 cool, GPIO9 warm PWM | Native USB and battery charge state |

A valid RTC value sets system time.
A successful NTP update writes UTC time to an available RTC.
Wi-Fi starts on demand.
Deep sleep stops Wi-Fi and turns off the front light.
Settings > Screen contains Brightness and Warmth on X4 Pro.

The CPU policy uses 10 MHz at idle.
It restores 160 MHz on ESP32-C3 and 240 MHz on ESP32-S3 for active work.

## Touch Input

Only the X4 Pro artifact includes GT911 support.
The controller uses the shared 100 kHz I2C bus on GPIO39/GPIO38.
GPIO2 controls its active-low power rail.
GPIO4 controls reset.
GPIO10 receives interrupts.

Reader touch zones use 24 percent of the width for Previous,
52 percent for Menu, and 24 percent for Next.
The side-button preference reverses the page zones.
Disabling Touch page turns disables the page zones only.
Menu taps, overlay taps, and physical buttons remain active.
The classifier rejects movement and multiple contacts.
It suppresses contacts during refresh and stale contacts after wake.

## Target Isolation

| Symbol group | C3 | X4 Pro |
| --- | --- | --- |
| X4 Pro board code | Forbidden | Allowed |
| GT911 | Forbidden | Allowed |
| PaperMono | Forbidden | Forbidden |
| FT6336 | Forbidden | Forbidden |
| Swipe | Forbidden | Forbidden |
| Bluetooth | Forbidden | Forbidden |
| `X4ProVariant` | Forbidden | Forbidden |
