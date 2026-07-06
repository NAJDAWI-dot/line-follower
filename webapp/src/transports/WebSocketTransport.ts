import { decodeTelemetryJson, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

export class WebSocketTransport implements Transport {
  private ws: WebSocket | null = null;

  constructor(private readonly url: string = 'ws://192.168.4.1/ws') {}

  connect(events: TransportEvents): Promise<void> {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(this.url);
      this.ws = ws;

      ws.onopen = () => {
        events.onStatusChange('connected');
        resolve();
      };
      ws.onmessage = (ev) => {
        if (typeof ev.data === 'string') {
          const telemetry = decodeTelemetryJson(ev.data);
          if (telemetry) events.onTelemetry(telemetry);
        }
      };
      ws.onerror = () => {
        events.onStatusChange('error', 'WebSocket connection error');
        reject(new Error('WebSocket connection failed'));
      };
      ws.onclose = () => {
        events.onStatusChange('disconnected');
      };
    });
  }

  send(command: Command): void {
    if (this.ws?.readyState === WebSocket.OPEN) {
      this.ws.send(encodeCommand(command));
    }
  }

  async disconnect(): Promise<void> {
    this.ws?.close();
    this.ws = null;
  }
}
