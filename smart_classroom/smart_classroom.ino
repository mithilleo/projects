#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ThingSpeak.h>

//  Pin Definitions 
#define SS_PIN 5
#define RST_PIN 4
#define LED_G 15
#define LED_R 2
#define MQ2_PIN 34
#define LM35_PIN 35
#define LDR_PIN 33
#define BUZZER_PIN 14
#define RELAY_LIGHT 12
#define RELAY_FAN 13

// RFID & LCD Setup 
MFRC522 rfid(SS_PIN, RST_PIN);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Teacher Card ID 
const String TEACHER_ID = "63F13929";
bool attendanceOpen = false;

//  Wi-Fi & ThingSpeak Setup
const char* ssid = "ssid";
const char* password = "password";
unsigned long myChannelNumber = "channel id";
const char* myWriteAPIKey = "API key";
WiFiClient client;      //object

//  Thresholds 
const int MQ2_THRESHOLD = 2000;     
const float TEMP_THRESHOLD = 30.0;  
const int LDR_THRESHOLD = 2000;     

// Setup 
void setup() {
  Serial.begin(115200);

  pinMode(LED_G, OUTPUT);
  pinMode(LED_R, OUTPUT);
  pinMode(MQ2_PIN, INPUT);
  pinMode(LM35_PIN, INPUT);
  pinMode(LDR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RELAY_LIGHT, OUTPUT);
  pinMode(RELAY_FAN, OUTPUT);

  digitalWrite(RELAY_LIGHT, LOW);
  digitalWrite(RELAY_FAN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  SPI.begin();
  rfid.PCD_Init();        //proximity coupling device
  lcd.init();
  lcd.backlight();

  lcd.setCursor(0, 0);
  lcd.print("RFID Attendance");
  lcd.setCursor(0, 1);
  lcd.print("Scan Teacher...");
  delay(2000);
  lcd.clear();

  //  Wi-Fi Setup
  WiFi.mode(WIFI_STA);      // station mode, connects to nearby wifi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {       //keyword from the wifi library
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  lcd.setCursor(0, 0);
  lcd.print("WiFi Connected");
  delay(1500);
  lcd.clear();

  ThingSpeak.begin(client);     //represents a network client connection
  Serial.println("System Ready");
  lcd.print("System Ready");
  delay(1000);
  lcd.clear();
}

// Main Loop 
void loop() {
  handleRFID();
  handleSensors();
  sendToThingSpeak();
}

// RFID Handling 
void handleRFID() {
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {     //Proximity Integrated Circuit Card
    String tagID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      //tagID += String(rfid.uid.uidByte[i] < 0x10 ? "0" : "");
      tagID += String(rfid.uid.uidByte[i], HEX);
    }
    tagID.toUpperCase();

    Serial.print("RFID Tag detected: ");
    Serial.println(tagID);

    lcd.clear();
    String statusMsg = "";

    // --- Teacher card logic ---
    if (tagID == TEACHER_ID) {
      if (!attendanceOpen) {
        attendanceOpen = true;
        lcd.setCursor(0, 0);
        lcd.print("Attendance Open");
        lcd.setCursor(0, 1);
        lcd.print("Scan Students");
        Serial.println("Attendance session started");
        statusMsg = "OPENED";
      } else {
        attendanceOpen = false;
        lcd.setCursor(0, 0);
        lcd.print("Attendance Closed");
        lcd.setCursor(0, 1);
        lcd.print("Session Ended");
        Serial.println("Attendance session closed");
        statusMsg = "CLOSED";
      }
    } 
    else {
      //  Student card logic 
      if (attendanceOpen) {
        lcd.setCursor(0, 0);
        lcd.print("Student Present:");
        lcd.setCursor(0, 1);
        lcd.print(tagID);
        Serial.print("Student present: ");
        Serial.println(tagID);
        digitalWrite(LED_G, HIGH);
        delay(1000);
        digitalWrite(LED_G, LOW);
        statusMsg = "PRESENT";
      } else {
        lcd.setCursor(0, 0);
        lcd.print("Attendance Closed");
        lcd.setCursor(0, 1);
        lcd.print("Wait Teacher");
        Serial.println("Tag ignored - attendance closed");
        digitalWrite(LED_R, HIGH);
        delay(1000);
        digitalWrite(LED_R, LOW);
        statusMsg = "DENIED";
      }
    }

    sendToThingSpeak(tagID, statusMsg);
    delay(1500);
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

// Sensor Handling 
void handleSensors() {
  int mq2Value = analogRead(MQ2_PIN);
  int ldrValue = analogRead(LDR_PIN);
  int lm35Value = analogRead(LM35_PIN);

  float temperature = (lm35Value / 4095.0) * 3.3 * 100.0;

  Serial.print("MQ2: "); Serial.print(mq2Value);
  Serial.print(" | LDR: "); Serial.print(ldrValue);
  Serial.print(" | Temp: "); Serial.print(temperature); Serial.println("°C");

  //Gas Alert 
  if (mq2Value > MQ2_THRESHOLD) {
    Serial.println("⚠ Gas Detected! Activating Buzzer!");
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Gas Alert!");
    lcd.setCursor(0, 1);
    lcd.print("Value: ");
    lcd.print(mq2Value);
    tone(BUZZER_PIN, 1000);
  } else {
    noTone(BUZZER_PIN);
  }

  // Light Automation
  if (ldrValue < LDR_THRESHOLD) {
    digitalWrite(RELAY_LIGHT, HIGH);
    Serial.println("💡 Lights ON (Dark detected)");
  } else {
    digitalWrite(RELAY_LIGHT, LOW);
  }

  //  Fan Control 
  if (temperature > TEMP_THRESHOLD) {
    digitalWrite(RELAY_FAN, HIGH);
    Serial.println("🌀 Fan ON (High temp)");
  } else {
    digitalWrite(RELAY_FAN, LOW);
  }

  //  LCD Display 
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print(temperature, 1);
  lcd.print("C G:");
  lcd.print(mq2Value);
  lcd.setCursor(0, 1);
  lcd.print("LDR:");
  lcd.print(ldrValue);
  delay(2000);

  // Upload Sensor Data 
  ThingSpeak.setField(4, mq2Value);
  ThingSpeak.setField(5, temperature);
  ThingSpeak.setField(6, ldrValue);

  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  if (result == 200)
    Serial.println("✅ Sensor data sent to ThingSpeak");
  else
    Serial.println("⚠ ThingSpeak update failed");
}

//  Send RFID Data to ThingSpeak 
void sendToThingSpeak(String tagID, String statusMsg) {
  /*if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, retrying...");
    WiFi.begin(ssid, password);
    delay(1000);
    return;
  }*/

  ThingSpeak.setField(1, tagID);
  ThingSpeak.setField(2, statusMsg);
  ThingSpeak.setField(3, attendanceOpen ? 1 : 0);

  int result = ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);

  if (result == 200)
    Serial.println("✅ RFID data sent to ThingSpeak");
  else
    Serial.println("⚠ RFID upload failed");
}
