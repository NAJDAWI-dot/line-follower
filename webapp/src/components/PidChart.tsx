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
