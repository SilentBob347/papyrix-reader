# Device Specifications

## Devices

| Device | MCU | Display | Storage | Touch |
| --- | --- | --- | --- | --- |
| [X3](x3-specifications.md) | ESP32-C3 | 792 × 528, UC8253 or UC8279d | SPI SD | None |
| [X4](x4-specifications.md) | ESP32-C3 | 800 × 480, SSD1677 | SPI SD | None |
| [X4 Pro](x4pro-specifications.md) | ESP32-S3, 8 MB octal PSRAM | 800 × 480, UC8279 or UC8179 | 1-bit SDMMC | GT911 |

See the [device support matrix](device-support-matrix.md) for hardware services
and physical verification limits.

## Firmware

- `papyrix-xteink-c3.bin` selects X3 or X4 at startup.
- `papyrix-x4pro.bin` uses the fixed X4 Pro board profile.
- Both artifacts use 16 MB flash and application offset `0x10000`.

Do not install an artifact for a different MCU or board.
An incorrect image can drive the wrong GPIOs and damage hardware.

## Hardware Selection

`lib/BoardSupport` owns board profiles, hardware identity, and selection policy.
The C3 artifact checks the manual override before the saved board selection.
It probes X3 I2C devices when no saved selection applies.
Board detection and display-controller detection are separate operations.
See [X3 controller detection](x3-specifications.md#controller-detection).
Each probe pass checks the BQ27220, DS3231, and QMI8658.
Two or more detected devices in both passes select X3.
Zero detected devices in both passes select X4.
The firmware saves either conclusive result in NVS.
Other results select X4 for that boot without saving a detection result.

Board and panel selections use the `papyrix_hw` NVS namespace.
The C3 keys are `dev_ovr`, `dev_det`, `epd_ovr`, `epd_det`, and `epd_ver`.
Recovery diagnostics use `papyrix_diag`.

## Power

The CPU policy uses 10 MHz at idle.
Active work uses 160 MHz on ESP32-C3 and 240 MHz on ESP32-S3.
GPIO3 receives the Power button input on all three devices.

The firmware powers the SD rail before storage access.
It completes the display power-off sequence before deep sleep.
Board-specific backends set the required rail and GPIO hold states.

## Storage

Papyrix supports FAT32 and exFAT SD cards.
Page caches use device-specific folders:

```text
/.papyrix/cache/          X4
/.papyrix/cache/x3/       X3
/.papyrix/cache/x4pro/    X4 Pro
```

Moving an SD card between devices does not reuse incompatible page layouts.
See [file formats](file-formats.md) for cache records.
See the [user guide](user_guide.md) for supported book formats.

## Hardware Libraries

| Library | Function |
| --- | --- |
| `BoardSupport` | Board profiles, selection, power, touch, battery, and RTC backends |
| `EInkDisplay` | Display controller access and frame buffers |
| `InputManager` | Physical button input |
| `BatteryMonitor` | Battery measurements |
| `SDCardManager` | SD card access |

## References

- [ESP32-C3 Technical Reference Manual](https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_en.pdf)
- [ESP32-C3 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)
