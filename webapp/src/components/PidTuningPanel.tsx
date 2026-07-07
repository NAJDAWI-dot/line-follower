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
