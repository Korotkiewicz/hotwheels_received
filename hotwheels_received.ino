#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ESP32Servo.h>

BLECharacteristic *notifyCharacteristic;
bool deviceConnected = false;
int connectionTime = 0;
std::string rxValue;
BLEServer *pServer;
ESP32PWM pwmForTurning;
ESP32PWM pwmForThrottling;
int PWM_FREQUENCY = 50;//1kHz
const int INPUT_LEFT_MAX_TURN = -1000;
const int INPUT_RIGHT_MAX_TURN = 1000;
int SERVO_TURN_MIN = 10;
int SERVO_TURN_MAX = 130;
const int INPUT_MAX_THROTTLING = 1000;
const int INPUT_MIN_THROTTLING = -1000;
int THROTTLING_MIN = 0;
int THROTTLING_MAX = 156;
const int PARGING_LIGHTS_LED = 27;
const int TRAFFIC_LIGHTS_LED = 25;
const int RED_LED = 14;
const int BLE_LED = 26;
const int PWM_FOR_TURNING_PIN = 13; 
const int PWM_FOR_THROTTLING_PIN = 16;

const std::string DEVICE_ID = "0001";//Test one
const std::string DEVICE_ID = "0002";//Dodge Challenger
#define COMMAND_PARKING_LIGHTS_TURN_ON    ":LIGHTS_ON;"
#define COMMAND_PARKING_LIGHTS_TURN_OFF   ":LIGHTS_OFF;"
#define COMMAND_TRAFFIC_LIGHTS_TURN_ON    ":TLIGHTS_ON;"
#define COMMAND_TRAFFIC_LIGHTS_TURN_OFF   ":TLIGHTS_OFF;"
const std::string COMMAND_CHANGE_MIN_TURN_PREFIX = ":x:min:";
const std::string COMMAND_CHANGE_MAX_TURN_PREFIX = ":x:max:";
const std::string COMMAND_CHANGE_THROTTLING_MIN_PREFIX = ":y:min:";
const std::string COMMAND_CHANGE_THROTTLING_MAX_PREFIX = ":y:max:";
#define COMMAND_SUFIX ";"
#define COMMAND_GET_INFO ":GET_INFO;"

// See the following for generating UUIDs:
// https://www.uuidgenerator.net/

#define SERVICE_UUID                            "5dfa6919-ce00-4e7c-8ddd-3d7a4060a2e0" // UART service UUID
#define CHARACTERISTIC_UUID_TX                  "5dfa6919-ce01-4e7c-8ddd-3d7a4060a2e0"
#define CHARACTERISTIC_UUID_COMMANDS            "5dfa6919-ce02-4e7c-8ddd-3d7a4060a2e0"
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

void setNoTurn() {
  pwmForTurning.write(map(0, INPUT_LEFT_MAX_TURN, INPUT_RIGHT_MAX_TURN, SERVO_TURN_MIN, SERVO_TURN_MAX)); //reset servo position to middle
}

void setNoThrottle() {
  pwmForThrottling.write(map(0, INPUT_MIN_THROTTLING, INPUT_MAX_THROTTLING, THROTTLING_MIN, THROTTLING_MAX)); //reset servo position to middle
}

void resetState() {
  digitalWrite(BLE_LED, LOW);  
  digitalWrite(PARGING_LIGHTS_LED, LOW);
  digitalWrite(TRAFFIC_LIGHTS_LED, LOW);
  digitalWrite(RED_LED, LOW);
  setNoTurn();
  setNoThrottle();
  deviceConnected = false;
  connectionTime = 0;
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      connectionTime = 0;
      Serial.println("Device connected!"); // Sending a test message

      notifyCharacteristic->setValue("Hello!");
      notifyCharacteristic->notify(); // Send the value to the app!
      
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
    void onWrite(BLECharacteristic *readCharacteristic) {
      std::string rxValue = readCharacteristic->getValue();

      if (rxValue.length() > 0) {
        String command = String(rxValue.c_str());
        Serial.println("*********");
        Serial.println("Received Value: " + command);

        // Do stuff based on the command received from the app
        if (rxValue.find(COMMAND_PARKING_LIGHTS_TURN_ON) != -1) { 
          Serial.println("Parking Lights ON!");
          digitalWrite(PARGING_LIGHTS_LED, HIGH);
        }
        else if (rxValue.find(COMMAND_PARKING_LIGHTS_TURN_OFF) != -1) {
          Serial.println("Parking Lights OFF!");
          digitalWrite(PARGING_LIGHTS_LED, LOW);
        }
        else if (rxValue.find(COMMAND_TRAFFIC_LIGHTS_TURN_ON) != -1) { 
          Serial.println("Traffic Lights ON!");
          digitalWrite(TRAFFIC_LIGHTS_LED, HIGH);
        }
        else if (rxValue.find(COMMAND_TRAFFIC_LIGHTS_TURN_OFF) != -1) {
          Serial.println("Traffic Lights OFF!");
          digitalWrite(TRAFFIC_LIGHTS_LED, LOW);
        }
        else if (rxValue.find(COMMAND_CHANGE_MIN_TURN_PREFIX) != -1) {
          int minTurn = command.substring(COMMAND_CHANGE_MIN_TURN_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change min turn to " + String(minTurn));
          SERVO_TURN_MIN = minTurn;
          setNoTurn();
        }
        else if (rxValue.find(COMMAND_CHANGE_MAX_TURN_PREFIX) != -1) {
          int maxTurn = command.substring(COMMAND_CHANGE_MAX_TURN_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change max turn to " + String(maxTurn));
          SERVO_TURN_MAX = maxTurn;
          setNoTurn();
        } else if (rxValue.find(COMMAND_CHANGE_THROTTLING_MIN_PREFIX) != -1) {
          int throttlingMin = command.substring(COMMAND_CHANGE_THROTTLING_MIN_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change throttling min speed to " + String(throttlingMin));
          THROTTLING_MIN = throttlingMin;
          setNoThrottle();
        }
        else if (rxValue.find(COMMAND_CHANGE_THROTTLING_MAX_PREFIX) != -1) {
          int throttlingMax = command.substring(COMMAND_CHANGE_THROTTLING_MAX_PREFIX.length(), command.indexOf(COMMAND_SUFIX)).toInt();          
          Serial.println("Change throttling max speed to " + String(throttlingMax));
          THROTTLING_MAX = throttlingMax;
          setNoThrottle();
        }
        else if (rxValue.find(COMMAND_GET_INFO) != -1) {
          String values[4] = {
            String(COMMAND_CHANGE_MIN_TURN_PREFIX.c_str()) + String(SERVO_TURN_MIN) + String(COMMAND_SUFIX),
            String(COMMAND_CHANGE_MAX_TURN_PREFIX.c_str()) + String(SERVO_TURN_MAX) + String(COMMAND_SUFIX),
            String(COMMAND_CHANGE_THROTTLING_MIN_PREFIX.c_str()) + String(THROTTLING_MIN) + String(COMMAND_SUFIX),
            String(COMMAND_CHANGE_THROTTLING_MAX_PREFIX.c_str()) + String(THROTTLING_MAX) + String(COMMAND_SUFIX),
          };

          for (int i = 0; i < 4; i++) {
            Serial.println("Send info " + values[i]);
            notifyCharacteristic->setValue(std::string(values[i].c_str())); // Send info
            notifyCharacteristic->notify();
          }
        } 
        else {
          Serial.println("Unknown command!");
        }

        Serial.println();
        Serial.println("*********");
      }
    }
};

class MovingCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *readCharacteristic) {
      String rxValue = readCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        digitalWrite(RED_LED, HIGH);
        int index = rxValue.indexOf(':');
        long turn = rxValue.substring(0,index).toInt();
        long throttle = rxValue.substring(index + 1, rxValue.length()).toInt();

        if (turn < INPUT_LEFT_MAX_TURN) {
          turn = INPUT_LEFT_MAX_TURN;
        } else if (turn > INPUT_RIGHT_MAX_TURN) {
          turn = INPUT_RIGHT_MAX_TURN;
        }
        if (throttle < INPUT_MIN_THROTTLING) {
          throttle = INPUT_MIN_THROTTLING;
        } else if (throttle > INPUT_MAX_THROTTLING) {
          throttle = INPUT_MAX_THROTTLING;
        }
        turn = map(turn, INPUT_LEFT_MAX_TURN, INPUT_RIGHT_MAX_TURN, SERVO_TURN_MIN, SERVO_TURN_MAX);
        throttle = map(throttle, INPUT_MIN_THROTTLING, INPUT_MAX_THROTTLING, THROTTLING_MIN, THROTTLING_MAX);
        
        Serial.println("Move: " + String(turn) + ":" + String(throttle) + "");
        
        pwmForTurning.write(turn);
        pwmForThrottling.write(throttle);
         
        digitalWrite(RED_LED, LOW);
      }
    }
};

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
  notifyCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    BLECharacteristic::PROPERTY_NOTIFY
  );
                      
  notifyCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
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
  pinMode(PARGING_LIGHTS_LED, OUTPUT);
  pinMode(TRAFFIC_LIGHTS_LED, OUTPUT);
  pinMode(BLE_LED, OUTPUT);
  setupPWM();
  resetState();
  initBle("HotWheels Drift " + DEVICE_ID, new CommandsCallbacks(), new MovingCallbacks());
}

void loop() {
  if (deviceConnected) {
    Serial.print("*** Connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
    delay(10000);
  } else {
    digitalWrite(BLE_LED, HIGH);
    Serial.print("*** No connection time: ");
    Serial.print(connectionTime);
    Serial.println(" ***");
    delay(100);
    digitalWrite(BLE_LED, LOW);
    delay(4900);
  }
  connectionTime += 1;
}