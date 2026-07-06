import { decodeTelemetryJson, encodeCommand, type Command } from '../protocol';
import type { Transport, TransportEvents } from './Transport';

export class SerialTransport implements Transport {
  private port: SerialPort | null = null;
  private reader: ReadableStreamDefaultReader<string> | null = null;
  private writer: WritableStreamDefaultWriter<string> | null = null;
  private keepReading = false;
  private events: TransportEvents | null = null;
  private readablePipePromise: Promise<void> | null = null;
  private writablePipePromise: Promise<void> | null = null;

  async connect(events: TransportEvents): Promise<void> {
    this.events = events;
    if (!('serial' in navigator)) {
      events.onStatusChange('error', 'Web Serial API not supported in this browser');
      throw new Error('Web Serial not supported');
    }

    this.port = await navigator.serial.requestPort();
    await this.port.open({ baudRate: 115200 });

    const textEncoder = new TextEncoderStream();
    this.writablePipePromise = textEncoder.readable.pipeTo(this.port.writable!).catch((err) => {
      this.events?.onStatusChange('error', err instanceof Error ? err.message : String(err));
    });
    this.writer = textEncoder.writable.getWriter();

    this.keepReading = true;
    events.onStatusChange('connected');
    this.readLoop();
  }

  private async readLoop(): Promise<void> {
    if (!this.port?.readable) return;
    const textDecoder = new TextDecoderStream();
    const readableClosed = (this.port.readable as ReadableStream<BufferSource>).pipeTo(textDecoder.writable);
    this.readablePipePromise = readableClosed;
    const reader = textDecoder.readable.getReader();
    this.reader = reader;

    let buffer = '';
    try {
      while (this.keepReading) {
        const { value, done } = await reader.read();
        if (done) break;
        buffer += value;
        let newlineIndex: number;
        while ((newlineIndex = buffer.indexOf('\n')) >= 0) {
          const line = buffer.slice(0, newlineIndex).trim();
          buffer = buffer.slice(newlineIndex + 1);
          if (line.length > 0) {
            const telemetry = decodeTelemetryJson(line);
            if (telemetry) this.events?.onTelemetry(telemetry);
          }
        }
      }
    } catch (err) {
      this.events?.onStatusChange('error', err instanceof Error ? err.message : String(err));
    } finally {
      reader.releaseLock();
      await readableClosed.catch(() => {});
    }
  }

  send(command: Command): void {
    this.writer?.write(encodeCommand(command) + '\n').catch((err) => {
      this.events?.onStatusChange('error', err instanceof Error ? err.message : String(err));
    });
  }

  async disconnect(): Promise<void> {
    this.keepReading = false;
    await this.reader?.cancel().catch(() => {});
    await this.writer?.close().catch(() => {});
    await Promise.all([
      this.readablePipePromise?.catch(() => {}),
      this.writablePipePromise?.catch(() => {}),
    ]);
    await this.port?.close().catch(() => {});
    this.port = null;
    this.reader = null;
    this.writer = null;
    this.readablePipePromise = null;
    this.writablePipePromise = null;
    this.events?.onStatusChange('disconnected');
  }
}
