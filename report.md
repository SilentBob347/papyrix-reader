# Verification of the supplied code review

## Scope

This report checks the supplied findings against the current `refactoring` code.
It records the corrections from this task. It is not a new whole-repository review.
The original diff totals and reviewer votes describe an earlier review, not this verification.

## Major findings

| # | Result | Verified correction |
| --- | --- | --- |
| 1 | Fixed | `SDCardManager::begin()` returns immediately when mounted. `SdmmcBlockDevice::begin()` also preserves an initialized device. Repeated initialization does not allocate another DMA buffer or cycle the rail. Recovery that calls `end()` first is not a repeated-initialization leak. |
| 2 | Fixed | SDMMC retries stop the host, disconnect the bus pins, disable their pulls, and cycle the rail before host initialization. The rail settles for 120 ms before host startup. Sleep also floats all three SD bus pins before rail-off. ESP-IDF documents that `gpio_reset_pin()` enables a pull-up, so the code also sets `GPIO_FLOATING`. |
| 3 | Fixed | A successful DS3231 time write clears status bit 7 (OSF) with a read-modify-write. It preserves the other status bits. Invalid RTC time remains rejected until time is set. |
| 4 | Fixed | CW2017 SOC and voltage reads use one-second caches. Failed reads preserve known values and do not invalidate the initialized profile. Transactions use a 10 ms Wire timeout and STOP. The old initialization path already had retry throttling; the original claim that every status call reinitialized it was too broad. |
| 5 | Fixed | `ConfirmView::hitTest()` receives the actual screen dimensions. Network SavePrompt passes those dimensions and handles the visible Select hint. Tests cover the real button bar and the old phantom Back region. |
| 6 | Fixed | The feature checker recognizes `::x4pro::`. Its tests use a real demangled board symbol. Unused C3 `x4pro` stubs are removed. |
| 7 | Fixed; proposed fix corrected | Wire test registers support explicit 16-bit addressing per device. Transaction length alone cannot identify address width: an 8-bit register write also sends two or more bytes. GT911 tests now program real frames and cover pointer NACK, short reads, failed acknowledgement, and recovery. |
| 8 | Fixed | The X4 pin summary includes the GPIO13 battery latch. The X3 comparison states its X4 role. The previous phrase “not used for SD power” was not itself false, but was incomplete. |

## Minor findings

| # | Result | Disposition |
| --- | --- | --- |
| 9 | Fixed | Shared `ui::battery()` renders unknown charge as `--%`, not `-1%`. A rendering-call regression distinguishes unknown charge from `0%`. Home and ClockApp use this function. |
| 10 | No behavior change | ErrorState dismisses on a button event. The X4 Pro capacitive Home control already emits `Button::Back` in `Input::pollTouch()`. A screen `Tap` is not required for that control. The claim that a touch user cannot dismiss the screen is incorrect. |
| 11 | No behavior change | Outside taps do not cancel the reader menu. Physical Back and capacitive Home both reach its existing Back handler. The claim that there is no touch cancellation path is incorrect. Outside-tap cancellation would be a separate interaction change. |
| 12 | Fixed | RTC transactions set a 10 ms Wire timeout and complete the register-pointer write with STOP. |
| 13 | Fixed | `clearGt911Status()` returns the transaction result. A failed clear rejects the fresh frame instead of reporting successful consumption. Tests cover acknowledgement recovery. |
| 14 | Coverage added; claim corrected | Tests cover stationary holds without repeated events, a long stationary hold followed by release, landscape raw-to-logical corners, and a zero calibration range. `TapClassifier` has no `LONG_PRESS_MS` contract. Its current stationary long hold produces one tap on release. |
| 15 | Coverage added | The UC8279 X4 Pro Half test checks inverted visible old-plane data before refresh, full waveform selection, and restoration of the actual old plane afterward. It checks protocol behavior instead of a complete golden trace. |
| 16 | Fixed | Keyboard hit tests use fixed coordinates from the rendered layout, not coordinates returned by the same hit-test helper. |
| 17 | Fixed | X3 specifications describe C3 support for both boards and link to startup selection policy. |
| 18 | Fixed | The contradictory minimal SSD1677 initialization example is removed. The retained sequence describes 480 gates with register value 479, not “479 gates.” |
| 19 | Fixed | Hardware selection documents both probe passes, the score thresholds, caching of conclusive X3 and X4 results, and the uncached X4 fallback. |
| 20 | Fixed | README identifies all three device cache roots and places its book-folder example beneath the selected root. |
| 21 | Fixed | The FreeInk attribution scope includes the X3 UC8279 driver and LUTs, the X3 half-refresh LUTs in `Display.cpp`, and both X4 Pro paths. |
| 22 | Fixed | The canonical SSD1677 guide explains the release single-buffer mode, controller old-image RAM, post-refresh synchronization, and the optional two-host-buffer mode. |

## Naming notes

- `EInkDisplay` remains the library name. The architecture guide now states this explicitly. The firmware class remains `papyrix::hal::Display`.
- The desktop tool's `EInkDisplay` mock is internally consistent with its own renderer and call sites. It does not use the firmware class. Renaming this independent mock is optional; this task leaves it unchanged.

## Verification

- `make format` completes.
- `make check` passes for its configured `default` environment. It reports 0 high, 1 medium, and 34 low diagnostics. The medium diagnostic is `ReaderState::bookmarks_` constructor initialization at `src/states/ReaderState.cpp:653`. This task does not change that member or suppress the warning.
- `make test-clean && make test` passes **188/188 C++ test suites** with zero failures. The four Python checks also pass: target features, SDMMC lifecycle, firmware packaging, and HTML determinism. Python checks are separate from the C++ suite count.
- Targeted regressions fail before their fixes and pass afterward for DS3231 OSF recovery, CW2017 transient reads, confirmation geometry, real namespace rejection, GT911 acknowledgement failure, unknown battery rendering, SDMMC startup sequencing, retry pull isolation, and SD sleep isolation.
- `make package` builds `release_xteink_c3` and `release_x4pro`. The feature checker scans 12,935 C3 symbols and 13,952 X4 Pro symbols, with zero forbidden symbols in either artifact. Both binary SHA-256 values match `dist/manifest.json`.
- `make reader-test` passes without renaming its independent display mock.
- `reader-test --dump --batch 5` parses a temporary TXT book into one page. Both input paragraphs remain intact across output line wrapping. The temporary book, cache, and earlier pre-fix harness are removed. Permanent regressions remain in the test runner.

The confirmed blockers from the supplied report have code corrections and host verification.
Static-analysis diagnostics and physical verification limits remain explicit.

## Hardware limits

This task does not flash a device or perform physical tests.
The current [support matrix](docs/device-support-matrix.md#physical-verification)
records X3 and X4 tests and partial X4 Pro UC8279 verification.
The original blanket statement that X4 Pro is hardware-unverified is stale.
UC8179 and the remaining X4 Pro operations listed in that matrix still lack physical verification.
The new SDMMC sequencing, RTC recovery, battery caching, and touch changes need device validation before release.
Host rendering checks observe drawing calls; they do not establish visual correctness on an e-paper panel.
