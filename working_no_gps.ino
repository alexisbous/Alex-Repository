/** School Bus Safety & Monitoring System
Purpose
The mC (ESP32-S3) acts as a central safety unit for a school bus.
It streams live video over HTTP, monitors vehicle orientation (tilt) to detect accidents,
measures cabin air quality (smoke/alcohol) and environment (temp/humidity), and tracks
real-time location via GPS. All telemetry data is transmitted via WiFi/MQTT to a 
remote Web Dashboard, and status is displayed locally on an LCD.

Hardware
- MCU: DFRobot FireBeetle 2 ESP32-S3.
- Camera: OV2640 (Onboard interface).
- Sensors: 
    * LIS2DW12 Accelerometer (I2C, SDA=Pin 1, SCL=Pin 2).
    * NEO-6M GPS Module (TX connects to ESP Pin 12, RX to ESP Pin 13).
    * MQ-3 Gas Sensor (Analog, connects to Pin 10).
    * DHT11 Temperature/Humidity (Digital, connects to Pin 14).
- I/O:
    * LCD 20x4 Display (I2C, SDA=Pin 1, SCL=Pin 2).
    * Push Button (Pin 9, internal pull-up).
    * Buzzer (Pin 18), Red LED (Pin 3), Green LED (Pin 38).

Software
Uses libraries: esp_camera, WiFi, PubSubClient (MQTT), Wire, LiquidCrystal_I2C, 
DHT, DFRobot_LIS2DW12, TinyGPSPlus, DFRobot_AXP313A.
Implements non-blocking loop for sensor reading and telemetry transmission.

Reference
v1, M. Velalopoulos, A. Boustris, Jan. 2026.
**/

#include "esp_camera.h"
#include <WiFi.h>
#include "DFRobot_AXP313A.h"
#include <PubSubClient.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "DHT.h"
#include "DFRobot_LIS2DW12.h"
#include <TinyGPSPlus.h>
#include <math.h>

//Pins
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     45
#define SIOD_GPIO_NUM     1
#define SIOC_GPIO_NUM     2
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       46
#define Y7_GPIO_NUM       8
#define Y6_GPIO_NUM       7
#define Y5_GPIO_NUM       4
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     42
#define PCLK_GPIO_NUM     5

const int PIN_LED_RED    = 3;   
const int PIN_LED_GREEN  = 38;  
const int PIN_BUZZER     = 18;  
const int PIN_BUTTON     = 9;   
const int PIN_DHT        = 14;  
const int PIN_MQ3_ADC    = 10; 
const int GPS_RX_PIN     = 12; 
const int GPS_TX_PIN     = 13; 

//Config
const char* ssid = "rocket4";
const char* password = "palestragymclub!";
const char* MQTT_HOST = "192.168.0.187"; 
const uint16_t MQTT_PORT = 1884;
const char* BUS_ID = "bus01";

WiFiClient espClient;
PubSubClient mqtt(espClient);
DFRobot_AXP313A axp;
LiquidCrystal_I2C lcd(0x27, 20, 4);
DFRobot_LIS2DW12_I2C acce(&Wire, 0x18);
DHT dht(PIN_DHT, DHT11);
TinyGPSPlus gps;
HardwareSerial GPSserial(1);

//Variables
bool acceOk = false;
enum AlarmState { ALARM_IDLE, ALARM_PENDING, ALARM_ACTIVE };
AlarmState alarmState = ALARM_IDLE;
unsigned long alarmTriggeredAt = 0;
const unsigned long ALARM_CONFIRM_WINDOW_MS = 10000;

float lastAx=0, lastAy=0, lastAz=1, lastTiltDeg=0;
int lastMq3Raw=0;
float lastTemp=0, lastHum=0;
double lastLat=0.0, lastLon=0.0;
float lastSpeedKmh=0.0;

unsigned long lastTelemetryMs=0, lastSensorsMs=0, lastLcdUpdateMs=0;

void startCameraServer();

//Functions

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  Serial.println(WiFi.localIP());
}

void connectMQTT() {
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  if (!mqtt.connected()) {
    String id = "esp-" + String(BUS_ID) + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    if (mqtt.connect(id.c_str())) {
      mqtt.subscribe("bus/bus01/cmd");
    }
  }
}

void publishEvent(String type, String msg) {
  if (!mqtt.connected()) return;
  String payload = "{\"type\":\"" + type + "\",\"msg\":\"" + msg + "\"}";
  mqtt.publish(("bus/" + String(BUS_ID) + "/event").c_str(), payload.c_str());
}

void publishTelemetry() {
  if (!mqtt.connected()) return;
  String payload = "{";
  payload += "\"lat\":" + String(lastLat, 6) + ",";
  payload += "\"lon\":" + String(lastLon, 6) + ",";
  payload += "\"speed_kmh\":" + String(lastSpeedKmh, 1) + ",";
  payload += "\"ax\":" + String(lastAx, 3) + ",";
  payload += "\"tilt_deg\":" + String(lastTiltDeg, 1) + ",";
  payload += "\"mq3_raw\":" + String(lastMq3Raw) + ",";
  payload += "\"temp\":" + String(lastTemp, 1) + ",";
  payload += "\"hum\":" + String(lastHum, 1);
  payload += "}";
  mqtt.publish(("bus/" + String(BUS_ID) + "/telemetry").c_str(), payload.c_str());
}

/** function readGps
Purpose
Reads available characters from the hardware serial port connected
to the GPS module and passes them to the TinyGPS++ library for parsing.
Results
Updates global variables lastLat, lastLon, and lastSpeedKmh (double/float)
when a valid location fix is updated by the GPS module. Returns void.
Hardware
NEO-6M GPS Module connected to RX Pin 12 and TX Pin 13 (UART1).
Software
Uses the TinyGPSPlus library object 'gps' to encode serial stream data.
Reference
v1, M. Velalopoulos, A. Boustris, Jan. 2026.
**/

void readGps() {
  while (GPSserial.available() > 0) gps.encode(GPSserial.read());
  if (gps.location.isValid()) {
    lastLat = gps.location.lat();
    lastLon = gps.location.lng();
    lastSpeedKmh = gps.speed.kmph();
  }
}

void checkButton() {
  static int lastState = HIGH;
  static unsigned long lastDebounce = 0;
  int reading = digitalRead(PIN_BUTTON);

  if (reading != lastState && (millis() - lastDebounce > 50)) {
    lastDebounce = millis();
    lastState = reading;
    if (reading == LOW) {
      //Beep για επιβεβαίωση
      digitalWrite(PIN_BUZZER, HIGH); delay(100); digitalWrite(PIN_BUZZER, LOW);
      
      //Αν υπάρχει συναγερμός, τον ακυρώνουμε
      if (alarmState != ALARM_IDLE) {
        alarmState = ALARM_IDLE;
        publishEvent("ACCIDENT_CANCELLED", "Cancelled by Driver");
      } 
      //Αν δεν υπάρχει συναγερμός, στέλνουμε απλό event ότι πατήθηκε
      else {
        publishEvent("BUTTON_PRESS", "Driver pressed button");
      }
    }
  }
}

/** function updateAlarmLogic
Purpose
Evaluates the current sensor readings (tilt angle and air quality)
to determine if an emergency condition exists (Accident or Smoke/Alcohol)
Manages the transition between alarm states (IDLE, PENDING, ACTIVE)
Results
Updates the global 'alarmState' variable based on thresholds.
Publishes MQTT events ("ACCIDENT_DETECTED", "SMOKE_ALCOHOL") via WiFi
if thresholds are exceeded. Returns void
Hardware
None directly, uses processed data from LIS2DW12 and MQ-3
Software
Uses millis() for non-blocking confirmation timers to prevent false alarms
Reference
v1, M. Velalopoulos, A. Boustris, Jan. 2026.
**/

void updateAlarmLogic() {
  bool tilt = (acceOk && lastTiltDeg > 45.0);
  if (alarmState == ALARM_IDLE && tilt) {
    alarmState = ALARM_PENDING;
    alarmTriggeredAt = millis();
    publishEvent("ACCIDENT_DETECTED", "Tilt Detected");
  }
  if (alarmState == ALARM_PENDING && (millis() - alarmTriggeredAt > ALARM_CONFIRM_WINDOW_MS)) {
    alarmState = ALARM_ACTIVE;
    publishEvent("ACCIDENT_CONFIRMED", "Confirmed Accident");
  }

  static bool mqHigh = false;
  if (lastMq3Raw > 2000 && !mqHigh) {
    mqHigh = true; publishEvent("SMOKE_ALCOHOL", "High Levels");
  } else if (lastMq3Raw < 1800 && mqHigh) {
    mqHigh = false; publishEvent("SMOKE_ALCOHOL_CLEAR", "Levels Normal");
  }
}

void updateLcd() {
  lcd.clear();
  
  //Γραμμή 0: Κατάσταση
  lcd.setCursor(0, 0); 
  if (alarmState == ALARM_ACTIVE) lcd.print("ALARM! ACCIDENT!");
  else if (alarmState == ALARM_PENDING) lcd.print("WARNING: TILT!");
  else lcd.print("Status: OK");

  //Γραμμή 1: GPS
  lcd.setCursor(0, 1);
  if (lastLat != 0.0) {
    lcd.print(lastLat, 4); lcd.print(" "); lcd.print(lastLon, 4);
  } else {
    lcd.print("GPS: Searching...");
  }

  //Γραμμή 2: Sensors
  lcd.setCursor(0, 2);
  lcd.print("T:"); lcd.print((int)lastTemp); 
  lcd.print(" H:"); lcd.print((int)lastHum);
  lcd.print(" MQ:"); lcd.print(lastMq3Raw);

  //Γραμμή 3: IP & Speed
  lcd.setCursor(0, 3);
  lcd.print("S:"); lcd.print((int)lastSpeedKmh); lcd.print("km ");
  lcd.print(WiFi.localIP().toString().substring(10)); 
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM); delay(100);
  
  if (!acce.begin()) acceOk = false; 
  else { acceOk = true; acce.setRange(DFRobot_LIS2DW12::e2_g); acce.setDataRate(DFRobot_LIS2DW12::eRate_100hz); acce.setPowerMode(DFRobot_LIS2DW12::eHighPerformance_14bit); }
  
  lcd.init(); lcd.backlight(); lcd.print("System Starting...");

  if(axp.begin() == 0) { axp.enableCameraPower(axp.eOV2640); delay(1000); }
  Wire.end(); delay(100);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM; config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM; config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM; config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM; config.pin_sscb_sda = SIOD_GPIO_NUM; config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG; config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM; config.jpeg_quality = 12; config.fb_count = 1;

  if(psramFound()){ config.jpeg_quality = 10; config.fb_count = 2; config.grab_mode = CAMERA_GRAB_LATEST; }
  else { config.frame_size = FRAMESIZE_SVGA; config.fb_location = CAMERA_FB_IN_DRAM; }
  
  esp_camera_init(&config);

  delay(200); Wire.begin(SIOD_GPIO_NUM, SIOC_GPIO_NUM); delay(200);
  if(acceOk) acce.begin();
  
  GPSserial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  pinMode(PIN_LED_RED, OUTPUT); pinMode(PIN_LED_GREEN, OUTPUT); pinMode(PIN_BUZZER, OUTPUT); pinMode(PIN_BUTTON, INPUT_PULLUP);
  dht.begin(); connectWiFi(); startCameraServer(); connectMQTT(); lcd.clear();
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) connectWiFi();
  if (!mqtt.connected()) connectMQTT();
  mqtt.loop();
  readGps();

  unsigned long now = millis();
  if (now - lastSensorsMs > 100) {
    lastSensorsMs = now;
    if(acceOk) {
      float x = acce.readAccX()/1000.0, y = acce.readAccY()/1000.0, z = acce.readAccZ()/1000.0;
      lastAx=x; lastAy=y; lastAz=z;
      lastTiltDeg = atan(sqrt(pow(x, 2) + pow(y, 2)) / z) * 180 / PI;
      if (lastTiltDeg < 0) lastTiltDeg = 180 + lastTiltDeg;
    }
    lastMq3Raw = analogRead(PIN_MQ3_ADC);
    updateAlarmLogic();
  }

  if (now - lastTelemetryMs > 2000) {
    lastTelemetryMs = now;
    float h = dht.readHumidity(); float t = dht.readTemperature();
    if(!isnan(h)) lastHum=h; if(!isnan(t)) lastTemp=t;
    publishTelemetry();
  }

  if(now - lastLcdUpdateMs > 1000) { lastLcdUpdateMs = now; updateLcd(); }
  checkButton();

  if(alarmState != ALARM_IDLE) { digitalWrite(PIN_LED_RED, HIGH); digitalWrite(PIN_LED_GREEN, LOW); }
  else { digitalWrite(PIN_LED_RED, LOW); digitalWrite(PIN_LED_GREEN, HIGH); }
}