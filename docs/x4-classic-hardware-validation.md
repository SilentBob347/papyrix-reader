# X4 v2 Classic Hardware Validation

## Status

Classic support has partial hardware validation.
Device checks cover an ESP32-S3 revision 0.2 with 16 MB flash and a UC8279 panel with variant `0x68`.
The release manifest reports `hardware_status: partial`.
Pro and Classic require separate firmware images.

## Verified behavior

| Area | Result |
| --- | --- |
| Display | The panel initializes at 10 MHz and displays menus and book pages. |
| Buttons | The operator confirms correct side-button directions. Up uses GPIO7. Down uses GPIO0. |
| Storage | SD and LittleFS mount. EPUB metadata and page caches load. |
| Reader | The device opens a book, creates page caches, and returns to UI mode. |
| RTC | The RTC supplies the initial system time. |
| Sleep | The operator confirms the selected automatic sleep screen and Power wake. |
| Installation | The installed bootloader, partition table, and application pass independent digest checks. |

Automatic sleep restores the active CPU frequency before reader exit and sleep-screen rendering.
The gauge uses factory calibration without programming BATINFO or resetting the calibration.
See the [device support matrix](device-support-matrix.md) for pins, panel selection, and target limits.

## Shared button input

The operator confirms rapid Back navigation on X3, X4 v2 Classic, and X4 Pro.
The check opens Settings, opens Reader settings, and presses Back twice during display refresh.
Both presses take effect and return to Home.
Each installed application passes an independent digest check against its release image.
Host regressions cover blocked rendering, pending release debounce, queue overflow, and sampler restart.
The [architecture](architecture.md#button-input) describes button sampling and event ownership.

## Automated checks

`make test` covers board selection, panel selection, digital buttons, storage, and power policy.
`make package` builds C3, Pro, and Classic images and checks target isolation.
The sleep regression scripts cover CPU restoration before reader exit and restart after rejected sleep.
Host tests use mock hardware interfaces.
They do not establish display quality, sleep current, or battery accuracy.

## Validation limits

The following areas are not verified on Classic hardware:

- SSD1677, UC8179, and UC8279 variants other than `0x68`.
- Grayscale quality, fast refresh, and all image rendering paths.
- PSRAM capacity and stability across CPU frequency changes.
- RTC retention and NTP synchronization.
- Charge detection across cable changes, sleep current, and battery accuracy.
- SD updates and headless `/force_update.bin` recovery.

## Installation

Use `papyrix-x4c.bin` for Classic.
Do not use the Pro image or a full-flash image from another device.
Check the device partition table and active OTA slot before an application-only update.
The application must fit the selected slot.
Back up factory NVS and preserve it at `0x9000`.

The standard partition table uses these regions:

| Region | Offset | Size |
| --- | --- | --- |
| NVS | `0x9000` | `0x5000` |
| OTA metadata | `0xE000` | `0x2000` |
| Application 0 | `0x10000` | `0x640000` |
| Application 1 | `0x650000` | `0x640000` |
| Internal filesystem | `0xC90000` | `0x360000` |
| Core dump | `0xFF0000` | `0x10000` |

Factory partitions can differ from this layout.
An application-only update does not change the partition table.
