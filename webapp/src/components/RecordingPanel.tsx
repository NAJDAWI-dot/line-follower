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
