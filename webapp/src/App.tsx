import { useConnection } from './state/useConnection';
import { ConnectionPanel } from './components/ConnectionPanel';

export default function App() {
  const { status, statusMessage, connect, disconnect } = useConnection();

  return (
    <div className="app">
      <h1>Line Follower Dashboard</h1>
      <ConnectionPanel
        status={status}
        statusMessage={statusMessage}
        onConnect={connect}
        onDisconnect={disconnect}
      />
    </div>
  );
}
