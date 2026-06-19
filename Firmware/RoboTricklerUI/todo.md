# TODO — State Machine Refactor Candidates

Places where the code currently encodes state *implicitly* (scattered booleans,
flags, or blocking loops) and would benefit from an explicit state machine.

Existing explicit state machine: `TricklerState` in `RoboTricklerUI.ino:165`,
dispatched in `handleTricklerState()` (`RoboTricklerUI.ino:201`).

## Strong candidates

### 1. WiFi / web-server lifecycle — `wifi_connection.ino`
Connection state is spread across five independent booleans plus timers:
`wifiActive`, `WIFI_SETUP_AP_ACTIVE`, `WEB_SERVER_ACTIVE`,
`wifiConnectTimeoutReported`, `wifiLastLoggedStatus`, `wifiConnectStartedMillis`,
`wifiPreviousMillis`.

- `maintainWifiConnection()` (`wifi_connection.ino:149`) reconstructs the current
  phase from these every loop.
- `initWebServer()` (`:253`), `stopWifiServices()` (`:212`),
  `startWifiSetupAccessPoint()` (`:353`) each poke a different subset.

Proposed states:
`WIFI_DISABLED -> CONNECTING -> CONNECTED(server up) -> RECONNECTING -> AP_FALLBACK`

Benefit: timeout->AP-fallback and "start web server once connected" transitions
become explicit instead of inferred from flag combinations.

### 2. Scale weight acquisition — `readWeight()` (`rs232.ino:416`)
Implicit, **blocking** state machine: request -> `serialWait()` (up to 2s
busy-wait, `rs232.ino:41`) -> read line -> stability counting -> timeout.
Runs inside the main loop and stalls everything (LVGL handoff, WiFi maintenance)
while waiting on a slow scale.

Proposed non-blocking machine driven once per loop:
`SCALE_REQUEST -> AWAIT_RESPONSE -> STABILIZING -> STABLE/TIMEOUT`

Benefit: removes the `delay(1)` / `delay(200)` busy-waits and keeps the UI
responsive. Biggest responsiveness win.

## Moderate candidates

### 3. Hidden sub-states inside `TRICKLER_RUNNING` — `trickler_runtime.ino`
`TRICKLER_RUNNING` hides several phases tracked by ad-hoc flags:
`firstProfileMovePending` (`handleProfileRunning`, `:313`), the waiting-for-zero
gate, the bulk move (`runBulkStepperMove`, `:80`), and per-step trickling.
The calibration prompt got promoted to a real state, but the rest stayed booleans.

Proposed nested sub-state: `WAIT_ZERO -> BULK_MOVE -> TRICKLE_STEP -> SETTLE`.
Removes `firstProfileMovePending` special-casing scattered across
`handleNewWeight` (`:355`) and `handleProfileRunning` (`:313`).

### 4. Modal dialog handling — `ui_dialogs.ino`
Dialog state lives in parallel `volatile bool` flags (`messageBoxOpen` `:1`,
plus a confirm flag) consumed by `pumpUntil()`. A single
`DialogState { NONE, MESSAGE, CONFIRM }` prevents inconsistent flag combinations
and simplifies `cancelInteractiveDialogs()` (`:7`). Lower urgency, small surface.

## Low priority (already fine or run-once)

- **Boot sequence — `initSetup()` (`startup.ino:1`):** long linear stage list with
  early returns. Genuinely sequential and runs once; a state machine adds little.
- **SD update — `update.ino`:** already uses clean result codes
  (`SD_UPDATE_NOT_FOUND/FAILED/SUCCEEDED`). No change needed.
- **Filesystem sync — `RoboTricklerUI.ino:134`:** `FilesystemSyncDirection` is
  already a small enum-state. Fine.

## Recommended priority
1. #2 Scale read — responsiveness gain.
2. #1 WiFi — correctness/clarity.
