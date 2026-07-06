#include "transport_ble.h"
#include <NimBLEDevice.h>

namespace {
constexpr const char* SERVICE_UUID = "6e400001-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* TELEMETRY_CHAR_UUID = "6e400002-b5a3-f393-e0a9-e50e24dcca9e";
constexpr const char* COMMAND_CHAR_UUID = "6e400003-b5a3-f393-e0a9-e50e24dcca9e";

NimBLECharacteristic* telemetryChar = nullptr;
std::function<void(const protocol::Command&)> commandCallback;
char cmdLineBuf[256];
size_t cmdLineLen = 0;

void handleCommandData(const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        char c = static_cast<char>(data[i]);
        if (c == '\n') {
            protocol::Command cmd;
            if (protocol::parseCommand(cmdLineBuf, cmdLineLen, &cmd) && commandCallback) {
                commandCallback(cmd);
            }
            cmdLineLen = 0;
            continue;
        }
        if (cmdLineLen < sizeof(cmdLineBuf) - 1) {
            cmdLineBuf[cmdLineLen++] = c;
        } else {
            cmdLineLen = 0;
        }
    }
}

// NOTE: NimBLE-Arduino's callback signature has changed across major
// versions (1.x passes only the characteristic; 2.x also passes a
// NimBLEConnInfo&). If this fails to compile against the resolved library
// version, check the installed NimBLE-Arduino examples for the current
// NimBLECharacteristicCallbacks::onWrite signature and adjust here.
class CommandCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* c) override {
        const std::string& v = c->getValue();
        handleCommandData(reinterpret_cast<const uint8_t*>(v.data()), v.size());
    }
};

CommandCallbacks commandCallbacks;
} // namespace

void TransportBle::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEServer* bleServer = NimBLEDevice::createServer();
    NimBLEService* service = bleServer->createService(SERVICE_UUID);

    telemetryChar = service->createCharacteristic(
        TELEMETRY_CHAR_UUID, NIMBLE_PROPERTY::NOTIFY);

    NimBLECharacteristic* commandChar = service->createCharacteristic(
        COMMAND_CHAR_UUID, NIMBLE_PROPERTY::WRITE);
    commandChar->setCallbacks(&commandCallbacks);

    service->start();
    NimBLEAdvertising* advertising = NimBLEDevice::getAdvertising();
    advertising->addServiceUUID(SERVICE_UUID);
    advertising->start();
}

void TransportBle::sendTelemetry(const protocol::Telemetry& t, uint16_t timestampDeltaMs) {
    if (!telemetryChar) return;
    protocol::TelemetryBinary bin;
    protocol::encodeTelemetryBinary(t, timestampDeltaMs, &bin);
    telemetryChar->setValue(reinterpret_cast<uint8_t*>(&bin), sizeof(bin));
    telemetryChar->notify();
}

void TransportBle::onCommand(std::function<void(const protocol::Command&)> cb) {
    commandCallback = cb;
}
