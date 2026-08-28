# Working-tree assessment

The review below describes an older snapshot. This section records the result of
checking every finding against the current source. It takes precedence over the
original verdicts and line numbers.

## Critical finding

**C1 — Fixed.** The backend retains contact and Home-key state between fresh GT911
frames. A successful unchanged idle poll can rearm the classifier. An unchanged
held-contact poll cannot rearm it. The report's proposed unconditional rearm would
accept a held contact after a refresh.

`TapClassifierTest` covers unchanged idle and held frames. `HalTouchInputTest`
executes the real GT911 backend and HAL input path. It checks the first
post-refresh tap, Home-to-Back delivery, duplicate suppression, and a refresh
during a held contact.

## Coordinator rejection

**R1 — Rejection confirmed.** CW2017 retains its initialized state and last valid
sample after a NACK. The existing transient-failure regression passes.

## Important findings

Numbers match the original Important list.

| ID | Disposition | Current result |
| --- | --- | --- |
| 1 | Already fixed | X4 Pro sleep makes CLK/CMD/DAT0 inputs before it disables the SD rail. `X4ProBoardTest` checks ordering. |
| 2 | Fixed | `earlyInit()` disables the active-low SD rail after it asserts the latch and before it enables touch. |
| 3 | Fixed | Recovery floats the SD bus before rail removal. `SdmmcBlockDevice::end()` deinitializes the host, removes GPIO pulls, and disables the saved rail. Failed initialization uses the same teardown. The SDMMC lifecycle regression checks powered-off pins. |
| 4 | Fixed | X4 Pro classification accepts UC8179 only for LUT `0x01`. LUTs `0x02`, `0x68`, and `0x69` select UC8279. The X3 field signature and LUT `0x66` use the existing SSD1677 fallback. Probe regressions cover both rejected signatures. |
| 5 | Fixed | The display facade returns sleep failure without clearing its screen state. `SleepState` cancels MCU sleep and enters Error. Emergency-update rendering reports failure. Shutdown logs failure. The real-facade regression injects a BUSY timeout and checks a successful retry. |
| 6 | Fixed | A failed GT911 probe calls `shutdown()`. The regression checks that the power rail is off and reset is low after both addresses fail. |
| 7 | Rejected | Default member initializers do not imply a startup constructor here. A C3 C++17 compilation of this type in `.rtc_noinit` emits only the retained data symbol, with no startup initializer. Removing safe member defaults does not repair a demonstrated defect. |
| 8 | Fixed | Boot logging reads and consumes display failure keys from NVS before the empty-RTC early return. `CrashDebugTest` covers a power-on reset with only NVS evidence. |
| 9 | Fixed | USB changes update Home battery state without forcing `needsRender`. Existing battery dirtiness selects the battery-window path. Other pending full renders remain intact. |
| 10 | Fixed | App and overlay taps on the button bar use the existing semantic button mapping and mode handlers. The Menu hit-test path remains separate. |
| 11 | Rejected | Capacitive Home supplies Back, so the reader menu does not require a physical Back button. Treating every outside tap as cancellation would change the interaction policy. That change was not applied. |
| 12 | Fixed | Recent statistics dispatches its Back and Open hits through the existing Stats button handlers. |
| 13 | Rejected | Production consumers use `readStatus()` and its known-value flags. A numeric zero before the first valid sample is not a known empty-battery reading. No consumer needs a fabricated 100-percent or 4100-mV fallback. |
| 14 | Fixed | CW2017 initializes Wire once per profile initialization attempt, not for each byte transaction. Polling and transient read failures do not reinitialize the shared bus. |
| 15 | Coverage added | The real `GfxRenderer` regression now selects X3, checks its runtime dimensions, and checks the origin pixel at the 99-byte physical stride. The older X4-only algorithm fixtures remain unchanged; the new check executes production rendering code rather than another copy. |
| 16 | Coverage replaced | `PanelDispatchTest` now compiles `Display.cpp` and the real UC8179/UC8279 X4 Pro drivers. It checks distinct initialization traffic and sleep failure/retry behavior. |
| 17 | Coverage added | Both X4 Pro driver tests execute Fast after grayscale. They check the redrive waveform, BUSY completion, old-plane synchronization, and the following return to OTP. |
| 18 | Already fixed | The C3 feature gate includes `::x4pro::`. It does not rely only on class-name capitalization. |
| 19 | Fixed | The unused `PAPYRIX_DISABLE_BLUETOOTH` define is removed. Artifact symbol checks remain the evidence for excluded features. |
| 20 | Already present | `make flash-x4pro` uploads `release_x4pro`. No flash command was run during this check. |
| 21 | Already fixed | The Wire mock supports GT911 16-bit register pointers. Backend and HAL tests inject real status and coordinate registers. |
| 22 | Coverage extended | Existing DS3231 tests already exercise begin/read/write and OSF repair. The same test now runs against BM8563 and checks VL repair, date register order, and weekday encoding. |
| 23 | Already fixed | CW2017 tests inject a NACK, check the last valid sample, recover polling, and verify that a sample failure does not rewrite the profile. |
| 24 | Fixed remaining docs | README already uses device-specific cache roots. Image and file-format documentation now uses the same roots through `<device-cache>`. |
| 25 | Already fixed | Device specifications state the two-pass score rule, conclusive NVS persistence, and the uncached X4 fallback. |
| 26 | Already fixed | X3 specifications state that the C3 image selects X3 or X4. |
| 27 | Already fixed | The X4 pin summary includes the GPIO13 battery latch. |
| 28 | Fixed | X4 Pro specifications list the accepted LUTs and the SSD1677 fallback for other results. |
| 29 | Fixed | The user guide names X4 Pro Up, Down, and Power GPIOs. It states that the other controls use touch and that capacitive Home sends Back. |

## Minor findings

Numbers follow the original Minor bullet order.

| ID | Disposition | Current result |
| --- | --- | --- |
| 1 | No change | Current C3 ADC-ladder profiles use GPIO1/GPIO2/GPIO3. No supported profile violates the existing implementation. |
| 2 | No change | Error dismissal uses Back. The real HAL test confirms that capacitive Home emits Back, not Tap. |
| 3 | Fixed | `clearDisplayFailure()` clears the RTC marker only when its phase is DisplayInit. A regression checks that an unrelated Home metadata marker survives. |
| 4 | Coverage added | Both X4 Pro drivers receive `requestResync()` followed by Fast. The tests require a full update from a known old-plane baseline. |
| 5 | No change | Settings declares SystemInfo before Count and tests menu wrapping. A separate enum-inequality assertion would pin the declaration rather than test an additional behavior. |
| 6 | Fixed | The SPI storage test now reads the actual X3 profile and checks its active-high controlled rail. |
| 7 | Coverage added | `HalTouchInputTest` checks the first Home release, a held Home key, and duplicate suppression through the real input implementation. |
| 8 | Fixed | The X4 introduction points to its local pin summary, not removed shared pin tables. |

## Verification

- `make format` completes.
- `make check` passes with 0 high, 1 medium, and 34 low findings. The medium
  finding is the existing `ReaderState::bookmarks_` constructor warning.
- `make test-clean && make test` passes all 191 C++ test suites and the target
  feature, SDMMC lifecycle, packaging, and HTML determinism script checks.
- `make package` builds both `release_xteink_c3` and `release_x4pro`.
  Their symbol gates scan 12,942 and 13,959 symbols, respectively, with no
  forbidden symbols. Packaging writes both artifacts and the manifest to `dist/`.
- `make reader-test` completes. A desktop smoke run preserves all 120 paragraphs
  across 18 pages. Five-page batches and an unbatched run produce identical page
  text and boundaries. Temporary smoke files are removed.
- A C3 C++17 compile of `RetainedEstimate` in `.rtc_noinit` produces only its
  data symbol. It produces no startup constructor.

The generated changelog remains unchanged. Its entries come from commits.

## Physical verification limits

This check does not flash a device or establish new physical verification.
See `docs/device-support-matrix.md` for previously documented device results.
The new SD-rail ordering needs electrical verification. UC8179 waveform quality
and the changed UI paths still need device checks. Host tests do not establish
inactive-rail voltage, sleep current, or visual quality.

---

# Original review snapshot

# Code review: `refactoring` vs `main`

- **Range:** `34444ab1` (main) .. `ce6b1a59` (refactoring)
- **Commit:** Add shared hardware support for X3, X4, and X4 Pro
- **Method:** 16 locality reviewers (`git diff`/`git show` first). Parent verified Critical claims against source.
- **Overall:** **With fixes** — do not merge until the Critical X4 Pro tap bug is fixed. Treat the X4 Pro SD-rail / panel-classify items as merge blockers for the S3 image.

X3 pogo was previously PASS. X4 / X4 Pro / PaperMono remain unverified. This branch is the X4 Pro bring-up surface.

## Locality verdicts

| Locality | Verdict | Notes |
|---|---|---|
| BoardSupport core | with_fixes | X4 Pro SD rail/pins |
| BoardSupport peripherals | with_fixes | **Critical** first-tap drop |
| EInkDisplay facade | with_fixes | POF/DSLP failure ignored |
| X4 Pro eink drivers | with_fixes | AA-exit redrive untested |
| X3 eink/probe | with_fixes | Field-fallback → UC8179 |
| HAL power/clock/usb | with_fixes | RTC EMA constructor wipe |
| HAL IO/storage/wifi | with_fixes | `SdmmcBlockDevice::end` rail |
| BatteryMonitor/InputManager | with_fixes | CW2017 I2C / 0% API (see rejected Critical) |
| Core/boot/main | with_fixes | DisplayInit NVS unread |
| Settings/dispatcher | **ready** | v15 `touchPageTurns` cutover is clean |
| States/apps | with_fixes | Touch holes on overlays |
| UI touch/views | with_fixes | Recent stats hits unused |
| Gfx/content tests | with_fixes | Stubs still 800×480 |
| Build/packaging | with_fixes | Symbol gate / flash target |
| Docs | with_fixes | Cache paths / NVS rule |
| I18n/locales | **ready** | SCREEN/TOUCH strings aligned |

## Critical (must fix)

### 1. First tap after boot / wake / refresh is dropped (X4 Pro)

- **File:** `lib/BoardSupport/src/TouchBackend.cpp:69-93`, `resetAfterWake` at `:120`
- **Also:** `src/hal/Input.cpp:30`, `:204` (calls `resetAfterWake` on init and every `suppressTouchUntilIdle` revision)
- **What's wrong:** `resetAfterWake()` sets `ignoreUntilIdle_ = true`. That flag is cleared only on a **fresh** sample with `contactCount == 0` (`:58`, `:76-78`). `decodeGt911Frame` sets `fresh` only when status bit 7 is set (`:29-30`). Idle GT911 polls after ACK have bit 7 clear → `fresh=false` → `update()` returns at `:69-73` and never clears the flag. The next one-contact frame hits `:93` and is discarded. `active_` was never set, so the lift is also silent.
- **Why it matters:** Input resets the classifier on boot, wake, Home, and every e-ink refresh. On X4 Pro the first tap after those events does nothing. `TapClassifierTest` hides this by feeding `idle()` with `fresh=true`, which production polls do not produce while the panel is untouched.
- **Fix:** When `!sample.fresh && sample.controllerOk && !active_`, set `ignoreUntilIdle_ = false`. Keep the existing fresh count-0 path for an explicit lift. Add a host test: `resetAfterWake(); update({fresh:false, controllerOk:true, count:0});` then a one-contact sample must emit down.

## Coordinator rejection

**Rejected Critical — CW2017 tears down init on one NACK** (`lib/BatteryMonitor/src/Cw2017Backend.cpp:84-97`)

The BatteryInput reviewer claimed `readCw2017Soc_` clears `_cw2017Initialized` and has no last-good cache. Current source does **not** do that: a failed SOC read leaves `_cw2017Initialized` set, stores `_lastGoodSoc` / `_haveCw2017Soc`, and returns the last good value. `ensureCw2017Profile_` returns early when already initialized (`:40`). Keep the remaining CW2017 items as Important (numeric 0% before first success; `Wire.begin` per byte).

## Important (should fix before merge)

### Hardware safety (X4 Pro unverified)

1. **Float SDMMC before cutting the rail (sleep)**
   - `lib/BoardSupport/src/X4ProBoard.cpp:77` — `prepareDeepSleep` holds GPIO5 high (active-low SD off) but never sets CLK/CMD/DAT0 (GPIO41/42/40) to input. X3 floats CS/CLK/MOSI/MISO first. Driving or leaving SDMMC attached while the rail is forced off can back-power the card.
   - Fix: `setInput()` those pins before `holdOutput(storage.powerPin, !powerActiveHigh)`. Extend `X4ProBoardTest`.

2. **Drive the X4 Pro SD rail off at `earlyInit`**
   - `lib/BoardSupport/src/X4ProBoard.cpp:57` — latch/buttons/touch are asserted; GPIO5 is never written. Characterization drives it high (off); production boot does not. A floating active-low enable can power the card during panel probe.
   - Fix: `setOutput(powerPin, !powerActiveHigh)` after the latch, matching sleep/characterization.

3. **Float SDMMC on recovery and `end()`**
   - `lib/BoardSupport/src/PowerPolicy.cpp:48` — `shutdownRecoveryRails` for non-SPI only drives `powerPin` inactive.
   - `lib/SDCardManager/src/SdmmcBlockDevice.cpp:78` — `end()` deinits the host and does not float pins or drop the rail. Failed `begin()` and display-recovery (`SdMan.end()` then `shutdownRecoveryRails()`) can leave the card powered or back-powered.
   - Fix: after `sdmmc_host_deinit()`, float CLK/CMD/DAT0 and write the inactive rail level.

4. **Do not map X3 field-fallback VER to UC8179**
   - `lib/EInkDisplay/src/X3DisplayProbeClassifier.cpp:57-61` — `classifyX4ProPanel` returns `Uc8179` for every `UC8279Confirmed` sample whose `pass2.ver[2]` is not `0x02/0x68/0x69`. That verdict includes X3 field-fallback (uniform VER `0xFF`, MTP confirm) and structured LUT `0x66`. BoardSelector then selects `UC8179_X4PRO` (host-test-only). Wrong PSR/waveforms → ghosting.
   - Fix: whitelist X4 Pro LUTs; `Uc8279` only for `0x02/0x68/0x69`, `Uc8179` only for the known stamp (`0x01`), else `Ssd1677` or refuse. Test field-fallback + LUT `0x66` → not Uc8179.

5. **Honor X4 Pro `deepSleep` POF/DSLP failure**
   - `lib/EInkDisplay/src/Display.cpp:1612` — `Display::deepSleep` ignores a false return from the UC8179/UC8279 X4 Pro drivers, then sets `isScreenOn=false`. Those drivers skip DSLP (`0x07, 0xA5`) when POF `waitBusy` fails, so the MCU can sleep with the charge pump on.
   - Fix: retry or abort sleep; only clear `isScreenOn` after DSLP runs.

6. **Power down GT911 if probe fails**
   - `lib/BoardSupport/src/TouchBackend.cpp:131` — `begin()` always `enableTouch()` then `beginGt911()`. Dual NACK leaves `available_=false` with RST released and GPIO2 on (shared I2C with CW2017/BM8563).
   - Fix: `shutdown()` on probe failure.

7. **RTC battery EMA does not survive USB deep sleep (X4)**
   - `src/hal/BatteryEstimatePolicy.h:11` — `RetainedEstimate` default-initializes members, so it is not trivially constructible. `Battery.cpp` places it in `RTC_NOINIT_ATTR`; the constructor still runs on wake and zeros `magic`. `isUsable()` fails and the next `readStatus()` reseeds from raw ADC.
   - Fix: drop default member initializers; keep `retainedX4Estimate = {}` only on `ESP_RST_POWERON`.

8. **Log NVS display-init breadcrumbs on boot**
   - `src/core/CrashDebug.cpp:136` — `markDisplayFailure()` writes NVS; `logBootInfo()` only inspects the RTC crash marker and returns if it is empty, then a later success deletes the NVS keys. Power-on reset clears RTC, so a crash during `display.begin()` is write-only.
   - Fix: log `DISPLAY_RESULT_KEY` / `DISPLAY_ATTEMPT_KEY` even when the RTC marker is empty; clear only after report.

### Touch / UI

9. **USB plug forces a full Home refresh** — `src/states/HomeState.cpp:150`. `onUsbStateChanged()` sets `view_.needsRender`; render takes the full clear/cover path instead of the battery-only window. Drop `needsRender`; keep `batteryNeedsRender`.

10. **App and overlay modes ignore touch** — `src/states/AppLauncherState.cpp:78`. Tap handling is Menu-only. Clock / Image Viewer cannot Back/Menu from tap.

11. **Reader menu swallows outside taps** — `src/states/ReaderState.cpp:2929`. `handleMenuInput()` returns on every Tap; non-item hits are discarded; no Back hit. Overlay cannot be dismissed without a physical Back. Treat a non-item tap as Back and arm `overlayTapGuard_`.

12. **Recent stats Back/Open hits are never dispatched** — `src/ui/views/ReaderViews.h:169` + `src/states/RecentState.cpp:83`. `BookStatsView::hitTest` exists and `showOpen=true` on Recent stats, but `RecentState::update` only hit-tests Browse. Touch cannot leave or open that overlay.

13. **CW2017 `readPercentage`/`readMillivolts` return 0 before first success** — `lib/BatteryMonitor/src/BatteryMonitor.cpp:89`. BQ path still returns 100/4100 so a 0 seed does not trip low-battery UI. `readStatus()` uses `percentageKnown=false` (safer). Align numeric APIs with last-good / known flags.

14. **`Wire.begin` on every CW2017 byte** — `lib/BatteryMonitor/src/Cw2017Backend.cpp:16-17`. Profile verify is 80+ transactions on the shared 100 kHz GT911/RTC bus. `setTimeOut(10)` is present (reviewer was wrong that it is missing). Still: begin the bus once at profile pins/freq.

### Tests / build

15. **Gfx stubs still use compile-time 800×480** — `test/unit/gfxrenderer/GfxRendererPixelTest.cpp:31` and ClearArea/FillRect/RenderChar/DrawBitmap. Production uses `display_.getDisplayWidth/Height()`. `setDisplayX3()` is never called. X3 792×528 mapping cannot fail this suite. Use runtime `getDisplay*()` and add one X3 origin case.

16. **`PanelDispatchTest` does not exercise `Display.cpp`** — `test/unit/einkdisplay/PanelDispatchTest.cpp:9`. It only asserts `panelDriverKind()`. A missed facade branch for UC8179/UC8279 X4 Pro still passes.

17. **AA-exit Fast-after-gray redrive untested** — `test/unit/einkdisplay/Uc8179X4ProDriverTest.cpp:175`, `Uc8279X4ProDriverTest.cpp:187`. `displayGray` is followed by `deepSleep`, never `display(Fast)`. GRAY_PRE_BW / `transitionGrayscaleBase` is the ghosting-sensitive Reader AA path and is host-test-only for UC8179.

18. **C3 symbol gate misses `::x4pro::`** — `scripts/check_target_features.py:15`. Pattern is `X4Pro(?!Variant)(?![a-z])`. Production symbols are `papyrix::board::x4pro::*`. The fixture uses fictional `X4ProPower::enableRails()`. C3 packaging can PASS an image that still contains X4 Pro board functions. Add `|::x4pro::` and a real demangled fixture.

19. **`PAPYRIX_DISABLE_BLUETOOTH` is a no-op** — `platformio.ini:34`. No sdkconfig / `lib_ignore` / source consumer. Checker only searches `Bluetooth`. Either disable BT in IDF config or drop the define.

20. **No X4 Pro flash Make target** — `Makefile:32`. `flash-release` always uploads `release_xteink_c3`. Docs tell operators `make flash-x4pro`. Add `flash-x4pro` (`pio run -e release_x4pro --target upload`).

21. **Wire mock is 8-bit only** — `test/mocks/Wire.h:31`. GT911 uses 16-bit big-endian pointers (`0x814E`). The mock stores `0x4E` at register `0x81`. Host GT911 poll tests cannot inject status/points.

22. **RTC tests cover only BCD** — `test/unit/hal/RtcBackendTest.cpp:5`. Never calls `begin`/`read`/`write`, never programs DS3231 OSF or BM8563 VL.

23. **CW2017 tests do not inject NACK/cache** — `test/unit/hal/Cw2017PolicyTest.cpp:6`. Pins profile bytes and VCELL math only.

### Docs (source of truth vs operators)

24. **Cache paths still show SD-root `.papyrix/epub_<hash>/`** — `README.md:372`, `docs/images.md:118`, `docs/file-formats.md:7`. Live roots are `/.papyrix/cache/`, `cache/x3/`, `cache/x4pro/`.

25. **NVS I2C score≥2 rule dropped** — `docs/device-specifications.md:27`. Source caches X3 only when both passes score ≥2, X4 only when both 0; inconclusive stays X4 for one boot and does not write `dev_det`.

26. **`docs/x3-specifications.md:5` says C3 firmware always selects X3.** C3 image selects X3 or X4.

27. **X4 GPIO13 latch missing from pin summary** — `docs/x4-specifications.md:178`. On X4 Pro the same number is display CS.

28. **X4 Pro inconclusive panel fallback undocumented** — `docs/x4pro-specifications.md:12`. Non-confirmed probe → SSD1677.

29. **X4 Pro physical buttons unspecified** — `docs/user_guide.md:20`. Up GPIO0, Down GPIO7, Power GPIO3; no Back/Confirm/Left/Right; rest is GT911.

## Minor

- `lib/InputManager/src/InputManager.cpp:41` — ADC ladder still hardcodes GPIO 1/2/3 instead of profile pins (current boards match).
- `src/states/ErrorState.cpp:50` — Tap events consumed with no dismiss.
- `src/core/CrashDebug.cpp:128` — `clearDisplayFailure()` zeros the whole RTC crash marker.
- `test/unit/einkdisplay/Uc8179X4ProDriverTest.cpp:155` — `requestResync` → GC untested on X4 Pro drivers.
- `test/unit/capacity/CapacityContractsTest.cpp:40` — dropped Settings `SystemInfo != Count` sentinel assert.
- `test/unit/device/StorageTransportTest.cpp:12` — SPI fixture uses `powerPin=-1`; real X3 is CS=12, power=13 active-high.
- `src/hal/Input.cpp:216` — no host test that GT911 home → Back.
- `docs/x4-specifications.md:3` — pointer to shared pin tables that this patch removed.

## Strengths

- C3 board classify still requires I2C score ≥2 on both passes; inconclusive stays X4 for one boot and is not cached.
- `preparePanelProbe` runs before `selectPanel`; X3 sleep still floats CS then drops the rail; UC8279 X3 HALF still uses full GC; `darkBackground_` still reaches UC8279_X3.
- `EInkDisplay` → `papyrix::hal::Display` cutover in assigned production files is complete; dual-boot still shares `earlyInit()`.
- Settings v14→15 appends `touchPageTurns` last with the break-pattern; tap zones 24/52/24 with reverse and cover `menuAllowed`; `ReaderTapZonesTest` covers bounds.
- Button-bar / list / keyboard hit boxes share the named render constants; `OverlayTapGuard` is wrap-safe and tested.
- I18n SCREEN / TOUCH_PAGE_TURNS / BRIGHTNESS / WARMTH are wired enum + KEY_MAP + defaults + five locales (230 keys, still inside the 4096-byte override buffer).
- Packaging builds both release envs, hashes `dist/` artifacts, and command-gates X4 Pro characterization.

## Assessment

**Ready to merge: With fixes**

The HAL/BoardSupport split, C3 probe/NVS policy, settings versioning, and I18n cutover are in good shape. The X4 Pro touch path as written drops the first tap after every refresh; SDMMC rail handling on the S3 board does not match the X3 CS-float invariant; panel classify can select the host-test-only UC8179 driver from an X3-shaped probe. Fix those three, then the Important test/build/docs items.

Skipped: no project-wide `make test` / PIO build in this review (reviewers were read-only). Add device checks after the Critical tap fix: X4 Pro first-tap after boot and after a page turn; X3 20 fwd / 5 back / sleep/wake (queued hardware).
