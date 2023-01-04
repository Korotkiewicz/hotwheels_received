/*
    Video: https://www.youtube.com/watch?v=oCMOYS71NIU
    Based on Neil Kolban example for IDF: https://github.com/nkolban/esp32-snippets/blob/master/cpp_utils/tests/BLE%20Tests/SampleNotify.cpp
    Ported to Arduino ESP32 by Evandro Copercini

   Create a BLE server that, once we receive a connection, will send periodic notifications.
   The service advertises itself as: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
   Has a characteristic of: 6E400002-B5A3-F393-E0A9-E50E24DCCA9E - used for receiving data with "WRITE" 
   Has a characteristic of: 6E400003-B5A3-F393-E0A9-E50E24DCCA9E - used to send data with  "NOTIFY"

   The design of creating the BLE server is:
   1. Create a BLE Server
   2. Create a BLE Service
   3. Create a BLE Characteristic on the Service
   4. Create a BLE Descriptor on the characteristic
   5. Start the service.
   6. Start advertising.

   In this example rxValue is the data received (only accessible inside that function).
   And txValue is the data to be sent, in this example just a byte incremented every second. 
*/
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
int connectionTime = 0;
std::string rxValue;
BLEServer *pServer;
const int LED = 0;
const int BLE_LED = 2; 

#define COMMAND_LIGHTS_TURN_ON    ":LIGHTS_ON;"
#define COMMAND_LIGHTS_TURN_OFF   ":LIGHTS_OFF;"
#define COMMAND_TURN_PREFIX       ":x:"
#define COMMAND_THROTTLE_PREFIX   ":y:"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID                            "5dfa6919-ce00-4e7c-8ddd-3d7a4060a2e0" // UART service UUID
#define CHARACTERISTIC_UUID_TX                  "5dfa6919-ce01-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_COMMANDS            "5dfa6919-ce02-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_TURNING             "5dfa6919-ce03-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_THROTLING           "5dfa6919-ce04-4e7c-8ddd-3d7a4060a2e0"

void startAdvertising() {
  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("Waiting a client connection to notify...");
}

void resetState() {
  digitalWrite(BLE_LED, LOW);  
  digitalWrite(LED, LOW);
  deviceConnected = false;
  connectionTime = 0;
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      connectionTime = 0;
      Serial.println("Device connected!");
      pCharacteristic->setValue("Hello!"); // Sending a test message
      pCharacteristic->notify(); // Send the value to the app!
      
      digitalWrite(BLE_LED, HIGH);
      delay(250);
      digitalWrite(BLE_LED, LOW);
      delay(250);
      digitalWrite(BLE_LED, HIGH);
    };

    void onDisconnect(BLEServer* pServer) {
      resetState();

      Serial.println("Device disconnected!");
      digitalWrite(BLE_LED, LOW);
      delay(500);
      digitalWrite(BLE_LED, HIGH);
      delay(500);
      digitalWrite(BLE_LED, LOW);

      startAdvertising();
    }
};

class CommandsCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      digitalWrite(BLE_LED, LOW);
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");

        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }

        Serial.println();

        // Do stuff based on the command received from the app
        if (rxValue.find(COMMAND_LIGHTS_TURN_ON) != -1) { 
          Serial.println("Lights ON!");
          digitalWrite(LED, HIGH);
        }
        else if (rxValue.find(COMMAND_LIGHTS_TURN_OFF) != -1) {
          Serial.println("Lights OFF!");
          digitalWrite(LED, LOW);
        } else {
          Serial.println("Unknown command!");
        }

        Serial.println();
        Serial.println("*********");
      }
      digitalWrite(BLE_LED, HIGH);
    }
};

class TurningCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      digitalWrite(BLE_LED, LOW);
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        int value = rxValue.toInt();
        Serial.println("*********");
        Serial.println("Turning " + rxValue + "!");
        digitalWrite(LED, LOW);
        delay(100);
        digitalWrite(LED, HIGH);
        delay(abs(value) * 10);
        digitalWrite(LED, LOW);
        Serial.println("*********");
      }
      digitalWrite(BLE_LED, HIGH);
    }
};

class ThrottlingCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      digitalWrite(BLE_LED, LOW);
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        int value = rxValue.toInt();
        Serial.println("*********");
        Serial.println("Throttling" + rxValue + "!");
        digitalWrite(LED, LOW);
        delay(100);
        digitalWrite(LED, HIGH);
        delay(value * 10);
        digitalWrite(LED, LOW);
        delay(500);
        digitalWrite(LED, HIGH);
        delay(abs(value) * 10);   
        digitalWrite(LED, LOW);   
        Serial.println("*********");
      }
      digitalWrite(BLE_LED, HIGH);
    }
};

void initBle(std::string name, BLECharacteristicCallbacks* commandsCollbacks, BLECharacteristicCallbacks* turningCollbacks, BLECharacteristicCallbacks* throttlingCollbacks) {
  digitalWrite(BLE_LED, HIGH);
  // Create the BLE Device
  BLEDevice::init(name); // Give it a name

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
                      
  pCharacteristic->addDescriptor(new BLE2902());

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_COMMANDS,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(commandsCollbacks);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TURNING,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(turningCollbacks);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_THROTLING,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(throttlingCollbacks);

  digitalWrite(BLE_LED, LOW);

  // Start the service
  pService->start();

  startAdvertising();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  pinMode(BLE_LED, OUTPUT);
  resetState();
  initBle("HotWheels Drift 2 (BLE)", new CommandsCallbacks(), new TurningCallbacks(), new ThrottlingCallbacks());
}

void loop() {
  if (deviceConnected) {
    pCharacteristic->setValue(connectionTime);
    pCharacteristic->notify();
    
    Serial.print("*** Connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
  } else {
    Serial.print("*** No connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
  }
  delay(1000);
  connectionTime += 1;
}