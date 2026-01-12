#include "WiFi.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <OSCMessage.h>
#include <WiFiUdp.h>
#include <Wire.h>

WiFiUDP outUDP;
WiFiUDP inUDP;

const char *ssid = "ESP32-AP";
const char *password = "12345678";
const IPAddress dstIP(192, 168, 4, 2);

const unsigned int outPort = 9999;
const unsigned int inPort = 6666;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3D

const int fsrPin = A2;          
const int waterPin = 37;         
const int firePin = 32;          
const int buttonPinJoyStick = 14;
const int joyStickXPin = A3;     
const int joyStickYPin = A4;    

// Buttons - alle verschiedene Pins!
const int moveLeftBlueButton =  12; //13; 27
const int moveRightYellowButton =  13; //27; 13
const int moveBackGreenButton = 33; //12;  33
const int moveForwardWhiteButton = 27;  //33; 12

// Status LED auf einen anderen Pin
const int statusLedPin = 15;           
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatTimeout = 1000;
int ledState = LOW;

// Timing
const unsigned long sensorInterval = 62; // ~16 times per second (1000/62 ≈ 16)
unsigned long lastSensorTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 500; // Update display every 500ms

String game = "elias";

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// OSC Sende-Funktionen
void sendFSR(int fsr)
{
  OSCMessage msg("/fsr");
  msg.add(fsr);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}

void sendWater(int water)
{
  OSCMessage msg("/water");
  msg.add(water);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}


void sendFire(int fire)
{
  OSCMessage msg("/fire");
  msg.add(fire);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}


void sendJoystick(float x, float y, int button)
{
  OSCMessage msg("/joystick");
  msg.add(x);
  msg.add(y);
  msg.add(button);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}

void sendMove(int left, int right, int forward, int back)
{
  OSCMessage msg("/move");
  msg.add(left);
  msg.add(right);
  msg.add(forward);
  msg.add(back);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}

void onHeartbeat()
{
  lastHeartbeat = millis();
  ledState = HIGH;
}

void setup()
{
  Serial.begin(115200);

  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3D))
  {
    Serial.println("SSD1306 allocation failed");
    while (true);
  }

  // WiFi Setup
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);
  Serial.print("AP IP address: ");
  Serial.println(WiFi.softAPIP());

  // UDP Setup
  outUDP.begin(outPort);
  inUDP.begin(inPort);

  // Pin Setup
  pinMode(firePin, INPUT);
  pinMode(fsrPin, INPUT);
  pinMode(waterPin, INPUT);
  pinMode(buttonPinJoyStick, INPUT_PULLUP);
  pinMode(joyStickXPin, INPUT);
  pinMode(joyStickYPin, INPUT);
  pinMode(moveLeftBlueButton, INPUT_PULLUP);
  pinMode(moveRightYellowButton, INPUT_PULLUP);
  pinMode(moveForwardWhiteButton, INPUT_PULLUP);
  pinMode(moveBackGreenButton, INPUT_PULLUP);
  pinMode(statusLedPin, OUTPUT);

  // Display initial setup
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.display();
}

void loopElias()
{

  unsigned long currentMillis = millis();

  // 1. Check for incoming OSC messages
  int packetSize = inUDP.parsePacket();
  if (packetSize > 0)
  {
    OSCMessage msg;
    while (packetSize--)
    {
      msg.fill(inUDP.read());
    }

    if (!msg.hasError())
    {
      if (msg.fullMatch("/heartbeat"))
      {
        onHeartbeat();
      }
    }
  }

  if (millis() - lastHeartbeat > heartbeatTimeout)
  {
    ledState = LOW;
  }

  digitalWrite(statusLedPin, ledState);

  // 2. Read and send sensor data at ~16Hz
  if (currentMillis - lastSensorTime >= sensorInterval)
  {
    lastSensorTime = currentMillis;

    // Read all sensors
    int fsrReading = analogRead(fsrPin);
    int waterReading = analogRead(waterPin);
    int fireReading = analogRead(firePin);
    Serial.println("fire " + String(fireReading));
   

    // Joystick
    float rawJoyX = analogRead(joyStickXPin);
    float rawJoyY = analogRead(joyStickYPin);

    // Normalisiere auf -1.0 bis +1.0
    float joyX = (rawJoyX - 2047.5) / 2047.5; // 4095/2 = 2047.5
    float joyY = (rawJoyY - 2047.5) / 2047.5;
    int joyButton = digitalRead(buttonPinJoyStick);


    // Send all data via OSC
    sendFSR(fsrReading);
    sendWater(waterReading);
    sendFire(fireReading);
    sendJoystick(joyX, joyY, joyButton);
  }

  // 3. Update display less frequently (every 500ms)
  if (currentMillis - lastDisplayUpdate >= displayInterval)
  {
    lastDisplayUpdate = currentMillis;

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Unity Controller");
    oled.print("IP: ");
    oled.println(WiFi.softAPIP());
    oled.print("Clients: ");
    oled.println(WiFi.softAPgetStationNum());
    oled.display();
  }

  int moveLeft = digitalRead(moveLeftBlueButton) == LOW;
  int moveRight = digitalRead(moveRightYellowButton) == LOW;
  int moveFwd = digitalRead(moveForwardWhiteButton) == LOW;
  int moveBack = digitalRead(moveBackGreenButton) == LOW;

  sendMove(moveLeft, moveRight, moveFwd, moveBack);
}

void loopAndrea() {

};

void loop()
{

  if (game == "elias")
  {
    loopElias();
  }
  else
  {
    loopAndrea();
  }
}