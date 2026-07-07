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
