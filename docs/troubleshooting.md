# Troubleshooting

Use `papyrix-xteink-c3.bin` for X3/X4 and `papyrix-x4pro.bin` for X4 Pro.
All application images use flash
offset `0x10000`. A wrong-target image can drive incorrect GPIOs. Erase flash
and install the correct artifact if the device does not enter its expected
startup path.

---

## X3 Display Controller Diagnostics

X3 production units can use a UC8253 or UC8279d display controller. The firmware detects the controller. It stores a conclusive result.

After a live controller probe, the firmware writes the raw probe data to the serial log with the `DEVICE` tag:

```
display probe: VER1=.. .. .. .. .. FLG1=..
display probe: VER2=.. .. .. .. .. FLG2=..
display probe MTP: ..
display probe verdict: uc8279-confirmed | uc8253-stable-default | inconclusive
display controller: UC8279_X3 (probe, cached)
```

The probe runs only when no valid cache or override exists. Clear the `epd_det` key in the `papyrix_hw` NVS namespace to run the probe again.

If an X3 screen stays blank or shows an incorrect image, attach the serial log to the issue report. Report the results for full refresh, fast refresh, grayscale, sleep, and wake.

## Firmware Recovery

Use **Settings → Firmware Update** for a device that can open the UI. Copy the
correct artifact to the SD root as `/firmware.bin`.

If the UI cannot start, copy the artifact as `/force_update.bin`. The boot path
applies it before UI initialization. It shows **Firmware update in progress**
and **Do not power off** when the display is available. A display initialization
failure does not block the update, so the previous e-ink frame can remain
visible during a headless recovery. Wait for the device to restart. The boot
path removes the file after the attempt. Copy it again before a retry.

There is no boot-button recovery mode.

## Display Initialization Recovery

The firmware performs one controller reset and initialization retry. If the
second attempt fails, it closes the SD transport, disables display power, then
disables storage power. USB serial and the power button remain active. The
firmware waits without rebooting.

Press the power button or send `retry` to restore storage power and retry
display initialization. The last result and bounded attempt count are stored in
RTC memory and the `papyrix_diag` NVS namespace.

X4 Pro physical verification does not cover display initialization recovery.

## Repeated Sleep at Startup

Custom firmware can enter sleep immediately after reset.
This can prevent USB flashing.
Remove the SD card and restart the device.
If USB becomes available, install the correct release artifact.
Do not connect wires to SD contacts to force download mode.
This repository does not provide a verified device-specific procedure for that operation.
