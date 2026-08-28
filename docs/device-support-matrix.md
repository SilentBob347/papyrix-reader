# Device Support Matrix

## Build Targets

| Checker target | PlatformIO environments | MCU | Devices |
| --- | --- | --- | --- |
| `default` | `default`, `release_xteink_c3` | ESP32-C3 | X3, X4 |
| `x4pro` | `x4pro`, `release_x4pro` | ESP32-S3 | X4 Pro |

## Release Artifacts

`make package` writes the release images and `manifest.json` to `dist/`.
Packaging checks target features before it copies an image.

| Artifact | Devices | Hardware status |
| --- | --- | --- |
| `papyrix-xteink-c3.bin` | X3, X4 | Partial verification |
| `papyrix-x4pro.bin` | X4 Pro | Partial verification on UC8279 |

The manifest contains the version, environment, MCU, flash offset, SHA-256,
profile schema, board values, panel values, and hardware status.
The panel list identifies supported controllers.
It does not identify physically tested variants.
The X4 Pro UC8179 path has host-test coverage only.

## Physical Verification

The table lists tested operations, not complete device certification.

| Device | Tested operations |
| --- | --- |
| X3 | USB and pogo installation; Settings and emergency firmware update; unplugged cold boot; Home; buttons; SD access and remount; book access; page turns; UI/Reader transitions; Wi-Fi start and shutdown; short and extended unplugged sleep; exact-page resume; four orientations; BQ27220 battery; RTC time; display patterns; forced reset during refresh |
| X4 | USB installation; Settings and emergency firmware update; unplugged cold boot; Home; buttons; SD access and remount; book access; page turns; UI/Reader transitions; Wi-Fi start and shutdown; short and extended unplugged sleep; exact-page resume; four orientations; ADC battery; USB charging indication; display patterns; forced reset during refresh |
| X4 Pro, UC8279 | Packaged USB installation; application power latch; SDMMC mount; buttons; GT911 taps; CW2017 battery; BM8563 RTC; Home and reader navigation; cover resume; Screen settings; front-light controls; Wi-Fi book transfer; antialiasing; 20 forward and 5 backward page turns; full refresh on each page; short sleep and wake; button and touch input after idle |

X4 Pro physical verification does not cover UC8179, extended sleep,
forced-reset display recovery, all touch orientations, Settings firmware update,
or emergency firmware update.
No current or inactive-rail voltage measurements establish power consumption.

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

Run the checker on a demangled symbol listing:

```bash
nm -C .pio/build/release_xteink_c3/firmware.elf > symbols.txt
python scripts/check_target_features.py symbols.txt default
```

The checker returns `0` for success, `1` for a forbidden symbol,
and `2` for a usage error.

| Symbol group | C3 | X4 Pro |
| --- | --- | --- |
| X4 Pro board code | Forbidden | Allowed |
| GT911 | Forbidden | Allowed |
| PaperMono | Forbidden | Forbidden |
| FT6336 | Forbidden | Forbidden |
| Swipe | Forbidden | Forbidden |
| Bluetooth | Forbidden | Forbidden |
| `X4ProVariant` | Forbidden | Forbidden |

`make test` checks valid listings, forbidden listings, and usage errors.
