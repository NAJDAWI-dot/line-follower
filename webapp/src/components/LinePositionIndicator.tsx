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
