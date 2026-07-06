# Line Follower Web Dashboard Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build a browser-only React/TypeScript dashboard that connects to the ESP32 line-follower firmware over USB serial, WiFi, or Bluetooth LE, visualizes live QTR-8A sensor readings and line position, lets you tune PID constants live, run sensor calibration, and record/export a run as CSV/JSON.

**Architecture:** A Vite + React + TypeScript app with no backend. A `Transport` interface has three implementations (Web Serial, WebSocket, Web Bluetooth) that all decode/encode the same logical messages defined in `protocol.ts` (mirroring `docs/protocol.md`, written by the firmware plan). A single `useConnection` hook owns the active transport and telemetry state; dashboard components are pure presentational consumers of that state.

**Tech Stack:** React 18, TypeScript 5, Vite 5, Vitest 2, native browser Web Serial / Web Bluetooth / WebSocket APIs (no chart library — hand-rolled SVG per the `dataviz` skill's guidance).

## Global Constraints

- No backend server — runs entirely client-side via `vite dev`/static build; talks to the ESP32 directly from the browser.
- Requires Chrome or Edge (desktop) — Web Serial and Web Bluetooth are unsupported in Firefox/Safari; this is a hard platform constraint, not a bug.
- Message shapes must match `docs/protocol.md` exactly: JSON field names (`t`, `s`, `pos`, `det`, `err`, `out`, `ls`, `rs`, `cal`, `cmin`, `cmax`, `pid`) and the 17-byte BLE binary struct's field order/offsets.
- WiFi default endpoint: `ws://192.168.4.1/ws` (the ESP32's access-point IP).
- BLE GATT UUIDs: service `6e400001-b5a3-f393-e0a9-e50e24dcca9e`, telemetry characteristic (notify) `6e400002-b5a3-f393-e0a9-e50e24dcca9e`, command characteristic (write) `6e400003-b5a3-f393-e0a9-e50e24dcca9e`.
- Telemetry history buffer is capped at 500 samples (rolling window) to bound memory during long sessions; the recording buffer is uncapped (user-controlled start/stop, competition runs are short).
- Chart/graph code follows the `dataviz` skill: fixed categorical color order, one axis per chart (no dual-axis), legends for ≥2 series, direct labels instead of dense per-point labeling.

---

### Task 1: Project scaffold + `protocol.ts`

**Files:**
- Create: `D:\line\webapp\package.json`
- Create: `D:\line\webapp\vite.config.ts`
- Create: `D:\line\webapp\vitest.config.ts`
- Create: `D:\line\webapp\tsconfig.json`
- Create: `D:\line\webapp\index.html`
- Create: `D:\line\webapp\src\main.tsx`
- Create: `D:\line\webapp\src\App.tsx`
- Create: `D:\line\webapp\src\index.css`
- Create: `D:\line\webapp\src\protocol.ts`
- Test: `D:\line\webapp\src\protocol.test.ts`

**Interfaces:**
- Produces: `Telemetry` type, `Command` type (`PidCommand | CalibrateCommand | StartCommand | StopCommand`), `decodeTelemetryJson(line: string): Telemetry | null`, `decodeTelemetryDeltaMs(view: DataView): number`, `decodeTelemetryBinary(view: DataView, timestampMs: number): Telemetry`, `encodeCommand(command: Command): string` — consumed by Tasks 2–9.

- [ ] **Step 1: Scaffold the project files**

Create directories `D:\line\webapp\src`.

Write `D:\line\webapp\package.json`:

```json
{
  "name": "line-follower-webapp",
  "private": true,
  "version": "0.1.0",
  "type": "module",
  "scripts": {
    "dev": "vite",
    "build": "tsc -b && vite build",
    "preview": "vite preview",
    "test": "vitest run"
  },
  "dependencies": {
    "react": "^18.3.1",
    "react-dom": "^18.3.1"
  },
  "devDependencies": {
    "@types/react": "^18.3.12",
    "@types/react-dom": "^18.3.1",
    "@types/w3c-web-serial": "^1.0.6",
    "@types/web-bluetooth": "^0.0.20",
    "@vitejs/plugin-react": "^4.3.3",
    "typescript": "^5.6.3",
    "vite": "^5.4.10",
    "vitest": "^2.1.4"
  }
}
```

Write `D:\line\webapp\vite.config.ts`:

```typescript
import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    host: true,
  },
});
```

Write `D:\line\webapp\vitest.config.ts`:

```typescript
import { defineConfig } from 'vitest/config';

export default defineConfig({
  test: {
    environment: 'node',
  },
});
```

Write `D:\line\webapp\tsconfig.json`:

```json
{
  "compilerOptions": {
    "target": "ES2020",
    "useDefineForClassFields": true,
    "lib": ["ES2020", "DOM", "DOM.Iterable"],
    "module": "ESNext",
    "skipLibCheck": true,
    "moduleResolution": "bundler",
    "resolveJsonModule": true,
    "isolatedModules": true,
    "noEmit": true,
    "jsx": "react-jsx",
    "strict": true,
    "types": ["@types/w3c-web-serial", "@types/web-bluetooth"]
  },
  "include": ["src"]
}
```

Write `D:\line\webapp\index.html`:

```html
<!doctype html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Line Follower Dashboard</title>
  </head>
  <body>
    <div id="root"></div>
    <script type="module" src="/src/main.tsx"></script>
  </body>
</html>
```

Write `D:\line\webapp\src\main.tsx`:

```tsx
import { StrictMode } from 'react';
import { createRoot } from 'react-dom/client';
import App from './App';
import './index.css';

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <App />
  </StrictMode>
);
```

Write `D:\line\webapp\src\App.tsx` (placeholder — extended in Tasks 5–9):

```tsx
export default function App() {
  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
    </div>
  );
}
```

Write `D:\line\webapp\src\index.css` (full stylesheet for every panel used by later tasks — CSS is additive, so defining it once here avoids revisiting this file repeatedly):

```css
:root {
  --surface-1: #fcfcfb;
  --page: #f9f9f7;
  --text-primary: #0b0b0b;
  --text-secondary: #52514e;
  --text-muted: #898781;
  --gridline: #e1e0d9;
  --baseline: #c3c2b7;
  --series-1: #2a78d6;
  --series-2: #1baf7a;
  --diverging-pos: #e34948;
  --diverging-neg: #2a78d6;
  --diverging-mid: #f0efec;
  --border: rgba(11, 11, 11, 0.10);
}

@media (prefers-color-scheme: dark) {
  :root {
    --surface-1: #1a1a19;
    --page: #0d0d0d;
    --text-primary: #ffffff;
    --text-secondary: #c3c2b7;
    --text-muted: #898781;
    --gridline: #2c2c2a;
    --baseline: #383835;
    --series-1: #3987e5;
    --series-2: #199e70;
    --diverging-pos: #e66767;
    --diverging-neg: #3987e5;
    --diverging-mid: #383835;
    --border: rgba(255, 255, 255, 0.10);
  }
}

* { box-sizing: border-box; }

body {
  margin: 0;
  background: var(--page);
  color: var(--text-primary);
  font-family: system-ui, -apple-system, "Segoe UI", sans-serif;
}

.app {
  max-width: 1000px;
  margin: 0 auto;
  padding: 24px;
  display: grid;
  gap: 16px;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
}

.app > h1 {
  grid-column: 1 / -1;
}

.panel {
  background: var(--surface-1);
  border: 1px solid var(--border);
  border-radius: 8px;
  padding: 16px;
}

.panel h2 {
  margin-top: 0;
  font-size: 16px;
  color: var(--text-secondary);
}

.muted {
  color: var(--text-muted);
  font-size: 13px;
}

.connection-buttons, .pid-buttons {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 12px;
}

.status-connected { color: #0ca30c; }
.status-error { color: #d03b3b; }
.status-connecting, .status-disconnected { color: var(--text-muted); }

.legend {
  display: flex;
  gap: 16px;
  font-size: 12px;
  color: var(--text-secondary);
  margin-top: 4px;
}

.legend-item {
  display: inline-flex;
  align-items: center;
  gap: 6px;
}

.swatch {
  width: 10px;
  height: 10px;
  border-radius: 2px;
  display: inline-block;
}

.sparkline-label {
  display: flex;
  justify-content: space-between;
  font-size: 12px;
  margin-top: 8px;
}

.cal-table {
  width: 100%;
  border-collapse: collapse;
  font-size: 12px;
  margin-top: 8px;
}

.cal-table th, .cal-table td {
  border-bottom: 1px solid var(--gridline);
  padding: 4px 8px;
  text-align: right;
}

label {
  display: flex;
  justify-content: space-between;
  gap: 8px;
  margin-bottom: 8px;
  font-size: 13px;
}

input[type="number"] {
  width: 100px;
}
```

- [ ] **Step 2: Write `protocol.ts`'s type definitions**

```typescript
// D:\line\webapp\src\protocol.ts
export interface Telemetry {
  timestampMs: number;
  sensors: number[]; // 8 values, 0-1000
  linePosition: number; // 0-7000, or -1 if not detected
  lineDetected: boolean;
  pidError: number;
  pidOutput: number;
  leftSpeed: number; // -255..255
  rightSpeed: number; // -255..255
  calState: 0 | 1 | 2; // idle / calibrating / done
  calMin?: number[];
  calMax?: number[];
  pidEnabled: boolean;
}

export interface PidCommand {
  cmd: 'setPid';
  kp: number;
  ki: number;
  kd: number;
  base: number;
}
export interface CalibrateCommand {
  cmd: 'calibrate';
  start: boolean;
}
export interface StartCommand {
  cmd: 'start';
}
export interface StopCommand {
  cmd: 'stop';
}
export type Command = PidCommand | CalibrateCommand | StartCommand | StopCommand;
```

- [ ] **Step 3: Write the failing test**

```typescript
// D:\line\webapp\src\protocol.test.ts
import { describe, expect, test } from 'vitest';
import {
  decodeTelemetryJson,
  decodeTelemetryDeltaMs,
  decodeTelemetryBinary,
  encodeCommand,
} from './protocol';

function buildBinaryView(fields: {
  deltaMs: number;
  sensors: number[]; // 8 values, 0-255
  linePosition: number;
  pidError: number;
  leftSpeed: number; // -127..127
  rightSpeed: number;
  flags: number;
}): DataView {
  const buf = new ArrayBuffer(17);
  const view = new DataView(buf);
  view.setUint16(0, fields.deltaMs, true);
  for (let i = 0; i < 8; i++) view.setUint8(2 + i, fields.sensors[i]);
  view.setInt16(10, fields.linePosition, true);
  view.setInt16(12, fields.pidError, true);
  view.setInt8(14, fields.leftSpeed);
  view.setInt8(15, fields.rightSpeed);
  view.setUint8(16, fields.flags);
  return view;
}

describe('decodeTelemetryJson', () => {
  test('parses a valid telemetry line', () => {
    const line =
      '{"t":12345,"s":[12,45,300,980,970,250,30,10],"pos":3500,"det":true,"err":-50,"out":1.5,"ls":120,"rs":130,"cal":0,"pid":true}';
    const t = decodeTelemetryJson(line);
    expect(t).not.toBeNull();
    expect(t!.timestampMs).toBe(12345);
    expect(t!.sensors).toEqual([12, 45, 300, 980, 970, 250, 30, 10]);
    expect(t!.linePosition).toBe(3500);
    expect(t!.lineDetected).toBe(true);
    expect(t!.pidOutput).toBe(1.5);
    expect(t!.pidEnabled).toBe(true);
  });

  test('returns null for malformed JSON', () => {
    expect(decodeTelemetryJson('{not json')).toBeNull();
  });

  test('returns null when required fields are missing', () => {
    expect(decodeTelemetryJson('{"foo":1}')).toBeNull();
  });
});

describe('binary telemetry decoding', () => {
  test('decodeTelemetryDeltaMs reads the first two bytes as little-endian uint16', () => {
    const view = buildBinaryView({
      deltaMs: 20,
      sensors: new Array(8).fill(0),
      linePosition: 0,
      pidError: 0,
      leftSpeed: 0,
      rightSpeed: 0,
      flags: 0,
    });
    expect(decodeTelemetryDeltaMs(view)).toBe(20);
  });

  test('decodeTelemetryBinary decodes sensors, position, speeds, and flags', () => {
    const view = buildBinaryView({
      deltaMs: 20,
      sensors: [255, 0, 0, 0, 0, 0, 0, 0],
      linePosition: 3500,
      pidError: -50,
      leftSpeed: 127,
      rightSpeed: -127,
      flags: 0b0011,
    });
    const t = decodeTelemetryBinary(view, 12345);
    expect(t.timestampMs).toBe(12345);
    expect(t.sensors[0]).toBe(1000);
    expect(t.sensors[1]).toBe(0);
    expect(t.linePosition).toBe(3500);
    expect(t.pidError).toBe(-50);
    expect(t.leftSpeed).toBe(255);
    expect(t.rightSpeed).toBe(-255);
    expect(t.lineDetected).toBe(true);
    expect(t.pidEnabled).toBe(true);
    expect(t.calState).toBe(0);
  });
});

describe('encodeCommand', () => {
  test('serializes a setPid command', () => {
    const json = encodeCommand({ cmd: 'setPid', kp: 0.06, ki: 0.0002, kd: 0.6, base: 150 });
    expect(JSON.parse(json)).toEqual({ cmd: 'setPid', kp: 0.06, ki: 0.0002, kd: 0.6, base: 150 });
  });

  test('serializes a calibrate command', () => {
    const json = encodeCommand({ cmd: 'calibrate', start: true });
    expect(JSON.parse(json)).toEqual({ cmd: 'calibrate', start: true });
  });
});
```

- [ ] **Step 4: Run the test to verify it fails**

Run: `cd D:\line\webapp && npm install && npm test`
Expected: FAILS to resolve `decodeTelemetryJson`/`decodeTelemetryDeltaMs`/`decodeTelemetryBinary`/`encodeCommand` from `./protocol` (not yet implemented).

- [ ] **Step 5: Implement the codec functions in `protocol.ts`**

Append to `D:\line\webapp\src\protocol.ts`:

```typescript
export function decodeTelemetryJson(line: string): Telemetry | null {
  let obj: any;
  try {
    obj = JSON.parse(line);
  } catch {
    return null;
  }
  if (typeof obj.t !== 'number' || !Array.isArray(obj.s)) return null;
  return {
    timestampMs: obj.t,
    sensors: obj.s,
    linePosition: obj.pos,
    lineDetected: !!obj.det,
    pidError: obj.err,
    pidOutput: obj.out,
    leftSpeed: obj.ls,
    rightSpeed: obj.rs,
    calState: obj.cal,
    calMin: obj.cmin,
    calMax: obj.cmax,
    pidEnabled: !!obj.pid,
  };
}

// Reads the first two bytes of a BLE telemetry notification (little-endian
// uint16 ms-since-previous-sample). The caller accumulates this into an
// absolute clock before calling decodeTelemetryBinary.
export function decodeTelemetryDeltaMs(view: DataView): number {
  return view.getUint16(0, true);
}

// Decodes the remaining 17-byte BLE telemetry struct. `timestampMs` is the
// caller's accumulated absolute timestamp (see decodeTelemetryDeltaMs).
export function decodeTelemetryBinary(view: DataView, timestampMs: number): Telemetry {
  const sensors: number[] = [];
  for (let i = 0; i < 8; i++) {
    sensors.push(Math.round((view.getUint8(2 + i) * 1000) / 255));
  }
  const linePosition = view.getInt16(10, true);
  const pidError = view.getInt16(12, true);
  const leftSpeed = Math.round((view.getInt8(14) * 255) / 127);
  const rightSpeed = Math.round((view.getInt8(15) * 255) / 127);
  const flags = view.getUint8(16);
  return {
    timestampMs,
    sensors,
    linePosition,
    lineDetected: (flags & 0x01) !== 0,
    pidError,
    pidOutput: 0, // not carried over the BLE binary encoding (JSON-only field)
    leftSpeed,
    rightSpeed,
    calState: ((flags >> 2) & 0x03) as 0 | 1 | 2,
    pidEnabled: (flags & 0x02) !== 0,
  };
}

export function encodeCommand(command: Command): string {
  return JSON.stringify(command);
}
```

- [ ] **Step 6: Run the test to verify it passes**

Run: `cd D:\line\webapp && npm test`
Expected: all 7 tests pass.

- [ ] **Step 7: Commit**

```bash
cd D:\line\webapp
git add package.json vite.config.ts vitest.config.ts tsconfig.json index.html src/main.tsx src/App.tsx src/index.css src/protocol.ts src/protocol.test.ts
git commit -m "feat(webapp): scaffold Vite/React/TS project + shared protocol module"
```

---

### Task 2: `Transport` interface + `SerialTransport` (Web Serial)

**Files:**
- Create: `D:\line\webapp\src\transports\Transport.ts`
- Create: `D:\line\webapp\src\transports\SerialTransport.ts`

**Interfaces:**
- Consumes: `Telemetry`, `Command`, `decodeTelemetryJson`, `encodeCommand` (Task 1)
- Produces: `interface TransportEvents { onTelemetry(t: Telemetry): void; onStatusChange(status: 'connected'|'disconnected'|'error', message?: string): void; }`, `interface Transport { connect(events: TransportEvents): Promise<void>; disconnect(): Promise<void>; send(command: Command): void; }`, `class SerialTransport implements Transport` — consumed by Tasks 3, 4, 5.

This module depends on the browser's Web Serial API, which is unavailable under Node/Vitest — it is verified by a TypeScript compile check and, once real hardware exists, manually in Task 10's end-to-end checklist.

- [ ] **Step 1: Write `Transport.ts`**

```typescript
// D:\line\webapp\src\transports\Transport.ts
import type { Command, Telemetry } from '../protocol';

export interface TransportEvents {
  onTelemetry: (telemetry: Telemetry) => void;
  onStatusChange: (status: 'connected' | 'disconnected' | 'error', message?: string) => void;
}

export interface Transport {
  connect(events: TransportEvents): Promise<void>;
  disconnect(): Promise<void>;
  send(command: Command): void;
}
```

- [ ] **Step 2: Write `SerialTransport.ts`**

```typescript
// D:\line\webapp\src\transports\SerialTransport.ts
import { decodeTelemetryJson, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

export class SerialTransport implements Transport {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<string> | null = null;
  private writer: WritableStreamDefaultWriter<string> | null = null;
  private keepReading = false;
  private events: TransportEvents | null = null;

  async connect(events: TransportEvents): Promise<void> {
    this.events = events;
    if (!('serial' in navigator)) {
      events.onStatusChange('error', 'Web Serial API not supported in this browser');
      throw new Error('Web Serial not supported');
    }

    this.port = await navigator.serial.requestPort();
    await this.port.open({ baudRate: 115200 });

    const textEncoder = new TextEncoderStream();
    textEncoder.readable.pipeTo(this.port.writable!);
    this.writer = textEncoder.writable.getWriter();

    this.keepReading = true;
    events.onStatusChange('connected');
    this.readLoop();
  }

  private async readLoop(): Promise<void> {
    if (!this.port?.readable) return;
    const textDecoder = new TextDecoderStream();
    const readableClosed = this.port.readable.pipeTo(textDecoder.writable);
    const reader = textDecoder.readable.getReader();
    this.reader = reader;

    let buffer = '';
    try {
      while (this.keepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let newlineIndex: number;
        while ((newlineIndex = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, newlineIndex).trim();
          buffer = buffer.slice(newlineIndex + 1);
          if (line.length > 0) {
            const telemetry = decodeTelemetryJson(line);
            if (telemetry) this.events?.onTelemetry(telemetry);
          }
        }
      }
    } catch (err) {
      this.events?.onStatusChange('error', err instanceof Error ? err.message : String(err));
    } finally {
      reader.releaseLock();
      await readableClosed.catch(() => {});
    }
  }

  send(command: Command): void {
    this.writer?.write(encodeCommand(command) + '\n').catch(() => {});
  }

  async disconnect(): Promise<void> {
    this.keepReading = false;
    await this.reader?.cancel().catch(() => {});
    await this.writer?.close().catch(() => {});
    await this.port?.close().catch(() => {});
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.events?.onStatusChange('disconnected');
  }
}
```

- [ ] **Step 3: Compile-check**

Run: `cd D:\line\webapp && npx tsc -b --noEmit`
Expected: no type errors (the `@types/w3c-web-serial` package supplies `SerialPort`/`navigator.serial`).

- [ ] **Step 4: Commit**

```bash
cd D:\line\webapp
git add src/transports/Transport.ts src/transports/SerialTransport.ts
git commit -m "feat(webapp): add Transport interface and Web Serial transport"
```

---

### Task 3: `WebSocketTransport` (WiFi)

**Files:**
- Create: `D:\line\webapp\src\transports\WebSocketTransport.ts`

**Interfaces:**
- Consumes: `Telemetry`, `Command`, `decodeTelemetryJson`, `encodeCommand` (Task 1), `Transport`/`TransportEvents` (Task 2)
- Produces: `class WebSocketTransport implements Transport` (constructor takes a `url` defaulting to `'ws://192.168.4.1/ws'`) — consumed by Task 5.

- [ ] **Step 1: Write `WebSocketTransport.ts`**

```typescript
// D:\line\webapp\src\transports\WebSocketTransport.ts
import { decodeTelemetryJson, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

export class WebSocketTransport implements Transport {
  private ws: WebSocket | null = null;

  constructor(private readonly url: string = 'ws://192.168.4.1/ws') {}

  connect(events: TransportEvents): Promise<void> {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(this.url);
      this.ws = ws;

      ws.onopen = () => {
        events.onStatusChange('connected');
        resolve();
      };
      ws.onmessage = (ev) => {
        if (typeof ev.data === 'string') {
          const telemetry = decodeTelemetryJson(ev.data);
          if (telemetry) events.onTelemetry(telemetry);
        }
      };
      ws.onerror = () => {
        events.onStatusChange('error', 'WebSocket connection error');
        reject(new Error('WebSocket connection failed'));
      };
      ws.onclose = () => {
        events.onStatusChange('disconnected');
      };
    });
  }

  send(command: Command): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(encodeCommand(command));
    }
  }

  async disconnect(): Promise<void> {
    this.ws?.close();
    this.ws = null;
  }
}
```

- [ ] **Step 2: Compile-check**

Run: `cd D:\line\webapp && npx tsc -b --noEmit`
Expected: no type errors.

- [ ] **Step 3: Commit**

```bash
cd D:\line\webapp
git add src/transports/WebSocketTransport.ts
git commit -m "feat(webapp): add WiFi WebSocket transport"
```

---

### Task 4: `BleTransport` (Web Bluetooth)

**Files:**
- Create: `D:\line\webapp\src\transports\BleTransport.ts`

**Interfaces:**
- Consumes: `Telemetry`, `Command`, `decodeTelemetryDeltaMs`, `decodeTelemetryBinary`, `encodeCommand` (Task 1), `Transport`/`TransportEvents` (Task 2)
- Produces: `class BleTransport implements Transport` — consumed by Task 5.

- [ ] **Step 1: Write `BleTransport.ts`**

```typescript
// D:\line\webapp\src\transports\BleTransport.ts
import { decodeTelemetryBinary, decodeTelemetryDeltaMs, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const TELEMETRY_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const COMMAND_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

export class BleTransport implements Transport {
  private device: BluetoothDevice | null = null;
  private commandChar: BluetoothRemoteGATTCharacteristic | null = null;
  private events: TransportEvents | null = null;
  private accumulatedTimestampMs = 0;

  async connect(events: TransportEvents): Promise<void> {
    this.events = events;
    if (!('bluetooth' in navigator)) {
      events.onStatusChange('error', 'Web Bluetooth API not supported in this browser');
      throw new Error('Web Bluetooth not supported');
    }

    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    });
    this.device.addEventListener('gattserverdisconnected', () => {
      this.events?.onStatusChange('disconnected');
    });

    const server = await this.device.gatt!.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    const telemetryChar = await service.getCharacteristic(TELEMETRY_CHAR_UUID);
    this.commandChar = await service.getCharacteristic(COMMAND_CHAR_UUID);

    await telemetryChar.startNotifications();
    telemetryChar.addEventListener('characteristicvaluechanged', (ev) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const view = target.value;
      if (!view) return;
      this.accumulatedTimestampMs += decodeTelemetryDeltaMs(view);
      const telemetry = decodeTelemetryBinary(view, this.accumulatedTimestampMs);
      this.events?.onTelemetry(telemetry);
    });

    events.onStatusChange('connected');
  }

  send(command: Command): void {
    if (!this.commandChar) return;
    const bytes = new TextEncoder().encode(encodeCommand(command) + '\n');
    this.commandChar.writeValueWithoutResponse(bytes).catch(() => {});
  }

  async disconnect(): Promise<void> {
    this.device?.gatt?.disconnect();
    this.device = null;
    this.commandChar = null;
  }
}
```

- [ ] **Step 2: Compile-check**

Run: `cd D:\line\webapp && npx tsc -b --noEmit`
Expected: no type errors (`@types/web-bluetooth` supplies `BluetoothDevice`/`navigator.bluetooth`).

- [ ] **Step 3: Commit**

```bash
cd D:\line\webapp
git add src/transports/BleTransport.ts
git commit -m "feat(webapp): add BLE transport (Web Bluetooth)"
```

---

### Task 5: `useConnection` hook + `ConnectionPanel` + App wiring

**Files:**
- Create: `D:\line\webapp\src\state\useConnection.ts`
- Create: `D:\line\webapp\src\components\ConnectionPanel.tsx`
- Modify: `D:\line\webapp\src\App.tsx`

**Interfaces:**
- Consumes: `Transport`, `SerialTransport`, `WebSocketTransport`, `BleTransport` (Tasks 2–4), `Telemetry`, `Command` (Task 1)
- Produces: `useConnection(): { status, statusMessage, telemetry, history, connect(kind), disconnect(), sendCommand(command) }`, `type TransportKind = 'serial' | 'wifi' | 'ble'`, `type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error'` — consumed by Tasks 6–9.

- [ ] **Step 1: Write `useConnection.ts`**

```typescript
// D:\line\webapp\src\state\useConnection.ts
import { useCallback, useRef, useState } from 'react';
import type { Command, Telemetry } from '../protocol';
import type { Transport } from '../transports/Transport';
import { SerialTransport } from '../transports/SerialTransport';
import { WebSocketTransport } from '../transports/WebSocketTransport';
import { BleTransport } from '../transports/BleTransport';

export type TransportKind = 'serial' | 'wifi' | 'ble';
export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error';

const HISTORY_LIMIT = 500;

export function useConnection() {
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const [statusMessage, setStatusMessage] = useState<string | undefined>(undefined);
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [history, setHistory] = useState<Telemetry[]>([]);
  const transportRef = useRef<Transport | null>(null);

  const connect = useCallback(async (kind: TransportKind) => {
    setStatus('connecting');
    setStatusMessage(undefined);

    const transport: Transport =
      kind === 'serial'
        ? new SerialTransport()
        : kind === 'wifi'
          ? new WebSocketTransport()
          : new BleTransport();

    transportRef.current = transport;
    try {
      await transport.connect({
        onTelemetry: (t) => {
          setTelemetry(t);
          setHistory((prev) => {
            const next = [...prev, t];
            return next.length > HISTORY_LIMIT ? next.slice(next.length - HISTORY_LIMIT) : next;
          });
        },
        onStatusChange: (s, message) => {
          setStatus(s);
          setStatusMessage(message);
        },
      });
    } catch (err) {
      setStatus('error');
      setStatusMessage(err instanceof Error ? err.message : String(err));
      transportRef.current = null;
    }
  }, []);

  const disconnect = useCallback(async () => {
    await transportRef.current?.disconnect();
    transportRef.current = null;
    setStatus('disconnected');
  }, []);

  const sendCommand = useCallback((command: Command) => {
    transportRef.current?.send(command);
  }, []);

  return { status, statusMessage, telemetry, history, connect, disconnect, sendCommand };
}
```

- [ ] **Step 2: Write `ConnectionPanel.tsx`**

```tsx
// D:\line\webapp\src\components\ConnectionPanel.tsx
import type { ConnectionStatus, TransportKind } from '../state/useConnection';

interface ConnectionPanelProps {
  status: ConnectionStatus;
  statusMessage?: string;
  onConnect: (kind: TransportKind) => void;
  onDisconnect: () => void;
}

export function ConnectionPanel({ status, statusMessage, onConnect, onDisconnect }: ConnectionPanelProps) {
  const busy = status === 'connected' || status === 'connecting';
  return (
    <section className="panel">
      <h2>Connection</h2>
      <div className="connection-buttons">
        <button disabled={busy} onClick={() => onConnect('serial')}>Connect via USB</button>
        <button disabled={busy} onClick={() => onConnect('wifi')}>Connect via WiFi</button>
        <button disabled={busy} onClick={() => onConnect('ble')}>Connect via Bluetooth</button>
        <button disabled={!busy} onClick={onDisconnect}>Disconnect</button>
      </div>
      <p className={`status-${status}`}>
        Status: {status}
        {statusMessage ? ` — ${statusMessage}` : ''}
      </p>
    </section>
  );
}
```

- [ ] **Step 3: Wire into `App.tsx`**

```tsx
// D:\line\webapp\src\App.tsx
import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';

export default function App() {
  const { status, statusMessage, connect, disconnect } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
    </div>
  );
}
```

- [ ] **Step 4: Manual verification**

Run: `cd D:\line\webapp && npm run dev`, open the printed `http://localhost:5173` URL in Chrome.
Expected: page shows "Line Follower Dashboard" and a Connection panel. Click "Connect via USB" — Chrome's native serial port picker dialog should appear (proves the Web Serial call path is wired correctly even without the ESP32 plugged in yet; cancel the dialog, no crash expected).

- [ ] **Step 5: Commit**

```bash
cd D:\line\webapp
git add src/state/useConnection.ts src/components/ConnectionPanel.tsx src/App.tsx
git commit -m "feat(webapp): add connection state hook and connection panel"
```

---

### Task 6: `SensorBarGraph` + `LinePositionIndicator`

**Files:**
- Create: `D:\line\webapp\src\components\SensorBarGraph.tsx`
- Create: `D:\line\webapp\src\components\LinePositionIndicator.tsx`
- Modify: `D:\line\webapp\src\App.tsx`

**Interfaces:**
- Consumes: `Telemetry` (Task 1), `useConnection` output (Task 5)
- Produces: `SensorBarGraph({ sensors: number[] })`, `LinePositionIndicator({ linePosition: number, lineDetected: boolean })` — consumed by App only in this plan.

Per the `dataviz` skill: the sensor graph is a single-series magnitude chart (flat categorical fill, one axis, no legend needed) with a per-bar hover label; the position indicator is a diverging encoding (blue↔red, neutral gray midpoint) since it represents signed deviation from center.

- [ ] **Step 1: Write `SensorBarGraph.tsx`**

```tsx
// D:\line\webapp\src\components\SensorBarGraph.tsx
import { useState } from 'react';

interface SensorBarGraphProps {
  sensors: number[]; // 8 values, 0-1000
}

const WIDTH = 320;
const HEIGHT = 140;
const BAR_GAP = 6;
const BAR_COUNT = 8;
const BAR_WIDTH = (WIDTH - BAR_GAP * (BAR_COUNT - 1)) / BAR_COUNT;

export function SensorBarGraph({ sensors }: SensorBarGraphProps) {
  const [hoverIndex, setHoverIndex] = useState<number | null>(null);

  return (
    <div className="panel">
      <h2>Sensor Readings</h2>
      <svg
        width={WIDTH}
        height={HEIGHT + 24}
        role="img"
        aria-label="Bar chart of 8 QTR-8A calibrated sensor readings"
      >
        <line x1={0} y1={HEIGHT} x2={WIDTH} y2={HEIGHT} stroke="var(--baseline)" strokeWidth={1} />
        {sensors.map((value, i) => {
          const clamped = Math.max(0, Math.min(1000, value));
          const barHeight = (clamped / 1000) * (HEIGHT - 4);
          const x = i * (BAR_WIDTH + BAR_GAP);
          const y = HEIGHT - barHeight;
          return (
            <g key={i} onMouseEnter={() => setHoverIndex(i)} onMouseLeave={() => setHoverIndex(null)}>
              <rect
                x={x}
                y={y}
                width={BAR_WIDTH}
                height={barHeight}
                rx={4}
                ry={4}
                fill="var(--series-1)"
                opacity={hoverIndex === null || hoverIndex === i ? 1 : 0.55}
              />
              <text x={x + BAR_WIDTH / 2} y={HEIGHT + 16} textAnchor="middle" fontSize={11} fill="var(--text-muted)">
                {i}
              </text>
              {hoverIndex === i && (
                <text
                  x={x + BAR_WIDTH / 2}
                  y={Math.max(12, y - 6)}
                  textAnchor="middle"
                  fontSize={11}
                  fill="var(--text-primary)"
                >
                  {clamped}
                </text>
              )}
            </g>
          );
        })}
      </svg>
    </div>
  );
}
```

- [ ] **Step 2: Write `LinePositionIndicator.tsx`**

```tsx
// D:\line\webapp\src\components\LinePositionIndicator.tsx
interface LinePositionIndicatorProps {
  linePosition: number; // 0-7000, or -1 if not detected
  lineDetected: boolean;
}

const TRACK_WIDTH = 320;
const TRACK_HEIGHT = 24;
const RANGE = 3500; // -3500..+3500 relative to center (3500)

export function LinePositionIndicator({ linePosition, lineDetected }: LinePositionIndicatorProps) {
  const error = lineDetected ? linePosition - 3500 : 0;
  const clampedError = Math.max(-RANGE, Math.min(RANGE, error));
  const fraction = clampedError / RANGE; // -1..1
  const markerX = TRACK_WIDTH / 2 + fraction * (TRACK_WIDTH / 2 - 8);

  return (
    <div className="panel">
      <h2>Line Position</h2>
      <svg width={TRACK_WIDTH} height={TRACK_HEIGHT + 20} role="img" aria-label="Line position relative to center">
        <defs>
          <linearGradient id="position-track" x1="0" y1="0" x2="1" y2="0">
            <stop offset="0%" stopColor="var(--diverging-neg)" />
            <stop offset="50%" stopColor="var(--diverging-mid)" />
            <stop offset="100%" stopColor="var(--diverging-pos)" />
          </linearGradient>
        </defs>
        <rect x={0} y={0} width={TRACK_WIDTH} height={TRACK_HEIGHT} rx={6} fill="url(#position-track)" opacity={0.35} />
        <line x1={TRACK_WIDTH / 2} y1={0} x2={TRACK_WIDTH / 2} y2={TRACK_HEIGHT} stroke="var(--baseline)" strokeWidth={1} />
        {lineDetected && <circle cx={markerX} cy={TRACK_HEIGHT / 2} r={8} fill="var(--text-primary)" />}
        <text x={TRACK_WIDTH / 2} y={TRACK_HEIGHT + 16} textAnchor="middle" fontSize={11} fill="var(--text-muted)">
          {lineDetected ? `error ${error}` : 'line not detected'}
        </text>
      </svg>
    </div>
  );
}
```

- [ ] **Step 3: Wire into `App.tsx`**

```tsx
// D:\line\webapp\src\App.tsx
import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';

export default function App() {
  const { status, statusMessage, telemetry, connect, disconnect } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
      {telemetry && (
        <>
          <SensorBarGraph sensors={telemetry.sensors} />
          <LinePositionIndicator linePosition={telemetry.linePosition} lineDetected={telemetry.lineDetected} />
        </>
      )}
    </div>
  );
}
```

- [ ] **Step 4: Manual verification**

Run: `cd D:\line\webapp && npm run dev`. Without hardware connected, temporarily confirm the components render correctly by checking they compile and the "no telemetry yet" state (panels hidden) looks correct at `http://localhost:5173`; full live-data verification happens in Task 10 against real hardware.

- [ ] **Step 5: Commit**

```bash
cd D:\line\webapp
git add src/components/SensorBarGraph.tsx src/components/LinePositionIndicator.tsx src/App.tsx
git commit -m "feat(webapp): add sensor bar graph and line position indicator"
```

---

### Task 7: `PidTuningPanel` + `PidChart`

**Files:**
- Create: `D:\line\webapp\src\components\PidTuningPanel.tsx`
- Create: `D:\line\webapp\src\components\PidChart.tsx`
- Modify: `D:\line\webapp\src\App.tsx`

**Interfaces:**
- Consumes: `Command`, `Telemetry` (Task 1), `useConnection` output (Task 5)
- Produces: `PidTuningPanel({ onApply, onStart, onStop, pidEnabled })`, `PidChart({ history })` — consumed by App only.

Per `dataviz`: error and output are different physical units, so rather than force them onto one shared/normalized axis (the dual-axis anti-pattern), they're rendered as two small-multiple sparklines, each with its own domain, a swatch + direct-label instead of a legend box, and a fixed categorical color per series (`--series-1` for error, `--series-2` for output).

- [ ] **Step 1: Write `PidTuningPanel.tsx`**

```tsx
// D:\line\webapp\src\components\PidTuningPanel.tsx
import { useState } from 'react';
import type { Command } from '../protocol';

interface PidTuningPanelProps {
  onApply: (command: Command) => void;
  onStart: () => void;
  onStop: () => void;
  pidEnabled: boolean;
}

export function PidTuningPanel({ onApply, onStart, onStop, pidEnabled }: PidTuningPanelProps) {
  const [kp, setKp] = useState(0.06);
  const [ki, setKi] = useState(0.0002);
  const [kd, setKd] = useState(0.6);
  const [base, setBase] = useState(150);

  return (
    <section className="panel">
      <h2>PID Tuning</h2>
      <label>
        Kp
        <input type="number" step="0.001" value={kp} onChange={(e) => setKp(Number(e.target.value))} />
      </label>
      <label>
        Ki
        <input type="number" step="0.0001" value={ki} onChange={(e) => setKi(Number(e.target.value))} />
      </label>
      <label>
        Kd
        <input type="number" step="0.01" value={kd} onChange={(e) => setKd(Number(e.target.value))} />
      </label>
      <label>
        Base speed
        <input type="number" min={0} max={255} value={base} onChange={(e) => setBase(Number(e.target.value))} />
      </label>
      <div className="pid-buttons">
        <button onClick={() => onApply({ cmd: 'setPid', kp, ki, kd, base })}>Apply</button>
        <button onClick={onStart} disabled={pidEnabled}>Start</button>
        <button onClick={onStop} disabled={!pidEnabled}>Stop</button>
      </div>
    </section>
  );
}
```

- [ ] **Step 2: Write `PidChart.tsx`**

```tsx
// D:\line\webapp\src\components\PidChart.tsx
interface PidChartProps {
  history: Array<{ pidError: number; pidOutput: number }>;
}

const WIDTH = 320;
const HEIGHT = 70;
const MAX_POINTS = 100;

function Sparkline({ values, color, label }: { values: number[]; color: string; label: string }) {
  const maxAbs = Math.max(1, ...values.map((v) => Math.abs(v)));
  const path = values
    .map((v, i) => {
      const x = (i / Math.max(1, values.length - 1)) * WIDTH;
      const y = HEIGHT / 2 - (v / maxAbs) * (HEIGHT / 2 - 6);
      return `${i === 0 ? 'M' : 'L'}${x.toFixed(1)},${y.toFixed(1)}`;
    })
    .join(' ');
  const latest = values[values.length - 1];

  return (
    <div>
      <div className="sparkline-label">
        <span className="legend-item">
          <span className="swatch" style={{ background: color }} /> {label}
        </span>
        <span className="muted">{latest.toFixed(2)}</span>
      </div>
      <svg width={WIDTH} height={HEIGHT} role="img" aria-label={`${label} over time`}>
        <line x1={0} y1={HEIGHT / 2} x2={WIDTH} y2={HEIGHT / 2} stroke="var(--gridline)" strokeWidth={1} />
        <path d={path} fill="none" stroke={color} strokeWidth={2} />
      </svg>
    </div>
  );
}

export function PidChart({ history }: PidChartProps) {
  const points = history.slice(-MAX_POINTS);
  if (points.length < 2) {
    return (
      <div className="panel">
        <h2>PID Error / Output</h2>
        <p className="muted">Waiting for telemetry...</p>
      </div>
    );
  }
  return (
    <div className="panel">
      <h2>PID Error / Output</h2>
      <Sparkline values={points.map((p) => p.pidError)} color="var(--series-1)" label="Error" />
      <Sparkline values={points.map((p) => p.pidOutput)} color="var(--series-2)" label="Output" />
    </div>
  );
}
```

- [ ] **Step 3: Wire into `App.tsx`**

```tsx
// D:\line\webapp\src\App.tsx
import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';
import { PidTuningPanel } from './components/PidTuningPanel';
import { PidChart } from './components/PidChart';

export default function App() {
  const { status, statusMessage, telemetry, history, connect, disconnect, sendCommand } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
      {telemetry && (
        <>
          <SensorBarGraph sensors={telemetry.sensors} />
          <LinePositionIndicator linePosition={telemetry.linePosition} lineDetected={telemetry.lineDetected} />
        </>
      )}
      <PidTuningPanel
        onApply={sendCommand}
        onStart={() => sendCommand({ cmd: 'start' })}
        onStop={() => sendCommand({ cmd: 'stop' })}
        pidEnabled={telemetry?.pidEnabled ?? false}
      />
      <PidChart history={history} />
    </div>
  );
}
```

- [ ] **Step 4: Manual verification**

Run: `cd D:\line\webapp && npm run dev`. Confirm the PID Tuning panel renders with default values and the PID chart shows "Waiting for telemetry..." with no connection. Full live behavior (Apply pushing values, chart updating) is verified against real hardware in Task 10.

- [ ] **Step 5: Commit**

```bash
cd D:\line\webapp
git add src/components/PidTuningPanel.tsx src/components/PidChart.tsx src/App.tsx
git commit -m "feat(webapp): add PID tuning panel and error/output sparklines"
```

---

### Task 8: `CalibrationPanel`

**Files:**
- Create: `D:\line\webapp\src\components\CalibrationPanel.tsx`
- Modify: `D:\line\webapp\src\App.tsx`

**Interfaces:**
- Consumes: `Command`, `Telemetry` (Task 1), `useConnection` output (Task 5)
- Produces: `CalibrationPanel({ telemetry, onSendCommand })` — consumed by App only.

The per-channel min/max readout is rendered as a plain table (per `dataviz`'s guidance that an accessible table view should exist alongside charts) rather than another chart, since it's 8 rows of two raw numbers — a table is the more direct read here.

- [ ] **Step 1: Write `CalibrationPanel.tsx`**

```tsx
// D:\line\webapp\src\components\CalibrationPanel.tsx
import { useState } from 'react';
import type { Command, Telemetry } from '../protocol';

interface CalibrationPanelProps {
  telemetry: Telemetry | null;
  onSendCommand: (command: Command) => void;
}

export function CalibrationPanel({ telemetry, onSendCommand }: CalibrationPanelProps) {
  const [running, setRunning] = useState(false);
  const calState = telemetry?.calState ?? 0;

  const start = () => {
    setRunning(true);
    onSendCommand({ cmd: 'calibrate', start: true });
  };
  const stop = () => {
    setRunning(false);
    onSendCommand({ cmd: 'calibrate', start: false });
  };

  return (
    <section className="panel">
      <h2>Calibration</h2>
      <p className="muted">Sweep the sensor array across the line surface by hand while calibrating.</p>
      <div className="pid-buttons">
        <button onClick={start} disabled={running}>Start Calibration</button>
        <button onClick={stop} disabled={!running}>Finish Calibration</button>
      </div>
      <p>State: {calState === 1 ? 'Calibrating...' : calState === 2 ? 'Done' : 'Idle'}</p>
      {calState === 1 && telemetry?.calMin && telemetry?.calMax && (
        <table className="cal-table">
          <thead>
            <tr>
              <th>Ch</th>
              <th>Min</th>
              <th>Max</th>
            </tr>
          </thead>
          <tbody>
            {telemetry.calMin.map((min, i) => (
              <tr key={i}>
                <td>{i}</td>
                <td>{min}</td>
                <td>{telemetry.calMax![i]}</td>
              </tr>
            ))}
          </tbody>
        </table>
      )}
    </section>
  );
}
```

- [ ] **Step 2: Wire into `App.tsx`**

```tsx
// D:\line\webapp\src\App.tsx
import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';
import { PidTuningPanel } from './components/PidTuningPanel';
import { PidChart } from './components/PidChart';
import { CalibrationPanel } from './components/CalibrationPanel';

export default function App() {
  const { status, statusMessage, telemetry, history, connect, disconnect, sendCommand } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
      {telemetry && (
        <>
          <SensorBarGraph sensors={telemetry.sensors} />
          <LinePositionIndicator linePosition={telemetry.linePosition} lineDetected={telemetry.lineDetected} />
        </>
      )}
      <PidTuningPanel
        onApply={sendCommand}
        onStart={() => sendCommand({ cmd: 'start' })}
        onStop={() => sendCommand({ cmd: 'stop' })}
        pidEnabled={telemetry?.pidEnabled ?? false}
      />
      <PidChart history={history} />
      <CalibrationPanel telemetry={telemetry} onSendCommand={sendCommand} />
    </div>
  );
}
```

- [ ] **Step 3: Manual verification**

Run: `cd D:\line\webapp && npm run dev`. Confirm the Calibration panel renders in the Idle state with both buttons in the correct enabled/disabled state. Full calibration-flow verification happens against real hardware in Task 10.

- [ ] **Step 4: Commit**

```bash
cd D:\line\webapp
git add src/components/CalibrationPanel.tsx src/App.tsx
git commit -m "feat(webapp): add calibration panel"
```

---

### Task 9: Recording/export (`recording.ts` + `RecordingPanel`)

**Files:**
- Create: `D:\line\webapp\src\recording.ts`
- Test: `D:\line\webapp\src\recording.test.ts`
- Create: `D:\line\webapp\src\components\RecordingPanel.tsx`
- Modify: `D:\line\webapp\src\App.tsx`

**Interfaces:**
- Consumes: `Telemetry` (Task 1)
- Produces: `buildCsv(rows: Telemetry[]): string`, `buildJson(rows: Telemetry[]): string`, `RecordingPanel({ telemetry })` — consumed by App only.

`buildCsv`/`buildJson` are pure functions and get real unit tests; the actual file-download mechanism (`Blob`/anchor-click) is a browser-only side effect verified manually in Task 10.

- [ ] **Step 1: Write the failing test**

```typescript
// D:\line\webapp\src\recording.test.ts
import { describe, expect, test } from 'vitest';
import { buildCsv, buildJson } from './recording';
import type { Telemetry } from './protocol';

function sampleTelemetry(overrides: Partial<Telemetry> = {}): Telemetry {
  return {
    timestampMs: 1000,
    sensors: [0, 1, 2, 3, 4, 5, 6, 7],
    linePosition: 3500,
    lineDetected: true,
    pidError: 0,
    pidOutput: 0,
    leftSpeed: 150,
    rightSpeed: 150,
    calState: 0,
    pidEnabled: true,
    ...overrides,
  };
}

describe('buildCsv', () => {
  test('includes a header row and one data row per telemetry sample', () => {
    const csv = buildCsv([sampleTelemetry()]);
    const lines = csv.split('\n');
    expect(lines).toHaveLength(2);
    expect(lines[0]).toContain('timestampMs');
    expect(lines[1]).toBe('1000,0,1,2,3,4,5,6,7,3500,true,0,0,150,150,true');
  });

  test('returns just the header for an empty list', () => {
    const csv = buildCsv([]);
    expect(csv.split('\n')).toHaveLength(1);
  });
});

describe('buildJson', () => {
  test('round-trips telemetry data', () => {
    const rows = [sampleTelemetry({ timestampMs: 2000 })];
    const json = buildJson(rows);
    expect(JSON.parse(json)).toEqual(rows);
  });
});
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `cd D:\line\webapp && npm test`
Expected: FAILS — `./recording` doesn't exist yet.

- [ ] **Step 3: Write `recording.ts`**

```typescript
// D:\line\webapp\src\recording.ts
import type { Telemetry } from './protocol';

const CSV_HEADER =
  'timestampMs,sensor0,sensor1,sensor2,sensor3,sensor4,sensor5,sensor6,sensor7,linePosition,lineDetected,pidError,pidOutput,leftSpeed,rightSpeed,pidEnabled';

export function buildCsv(rows: Telemetry[]): string {
  const lines = rows.map((t) =>
    [
      t.timestampMs,
      ...t.sensors,
      t.linePosition,
      t.lineDetected,
      t.pidError,
      t.pidOutput,
      t.leftSpeed,
      t.rightSpeed,
      t.pidEnabled,
    ].join(',')
  );
  return [CSV_HEADER, ...lines].join('\n');
}

export function buildJson(rows: Telemetry[]): string {
  return JSON.stringify(rows, null, 2);
}
```

- [ ] **Step 4: Run the test to verify it passes**

Run: `cd D:\line\webapp && npm test`
Expected: all tests pass (10 total across `protocol.test.ts` and `recording.test.ts`).

- [ ] **Step 5: Write `RecordingPanel.tsx`**

```tsx
// D:\line\webapp\src\components\RecordingPanel.tsx
import { useEffect, useState } from 'react';
import type { Telemetry } from '../protocol';
import { buildCsv, buildJson } from '../recording';

interface RecordingPanelProps {
  telemetry: Telemetry | null;
}

function download(filename: string, contents: string, mimeType: string): void {
  const blob = new Blob([contents], { type: mimeType });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = filename;
  a.click();
  URL.revokeObjectURL(url);
}

export function RecordingPanel({ telemetry }: RecordingPanelProps) {
  const [recording, setRecording] = useState(false);
  const [rows, setRows] = useState<Telemetry[]>([]);

  useEffect(() => {
    if (recording && telemetry) {
      setRows((prev) => [...prev, telemetry]);
    }
  }, [telemetry, recording]);

  const start = () => {
    setRows([]);
    setRecording(true);
  };
  const stop = () => setRecording(false);

  return (
    <section className="panel">
      <h2>Recording</h2>
      <div className="pid-buttons">
        <button onClick={start} disabled={recording}>Start Recording</button>
        <button onClick={stop} disabled={!recording}>Stop Recording</button>
      </div>
      <p className="muted">{rows.length} samples recorded</p>
      <div className="pid-buttons">
        <button disabled={rows.length === 0} onClick={() => download('line-follower-run.csv', buildCsv(rows), 'text/csv')}>
          Export CSV
        </button>
        <button
          disabled={rows.length === 0}
          onClick={() => download('line-follower-run.json', buildJson(rows), 'application/json')}
        >
          Export JSON
        </button>
      </div>
    </section>
  );
}
```

- [ ] **Step 6: Wire into `App.tsx`**

```tsx
// D:\line\webapp\src\App.tsx
import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';
import { PidTuningPanel } from './components/PidTuningPanel';
import { PidChart } from './components/PidChart';
import { CalibrationPanel } from './components/CalibrationPanel';
import { RecordingPanel } from './components/RecordingPanel';

export default function App() {
  const { status, statusMessage, telemetry, history, connect, disconnect, sendCommand } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
      {telemetry && (
        <>
          <SensorBarGraph sensors={telemetry.sensors} />
          <LinePositionIndicator linePosition={telemetry.linePosition} lineDetected={telemetry.lineDetected} />
        </>
      )}
      <PidTuningPanel
        onApply={sendCommand}
        onStart={() => sendCommand({ cmd: 'start' })}
        onStop={() => sendCommand({ cmd: 'stop' })}
        pidEnabled={telemetry?.pidEnabled ?? false}
      />
      <PidChart history={history} />
      <CalibrationPanel telemetry={telemetry} onSendCommand={sendCommand} />
      <RecordingPanel telemetry={telemetry} />
    </div>
  );
}
```

- [ ] **Step 7: Commit**

```bash
cd D:\line\webapp
git add src/recording.ts src/recording.test.ts src/components/RecordingPanel.tsx src/App.tsx
git commit -m "feat(webapp): add run recording and CSV/JSON export"
```

---

### Task 10: README + end-to-end hardware verification

**Files:**
- Create: `D:\line\webapp\README.md`

**Interfaces:**
- Consumes: the fully wired `App.tsx` from Task 9, and the flashed firmware from the firmware plan's Task 9.

- [ ] **Step 1: Write `README.md`**

```markdown
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
```

- [ ] **Step 2: Run the full test suite one more time**

Run: `cd D:\line\webapp && npm test`
Expected: all tests still pass.

- [ ] **Step 3: Perform the manual end-to-end checklist above against real hardware**

This requires the ESP32 firmware flashed per the firmware plan's Task 9. Work through all 6 checklist items and note any deviations directly in this README's checklist (e.g. strike through and annotate any step that needed a fix) before considering the dashboard done.

- [ ] **Step 4: Commit**

```bash
cd D:\line\webapp
git add README.md
git commit -m "docs(webapp): add README and end-to-end hardware verification checklist"
```
