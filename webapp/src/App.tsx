import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';
import { SensorBarGraph } from './components/SensorBarGraph';
import { LinePositionIndicator } from './components/LinePositionIndicator';
import { PidTuningPanel } from './components/PidTuningPanel';
import { PidChart } from './components/PidChart';
import { CalibrationPanel } from './components/CalibrationPanel';

export default function App() {
  const { status, statusMessage, telemetry, history, connect, disconnect, sendCommand } = useConnection();

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
      <PidTuningPanel
        onApply={sendCommand}
        onStart={() => sendCommand({ cmd: 'start' })}
        onStop={() => sendCommand({ cmd: 'stop' })}
        pidEnabled={telemetry?.pidEnabled ?? false}
      />
      <PidChart history={history} />
      <CalibrationPanel telemetry={telemetry} onSendCommand={sendCommand} />
    </div>
  );
}
