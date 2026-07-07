import { useCallback, useEffect, useRef, useState } from 'react';
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
  const telemetryListenersRef = useRef<Set<(t: Telemetry) => void>>(new Set());

  const subscribeTelemetry = useCallback((listener: (t: Telemetry) => void) => {
    telemetryListenersRef.current.add(listener);
    return () => {
      telemetryListenersRef.current.delete(listener);
    };
  }, []);

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
          telemetryListenersRef.current.forEach((listener) => listener(t));
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
    try {
      await transportRef.current?.disconnect();
    } finally {
      transportRef.current = null;
      setStatus('disconnected');
    }
  }, []);

  const sendCommand = useCallback((command: Command) => {
    transportRef.current?.send(command);
  }, []);

  useEffect(() => {
    return () => {
      transportRef.current?.disconnect().catch(() => {});
      transportRef.current = null;
    };
  }, []);

  return { status, statusMessage, telemetry, history, connect, disconnect, sendCommand, subscribeTelemetry };
}
