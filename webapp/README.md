# Line Follower Web Dashboard

Browser-only React/TypeScript dashboard for the ESP32 line-follower robot.
Connects over USB (Web Serial), WiFi (WebSocket to the robot's access
point), or Bluetooth LE (Web Bluetooth) — no backend server involved.

## Requirements

- Chrome or Edge (desktop). Web Serial and Web Bluetooth are not supported
  in Firefox or Safari.
- The ESP32 firmware from `../firmware` flashed and running.

## Run it

```bash
npm install
npm run dev
```

Open the printed `http://localhost...` URL in Chrome/Edge.

## Run the unit tests

```bash
npm test
```

Covers `protocol.ts` (JSON + BLE binary codec) and `recording.ts` (CSV/JSON
export formatting) — the only parts of this app that are pure functions.
Everything touching Web Serial/Web Bluetooth/WebSocket against real
hardware is verified manually (see below), since those APIs aren't
available under Node/Vitest.

## Protocol

See `../docs/protocol.md` for the full message reference this app's
`protocol.ts` implements.

## End-to-end hardware verification checklist

With the firmware flashed and powered on:

1. **USB**: click "Connect via USB", pick the ESP32's serial port in the
   browser dialog. Sensor bars and line position should update live.
2. **WiFi**: connect your laptop's WiFi to the `LineFollower` access point
   (password `linefollow`), then click "Connect via WiFi". Same live data.
3. **Bluetooth**: click "Connect via Bluetooth", pick "LineFollower" from
   the browser's Bluetooth device picker. Same live data (may update at a
   visibly lower rate than USB/WiFi — this is normal for BLE).
4. **PID tuning**: with wheels off the ground, adjust Kp/Ki/Kd/base speed,
   click Apply, then Start — confirm the PID chart's Error/Output
   sparklines move and the motors respond; Stop halts them immediately.
5. **Calibration**: click Start Calibration, sweep the sensor array over
   the line by hand for a few seconds, click Finish Calibration — confirm
   the state changes Idle → Calibrating → Idle and sensor bars read more
   sharply (closer to 0 or 1000) afterward.
6. **Recording**: click Start Recording, let telemetry accumulate for a
   few seconds, click Stop Recording, then Export CSV and Export JSON —
   confirm both files download and open with the expected columns/fields.
