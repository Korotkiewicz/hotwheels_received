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
#include <ESP32Servo.h>

BLECharacteristic *pCharacteristic;
bool deviceConnected = false;
int connectionTime = 0;
std::string rxValue;
BLEServer *pServer;
ESP32PWM pwmForTurning;
ESP32PWM pwmForThrottling;
int PWM_FREQUENCY = 50;//1kHz
int LEFT_MAX_TURN = -100;
int RIGHT_MAX_TURN = 100;
int SERVO_TURN_MIN = 10;
int SERVO_TURN_MAX = 130;
int FORWARD_MAX = 100;
int BACKWARD_MAX = -100;
const int GREEN_LED = 0;
const int RED_LED = 4;
const int BLE_LED = 2; 
const int PWM_FOR_TURNING_PIN = 13; 
const int PWM_FOR_THROTTLING_PIN = 17; 
// const int THROTTLING_DIRECTION_PIN = 5;

#define COMMAND_LIGHTS_TURN_ON    ":LIGHTS_ON;"
#define COMMAND_LIGHTS_TURN_OFF   ":LIGHTS_OFF;"
const std::string COMMAND_CHANGE_MIN_TURN_PREFIX = ":x:min:";
const std::string COMMAND_CHANGE_MAX_TURN_PREFIX = ":x:max:";
#define COMMAND_SUFIX ";"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID                            "5dfa6919-ce00-4e7c-8ddd-3d7a4060a2e0" // UART service UUID
#define CHARACTERISTIC_UUID_TX                  "5dfa6919-ce01-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_COMMANDS            "5dfa6919-ce02-4e7c-8ddd-3d7a4060a2e0"
// #define CHARACTERISTIC_UUID_TURNING             "5dfa6919-ce03-4e7c-8ddd-3d7a4060a2e0"
// #define CHARACTERISTIC_UUID_THROTLING           "5dfa6919-ce04-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_MOVING           "5dfa6919-ce03-4e7c-8ddd-3d7a4060a2e0"


void startAdvertising() {
  // Start advertising
  BLEAdvertising *pAdvertising = pServer->getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("Waiting a client connection to notify...");
}

void setupPWM() {
  ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
  pwmForTurning.attachPin(PWM_FOR_TURNING_PIN, PWM_FREQUENCY, 10); // 1kHz 8 bit
  pwmForThrottling.attachPin(PWM_FOR_THROTTLING_PIN, PWM_FREQUENCY, 10); // 1kHz 8 bit
}

void resetState() {
  digitalWrite(BLE_LED, LOW);  
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, LOW);
  // digitalWrite(THROTTLING_DIRECTION_PIN, LOW);
  pwmForTurning.write(map(0, LEFT_MAX_TURN, RIGHT_MAX_TURN, SERVO_TURN_MIN, SERVO_TURN_MAX)); //reset servo position to middle
  pwmForThrottling.write(0); //reset servo position to middle
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
      delay(250);
      digitalWrite(BLE_LED, LOW);
    };

    void onDisconnect(BLEServer* pServer) {
      resetState();

      Serial.println("Device disconnected!");
      digitalWrite(BLE_LED, HIGH);
      delay(500);
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
      std::string rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        String command = String(rxValue.c_str());
        Serial.println("*********");
        Serial.print("Received Value: " + command);

        // Do stuff based on the command received from the app
        if (rxValue.find(COMMAND_LIGHTS_TURN_ON) != -1) { 
          Serial.println("Lights ON!");
          digitalWrite(GREEN_LED, HIGH);
        }
        else if (rxValue.find(COMMAND_LIGHTS_TURN_OFF) != -1) {
          Serial.println("Lights OFF!");
          digitalWrite(GREEN_LED, LOW);
        }
        else if (rxValue.find(COMMAND_CHANGE_MIN_TURN_PREFIX) != -1) {
          int minTurn = command.substring(COMMAND_CHANGE_MIN_TURN_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change min turn to " + String(minTurn));
          SERVO_TURN_MIN = minTurn;
        }
        else if (rxValue.find(COMMAND_CHANGE_MAX_TURN_PREFIX) != -1) {
          int maxTurn = command.substring(COMMAND_CHANGE_MAX_TURN_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change max turn to " + String(maxTurn));
          SERVO_TURN_MAX = maxTurn;
        } else {
          Serial.println("Unknown command!");
        }

        Serial.println();
        Serial.println("*********");
      }
    }
};

class MovingCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        digitalWrite(RED_LED, HIGH);
        int index = rxValue.indexOf(':');
        long turn = rxValue.substring(0,index).toInt();
        long throttle = rxValue.substring(index + 1, rxValue.length()).toInt();

        if (turn < LEFT_MAX_TURN) {
          turn = LEFT_MAX_TURN;
        } else if (turn > RIGHT_MAX_TURN) {
          turn = RIGHT_MAX_TURN;
        }
        if (throttle < BACKWARD_MAX) {
          throttle = BACKWARD_MAX;
        } else if (throttle > FORWARD_MAX) {
          throttle = FORWARD_MAX;
        }
        turn = map(turn, LEFT_MAX_TURN, RIGHT_MAX_TURN, SERVO_TURN_MIN, SERVO_TURN_MAX);
        throttle = map(throttle, BACKWARD_MAX, FORWARD_MAX, 0, 180);
        
        Serial.println("Move: " + String(turn) + ":" + String(throttle) + "");
        
        pwmForTurning.write(turn); 
        // digitalWrite(THROTTLING_DIRECTION_PIN, throttle <= 0 ? LOW : HIGH);
        pwmForThrottling.write(throttle);
         
        digitalWrite(RED_LED, LOW);
      }
    }
};

// class TurningCallbacks: public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic *pCharacteristic) {
//       String rxValue = pCharacteristic->getValue().c_str();

//       if (rxValue.length() > 0) {
//         digitalWrite(RED_LED, HIGH);
//         long turn = rxValue.toInt();
//         if (turn < LEFT_MAX_TURN) {
//           turn = LEFT_MAX_TURN;
//         } else if (turn > RIGHT_MAX_TURN) {
//           turn = RIGHT_MAX_TURN;
//         }
//         turn = map(turn, LEFT_MAX_TURN, RIGHT_MAX_TURN, SERVO_TURN_MIN, SERVO_TURN_MAX);
        
//         Serial.println("*********");
//         Serial.println("Turning: " + String(turn) + "!");
//         Serial.println("*********");
        
//         pwmForTurning.write(turn);
         
//         digitalWrite(RED_LED, LOW);
//       }
//     }
// };

// class ThrottlingCallbacks: public BLECharacteristicCallbacks {
//     void onWrite(BLECharacteristic *pCharacteristic) {
//       String rxValue = pCharacteristic->getValue().c_str();

//       if (rxValue.length() > 0) {
//         digitalWrite(RED_LED, HIGH);
//         long throttle = rxValue.toInt();
//         if (throttle < BACKWARD_MAX) {
//           throttle = BACKWARD_MAX;
//         } else if (throttle > FORWARD_MAX) {
//           throttle = FORWARD_MAX;
//         }
//         throttle = map(throttle, BACKWARD_MAX, FORWARD_MAX, 0, 180);
        
//         Serial.println("*********");
//         Serial.println("Throttling: " + String(throttle) + "!");
//         Serial.println("*********");
        
//         // digitalWrite(THROTTLING_DIRECTION_PIN, throttle <= 0 ? LOW : HIGH);
//         pwmForThrottling.write(throttle);
         
//         digitalWrite(RED_LED, LOW);
//       }
//     }
// };

void initBle(std::string name, BLECharacteristicCallbacks* commandsCollbacks, BLECharacteristicCallbacks* movingCallbacks) {
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
    CHARACTERISTIC_UUID_MOVING,
    BLECharacteristic::PROPERTY_WRITE
  );
  pCharacteristic->setCallbacks(movingCallbacks);

  digitalWrite(BLE_LED, LOW);

  // Start the service
  pService->start();

  startAdvertising();
}

void setup() {
  Serial.begin(115200);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  pinMode(BLE_LED, OUTPUT);
  // pinMode(THROTTLING_DIRECTION_PIN, OUTPUT);
  setupPWM();
  resetState();
  initBle("HotWheels Drift 0001", new CommandsCallbacks(), new MovingCallbacks());
}

void loop() {
  if (deviceConnected) {
    pCharacteristic->setValue(connectionTime);
    pCharacteristic->notify();
    Serial.print("*** Connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
    delay(1000);
  } else {
    digitalWrite(BLE_LED, HIGH);
    Serial.print("*** No connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
    delay(100);
    digitalWrite(BLE_LED, LOW);
    delay(900);
  }
  connectionTime += 1;
}