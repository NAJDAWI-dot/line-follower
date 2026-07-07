import type { ConnectionStatus, TransportKind } from '../state/useConnection';

interface ConnectionPanelProps {
  status: ConnectionStatus;
  statusMessage?: string;
  onConnect: (kind: TransportKind) => void;
  onDisconnect: () => void;
}

export function ConnectionPanel({ status, statusMessage, onConnect, onDisconnect }: ConnectionPanelProps) {
  const busy = status === 'connected' || status === 'connecting';
  return (
    <section className="panel">
      <h2>Connection</h2>
      <div className="connection-buttons">
        <button disabled={busy} onClick={() => onConnect('serial')}>Connect via USB</button>
        <button disabled={busy} onClick={() => onConnect('wifi')}>Connect via WiFi</button>
        <button disabled={busy} onClick={() => onConnect('ble')}>Connect via Bluetooth</button>
        <button disabled={!busy} onClick={onDisconnect}>Disconnect</button>
      </div>
      <p className={`status-${status}`}>
        Status: {status}
        {statusMessage ? ` — ${statusMessage}` : ''}
      </p>
    </section>
  );
}
