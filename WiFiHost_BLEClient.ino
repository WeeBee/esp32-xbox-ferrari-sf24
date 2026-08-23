#include <WiFi.h>
#include <WiFiUdp.h>
#include <BLEDevice.h>
#include "arduino_secrets.h"

const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
unsigned int localPort = 4210;

// Bburago SL-SF-24 profiles
static BLEUUID serviceUUID("0000fff0-0000-1000-8000-00805f9b34fb");
static BLEUUID    charUUID("0000fff1-0000-1000-8000-00805f9b34fb");

static BLERemoteCharacteristic* pRemoteCharacteristic = nullptr;
static BLEClient* pClient = nullptr;
static BLEAddress* pTargetAddress = nullptr;

static bool connected = false;
static bool doConnect = false;

WiFiUDP udp;

// FIX: Expanded the memory size to 64 bytes to completely stop the buffer overflow crash!
uint8_t packetBuffer[64]; 

// --- FUNCTIONS ---

bool connectToCar(BLEAddress pAddress) {
    Serial.println("🔗 Launching clean connection sequence...");
    
    if (pClient != nullptr) {
        delete pClient;
        pClient = nullptr;
    }

    pClient = BLEDevice::createClient();
    delay(200); 

    if (!pClient->connect(pAddress, BLE_ADDR_TYPE_PUBLIC)) {
        Serial.println("⚠️ Public type failed. Trying Random address type...");
        if (!pClient->connect(pAddress, BLE_ADDR_TYPE_RANDOM)) {
            Serial.println("❌ Critical: Bluetooth radio rejected connection.");
            delete pClient;
            pClient = nullptr;
            return false;
        }
    }

    Serial.println("📡 Connected! Searching for Ferrari Control Profiles...");
    delay(300);

    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("❌ Error: Service UUID not found on this vehicle.");
        pClient->disconnect();
        return false;
    }

    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("❌ Error: Write Characteristic not found.");
        pClient->disconnect();
        return false;
    }

    connected = true;
    Serial.println("🎉 SUCCESS! Stable link established with Ferrari SF-24!");
    return true;
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String deviceName = String(advertisedDevice.getName().c_str());
    if (deviceName.length() > 0) {
      Serial.print("Found Device: ");
      Serial.println(deviceName);
    }

    if (deviceName.indexOf("SL-SF-24") != -1) {
      Serial.println("🎯 Targeted SL-SF-24 found! Saving target and stopping scan...");
      if (pTargetAddress != nullptr) delete pTargetAddress;
      pTargetAddress = new BLEAddress(advertisedDevice.getAddress());
      BLEDevice::getScan()->stop();
      doConnect = true; 
    }
  }
};

void sendCarCommand(uint8_t drive, uint8_t steer, uint8_t turbo, uint8_t lights) {
  if (!connected || pRemoteCharacteristic == nullptr) return;

  uint8_t blePacket[] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

  blePacket[0] = 0x01; // Mode Header
  if (drive == 1) blePacket[1] = 0x01; // Forward
  if (drive == 2) blePacket[2] = 0x01; // Backward
  if (steer == 1) blePacket[3] = 0x01; // Left
  if (steer == 2) blePacket[4] = 0x01; // Right
  blePacket[5] = lights;
  blePacket[6] = turbo;
  blePacket[7] = 0x00; // Extra Action Space

  pRemoteCharacteristic->writeValue((uint8_t*)blePacket, (size_t)8, false);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\n📶 ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  configTime(-3 * 3600, 0, "pool.ntp.org");
  Serial.println("⏰ Synchronizing internal clock with internet time...");
  
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    Serial.print("📆 Current Synced Date/Time: ");
    Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
  }

  udp.begin(localPort);

  Serial.println("📡 Broadcasting presence to local network...");
  for(int i = 0; i < 30; i++) {
    udp.beginPacket("255.255.255.255", localPort);
    udp.print("FERRARI_BRIDGE_HERE");
    udp.endPacket();
    delay(1000);
  }
  // --------------------------------------

  BLEDevice::init("ESP32_Bridge");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  Serial.println("🚗 Engine online. Turn on Ferrari now...");
  pBLEScan->start(5, false);
}

void loop() {
  if (doConnect) {
    doConnect = false;
    if (pTargetAddress != nullptr) {
      connectToCar(*pTargetAddress);
    }
  }

  if (!connected && !doConnect) {
    Serial.println("🔍 Scan Pass Active...");
    BLEScanResults* foundDevices = BLEDevice::getScan()->start(3, false);
    BLEDevice::getScan()->clearResults(); 
    delay(2000); 
    return;
  }

  // Handle incoming UDP network packets
  int packetSize = udp.parsePacket();
  if (packetSize > 0) {
    if (packetSize == 1) {
      char request;
      udp.read(&request, 1);
      if (request == '?') {
        // Reply back to the computer's IP and Port with an identity string
        udp.beginPacket(udp.remoteIP(), udp.remotePort());
        udp.print("FERRARI_BRIDGE");
        udp.endPacket();
        Serial.println("📡 Sent Auto-Discovery reply to computer!");
      }
      return;
    }
    if (packetSize >= 4) {
      udp.read(packetBuffer, 4);
      sendCarCommand(packetBuffer[0], packetBuffer[1], packetBuffer[2], packetBuffer[3]);
    }
  }
}
