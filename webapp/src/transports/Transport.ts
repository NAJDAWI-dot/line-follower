import type { Command, Telemetry } from '../protocol';

export interface TransportEvents {
  onTelemetry: (telemetry: Telemetry) => void;
  onStatusChange: (status: 'connected' | 'disconnected' | 'error', message?: string) => void;
}

export interface Transport {
  connect(events: TransportEvents): Promise<void>;
  disconnect(): Promise<void>;
  send(command: Command): void;
}
