# Working-tree assessment

The original review below describes an older snapshot. This section records the
current source changes. It takes precedence over the original verdicts and line
numbers. The changes remain uncommitted.

## Blocking findings

Numbers match the original must-fix list.

| ID | Disposition | Current result |
| --- | --- | --- |
| 1 | Rejected | X4 Pro always runs the panel probe before controller selection. `X3DisplayProbe.cpp` releases the reset hold before it pulses reset. This also covers the SSD1677 fallback. No extra hold release is necessary. |
| 2 | Existing fix preserved | `Display::deepSleep()` propagates driver failure. `SleepState` cancels MCU sleep and enters Error. Emergency-update rendering also checks the result. |
| 3 | Fixed | CW2017 profile verification returns on a failed register read. Only a confirmed mismatch or a missing update flag can rewrite the profile. Sampling errors still retain the last valid sample. |
| 4 | Fixed | Reader menu gating uses the actual cover sentinel: spine zero and section page minus one. First text pages and XTC pages allow the center-tap menu. The reader uses the same predicate for its other cover guards. |
| 5 | Fixed | GPIO and EXT1 deep-sleep wakes identify the power button with or without USB. X4 Pro retains its latch during battery-powered sleep, so the USB condition was also wrong for that path. USB cold boot, timer wake, and software-reset rules remain separate. |
| 6 | Geometry claim rejected; action fixed | Settings dialog bounds already contain screen dimensions. Back coordinates were correct. The actual defect was the missing Confirm action. Settings and FileList now dispatch that tap through the existing current-selection handlers. Direct Yes, No, and Back actions remain intact. |

## Additional panel defect

Both X4 Pro drivers now invalidate the old-plane state when an LSB upload
overwrites DTM1. If grayscale processing stops before MSB upload or refresh,
cleanup restores the black-white reference planes. The fix preserves each
controller's plane layout and completed-grayscale update sequence.

The saved pre-fix UC8179 driver fails the new cleanup checks with no old-plane or
new-plane restoration. The saved pre-fix UC8279 driver fails the old-plane
restoration check. Both repaired drivers pass.

## Non-blocking findings

Numbers follow the original P3 bullet order.

| ID | Disposition | Current result |
| --- | --- | --- |
| 1 | Removed | The duplicate `PanelDispatchTest` CMake branch was a working-tree-only leftover. The real display-facade target remains. |
| 2 | Removed | `fatfs-ng`, `littlefs-python`, and `pyyaml` are removed from `pyproject.toml`. `uv lock` updates the lockfile. |
| 3 | Removed | The current tree had no consumers of `PanelDriver.h`. The committed test had used it, but the current test exercises the real display facade. |
| 4 | Removed | The unused method was `GfxRenderer::drawButtonHints`, not a Display method. Its declaration, definition, mock, and stale wrapper comment are removed. The live Elements button-bar renderer remains. |
| 5 | Removed | Unused retry and shutdown-plan APIs and their policy-only assertions are removed. The real recovery counter and hardware shutdown sequence remain. |
| 6 | Removed | Unused display initialization results are removed. `OutOfMemory` keeps numeric value two for stored diagnostics. |
| 7 | Simplified | The constant UC8253 update plan and its field-copy test are removed. The display window path still performs the same full-frame update. |
| 8 | Fixed | Front-light brightness and warm-channel products use 64-bit arithmetic. Tests cover 19-bit and 31-bit limits, channel totals, inversion, and existing 10-bit behavior. |
| 9 | Rejected | Cancelled sleep returns to a live Error screen. Moving filesystem and peripheral shutdown before the display check would change that recovery path. The existing ordering remains. |
| 10 | Fixed | Network mode hit-testing uses `NetworkModeView::ROW_SPACING`, as rendering does. |
| 11 | Coverage corrected | Storage transport tests now build for both target families and use their real board profiles instead of an X4 Pro literal. |
| 12 | Tool extended | The gated white-refresh characterization command selects the detected UC8179 or UC8279 driver. It retains allocation, BUSY, sleep, and pin-release checks. Physical panel validation is not claimed. |
| 13 | Simplified | Display recovery and font setup return void. Impossible failure branches are removed. Repeated recovery still waits for a power-button press or serial retry command. NVS setter calls do not establish repeated physical flash writes. |
| 14 | Cleaned up | Unused Input setters and TouchDown/TouchUp queue events are removed. Tap and Home-to-Back delivery, board capability checks, and refresh/orientation suppression remain. The two dialog layouts remain distinct because they render different geometry. |

## Preserved earlier fixes

The earlier SD-rail, SDMMC lifecycle, display sleep, GT911 probe and acknowledgement,
network touch, and unknown-battery fixes remain in the working tree. The existing
regressions remain part of the test gate.

## Reproduction evidence

- The CW2017 regression fails against saved pre-fix code with three MODE writes
  after a profile-read failure. It passes after the fix with no rewrite or restart.
- Both panel regressions fail against the saved pre-fix drivers and pass with the
  repaired reference-plane state.
- A host check executes the actual ReaderState menu assignment. The first text
  page fails before the fix and opens the menu after it.
- Host checks reject the old 31-bit light calculation and accept the corrected
  channel split.
- Host checks exercise Confirm button hits in portrait and landscape, with both
  button layouts. The old view ignores the action. The repaired view accepts it.
- `test/scripts/test_wakeup.py` executes the firmware wake classifier. The
  regression covers GPIO and EXT1 with USB and battery power, plus non-button
  wakes and cold boots.
- The rebuilt desktop reader produces identical 55-page dumps with batch sizes
  zero and five. Both dumps retain all 120 source paragraphs.

No device was flashed. Touch and display checks use host instrumentation, not
physical screen observation.

## Final verification

- `make test`: 191 suites passed, zero failed. Target-feature, SDMMC lifecycle,
  wake classification, packaging, and HTML determinism checks also passed.
- `make package`: both `release_xteink_c3` and `release_x4pro` built and packaged
  successfully after the final battery-powered wake fix. Both symbol gates
  reported zero forbidden symbols.
- `.venv/bin/pio run -e x4pro_characterize`: passed with the final wake source.
- `make reader-test`: passed. The desktop batch comparison described above also
  passed.
- `make format`: completed. Subsequent C++ edits were formatted with clang-format.
- `make check`: passed with zero high, one medium, and 33 low findings. The
  existing medium finding concerns `ReaderState::bookmarks_` initialization.
  Static analysis and firmware compilation are not warning-free.
- Two focused reviewers checked the hardware and UI changes. Their actionable
  findings were fixed. No unresolved finding remains from those checks.

These results apply to the working tree, not to an unchanged committed revision.

---

# PR Review: main → refactoring (16 locality reviewers)

Scope: 276 files, +13378/−6946. All results below are reviewer-reported; several reviewers ran suites against the working tree (which contains uncommitted fixes), so passes do NOT prove the committed main→refactoring diff passes — see Tested.

## Verdicts

| Reviewer | Verdict | Key result |
|---|---|---|
| Docs | correct | All code-checkable claims match; SSD1677-on-X3 corrections proper; no findings |
| BuildScripts | correct | 2× P3 only |
| BoardSupport | correct | Faithful port; X4Pro SD-rail defect already fixed in working tree — must land |
| PowerRtcLight | **incorrect** | **P1**: X4Pro display RST hold never released after wake |
| EInkX3 | correct | P3 nit only |
| EInkX4Pro | correct | 2× P3 only; FreeInk port verified vs SDK 6fabbec |
| DisplayCutover | **incorrect (committed)** | **P2**: X4Pro deepSleep discards driver failure (fixed in working tree — must commit) |
| Battery | **incorrect** | **P2**: CW2017 transient I2C error rewrites profile + soft-resets gauge |
| StorageSD | correct | P3 coverage gap only |
| HalCore | correct | P3 only, against working-tree SleepState; alleged WiFi leak disproven (byte-identical rename) |
| InputTouch | **incorrect** | **P2**: `menuAllowed` blocks tap menu on first text page |
| GfxReader | correct | No findings; suite run included an uncommitted X3 runtime-geometry test (working-tree, not PR) |
| CoreMain | **incorrect** | **P2**: X4Pro EXT1 wake bypasses long-press verify when USB attached |
| StatesHome | correct | No findings |
| StatesReader | **incorrect** | **P2**: same `menuAllowed` cover-sentinel bug, confirmed independently |
| StatesSystem | correct | P3 only; flags 2 working-tree-only fixes that must ship |

## Must-fix before merge (P1/P2)

1. **P1 — X4Pro display RST hold survives wake** (`lib/BoardSupport/src/PowerPolicy.cpp:15-20`, `lib/EInkDisplay/src/Display.cpp:397`). `prepareDeepSleep` holds GPIO14 (RTC pad, survives wake); `Display::begin()` releases the hold only under `_x3Mode`. SSD1677 was sent to DSL (exit = hardware reset only), reset pulse swallowed, init loops in recovery forever. Fix: unconditional `gpio_hold_dis(_rst)` in `begin()`, or release in `x4pro::earlyInit`. (PowerRtcLight, conf 0.8)
2. **P2 — X4Pro deepSleep discards failure** (`lib/EInkDisplay/src/Display.cpp:1610-1619`). Committed facade is `void`, ignores driver `false` (POF wait fail → DSLP skipped → charge pump left on, `isScreenOn` lies). Fixed in working tree (bool + `main.cpp` check) — commit it. (DisplayCutover, conf 0.85)
3. **P2 — CW2017 profile rewrite on transient I2C error** (`lib/BatteryMonitor/src/Cw2017Backend.cpp`). 80-byte compare error treated as mismatch → spurious rewrite + soft reset. Present in committed AND working tree. Guard per OEM reference. (Battery, conf 0.72)
4. **P2 — Reader tap menu blocked on page 1** (`src/states/ReaderState.cpp:1025`). `menuAllowed` uses `page != 0`; cover sentinel is `page == -1` (file's own idiom, confirmed by two reviewers). Dead center-tap zone on first content page of every book. (InputTouch + StatesReader)
5. **P2 — X4Pro EXT1 wake bypasses long-press verify** (CoreMain, conf 0.65). Wake cause never matched as power-button wake → verification skipped whenever USB attached.
6. **Confirm dialog hit-test uses dialog dims as screen dims** (`src/ui/views/SettingsViews.h:366-372`, same class of bug as fixed `UtilityViews.h` ConfirmView). Real button-bar zone never registers Back; false Back band inside dialog. UtilityViews copy fixed in working tree; SettingsViews copy NOT fixed. (StatesSystem cross-boundary)

## Must-land: uncommitted working-tree fixes

Reviewers found the committed branch alone contains bugs fixed only in the working tree. Commit all of these with the PR: X4Pro SD-rail (`X4ProBoard.cpp`), `Display::deepSleep` bool + `main.cpp` check, ConfirmView 5-arg + `ServiceViewsTouchTest`, `SleepState` deepSleep-failure → ErrorState, GT911 rail/ack, `NetworkState`/`Elements`/`UtilityViews` touch fixes, battery display fix.

## P3 cleanup (non-blocking)

- `test/CMakeLists.txt:1072` — dead duplicate `PanelDispatchTest` branch (first match wins; delete).
- `pyproject.toml:8-12` — unused `fatfs-ng`, `littlefs-python`, `pyyaml` (no importer in repo).
- `lib/EInkDisplay/include/PanelDriver.h` — zero consumers; delete.
- `lib/EInkDisplay/src/Display.cpp` — `drawButtonHints` now has zero callers (Elements duplicated loop); delete or delegate.
- `src/hal/Recovery.h:20-28` — shutdown plan API (`RetryRequested`, `requestRetry`) has no production consumer; main.cpp sequences inline. Delete or route through plan.
- `lib/EInkDisplay/include/Display.h:20` — `InitResult::UnsupportedPanel/HardwareError` never produced; drop or produce.
- `lib/EInkDisplay/src/Uc8253X3Driver.h:7-14` — `makeUc8253UpdatePlan` constant behind policy API, dead `transferBytes`, 4 unused modes.
- `lib/BoardSupport/include/FrontLightBackend.h:23-24` — `frontLightDuties` uint32 overflow for resolutionBits ≥ 19 (10-bit profiles safe today; compute in uint64).
- `src/states/SleepState.cpp:113-121` (working-tree only, NOT in committed PR) — cancelled sleep skips WiFi/front-light/LittleFS teardown; move teardown before display check. The committed PR facade is `void deepSleep()`, so this path exists only with the uncommitted bool-return fix.
- `src/ui/views/NetworkViews` — `NetworkState.cpp:82` hardcodes pitch 20 vs `ROW_SPACING`.
- `test/unit/device/StorageTransportTest` — X4Pro transport literal can't detect profile drift.
- UC8179 X4Pro path has no gated hardware validation (`X4ProCharacterization.cpp:124-128` gates Uc8279 only).
- CoreMain P3: `initializeDisplayWithRecovery` never returns false → dead caller branches, infinite retry + per-attempt NVS writes on dead panel.
- InputTouch P3s: dead `setDisplayRefreshing`, constant `setTouchEnabled`, unconsumed TouchDown/Up; confirm dialog drops taps on Confirm bar, duplicated bounds helpers.

## Tested (reviewer-reported, NOT a PR gate)

Reviewers report building and passing assigned suites, but two runs provably covered working-tree code, not the committed diff: Battery ran against a tree with uncommitted CW2017 poll caching + single `Wire.begin`, and GfxReader included an uncommitted X3 runtime-geometry test. HalCore's cancelled-sleep finding likewise targets the uncommitted `deepSleep`-returns-bool `SleepState`. Treat all passes as working-tree evidence only; re-run `make test` on the committed refactoring tip before merge. Reported runs: X3 policy/probe/SPI/driver tests, X4Pro driver + PanelDispatch + Ssd1677 + characterization tests, GfxRenderer (12 binaries, incl. uncommitted geometry test), battery (49 assertions, working tree), CoreMain's 9 targets, CoreNavigationTouch 28/28, ReaderOverlay 17/17, ServiceViewsTouch 27/27, ScanRetryLogic, BoardSupport's 8 binaries.
