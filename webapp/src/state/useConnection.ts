import { useCallback, useRef, useState } from 'react';
import type { Command, Telemetry } from '../protocol';
import type { Transport } from '../transports/Transport';
import { SerialTransport } from '../transports/SerialTransport';
import { WebSocketTransport } from '../transports/WebSocketTransport';
import { BleTransport } from '../transports/BleTransport';

export type TransportKind = 'serial' | 'wifi' | 'ble';
export type ConnectionStatus = 'disconnected' | 'connecting' | 'connected' | 'error';

const HISTORY_LIMIT = 500;

export function useConnection() {
  const [status, setStatus] = useState<ConnectionStatus>('disconnected');
  const [statusMessage, setStatusMessage] = useState<string | undefined>(undefined);
  const [telemetry, setTelemetry] = useState<Telemetry | null>(null);
  const [history, setHistory] = useState<Telemetry[]>([]);
  const transportRef = useRef<Transport | null>(null);

  const connect = useCallback(async (kind: TransportKind) => {
    setStatus('connecting');
    setStatusMessage(undefined);

    const transport: Transport =
      kind === 'serial'
        ? new SerialTransport()
        : kind === 'wifi'
          ? new WebSocketTransport()
          : new BleTransport();

    transportRef.current = transport;
    try {
      await transport.connect({
        onTelemetry: (t) => {
          setTelemetry(t);
          setHistory((prev) => {
            const next = [...prev, t];
            return next.length > HISTORY_LIMIT ? next.slice(next.length - HISTORY_LIMIT) : next;
          });
        },
        onStatusChange: (s, message) => {
          setStatus(s);
          setStatusMessage(message);
        },
      });
    } catch (err) {
      setStatus('error');
      setStatusMessage(err instanceof Error ? err.message : String(err));
      transportRef.current = null;
    }
  }, []);

  const disconnect = useCallback(async () => {
    await transportRef.current?.disconnect();
    transportRef.current = null;
    setStatus('disconnected');
  }, []);

  const sendCommand = useCallback((command: Command) => {
    transportRef.current?.send(command);
  }, []);

  return { status, statusMessage, telemetry, history, connect, disconnect, sendCommand };
}
