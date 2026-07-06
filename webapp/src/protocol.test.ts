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
    // Payload with t and s but missing other required fields (pos, det, err, out, ls, rs, cal, pid)
    expect(decodeTelemetryJson('{"t":1,"s":[1,2,3,4,5,6,7,8]}')).toBeNull();
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

  test('serializes a start command', () => {
    const json = encodeCommand({ cmd: 'start' });
    expect(JSON.parse(json)).toEqual({ cmd: 'start' });
  });

  test('serializes a stop command', () => {
    const json = encodeCommand({ cmd: 'stop' });
    expect(JSON.parse(json)).toEqual({ cmd: 'stop' });
  });
});
