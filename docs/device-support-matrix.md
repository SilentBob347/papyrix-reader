# Device Support Matrix

## Build Targets

| Checker target | PlatformIO environments | MCU | Devices |
| --- | --- | --- | --- |
| `default` | `default`, `release_xteink_c3` | ESP32-C3 | X3, X4 |
| `x4pro` | `x4pro`, `release_x4pro` | ESP32-S3 | X4 Pro |
| `x4c` | `x4c`, `release_x4c` | ESP32-S3 | X4 v2 Classic |

## Release Artifacts

`make package` writes the release images and `manifest.json` to `dist/`.
Packaging checks target features before it copies an image.

| Artifact | Devices |
| --- | --- |
| `papyrix-xteink-c3.bin` | X3, X4 |
| `papyrix-x4pro.bin` | X4 Pro |
| `papyrix-x4c.bin` | X4 v2 Classic; partial hardware validation |

The manifest contains the version, environment, MCU, flash offset, SHA-256,
profile schema, board values, and panel values.
The panel list identifies supported controllers.

## Hardware Services

| Device | Battery | RTC | Front light | USB detection |
| --- | --- | --- | --- | --- |
| X3 | BQ27220 | DS3231 | None | Native USB and battery charge state |
| X4 | ADC | None | None | Native USB and GPIO20 |
| X4 Pro | CW2017 and GPIO21 | BM8563 | GPIO8 cool, GPIO9 warm PWM | Native USB and battery charge state |
| X4 v2 Classic | CW2017 factory calibration, GPIO21 | BM8563 | None | Native USB and battery charge state |

A valid RTC value sets system time.
A successful NTP update writes UTC time to an available RTC.
Wi-Fi starts on demand.
Deep sleep stops Wi-Fi and turns off the front light.
Settings > Screen contains Brightness and Warmth on X4 Pro.

The CPU policy uses 10 MHz at idle.
It restores 160 MHz on ESP32-C3 and 240 MHz on ESP32-S3 for active work.
Classic PSRAM capacity and operation at the idle frequency are not verified.

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

| Symbol group | C3 | X4 Pro | X4 Classic |
| --- | --- | --- | --- |
| Pro touch board code | Forbidden | Allowed | Forbidden |
| Shared S3 panel drivers | Forbidden | Allowed | Allowed |
| GT911 | Forbidden | Allowed | Forbidden |
| Front-light backend | Not used | Allowed | Forbidden |
| PaperMono | Forbidden | Forbidden | Forbidden |
| FT6336 | Forbidden | Forbidden | Forbidden |
| Swipe | Forbidden | Forbidden | Forbidden |
| Bluetooth | Forbidden | Forbidden | Forbidden |
| `X4ProVariant` | Forbidden | Forbidden | Forbidden |

## X4 Classic

Classic has seven active-low buttons.
Back, Confirm, Left, Right, Up, Down, and Power use GPIO9, GPIO8, GPIO5, GPIO2, GPIO7, GPIO0, and GPIO3.
The display uses SCLK12, MOSI11, CS13, DC14, RESET10, and BUSY18 at 10 MHz.
SDMMC uses CLK41, CMD42, DAT0=40, and the active-low GPIO6 power rail.
Sleep keeps GPIO1 and RESET10 HIGH and holds the SD rail off.
RTC and gauge use SDA39/SCL38 at 400 kHz.
GPIO4 remains unused.

The panel selector reads `hw_calib/screenType` without writing NVS.
Values 1 and 0x0B select UC8179. Values 2 and 0x0C select UC8279.
Value 3 selects SSD1677.
Missing or invalid values use a bounded three-byte display probe.
An unknown response leaves the display disabled.
USB/power recovery can retry selection. SD `/force_update.bin` recovery can run without a display.

Classic UC8279 keeps the factory PLL setting.
Variants 0x02 and 0x03 use QY grayscale tables.
Variants 0x68 and 0x69 use ZHX tables.
Variant 0x67 and unknown variants use monochrome rendering for text and images.
The firmware preserves the user antialiasing preference.

Classic does not program CW2017 BATINFO or restart the gauge to install a profile.
A running factory gauge supplies measurements.
An unavailable or uninitialized gauge reports unknown charge.
Device checks pass on a Classic with UC8279 variant `0x68`.
Other Classic panel variants, sleep current, and battery accuracy are not verified.
See [hardware qualification](x4-classic-hardware-validation.md).
USB mass storage and motion-based navigation are not included.
