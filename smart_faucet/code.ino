#include <ESP8266WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------- OLED ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- WiFi ----------
const char* ssid = "SSID";
const char* password = "SSID_password";

// ---------- ThingSpeak ----------
const char* server = "api.thingspeak.com";
String apiKey = "API_key";
WiFiClient client;

// ---------- Pins ----------
#define IR_SENSOR   14   // D5
#define RELAY_PIN   13   // D7
#define TDS_PIN     A0

#define TDS_THRESHOLD 300

float tdsValue = 0;
bool tapStatus = false;
bool previousTapStatus = false;

unsigned long lastTDSUpdate = 0;

// ---------- Function to send data ----------
void sendToThingSpeak(float tds, bool status)
{
  if (WiFi.status() == WL_CONNECTED && client.connect(server, 80)) {
    String postStr = "api_key=" + apiKey;
    postStr += "&field1=" + String(tds);
    postStr += "&field2=" + String(status ? 1 : 0);
    postStr += "\r\n\r\n";

    client.print("POST /update HTTP/1.1\r\n");
    client.print("Host: api.thingspeak.com\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Type: application/x-www-form-urlencoded\r\n");
    client.print("Content-Length: ");
    client.print(postStr.length());
    client.print("\r\n\r\n");
    client.print(postStr);
  }
  client.stop();
}

// ---------- Function to update OLED ----------
void updateDisplay(float tds, String line2)
{
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("TDS: ");
  display.print((int)tds);
  display.print(" ppm");

  display.setCursor(0, 16);
  display.print(line2);

  display.display();
}

void setup() {
  pinMode(IR_SENSOR, INPUT);
  pinMode(RELAY_PIN, OUTPUT);

  // Active LOW relay -> HIGH means OFF
  digitalWrite(RELAY_PIN, HIGH);

  Serial.begin(9600);

  // I2C for OLED: SDA = D2(GPIO4), SCL = D1(GPIO5)
  Wire.begin(4, 5);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    Serial.println("OLED not found");
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Connecting WiFi");
  display.display();

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  display.clearDisplay();
  display.setCursor(0, 0);
  display.println("WiFi Connected");
  display.display();
  delay(1500);
}

void loop() {
  // ----------- TDS Reading -----------
  int adcValue = analogRead(TDS_PIN);
  float voltage = adcValue * (3.3 / 1023.0);
  tdsValue = (voltage * 1000.0) / 2.0;

  tapStatus = false;

  // ----------- Safety Check -----------
  if (tdsValue > TDS_THRESHOLD) {
    digitalWrite(RELAY_PIN, HIGH);   // Pump OFF
    updateDisplay(tdsValue, "Water: UNSAFE");
  } else {
    int handDetected = digitalRead(IR_SENSOR);

    if (handDetected == LOW) {
      digitalWrite(RELAY_PIN, LOW);  // Pump ON
      tapStatus = true;
      updateDisplay(tdsValue, "Water Flowing");
    } else {
      digitalWrite(RELAY_PIN, HIGH); // Pump OFF
      updateDisplay(tdsValue, "Place Hand");
    }
  }

  // ----------- Immediate Tap Status Update -----------
  if (tapStatus != previousTapStatus) {
    sendToThingSpeak(tdsValue, tapStatus);
    previousTapStatus = tapStatus;
  }

  // ----------- Periodic TDS Update -----------
  if (millis() - lastTDSUpdate > 15000) {
    lastTDSUpdate = millis();
    sendToThingSpeak(tdsValue, tapStatus);
  }

  delay(300);
}
