import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';

export default function App() {
  const { status, statusMessage, telemetry, connect, disconnect } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
      {telemetry && (
        <>
          <SensorBarGraph sensors={telemetry.sensors} />
          <LinePositionIndicator linePosition={telemetry.linePosition} lineDetected={telemetry.lineDetected} />
        </>
      )}
    </div>
  );
}
