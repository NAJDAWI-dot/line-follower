import { decodeTelemetryBinary, decodeTelemetryDeltaMs, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

const SERVICE_UUID = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const TELEMETRY_CHAR_UUID = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';
const COMMAND_CHAR_UUID = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';

export class BleTransport implements Transport {
  private device: BluetoothDevice | null = null;
  private commandChar: BluetoothRemoteGATTCharacteristic | null = null;
  private events: TransportEvents | null = null;
  private accumulatedTimestampMs = 0;
  private disconnectListener: (() => void) | null = null;

  async connect(events: TransportEvents): Promise<void> {
    this.events = events;
    if (!('bluetooth' in navigator)) {
      events.onStatusChange('error', 'Web Bluetooth API not supported in this browser');
      throw new Error('Web Bluetooth not supported');
    }

    this.device = await navigator.bluetooth.requestDevice({
      filters: [{ services: [SERVICE_UUID] }],
    });

    this.disconnectListener = () => {
      this.events?.onStatusChange('disconnected');
    };
    this.device.addEventListener('gattserverdisconnected', this.disconnectListener);

    const server = await this.device.gatt!.connect();
    const service = await server.getPrimaryService(SERVICE_UUID);
    const telemetryChar = await service.getCharacteristic(TELEMETRY_CHAR_UUID);
    this.commandChar = await service.getCharacteristic(COMMAND_CHAR_UUID);

    await telemetryChar.startNotifications();
    telemetryChar.addEventListener('characteristicvaluechanged', (ev) => {
      const target = ev.target as BluetoothRemoteGATTCharacteristic;
      const view = target.value;
      if (!view) return;
      this.accumulatedTimestampMs += decodeTelemetryDeltaMs(view);
      const telemetry = decodeTelemetryBinary(view, this.accumulatedTimestampMs);
      this.events?.onTelemetry(telemetry);
    });

    events.onStatusChange('connected');
  }

  send(command: Command): void {
    if (!this.commandChar) return;
    const bytes = new TextEncoder().encode(encodeCommand(command) + '\n');
    this.commandChar.writeValueWithoutResponse(bytes).catch(() => {});
  }

  async disconnect(): Promise<void> {
    if (this.device && this.disconnectListener) {
      this.device.removeEventListener('gattserverdisconnected', this.disconnectListener);
    }
    this.device?.gatt?.disconnect();
    this.device = null;
    this.commandChar = null;
    this.disconnectListener = null;
    this.events?.onStatusChange('disconnected');
  }
}
