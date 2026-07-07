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
