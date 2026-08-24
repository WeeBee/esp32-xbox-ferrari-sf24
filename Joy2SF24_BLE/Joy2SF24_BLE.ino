#include <Arduino.h>
#include <NimBLEDevice.h>

// Bburago SL-SF-24
static NimBLEUUID carServiceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
static NimBLEUUID carCharacteristicUUID("0000fff1-0000-1000-8000-00805f9b34fb");
// Standard Bluetooth HID service used by Xbox One S / Series controllers.
static NimBLEUUID hidServiceUUID("1812");
static NimBLEUUID hidReportUUID("2a4d");
static NimBLEUUID hidProtocolModeUUID("2a4e");
static NimBLEUUID hidReportMapUUID("2a4b");
static NimBLEUUID hidControlPointUUID("2a4c");
static NimBLEUUID clientConfigUUID("2902");

static NimBLEClient* carClient = nullptr;
static NimBLEClient* controllerClient = nullptr;
static NimBLERemoteCharacteristic* carCharacteristic = nullptr;
static NimBLERemoteCharacteristic* controllerReport = nullptr;
static NimBLEAddress carAddress;
static NimBLEAddress controllerAddress;

static bool carConnected = false;
static bool controllerConnected = false;
static bool carFound = false;
static bool controllerFound = false;
static bool scanRequested = true;
static uint8_t lightsOn = 0;
static bool previousX = false;
static bool printedFirstInput = false;
static uint32_t inputReportCount = 0;
static uint32_t lastCommandAt = 0;

struct ControllerState {
  uint8_t drive = 0;
  uint8_t steer = 0;
  uint8_t turbo = 0;
};

static ControllerState controllerState;

void sendCarCommand(uint8_t drive, uint8_t steer, uint8_t turbo, uint8_t lights) {
  if (!carConnected || carCharacteristic == nullptr) return;

  uint8_t packet[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  if (drive == 1) packet[1] = 0x01;
  if (drive == 2) packet[2] = 0x01;
  if (steer == 1) packet[3] = 0x01;
  if (steer == 2) packet[4] = 0x01;
  packet[5] = lights;
  packet[6] = turbo;
  carCharacteristic->writeValue(packet, sizeof(packet), false);
}

void controllerInputCallback(NimBLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  inputReportCount++;
  // Xbox Series BLE reports may be 17 bytes with report ID 1 or 16 bytes without it, depending on the controller firmware and HOGP configuration.
  if (!printedFirstInput) {
    Serial.print("Xbox input report (");
    Serial.print(length);
    Serial.print(" bytes):");
    for (size_t index = 0; index < length; ++index) {
      Serial.printf(" %02X", data[index]);
    }
    Serial.println();
    printedFirstInput = true;
  }
  if (length < 16) return;

  const size_t offset = (length >= 17 && data[0] == 0x01) ? 1 : 0;
  if (length < offset + 16) return;

  const int32_t leftX = static_cast<int32_t>(data[offset] | (data[offset + 1] << 8)) - 32768;
  const uint16_t leftTrigger = data[offset + 8] | (data[offset + 9] << 8);
  const uint16_t rightTrigger = data[offset + 10] | (data[offset + 11] << 8);
  const uint16_t buttons = data[offset + 13] | (data[offset + 14] << 8);

  // Xbox BLE trigger fields are 10-bit values, while stick axes are unsigned 16-bit values centered at 0x8000
  controllerState.drive = rightTrigger > 32 ? 1 : (leftTrigger > 32 ? 2 : 0);
  controllerState.steer = leftX < -10000 ? 1 : (leftX > 10000 ? 2 : 0);
  controllerState.turbo = (buttons & 0x0001) != 0 ? 1 : 0;  // A

  const bool xPressed = (buttons & 0x0008) != 0;  // X
  if (xPressed && !previousX) lightsOn = lightsOn == 0 ? 1 : 0;
  previousX = xPressed;
}

bool connectCar() {
  if (carClient == nullptr) carClient = NimBLEDevice::createClient();
  Serial.println("Connecting to car...");
  if (!carClient->connect(carAddress)) {
    Serial.println("Car connection failed");
    return false;
  }

  NimBLERemoteService* service = carClient->getService(carServiceUUID);
  if (service == nullptr) {
    Serial.println("Car service FFF0 not found");
    carClient->disconnect();
    return false;
  }
  carCharacteristic = service->getCharacteristic(carCharacteristicUUID);
  carConnected = carCharacteristic != nullptr;
  if (!carConnected) {
    Serial.println("Car characteristic FFF1 not found");
    carClient->disconnect();
  }
  if (carConnected) Serial.println("Car connected");
  return carConnected;
}

bool connectController() {
  if (controllerClient == nullptr) controllerClient = NimBLEDevice::createClient();
  Serial.println("Connecting to Xbox controller...");
  if (!controllerClient->connect(controllerAddress)) {
    Serial.println("Xbox controller connection failed");
    return false;
  }

  Serial.println("Securing Xbox controller link...");
  if (!controllerClient->secureConnection()) {
    Serial.println("Xbox controller security failed");
    controllerClient->disconnect();
    return false;
  }
  Serial.println(controllerClient->getConnInfo().isEncrypted() ? "Xbox link encrypted" : "Xbox link not encrypted");

  NimBLERemoteService* service = controllerClient->getService(hidServiceUUID);
  if (service == nullptr) {
    Serial.println("Xbox HID service not found");
    controllerClient->disconnect();
    return false;
  }

  NimBLERemoteCharacteristic* protocolMode = service->getCharacteristic(hidProtocolModeUUID);
  if (protocolMode != nullptr && (protocolMode->canWrite() || protocolMode->canWriteNoResponse())) {
    const uint8_t reportProtocol = 0x01;
    if (!protocolMode->writeValue(&reportProtocol, sizeof(reportProtocol), protocolMode->canWrite())) {
      Serial.println("Xbox report protocol negotiation failed");
      controllerClient->disconnect();
      return false;
    }
    Serial.println("Xbox report protocol selected");
  } else {
    Serial.println("Xbox protocol mode characteristic not found");
  }

  NimBLERemoteCharacteristic* reportMap = service->getCharacteristic(hidReportMapUUID);
  if (reportMap != nullptr && reportMap->canRead()) {
    NimBLEAttValue descriptor = reportMap->readValue();
    Serial.print("Xbox HID report map: ");
    Serial.print(descriptor.size());
    Serial.println(" bytes");
  }

  NimBLERemoteCharacteristic* controlPoint = service->getCharacteristic(hidControlPointUUID);
  if (controlPoint != nullptr && (controlPoint->canWrite() || controlPoint->canWriteNoResponse())) {
    const uint8_t exitSuspend = 0x00;
    controlPoint->writeValue(&exitSuspend, sizeof(exitSuspend), controlPoint->canWrite());
  }

  controllerReport = nullptr;
  const auto& characteristics = service->getCharacteristics(true);
  for (NimBLERemoteCharacteristic* characteristic : characteristics) {
    if (characteristic->getUUID() == hidReportUUID) {
      Serial.print("Xbox HID report: ");
      Serial.print(characteristic->canNotify() ? "notify" : "no notify");
      Serial.print(", ");
      Serial.println(characteristic->canRead() ? "read" : "no read");
      if (characteristic->canNotify()) {
        if (characteristic->getDescriptor(clientConfigUUID) == nullptr) {
          Serial.println("Xbox HID report has no notification descriptor");
          continue;
        }
        const bool subscribed = characteristic->subscribe(true, controllerInputCallback, true);
        Serial.println(subscribed ? "Xbox HID notification enabled" : "Xbox HID notification failed");
        if (controllerReport == nullptr && subscribed) controllerReport = characteristic;
      }
    }
  }
  if (controllerReport == nullptr) {
    Serial.println("Xbox HID input report not found");
    controllerClient->disconnect();
    return false;
  }
  controllerConnected = true;
  Serial.println("Xbox controller connected");
  return true;
}

void scanForDevices() {
  NimBLEScan* scan = NimBLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(100);
  scan->setWindow(80);
  Serial.println("Scanning...");
  NimBLEScanResults results = scan->getResults(5000, false);

  for (int index = 0; index < results.getCount(); ++index) {
    const NimBLEAdvertisedDevice* device = results.getDevice(index);
    const String name = String(device->getName().c_str());
    Serial.print("BLE device: ");
    Serial.print(name.length() ? name : "(no name)");
    Serial.print(" ");
    Serial.println(device->getAddress().toString().c_str());

    const bool looksLikeCar = device->isAdvertisingService(carServiceUUID) ||
                              name.indexOf("SL-SF-24") >= 0;
    if (!carFound && looksLikeCar) {
      carAddress = device->getAddress();
      carFound = true;
      Serial.println("Found SL-SF-24 car");
    }
    const bool looksLikeController = device->isAdvertisingService(hidServiceUUID);
    if (!controllerFound && looksLikeController) {
      controllerAddress = device->getAddress();
      controllerFound = true;
      Serial.println("Found Xbox HID controller");
    }
  }
  scan->clearResults();
}

void setup() {
  Serial.begin(115200);
  NimBLEDevice::init("F124 BLE bridge");
  NimBLEDevice::setSecurityAuth(false, false, true);
  NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
  NimBLEDevice::setPower(9);
  Serial.println("Turn on the car and Xbox controller");
}

void loop() {
  if (scanRequested || !carFound || !controllerFound) {
    scanRequested = false;
    scanForDevices();
  }

  if (!carConnected && carFound) carConnected = connectCar();
  if (!controllerConnected && controllerFound) controllerConnected = connectController();

  if (millis() - lastCommandAt >= 20) {
    lastCommandAt = millis();
    sendCarCommand(controllerState.drive, controllerState.steer, controllerState.turbo, lightsOn);
  }

  if (carClient != nullptr && !carClient->isConnected()) {
    carConnected = false;
    carFound = false;
    carCharacteristic = nullptr;
    scanRequested = true;
  }
  if (controllerClient != nullptr && !controllerClient->isConnected()) {
    controllerConnected = false;
    controllerFound = false;
    controllerReport = nullptr;
    scanRequested = true;
    controllerState = ControllerState();
    sendCarCommand(0, 0, 0, lightsOn);
  }
  delay(5);
}
