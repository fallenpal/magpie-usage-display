#include "ble_bridge.h"

#include <ArduinoJson.h>

#include "status_parser.h"
#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLESecurity.h>

#if defined(CONFIG_NIMBLE_ENABLED)
#include <host/ble_store.h>
#endif

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace {

constexpr char kDeviceName[] = "Magpie Display";
constexpr char kServiceUuid[] = "7d8b6c20-8f6d-4b44-a0f8-1b6570c0de01";
constexpr char kStatusUuid[] = "7d8b6c20-8f6d-4b44-a0f8-1b6570c0de02";
constexpr char kCommandUuid[] = "7d8b6c20-8f6d-4b44-a0f8-1b6570c0de03";
constexpr char kResultUuid[] = "7d8b6c20-8f6d-4b44-a0f8-1b6570c0de04";
constexpr char kDeviceInfoUuid[] = "7d8b6c20-8f6d-4b44-a0f8-1b6570c0de05";
constexpr uint8_t kProtocolVersion = 5;
constexpr uint32_t kStatusWatchdogMs = 75 * 1000;
constexpr size_t kMaxStatusMessageBytes = 250;

BLEServer* server = nullptr;
BLECharacteristic* command_characteristic = nullptr;
QueueHandle_t state_queue = nullptr;
QueueHandle_t result_queue = nullptr;
QueueHandle_t pairing_code_queue = nullptr;
QueueHandle_t pairing_finished_queue = nullptr;
volatile bool connected = false;
volatile bool status_received_on_connection = false;
volatile uint32_t last_status_at_ms = 0;
uint32_t next_request_id = 1;
uint32_t command_session_id = 0;
uint32_t last_sequence = 0;
StatusParser status_parser;
bool has_bonded_device = false;
bool pairing_code_announced = false;

class ServerCallbacks final : public BLEServerCallbacks {
 public:
  void onConnect(BLEServer*) override {
    connected = true;
    status_received_on_connection = false;
    last_sequence = 0;
    status_parser.resetConnection();
    Serial.println("[ble] connected");
  }

  void onDisconnect(BLEServer* current_server) override {
    connected = false;
    status_received_on_connection = false;
    Serial.println("[ble] disconnected");
    current_server->startAdvertising();
  }
};

class StatusCallbacks final : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic* characteristic) override {
    const String value = characteristic->getValue();
    if (value.isEmpty() || value.length() > kMaxStatusMessageBytes) {
      Serial.println("[ble] rejected invalid status length");
      return;
    }

    AppState state;
    uint32_t sequence = 0;
    const StatusParseResult result = status_parser.ingest(
        reinterpret_cast<const uint8_t*>(value.c_str()), value.length(),
        millis(), state, sequence);

    if (result == StatusParseResult::kCommitted) {
      last_sequence = sequence;
      last_status_at_ms = millis();
      status_received_on_connection = true;
      xQueueOverwrite(state_queue, &state);
      unsigned int total_windows = 0;
      for (uint8_t index = 0; index < state.provider_count; ++index) {
        total_windows += state.providers[index].window_count;
      }
      Serial.printf("[ble] committed seq=%lu providers=%u windows=%u\n",
                    static_cast<unsigned long>(sequence),
                    static_cast<unsigned int>(state.provider_count),
                    total_windows);
    } else if (result == StatusParseResult::kIgnored &&
               status_parser.lastError()[0] != '\0') {
      Serial.printf("[ble] ignored status: %s\n",
                    status_parser.lastError());
    }
  }
};

class ResultCallbacks final : public BLECharacteristicCallbacks {
 public:
  void onWrite(BLECharacteristic* characteristic) override {
    const String value = characteristic->getValue();
    JsonDocument document;
    if (deserializeJson(document, value.c_str(), value.length()) ||
        document["v"].as<uint8_t>() != kProtocolVersion) {
      return;
    }

    BleActionResult result{};
    const bool ok = (document["ok"] | 0) == 1;
    const char* message = document["m"] | (ok ? "DONE" : "FAILED");
    strlcpy(result.message, message, sizeof(result.message));
    xQueueOverwrite(result_queue, &result);
  }
};

class SecurityCallbacks final : public BLESecurityCallbacks {
 public:
  void onPassKeyNotify(uint32_t passkey) override {
    if (has_bonded_device) {
      Serial.println("[ble] bonded reconnect; passkey UI suppressed");
      return;
    }
    pairing_code_announced = true;
    xQueueOverwrite(pairing_code_queue, &passkey);
    Serial.printf("[ble] pairing passkey %06lu\n",
                  static_cast<unsigned long>(passkey));
  }

  bool onSecurityRequest() override {
    return true;
  }

#if defined(CONFIG_BLUEDROID_ENABLED)
  void onAuthenticationComplete(esp_ble_auth_cmpl_t result) override {
    const bool success = result.success;
    if (success) {
      has_bonded_device = true;
    }
    if (pairing_code_announced) {
      xQueueOverwrite(pairing_finished_queue, &success);
      pairing_code_announced = false;
    }
    Serial.printf("[ble] pairing %s\n", success ? "complete" : "failed");
  }
#endif
};

}  // namespace

bool bleBegin() {
  state_queue = xQueueCreate(1, sizeof(AppState));
  result_queue = xQueueCreate(1, sizeof(BleActionResult));
  pairing_code_queue = xQueueCreate(1, sizeof(uint32_t));
  pairing_finished_queue = xQueueCreate(1, sizeof(bool));
  if (state_queue == nullptr || result_queue == nullptr ||
      pairing_code_queue == nullptr || pairing_finished_queue == nullptr) {
    Serial.println("[ble] failed to allocate queues");
    return false;
  }

  BLEDevice::init(kDeviceName);
#if defined(CONFIG_BLUEDROID_ENABLED)
  has_bonded_device = esp_ble_get_bond_device_num() > 0;
#endif
  command_session_id = esp_random();
  if (command_session_id == 0) {
    command_session_id = 1;
  }
  BLEDevice::setMTU(256);
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks());

  auto* security = new BLESecurity();
  security->setAuthenticationMode(true, true, true);
  security->setCapability(ESP_IO_CAP_OUT);
  security->setPassKey(false);
  security->regenPassKeyOnConnect(true);

  server = BLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());
  server->advertiseOnDisconnect(true);

  BLEService* service = server->createService(kServiceUuid);
  BLECharacteristic* status = service->createCharacteristic(
      kStatusUuid, BLECharacteristic::PROPERTY_WRITE);
  status->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  status->setCallbacks(new StatusCallbacks());

  command_characteristic = service->createCharacteristic(
      kCommandUuid,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  command_characteristic->setAccessPermissions(ESP_GATT_PERM_READ_ENCRYPTED);
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
  auto* command_cccd = new BLE2902();
#pragma GCC diagnostic pop
  command_cccd->setAccessPermissions(ESP_GATT_PERM_READ |
                                     ESP_GATT_PERM_WRITE_ENCRYPTED);
  command_characteristic->addDescriptor(command_cccd);

  BLECharacteristic* result = service->createCharacteristic(
      kResultUuid, BLECharacteristic::PROPERTY_WRITE);
  result->setAccessPermissions(ESP_GATT_PERM_WRITE_ENCRYPTED);
  result->setCallbacks(new ResultCallbacks());

  BLECharacteristic* device_info = service->createCharacteristic(
      kDeviceInfoUuid, BLECharacteristic::PROPERTY_READ);
  device_info->setValue("{\"v\":5,\"fw\":\"0.5.0\",\"screen\":\"466x466\"}");

  service->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(kServiceUuid);
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  Serial.println("[ble] advertising as Magpie Display");
  return true;
}

bool bleConnected() {
  return connected;
}

void bleMaintainConnection() {
  status_parser.expire(millis());
  if (!connected || !status_received_on_connection || server == nullptr ||
      millis() - last_status_at_ms < kStatusWatchdogMs) {
    return;
  }

  status_received_on_connection = false;
  Serial.println("[ble] status watchdog expired; disconnecting stale client");
  server->disconnect(server->getConnId());
}

void bleStartAdvertising() {
  if (!connected && server != nullptr) {
    server->startAdvertising();
  }
}

bool bleTakeState(AppState& state) {
  return state_queue != nullptr &&
         xQueueReceive(state_queue, &state, 0) == pdTRUE;
}

bool bleTakeActionResult(BleActionResult& result) {
  return result_queue != nullptr &&
         xQueueReceive(result_queue, &result, 0) == pdTRUE;
}

bool bleTakePairingCode(uint32_t& passkey) {
  return pairing_code_queue != nullptr &&
         xQueueReceive(pairing_code_queue, &passkey, 0) == pdTRUE;
}

bool bleTakePairingFinished(bool& success) {
  return pairing_finished_queue != nullptr &&
         xQueueReceive(pairing_finished_queue, &success, 0) == pdTRUE;
}

bool bleSendAction(const char* action) {
  if (!connected || command_characteristic == nullptr) {
    return false;
  }

  char message[96];
  const uint32_t request_id = next_request_id++;
  snprintf(message, sizeof(message),
           "{\"v\":%u,\"sid\":%lu,\"id\":%lu,\"a\":\"%s\"}",
           static_cast<unsigned int>(kProtocolVersion),
           static_cast<unsigned long>(command_session_id),
           static_cast<unsigned long>(request_id), action);
  command_characteristic->setValue(message);
  command_characteristic->notify();
  return true;
}

bool bleClearBonds() {
#if defined(CONFIG_BLUEDROID_ENABLED)
  int count = esp_ble_get_bond_device_num();
  if (count == 0) {
    return true;
  }
  if (count < 0) {
    Serial.printf("[ble] failed to count bonds: %d\n", count);
    return false;
  }

  auto* devices = static_cast<esp_ble_bond_dev_t*>(
      malloc(sizeof(esp_ble_bond_dev_t) * count));
  if (devices == nullptr) {
    Serial.println("[ble] failed to allocate bond list");
    return false;
  }

  const esp_err_t list_result =
      esp_ble_get_bond_device_list(&count, devices);
  if (list_result != ESP_OK) {
    Serial.printf("[ble] failed to read bonds: %d\n", list_result);
    free(devices);
    return false;
  }

  bool cleared = true;
  for (int i = 0; i < count; ++i) {
    const esp_err_t remove_result =
        esp_ble_remove_bond_device(devices[i].bd_addr);
    if (remove_result != ESP_OK) {
      Serial.printf("[ble] failed to remove bond %d: %d\n", i,
                    remove_result);
      cleared = false;
    }
  }
  free(devices);
  return cleared;
#elif defined(CONFIG_NIMBLE_ENABLED)
  const int result = ble_store_clear();
  if (result != 0) {
    Serial.printf("[ble] failed to clear NimBLE bonds: %d\n", result);
    return false;
  }
  return true;
#else
  Serial.println("[ble] no supported BLE host stack");
  return false;
#endif
}
