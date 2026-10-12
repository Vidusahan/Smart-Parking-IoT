#include <Wire.h>

#include <LiquidCrystal_I2C.h>

#include <ESP32Servo.h>

#include <WiFi.h>

#include <PubSubClient.h>


// =====================================================

// SMARTPARK - STAGE 3.3

// FULL LOCAL + Wi-Fi + MQTT FIRMWARE

// =====================================================
// =====================================================

// PIN DEFINITIONS

// =====================================================
// ---------------- Ultrasonic - Slot 1 ----------------

#define SLOT1_TRIG 5

#define SLOT1_ECHO 18

// ---------------- Ultrasonic - Slot 2 ----------------

#define SLOT2_TRIG 19

#define SLOT2_ECHO 21

// ---------------- Ultrasonic - Slot 3 ----------------

#define SLOT3_TRIG 22

#define SLOT3_ECHO 23



// ---------------- IR Sensors ----------------

#define ENTRANCE_IR 27

#define EXIT_IR 32

// ---------------- Servo Motors ----------------

#define ENTRANCE_SERVO_PIN 13

#define EXIT_SERVO_PIN 14

// ---------------- LCD ----------------

#define LCD_SDA 16

#define LCD_SCL 17

// =====================================================

// LCD

// =====================================================

LiquidCrystal_I2C lcd(0x3F, 16, 2);

// =====================================================

// SERVOS

// =====================================================

Servo entranceServo;

Servo exitServo;


// Servo angles

const int GATE_CLOSED_ANGLE = 90;

const int GATE_OPEN_ANGLE = 0;


// =====================================================

// PARKING SETTINGS

// =====================================================


// Hysteresis thresholds

const float OCCUPIED_THRESHOLD_CM = 12.0;

const float FREE_THRESHOLD_CM = 18.0;


// =====================================================

// GATE SETTINGS

// =====================================================

const unsigned long GATE_OPEN_TIME = 3000;


// =====================================================

// IR SETTINGS

// =====================================================

// Current configuration:

// Active LOW

//

// If your physical HW-201 test shows:

// Nothing = HIGH

// Object  = LOW

//

// keep this as LOW.

//

// If your actual sensor behaves:

// Nothing = LOW

// Object  = HIGH

//

// change this to HIGH.



#define IR_ACTIVE_STATE LOW



const unsigned long IR_COOLDOWN = 2000;

// =====================================================

// LCD EVENT SETTINGS

// =====================================================



const unsigned long LCD_EVENT_TIME = 2500;

// =====================================================

// Wi-Fi CONFIGURATION

// =====================================================

const char* WIFI_SSID = "Vidda’s iphone";

const char* WIFI_PASSWORD = "vidu2002";



unsigned long lastWiFiAttempt = 0;

const unsigned long WIFI_RETRY_INTERVAL = 10000;

bool wifiConnected = false;


// =====================================================

// MQTT CONFIGURATION

// =====================================================
const char* MQTT_BROKER = "172.20.10.3";

const int MQTT_PORT = 1883;
const char* FIWARE_CMD_TOPIC = "/smartpark2026key/parking001/cmd";


const char* FIWARE_TOPIC = "/json/smartpark2026key/parking001/attrs";
String lastFiwareEvent = "SYSTEM_READY";
unsigned long lastFiwarePublish = 0;
const unsigned long FIWARE_PUBLISH_INTERVAL = 5000;


WiFiClient espClient;

PubSubClient mqttClient(espClient);

unsigned long lastMQTTAttempt = 0;

const unsigned long MQTT_RETRY_INTERVAL = 5000;

// Periodic parking status

unsigned long lastMQTTStatusPublish = 0;

const unsigned long MQTT_STATUS_INTERVAL = 5000;

// Periodic device status

unsigned long lastMQTTDevicePublish = 0;

const unsigned long MQTT_DEVICE_INTERVAL = 10000;

// =====================================================

// SENSOR DISTANCE VARIABLES

// =====================================================

float slot1Distance = -1;

float slot2Distance = -1;

float slot3Distance = -1;

// =====================================================

// DATA STRUCTURES

// =====================================================



struct Gate {

  bool isOpen;

  unsigned long openedAt;

};



Gate entranceGate;

Gate exitGate;


// =====================================================

// GLOBAL VARIABLES

// =====================================================



// Parking states

bool slot1Occupied = false;

bool slot2Occupied = false;

bool slot3Occupied = false;



// Previous IR states

int previousEntranceState;

int previousExitState;



// IR cooldown

unsigned long lastEntranceEvent = 0;

unsigned long lastExitEvent = 0;



// LCD event state

bool lcdShowingEvent = false;

unsigned long lcdEventStarted = 0;



// Current LCD message

String lcdLine1 = "";

String lcdLine2 = "";





// =====================================================

// FUNCTION DECLARATIONS

// =====================================================



// Ultrasonic

float readDistance(int trigPin, int echoPin);

void updateParkingStatus();



// Parking

int getOccupiedCount();

int getAvailableCount();



// IR

void handleIR();

void handleEntranceEvent();

void handleExitEvent();



// Gates

void openEntranceGate();

void openExitGate();

void updateGates();

void closeEntranceGate();

void closeExitGate();



// LCD

void showLCDMessage(String line1, String line2);

void showNormalLCD();

void updateLCD();



// Serial

void printParkingStatus();



// Wi-Fi

void connectWiFi();

void maintainWiFi();



// MQTT

void mqttCallback(char* topic, byte* payload, unsigned int length);

void maintainMQTT();



void publishParkingStatus();

void publishSlotSensors();

void publishGateStatus();

void publishDeviceStatus();
void publishFiwareState();



void publishEntranceEvent(const char* eventName);

void publishExitEvent();



void handleMQTTPublishing();

// =====================================================

// SETUP

// =====================================================



void setup() {



  Serial.begin(115200);



  Serial.println();

  Serial.println("================================");

  Serial.println("       SMARTPARK SYSTEM");

  Serial.println("   STAGE 3.3 MQTT FIRMWARE");

  Serial.println("================================");





  // ===================================================

  // ULTRASONIC PINS

  // ===================================================



  pinMode(SLOT1_TRIG, OUTPUT);

  pinMode(SLOT1_ECHO, INPUT);



  pinMode(SLOT2_TRIG, OUTPUT);

  pinMode(SLOT2_ECHO, INPUT);



  pinMode(SLOT3_TRIG, OUTPUT);

  pinMode(SLOT3_ECHO, INPUT);



  digitalWrite(SLOT1_TRIG, LOW);

  digitalWrite(SLOT2_TRIG, LOW);

  digitalWrite(SLOT3_TRIG, LOW);





  // ===================================================

  // IR PINS

  // ===================================================



  pinMode(ENTRANCE_IR, INPUT);

  pinMode(EXIT_IR, INPUT);



  // Read initial IR states

  previousEntranceState = digitalRead(ENTRANCE_IR);

  previousExitState = digitalRead(EXIT_IR);





  // ===================================================

  // LCD

  // ===================================================



  Wire.begin(LCD_SDA, LCD_SCL);



  lcd.init();

  lcd.backlight();



  lcd.clear();



  lcd.setCursor(0, 0);

  lcd.print("SMARTPARK");



  lcd.setCursor(0, 1);

  lcd.print("Starting...");



  delay(1500);





  // ===================================================

  // SERVO SETUP

  // ===================================================



  entranceServo.setPeriodHertz(50);

  exitServo.setPeriodHertz(50);



  entranceServo.attach(

    ENTRANCE_SERVO_PIN,

    500,

    2400

  );



  exitServo.attach(

    EXIT_SERVO_PIN,

    500,

    2400

  );





  // Start gates closed

  entranceServo.write(GATE_CLOSED_ANGLE);

  exitServo.write(GATE_CLOSED_ANGLE);



  entranceGate.isOpen = false;

  exitGate.isOpen = false;



  entranceGate.openedAt = 0;

  exitGate.openedAt = 0;





  // ===================================================

  // INITIAL PARKING DETECTION

  // ===================================================



  updateParkingStatus();



  showNormalLCD();





  // ===================================================

  // WI-FI

  // ===================================================



  connectWiFi();





  // ===================================================

  // MQTT

  // ===================================================



  mqttClient.setServer(

    MQTT_BROKER,

    MQTT_PORT

  );



  mqttClient.setCallback(mqttCallback);



  // Larger buffer for JSON messages

  mqttClient.setBufferSize(512);





  Serial.println("System ready.");

  Serial.println();

}





// =====================================================

// MAIN LOOP

// =====================================================



void loop() {



  // ---------------------------------------------------

  // 1. Maintain Wi-Fi

  // ---------------------------------------------------



  maintainWiFi();





  // ---------------------------------------------------

  // 2. Maintain MQTT

  // ---------------------------------------------------



  maintainMQTT();





  // ---------------------------------------------------

  // 3. Update parking sensors

  // ---------------------------------------------------



  updateParkingStatus();





  // ---------------------------------------------------

  // 4. Check entrance / exit IR

  // ---------------------------------------------------



  handleIR();





  // ---------------------------------------------------

  // 5. Update gate timers

  // ---------------------------------------------------



  updateGates();





  // ---------------------------------------------------

  // 6. Update LCD

  // ---------------------------------------------------



  updateLCD();





  // ---------------------------------------------------

  // 7. MQTT publishing

  // ---------------------------------------------------



  handleMQTTPublishing();





  // ---------------------------------------------------

  // 8. Serial status

  // ---------------------------------------------------



  static unsigned long lastStatusPrint = 0;



  if (millis() - lastStatusPrint >= 2000) {



    lastStatusPrint = millis();



    printParkingStatus();

  }

  // Periodically publish the current parking state to FIWARE
  if (mqttClient.connected() &&
    millis() - lastFiwarePublish >= FIWARE_PUBLISH_INTERVAL) {

  publishFiwareState();
  lastFiwarePublish = millis();
}



  // Small loop delay

  delay(50);

}





// =====================================================

// MQTT CALLBACK

// =====================================================



void mqttCallback(char* topic, byte* payload, unsigned int length) {

  Serial.print("MQTT message received [");
  Serial.print(topic);
  Serial.print("]: ");

  String message = "";

  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.println(message);

  // Only process FIWARE command topic
  if (String(topic) != FIWARE_CMD_TOPIC) {
    return;
  }

  // Remove characters that may be changed/removed by MQTT testing
  String command = message;
  command.replace("{", "");
  command.replace("}", "");
  command.replace("\"", "");
  command.replace(" ", "");

  Serial.print("Parsed command: ");
  Serial.println(command);

  // =========================================
  // ENTRANCE GATE
  // =========================================

  if (command == "gate:open") {

    Serial.println("Manual command: OPEN ENTRANCE GATE");

    openEntranceGate();

    showLCDMessage(
      "MANUAL CONTROL",
      "ENTRY OPEN"
    );
  }

  else if (command == "gate:close") {

    Serial.println("Manual command: CLOSE ENTRANCE GATE");

    closeEntranceGate();

    showLCDMessage(
      "MANUAL CONTROL",
      "ENTRY CLOSED"
    );
  }

  // =========================================
  // EXIT GATE
  // =========================================

  else if (command == "exitGate:open") {

    Serial.println("Manual command: OPEN EXIT GATE");

    openExitGate();

    showLCDMessage(
      "MANUAL CONTROL",
      "EXIT OPEN"
    );
  }

  else if (command == "exitGate:close") {

    Serial.println("Manual command: CLOSE EXIT GATE");

    closeExitGate();

    showLCDMessage(
      "MANUAL CONTROL",
      "EXIT CLOSED"
    );
  }

  else {
    Serial.println("Unknown gate command.");
  }
}


// =====================================================

// MQTT MAINTENANCE

// =====================================================



void maintainMQTT() {



  // MQTT requires Wi-Fi

  if (WiFi.status() != WL_CONNECTED) {



    if (mqttClient.connected()) {

      mqttClient.disconnect();

    }



    return;

  }





  // Already connected

  if (mqttClient.connected()) {



    mqttClient.loop();



    return;

  }





  // Don't retry continuously

  if (millis() - lastMQTTAttempt < MQTT_RETRY_INTERVAL) {

    return;

  }





  lastMQTTAttempt = millis();



  Serial.println();

  Serial.println("Connecting to MQTT broker...");



  Serial.print("Broker: ");

  Serial.print(MQTT_BROKER);

  Serial.print(":");

  Serial.println(MQTT_PORT);





  // Unique client ID

  String clientId = "SmartPark-ESP32-";



  clientId += String(

    (uint32_t)ESP.getEfuseMac(),

    HEX

  );





  if (mqttClient.connect(clientId.c_str())) {



    Serial.println("MQTT connected!");



    // Test subscription

    mqttClient.subscribe(FIWARE_CMD_TOPIC);

    Serial.print("Subscribed to: ");
    Serial.println(FIWARE_CMD_TOPIC);



    Serial.println("MQTT ready.");



    // Immediately publish current system state

    publishParkingStatus();

    publishSlotSensors();

    publishGateStatus();

    publishDeviceStatus();
    publishFiwareState();



  }

  else {



    Serial.print("MQTT connection failed. State: ");

    Serial.println(mqttClient.state());

  }

}





// =====================================================

// MQTT - PARKING STATUS

// =====================================================



void publishParkingStatus() {



  if (!mqttClient.connected()) {

    return;

  }





  int occupied = getOccupiedCount();

  int available = getAvailableCount();





  String payload = "{";





  payload += "\"slot1\":\"";

  payload += slot1Occupied ? "occupied" : "free";

  payload += "\",";





  payload += "\"slot2\":\"";

  payload += slot2Occupied ? "occupied" : "free";

  payload += "\",";





  payload += "\"slot3\":\"";

  payload += slot3Occupied ? "occupied" : "free";

  payload += "\",";





  payload += "\"occupiedCount\":";

  payload += String(occupied);

  payload += ",";





  payload += "\"availableCount\":";

  payload += String(available);





  payload += "}";





  mqttClient.publish(

    "smartpark/parking/status",

    payload.c_str(),

    true

  );





  Serial.print("MQTT parking status: ");

  Serial.println(payload);

}





// =====================================================

// MQTT - SLOT SENSOR DATA

// =====================================================



void publishSlotSensors() {



  if (!mqttClient.connected()) {

    return;

  }





  String payload = "{";





  payload += "\"slot1Distance\":";

  payload += String(slot1Distance, 1);

  payload += ",";





  payload += "\"slot2Distance\":";

  payload += String(slot2Distance, 1);

  payload += ",";





  payload += "\"slot3Distance\":";

  payload += String(slot3Distance, 1);





  payload += "}";





  mqttClient.publish(

    "smartpark/sensors/slots",

    payload.c_str(),

    true

  );





  Serial.print("MQTT sensor data: ");

  Serial.println(payload);

}





// =====================================================

// MQTT - GATE STATUS

// =====================================================



void publishGateStatus() {



  if (!mqttClient.connected()) {

    return;

  }



  String payload = "{";



  payload += "\"entranceGate\":\"";

  payload += entranceGate.isOpen ? "open" : "closed";

  payload += "\",";



  payload += "\"exitGate\":\"";

  payload += exitGate.isOpen ? "open" : "closed";



  payload += "\"}";



  mqttClient.publish(

    "smartpark/gates/status",

    payload.c_str(),

    true

  );



  Serial.print("MQTT gate status: ");

  Serial.println(payload);

}





// =====================================================

// MQTT - DEVICE STATUS

// =====================================================



void publishDeviceStatus() {



  if (!mqttClient.connected()) {

    return;

  }





  String payload = "{";





  payload += "\"device\":\"SmartPark-ESP32\",";





  payload += "\"wifi\":\"connected\",";





  payload += "\"ip\":\"";

  payload += WiFi.localIP().toString();

  payload += "\",";





  payload += "\"rssi\":";

  payload += String(WiFi.RSSI());

  payload += ",";





  payload += "\"uptime\":";

  payload += String(millis());





  payload += "}";





  mqttClient.publish(

    "smartpark/device/status",

    payload.c_str(),

    true

  );





  Serial.print("MQTT device status: ");

  Serial.println(payload);

}





// =====================================================

// =====================================================
// FIWARE - CURRENT PARKING STATE
// =====================================================
void publishFiwareState() {

  if (!mqttClient.connected()) return;

  int occupied = getOccupiedCount();
  int available = getAvailableCount();
  float occupancyRate = (occupied / 3.0) * 100.0;

  String payload = "{";
  payload += "\"s1\":\""; payload += slot1Occupied ? "occupied" : "free"; payload += "\",";
  payload += "\"s2\":\""; payload += slot2Occupied ? "occupied" : "free"; payload += "\",";
  payload += "\"s3\":\""; payload += slot3Occupied ? "occupied" : "free"; payload += "\",";
  payload += "\"occ\":"; payload += String(occupied); payload += ",";
  payload += "\"free\":"; payload += String(available); payload += ",";
  payload += "\"rate\":"; payload += String(occupancyRate, 2); payload += ",";
  payload += "\"gate\":\""; payload += entranceGate.isOpen ? "open" : "closed"; payload += "\",";
  payload += "\"eg\":\""; payload += exitGate.isOpen ? "open" : "closed"; payload += "\",";
  payload += "\"ev\":\""; payload += lastFiwareEvent; payload += "\",";
  payload += "\"rssi\":"; payload += String(WiFi.RSSI());
  payload += "}";

  mqttClient.publish(FIWARE_TOPIC, payload.c_str(), true);
  Serial.print("FIWARE state: ");
  Serial.println(payload);
}


// MQTT - ENTRANCE EVENT

// =====================================================



void publishEntranceEvent(

  const char* eventName

) {

  lastFiwareEvent = String(eventName);



  if (!mqttClient.connected()) {

    return;

  }





  String payload = "{";





  payload += "\"event\":\"";

  payload += eventName;

  payload += "\",";





  payload += "\"availableCount\":";

  payload += String(getAvailableCount());





  payload += "}";





  // Events are NOT retained

  mqttClient.publish(

    "smartpark/events/entrance",

    payload.c_str(),

    false

  );





  Serial.print("MQTT entrance event: ");

  Serial.println(payload);

}





// =====================================================

// MQTT - EXIT EVENT

// =====================================================



void publishExitEvent() {

  lastFiwareEvent = "EXIT_DETECTED";



  if (!mqttClient.connected()) {

    return;

  }





  String payload =

    "{\"event\":\"EXIT_DETECTED\"}";





  // Events are NOT retained

  mqttClient.publish(

    "smartpark/events/exit",

    payload.c_str(),

    false

  );





  Serial.print("MQTT exit event: ");

  Serial.println(payload);

}





// =====================================================

// MQTT PERIODIC PUBLISHING

// =====================================================



void handleMQTTPublishing() {



  if (!mqttClient.connected()) {

    return;

  }





  unsigned long now = millis();





  // ---------------------------------------------------

  // Parking + sensor data every 5 seconds

  // ---------------------------------------------------



  if (

    now - lastMQTTStatusPublish >=

    MQTT_STATUS_INTERVAL

  ) {



    lastMQTTStatusPublish = now;



    publishParkingStatus();



    publishSlotSensors();

  }





  // ---------------------------------------------------

  // Device status every 10 seconds

  // ---------------------------------------------------



  if (

    now - lastMQTTDevicePublish >=

    MQTT_DEVICE_INTERVAL

  ) {



    lastMQTTDevicePublish = now;



    publishDeviceStatus();

  }

}





// =====================================================

// WI-FI CONNECTION

// =====================================================



void connectWiFi() {



  Serial.println();

  Serial.println("================================");

  Serial.println("       Wi-Fi CONNECTION");

  Serial.println("================================");





  WiFi.mode(WIFI_STA);



  WiFi.begin(

    WIFI_SSID,

    WIFI_PASSWORD

  );





  unsigned long start = millis();





  while (

    WiFi.status() != WL_CONNECTED &&

    millis() - start < 20000

  ) {



    delay(500);



    Serial.print(".");

  }





  Serial.println();





  if (WiFi.status() == WL_CONNECTED) {



    wifiConnected = true;



    Serial.println("Wi-Fi connected!");





    Serial.print("IP address: ");

    Serial.println(WiFi.localIP());





    Serial.print("RSSI: ");

    Serial.print(WiFi.RSSI());

    Serial.println(" dBm");



  }

  else {



    wifiConnected = false;



    Serial.println("Wi-Fi connection failed.");



    Serial.println(

      "Continuing in offline mode."

    );

  }





  Serial.println("================================");

}





// =====================================================

// WI-FI MAINTENANCE

// =====================================================



void maintainWiFi() {



  // ---------------------------------------------------

  // Wi-Fi connected

  // ---------------------------------------------------



  if (WiFi.status() == WL_CONNECTED) {



    if (!wifiConnected) {



      wifiConnected = true;



      Serial.println();

      Serial.println("Wi-Fi reconnected!");



      Serial.print("IP address: ");

      Serial.println(WiFi.localIP());

    }



    return;

  }





  // ---------------------------------------------------

  // Wi-Fi disconnected

  // ---------------------------------------------------



  if (wifiConnected) {



    wifiConnected = false;



    Serial.println();

    Serial.println("Wi-Fi disconnected!");



    Serial.println(

      "SmartPark continuing offline."

    );

  }





  // ---------------------------------------------------

  // Reconnection attempt

  // ---------------------------------------------------



  if (

    millis() - lastWiFiAttempt >=

    WIFI_RETRY_INTERVAL

  ) {



    lastWiFiAttempt = millis();



    Serial.println();

    Serial.println(

      "Attempting Wi-Fi reconnection..."

    );





    WiFi.disconnect();



    WiFi.begin(

      WIFI_SSID,

      WIFI_PASSWORD

    );

  }

}





// =====================================================

// ULTRASONIC DISTANCE

// =====================================================



float readDistance(

  int trigPin,

  int echoPin

) {



  // Ensure trigger is LOW

  digitalWrite(

    trigPin,

    LOW

  );



  delayMicroseconds(2);





  // Send 10 us pulse

  digitalWrite(

    trigPin,

    HIGH

  );



  delayMicroseconds(10);



  digitalWrite(

    trigPin,

    LOW

  );





  // Measure echo

  long duration = pulseIn(

    echoPin,

    HIGH,

    30000

  );





  // No echo

  if (duration == 0) {

    return -1;

  }





  // Convert to centimeters

  float distance =

    duration * 0.0343 / 2.0;





  return distance;

}





// =====================================================

// PARKING STATUS

// =====================================================



void updateParkingStatus() {



  // ---------------------------------------------------

  // Read Slot 1

  // ---------------------------------------------------



  slot1Distance =

    readDistance(

      SLOT1_TRIG,

      SLOT1_ECHO

    );





  // Small delay to reduce ultrasonic interference

  delay(40);





  // ---------------------------------------------------

  // Read Slot 2

  // ---------------------------------------------------



  slot2Distance =

    readDistance(

      SLOT2_TRIG,

      SLOT2_ECHO

    );





  delay(40);





  // ---------------------------------------------------

  // Read Slot 3

  // ---------------------------------------------------



  slot3Distance =

    readDistance(

      SLOT3_TRIG,

      SLOT3_ECHO

    );





  // ===================================================

  // SLOT 1 HYSTERESIS

  // ===================================================



  if (slot1Distance >= 0) {



    if (

      !slot1Occupied &&

      slot1Distance <=

      OCCUPIED_THRESHOLD_CM

    ) {



      slot1Occupied = true;

    }



    else if (

      slot1Occupied &&

      slot1Distance >=

      FREE_THRESHOLD_CM

    ) {



      slot1Occupied = false;

    }

  }





  // ===================================================

  // SLOT 2 HYSTERESIS

  // ===================================================



  if (slot2Distance >= 0) {



    if (

      !slot2Occupied &&

      slot2Distance <=

      OCCUPIED_THRESHOLD_CM

    ) {



      slot2Occupied = true;

    }



    else if (

      slot2Occupied &&

      slot2Distance >=

      FREE_THRESHOLD_CM

    ) {



      slot2Occupied = false;

    }

  }





  // ===================================================

  // SLOT 3 HYSTERESIS

  // ===================================================



  if (slot3Distance >= 0) {



    if (

      !slot3Occupied &&

      slot3Distance <=

      OCCUPIED_THRESHOLD_CM

    ) {



      slot3Occupied = true;

    }



    else if (

      slot3Occupied &&

      slot3Distance >=

      FREE_THRESHOLD_CM

    ) {



      slot3Occupied = false;

    }

  }

}





// =====================================================

// PARKING COUNTS

// =====================================================



int getOccupiedCount() {



  int count = 0;



  if (slot1Occupied) {

    count++;

  }



  if (slot2Occupied) {

    count++;

  }



  if (slot3Occupied) {

    count++;

  }



  return count;

}





int getAvailableCount() {



  return 3 - getOccupiedCount();

}





// =====================================================

// IR HANDLING

// =====================================================



void handleIR() {



  int currentEntranceState =

    digitalRead(ENTRANCE_IR);



  int currentExitState =

    digitalRead(EXIT_IR);





  // ===================================================

  // ENTRANCE

  // ===================================================



  if (

    previousEntranceState !=

    IR_ACTIVE_STATE &&



    currentEntranceState ==

    IR_ACTIVE_STATE

  ) {



    if (

      millis() - lastEntranceEvent >=

      IR_COOLDOWN

    ) {



      lastEntranceEvent = millis();



      handleEntranceEvent();

    }

  }





  // ===================================================

  // EXIT

  // ===================================================



  if (

    previousExitState !=

    IR_ACTIVE_STATE &&



    currentExitState ==

    IR_ACTIVE_STATE

  ) {



    if (

      millis() - lastExitEvent >=

      IR_COOLDOWN

    ) {



      lastExitEvent = millis();



      handleExitEvent();

    }

  }





  // Save current states

  previousEntranceState =

    currentEntranceState;



  previousExitState =

    currentExitState;

}





// =====================================================

// ENTRANCE EVENT

// =====================================================



void handleEntranceEvent() {



  Serial.println();

  Serial.println("================================");

  Serial.println("    VEHICLE AT ENTRANCE");

  Serial.println("================================");





  int available =

    getAvailableCount();





  Serial.print("Available slots: ");

  Serial.println(available);





  // ===================================================

  // ENTRY ALLOWED

  // ===================================================



  if (available > 0) {



    Serial.println("ENTRY ALLOWED");





    // MQTT event

    publishEntranceEvent(

      "ENTRY_ALLOWED"

    );





    showLCDMessage(

      "ENTRY ALLOWED",

      "Gate: OPEN"

    );





    openEntranceGate();

  }





  // ===================================================

  // PARKING FULL

  // ===================================================



  else {



    Serial.println("PARKING FULL");

    Serial.println("ENTRY DENIED");





    // MQTT event

    publishEntranceEvent(

      "ENTRY_DENIED_FULL"

    );





    showLCDMessage(

      "PARKING FULL",

      "ENTRY DENIED"

    );

  }

}





// =====================================================

// EXIT EVENT

// =====================================================



void handleExitEvent() {



  Serial.println();

  Serial.println("================================");

  Serial.println("      VEHICLE AT EXIT");

  Serial.println("================================");



  Serial.println("EXIT DETECTED");

  Serial.println("EXIT GATE OPEN");





  // MQTT event

  publishExitEvent();





  showLCDMessage(

    "EXIT DETECTED",

    "Gate: OPEN"

  );





  openExitGate();

}





// =====================================================

// OPEN ENTRANCE GATE

// =====================================================



void openEntranceGate() {



  // Already open

  if (entranceGate.isOpen) {

    return;

  }





  entranceServo.write(

    GATE_OPEN_ANGLE

  );





  entranceGate.isOpen = true;



  entranceGate.openedAt =

    millis();





  Serial.println(

    "Entrance gate opened."

  );





  // MQTT gate status

  publishGateStatus();

}





// =====================================================

// OPEN EXIT GATE

// =====================================================



void openExitGate() {



  // Already open

  if (exitGate.isOpen) {

    return;

  }





  exitServo.write(

    GATE_OPEN_ANGLE

  );





  exitGate.isOpen = true;



  exitGate.openedAt =

    millis();





  Serial.println(

    "Exit gate opened."

  );





  // MQTT gate status

  publishGateStatus();

}





// =====================================================

// UPDATE GATES

// =====================================================



void updateGates() {



  // ---------------------------------------------------

  // Entrance gate

  // ---------------------------------------------------



  if (entranceGate.isOpen) {



    if (

      millis() -

      entranceGate.openedAt >=

      GATE_OPEN_TIME

    ) {



      closeEntranceGate();

    }

  }





  // ---------------------------------------------------

  // Exit gate

  // ---------------------------------------------------



  if (exitGate.isOpen) {



    if (

      millis() -

      exitGate.openedAt >=

      GATE_OPEN_TIME

    ) {



      closeExitGate();

    }

  }

}





// =====================================================

// CLOSE ENTRANCE GATE

// =====================================================



void closeEntranceGate() {



  entranceServo.write(

    GATE_CLOSED_ANGLE

  );





  entranceGate.isOpen = false;





  Serial.println(

    "Entrance gate closed."

  );





  // MQTT gate status

  publishGateStatus();

}





// =====================================================

// CLOSE EXIT GATE

// =====================================================



void closeExitGate() {



  exitServo.write(

    GATE_CLOSED_ANGLE

  );





  exitGate.isOpen = false;





  Serial.println(

    "Exit gate closed."

  );





  // MQTT gate status

  publishGateStatus();

}





// =====================================================

// LCD EVENT MESSAGE

// =====================================================



void showLCDMessage(

  String line1,

  String line2

) {



  lcd.clear();





  lcd.setCursor(0, 0);

  lcd.print(line1);





  lcd.setCursor(0, 1);

  lcd.print(line2);





  lcdShowingEvent = true;



  lcdEventStarted =

    millis();

}





// =====================================================

// NORMAL LCD

// =====================================================



void showNormalLCD() {



  lcd.clear();





  // Line 1

  lcd.setCursor(0, 0);



  lcd.print("Free:");

  lcd.print(getAvailableCount());



  lcd.print(" Occ:");

  lcd.print(getOccupiedCount());





  // Line 2

  lcd.setCursor(0, 1);



  lcd.print("S1:");

  lcd.print(

    slot1Occupied ? "X" : "O"

  );





  lcd.print(" S2:");

  lcd.print(

    slot2Occupied ? "X" : "O"

  );





  lcd.print(" S3:");

  lcd.print(

    slot3Occupied ? "X" : "O"

  );

}





// =====================================================

// LCD UPDATE

// =====================================================



void updateLCD() {



  // ---------------------------------------------------

  // Event message currently displayed

  // ---------------------------------------------------



  if (lcdShowingEvent) {



    if (

      millis() -

      lcdEventStarted >=

      LCD_EVENT_TIME

    ) {



      lcdShowingEvent = false;



      showNormalLCD();

    }



    return;

  }





  // ---------------------------------------------------

  // Normal display

  // ---------------------------------------------------



  static unsigned long lastLCDUpdate = 0;





  if (

    millis() -

    lastLCDUpdate >=

    500

  ) {



    lastLCDUpdate = millis();



    showNormalLCD();

  }

}





// =====================================================

// SERIAL STATUS

// =====================================================



void printParkingStatus() {



  Serial.println(

    "--------------------------------"

  );





  Serial.print("Slot 1: ");



  Serial.println(

    slot1Occupied

      ? "OCCUPIED"

      : "FREE"

  );





  Serial.print("Slot 2: ");



  Serial.println(

    slot2Occupied

      ? "OCCUPIED"

      : "FREE"

  );





  Serial.print("Slot 3: ");



  Serial.println(

    slot3Occupied

      ? "OCCUPIED"

      : "FREE"

  );





  Serial.print("Occupied: ");



  Serial.println(

    getOccupiedCount()

  );





  Serial.print("Available: ");



  Serial.println(

    getAvailableCount()

  );





  Serial.print("Entrance Gate: ");



  Serial.println(

    entranceGate.isOpen

      ? "OPEN"

      : "CLOSED"

  );





  Serial.print("Exit Gate: ");



  Serial.println(

    exitGate.isOpen

      ? "OPEN"

      : "CLOSED"

  );





  Serial.print("Wi-Fi: ");



  Serial.println(

    WiFi.status() == WL_CONNECTED

      ? "CONNECTED"

      : "OFFLINE"

  );





  Serial.print("MQTT: ");



  Serial.println(

    mqttClient.connected()

      ? "CONNECTED"

      : "OFFLINE"

  );





  Serial.println(

    "--------------------------------"

  );

}