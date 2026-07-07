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
