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
