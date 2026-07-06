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

export function decodeTelemetryJson(line: string): Telemetry | null {
  let obj: any;
  try {
    obj = JSON.parse(line);
  } catch {
    return null;
  }
  if (
    typeof obj.t !== 'number' ||
    !Array.isArray(obj.s) ||
    typeof obj.pos !== 'number' ||
    typeof obj.det !== 'boolean' ||
    typeof obj.err !== 'number' ||
    typeof obj.out !== 'number' ||
    typeof obj.ls !== 'number' ||
    typeof obj.rs !== 'number' ||
    typeof obj.cal !== 'number' ||
    typeof obj.pid !== 'boolean'
  ) {
    return null;
  }
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
