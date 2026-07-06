# Line Follower Protocol Reference

Single logical protocol, two wire encodings. See
`firmware/include/protocol.h` for the authoritative struct/field
definitions — this document mirrors it for the web app implementation.

## Telemetry (ESP32 -> browser)

### JSON encoding (Serial, WiFi) — one line per sample, no trailing newline in the payload itself (transports append `\n` / send as one WS text frame)

```json
{
  "t": 12345,
  "s": [12, 45, 300, 980, 970, 250, 30, 10],
  "pos": 3500,
  "det": true,
  "err": -50,
  "out": 1.5,
  "ls": 120,
  "rs": 130,
  "cal": 0,
  "pid": true
}
```

Field meanings:
- `t`: timestamp, ms since ESP32 boot
- `s`: 8 calibrated sensor readings, 0-1000
- `pos`: line position, 0-7000, center (on-line) = 3500, or `-1` if not detected
- `det`: whether the line was detected this sample
- `err`: PID error (`pos - 3500`)
- `out`: raw PID output
- `ls`/`rs`: applied left/right motor speed, -255..255
- `cal`: calibration state — `0` idle, `1` calibrating, `2` done
- `cmin`/`cmax`: arrays of 8 raw calibration min/max values — present only while `cal != 0`
- `pid`: whether the PID line-following loop is currently enabled

### Binary encoding (BLE only) — 17 bytes, little-endian, packed

| Bytes | Field | Type | Notes |
|---|---|---|---|
| 0-1 | timestampDeltaMs | uint16 | ms since previous sample |
| 2-9 | sensors[8] | uint8[8] | calibrated 0-1000 scaled to 0-255 |
| 10-11 | linePosition | int16 | 0-7000, or -1 |
| 12-13 | pidError | int16 | |
| 14 | leftSpeed | int8 | -255..255 scaled to -127..127 |
| 15 | rightSpeed | int8 | same scaling |
| 16 | flags | uint8 | bit0=lineDetected, bit1=pidEnabled, bits2-3=calState |

Calibration min/max are NOT sent over BLE telemetry (JSON-only, since
calibration is a rare/manual flow) — the calibration panel should be used
over serial or WiFi if you need live min/max feedback while sweeping over
the line via BLE, or simply trust the "done" state.

## Commands (browser -> ESP32) — always JSON text, one command per line

```json
{"cmd": "setPid", "kp": 0.06, "ki": 0.0002, "kd": 0.6, "base": 150}
{"cmd": "calibrate", "start": true}
{"cmd": "calibrate", "start": false}
{"cmd": "start"}
{"cmd": "stop"}
```

Over Serial and WiFi, send one JSON object terminated by `\n` (serial) or
as one WebSocket text frame (WiFi — no `\n` needed, the frame boundary is
the message boundary). Over BLE, write to the command characteristic
(`6e400003-b5a3-f393-e0a9-e50e24dcca9e`); the firmware reassembles bytes
until it sees a `\n`, so it's fine to split a command across multiple BLE
writes.

## GATT UUIDs (BLE)

- Service: `6e400001-b5a3-f393-e0a9-e50e24dcca9e`
- Telemetry characteristic (notify): `6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- Command characteristic (write): `6e400003-b5a3-f393-e0a9-e50e24dcca9e`
