# Line Follower Firmware

PlatformIO project for the ESP32. Reads a QTR-8A sensor array, runs a PID
line-following loop driving a TB6612FNG motor driver, and streams
telemetry / accepts commands over USB serial, WiFi (as its own access
point), and BLE simultaneously.

## Build & flash

```bash
cd firmware
pio run -e esp32dev -t upload
pio device monitor -b 115200
```

## Run native unit tests (no hardware required)

```bash
cd firmware
pio test -e native
```

Covers the pure logic modules: line position calculation, PID math, and
protocol encode/decode. Hardware-dependent modules (sensors, motors,
transports) are verified manually — see `docs/wiring.md` and the
end-to-end checklist in the implementation plan.

## Pin configuration

All pin assignments live in `include/config.h`. See `docs/wiring.md` for
the full wiring table and instructions for swapping to a different ESP32
board.

## WiFi

The ESP32 broadcasts its own access point:
- SSID: `LineFollower`
- Password: `linefollow`
- IP: `192.168.4.1`
- WebSocket endpoint: `ws://192.168.4.1/ws`

## Protocol

See `docs/protocol.md` for the full message reference (JSON schema, BLE
binary layout, GATT UUIDs).
