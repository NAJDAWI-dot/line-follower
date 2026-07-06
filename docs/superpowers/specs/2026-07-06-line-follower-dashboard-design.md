# Line Follower Competition Dashboard — Design Spec

Date: 2026-07-06

## Purpose

A companion system for a line-follower competition robot built around a QTR-8A
reflectance sensor array and an ESP32. It has two halves:

1. **ESP32 firmware** that reads the QTR-8A sensors, computes line position,
   runs a PID-based line-following loop driving motors through a TB6612FNG
   driver, and exposes live telemetry + remote tuning over three
   simultaneously-available transports: USB serial (wire), WiFi, and
   Bluetooth Low Energy.
2. **A browser-based web app** (Chrome/Edge) that connects over whichever of
   those three transports is convenient, visualizes live sensor data and line
   position, lets you tune PID constants live, trigger sensor calibration,
   and record/export a run's data for post-competition analysis.

Non-goals: full remote driving/joystick control of the robot, a native
mobile/desktop app, or supporting sensor arrays other than QTR-8A.

## Repository Layout

```
D:\line\
├── firmware\     PlatformIO project (C++) for the ESP32
├── webapp\       React + TypeScript + Vite web app
└── docs\         Wiring guide + protocol spec (shared source of truth)
```

## Protocol Strategy

One logical protocol, two wire encodings, chosen per-transport for fit:

- **Serial (USB) and WiFi (WebSocket):** newline-delimited JSON in both
  directions. Bandwidth is plentiful on both, and JSON is human-readable —
  you can watch it in a serial monitor or the WebSocket frame inspector while
  debugging.
- **BLE:** telemetry (ESP32 → browser) uses a packed binary struct (~16
  bytes) over a notify characteristic, sized to fit inside BLE's default ATT
  MTU (~20 usable bytes) without relying on MTU negotiation succeeding.
  Commands (browser → ESP32, infrequent and small) still use the same JSON
  text as the other transports, written to a command characteristic and
  reassembled by the firmware until a newline terminator is seen (handles
  writes split across multiple BLE packets).

All three transports expose the same logical messages and can be connected
simultaneously; the web app only ever talks to "the current transport."

### Logical messages

**Telemetry (ESP32 → browser), sent at a configurable rate (default 50 Hz):**
- 8 calibrated sensor readings (0–1000 scale)
- computed line position (0–7000, Pololu-style weighted average, center =
  3500)
- PID error and output
- left/right motor speed (post-PID, as applied)
- calibration status (idle / calibrating / done) with per-channel min/max
  when relevant
- monotonic timestamp (ms since boot)

**Commands (browser → ESP32):**
- `setPid` — Kp, Ki, Kd, base speed
- `calibrate` — start/stop calibration mode
- `start` / `stop` — enable/disable the line-following PID loop (sensors and
  telemetry keep running either way)

### Binary telemetry struct (BLE only)

Packed, little-endian, 17 bytes: `uint16` timestamp delta, 8× `uint8`
sensor readings (normalized 0–255), `int16` line position, `int16` PID
error, `int8`×2 motor speeds, `uint8` status flags. Exact field order and
offsets live in `docs/protocol.md` and
is mirrored by both the firmware encoder and the web app decoder so they
can't silently drift apart.

## Firmware Design

**`config.h`** is the single place all pin assignments and board-specific
tunables live — swapping ESP32 boards means editing this one file, not
hunting through logic code. It documents the ESP32 ADC1-vs-ADC2 constraint
(the 8 sensor inputs must be on ADC1 pins, since ADC2 conflicts with the WiFi
radio) and ships a working default mapping for a classic ESP32
DevKit/WROOM-32 (GPIO32–39 — all 8 ADC1 channels, one per sensor).

**Modules:**
- `sensors` — reads the QTR-8A array, runs calibration (per-channel min/max,
  persisted across power cycles via `Preferences`/NVS), computes line
  position via the classic Pololu-style weighted-average algorithm.
- `motors` — drives the TB6612FNG via ESP32 LEDC PWM (speed + direction per
  side).
- `pid` — PID over line-position error, biasing left/right motor speed
  around a configurable base speed. Includes a lost-line failsafe: if no
  sensor sees the line for a configurable timeout, motors stop rather than
  driving blind.
- `protocol` — builds/parses the shared logical messages (JSON encode/decode
  plus the BLE binary struct); one implementation, reused by all transports
  so serial/WiFi/BLE can never disagree about message meaning.
- `transport_serial` — raw JSON lines over USB serial.
- `transport_wifi` — ESP32 runs as its own WiFi access point (fixed IP,
  `192.168.4.1`) with a WebSocket server (ESPAsyncWebServer/AsyncTCP). Chosen
  over joining an existing network so the robot works at any competition
  venue without depending on venue WiFi.
- `transport_ble` — NimBLE GATT server (chosen over the stock ESP32 BLE
  stack for lower memory use and better stability): one notify characteristic
  for binary telemetry, one write characteristic for JSON commands.

All three transports run concurrently; none is exclusive.

## Web App Design

React + TypeScript + Vite, runs entirely client-side in Chrome/Edge — no
backend server. Uses Web Serial, Web Bluetooth, and WebSocket APIs directly
against the ESP32.

**Transport layer** — `SerialTransport`, `WebSocketTransport`, `BleTransport`
all implement one `Transport` interface (`connect()`, `disconnect()`,
`send()`, `onTelemetry`, `onStatus`). The dashboard only depends on this
interface, so switching wire/WiFi/BLE is just swapping the active
implementation.

**Dashboard components:**
- **Connection panel** — pick a transport, connect/disconnect, live status
  (message rate, last-seen, errors).
- **Sensor bar graph + line position** — real-time bar chart of the 8
  calibrated readings, plus a visual line-position indicator (−3500…+3500,
  center = on-line). Built following the `dataviz` skill's guidance for
  clean, accessible real-time charts.
- **PID tuning panel** — Kp/Ki/Kd + base-speed controls; "Apply" pushes a
  live update over the current transport. Plots PID error/output over time.
- **Calibration panel** — "Start Calibration" triggers ESP32 calibration
  mode (sensor array is swept over the line by hand); shows progress and the
  resulting per-channel min/max once done.
- **Recording panel** — Start/Stop buffers timestamped telemetry+PID data
  client-side; Stop offers CSV or JSON download.

**Error handling:** transport disconnects surface immediately in the
connection panel rather than leaving stale data on screen; malformed/late
packets are dropped, not crashed on; switching transports tears down the
previous one cleanly before starting the next.

## Testing Approach

- **Firmware:** line-position and PID math are pure functions and get native
  PlatformIO unit tests (no hardware required). Sensor I/O, motor driving,
  and the three transports are inherently hardware-dependent and will be
  verified manually (flash + observe) rather than automated — this will be
  stated explicitly, not glossed over.
- **Web app:** protocol encode/decode (JSON messages and the BLE binary
  struct) gets Vitest unit tests, since that logic is pure and easy to get
  subtly wrong (byte order, scaling). Actual Web Serial/Web Bluetooth/
  WebSocket connections to real hardware cannot be automated in this
  environment and will be verified manually against your robot.

## Open Items For Later (explicitly out of scope for v1)

- Full remote driving/control beyond start/stop.
- Supporting sensor arrays other than QTR-8A.
- Multi-robot support in a single dashboard session.
