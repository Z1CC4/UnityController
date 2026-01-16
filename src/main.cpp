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

String game = "andrea";

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Simple OLED animations (Andrea mode only) ---
// Monochrome, drawn with Adafruit_GFX primitives (no bitmaps needed).
static void drawFaucetAnim(int x, int y, uint8_t frame)
{
  // Bounding box: ~56x36 (fits well on 128x64)
  // Faucet body (simple stylized shape)
  oled.drawRoundRect(x + 6, y + 8, 26, 12, 2, SSD1306_WHITE);   // main body
  oled.fillRect(x + 10, y + 18, 10, 6, SSD1306_WHITE);         // base block

  // Vertical pipe + top cap
  oled.fillRect(x + 14, y + 2, 6, 6, SSD1306_WHITE);
  oled.drawRect(x + 12, y + 0, 10, 4, SSD1306_WHITE);

  // Spout to the right with a little down nozzle
  oled.fillRect(x + 30, y + 10, 16, 5, SSD1306_WHITE);
  oled.fillRect(x + 44, y + 12, 4, 10, SSD1306_WHITE);
  oled.drawRect(x + 42, y + 11, 8, 12, SSD1306_WHITE);

  // Water drops (animated)
  // frame 0/1/2 shifts drops downward to simulate flow
  const int dy = (frame % 3) * 3;
  const int dx = (frame % 2) ? 1 : 0;
  oled.fillCircle(x + 46 + dx, y + 26 + dy, 2, SSD1306_WHITE);
  oled.fillCircle(x + 42 - dx, y + 32 + ((dy + 2) % 9), 1, SSD1306_WHITE);
  oled.fillCircle(x + 49, y + 35 + ((dy + 4) % 9), 1, SSD1306_WHITE);
}

static void drawFlameAnim(int x, int y, uint8_t frame)
{
  // Bounding box: ~48x40
  // Flicker by slightly shifting the top point left/right and changing height.
  const int shift = (frame == 1) ? -2 : (frame == 2) ? 2 : 0;
  const int topY = y + ((frame == 1) ? 2 : (frame == 2) ? 0 : 1);

  // Outer flame (filled)
  oled.fillTriangle(x + 24 + shift, topY, x + 6, y + 28, x + 42, y + 28, SSD1306_WHITE);
  oled.fillTriangle(x + 24 - shift, topY + 10, x + 10, y + 34, x + 38, y + 34, SSD1306_WHITE);

  // Cut a small inner hollow to create "two-tone" feel (erase with BLACK)
  oled.fillTriangle(x + 24 + shift, topY + 12, x + 16, y + 28, x + 32, y + 28, SSD1306_BLACK);

  // Little ember/spark (optional subtle animation)
  if (frame == 2)
  {
    oled.drawPixel(x + 10, y + 10, SSD1306_WHITE);
    oled.drawPixel(x + 38, y + 12, SSD1306_WHITE);
  }
}

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
  unsigned long currentMillis = millis();
  // Persisted binary states for display + OSC
  static int waterState = 0;
  static int fireState = 0;
  // Local display timing (so we don't interfere with loopElias timing vars)
  static unsigned long lastDisplayUpdateAndrea = 0;
  static uint8_t faucetFrame = 0;
  static uint8_t flameFrame = 0;

  // 1. Check for incoming OSC messages (heartbeat from Unity)
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

  // Connection/LED state (keep behavior identical to Elias)
  if (millis() - lastHeartbeat > heartbeatTimeout)
  {
    ledState = LOW;
  }
  digitalWrite(statusLedPin, ledState);

  // 2. Read and send sensor data continuously at ~16Hz
  if (currentMillis - lastSensorTime >= sensorInterval)
  {
    lastSensorTime = currentMillis;

    // Raw reads (0..4095)
    const int fsrReading = analogRead(fsrPin);
    const int waterRaw = analogRead(waterPin);
    const int fireRaw = analogRead(firePin);

    // If your sensor is ACTIVE-LOW (common with INPUT_PULLUP-style wiring),
    // set these to true to invert the raw value.
    constexpr bool WATER_ACTIVE_LOW = false;
    constexpr bool FIRE_ACTIVE_LOW = true;
    const int waterEffRaw = WATER_ACTIVE_LOW ? (4095 - waterRaw) : waterRaw;
    const int fireEffRaw = FIRE_ACTIVE_LOW ? (4095 - fireRaw) : fireRaw;

    // Threshold + hysteresis to convert to ON/OFF (0/1)
    // Tune these values if needed (TH_OFF < TH_ON to avoid flicker).
    constexpr int WATER_TH_ON = 2500;
    constexpr int WATER_TH_OFF = 2200;
    constexpr int FIRE_TH_ON = 2500;
    constexpr int FIRE_TH_OFF = 2200;

    auto hysteresis01 = [](int raw, int prevState, int thOn, int thOff) -> int {
      if (prevState == 0)
      {
        return (raw > thOn) ? 1 : 0;
      }
      return (raw < thOff) ? 0 : 1;
    };

    waterState = hysteresis01(waterEffRaw, waterState, WATER_TH_ON, WATER_TH_OFF);
    fireState = hysteresis01(fireEffRaw, fireState, FIRE_TH_ON, FIRE_TH_OFF);

    // Unity contract (match Elias):
    // - /water: single int 0..4095 (raw)
    // - /fire:  single int 0..4095 (raw)
    // - /fsr:   single int 0..4095 (raw)
    // Note: waterState/fireState are still kept for OLED animations.
    sendWater(waterRaw);
    sendFire(fireRaw);
    sendFSR(fsrReading);

    // Joystick (same contract as Elias):
    // - /joystick: float x (-1..+1), float y (-1..+1), int button (PULLUP: 1=not pressed, 0=pressed)
    const float rawJoyX = (float)analogRead(joyStickXPin);
    const float rawJoyY = (float)analogRead(joyStickYPin);
    const float joyX = (rawJoyX - 2047.5f) / 2047.5f; // 4095/2 = 2047.5
    const float joyY = (rawJoyY - 2047.5f) / 2047.5f;
    const int joyButton = digitalRead(buttonPinJoyStick);
    sendJoystick(joyX, joyY, joyButton);
  }

  // 3. Display:
  // - idle: keep the old 500ms refresh
  // - when water/fire active: refresh faster to animate
  constexpr unsigned long animInterval = 150; // ~6-7 FPS
  const bool showWater = (waterState == 1);
  const bool showFire = (fireState == 1);
  const unsigned long wantedInterval = (showWater || showFire) ? animInterval : displayInterval;

  if (currentMillis - lastDisplayUpdateAndrea >= wantedInterval)
  {
    lastDisplayUpdateAndrea = currentMillis;
    faucetFrame = (uint8_t)((faucetFrame + 1) % 3);
    flameFrame = (uint8_t)((flameFrame + 1) % 3);

    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("Unity Controller");

    // Footer info (kept, but compact)
    oled.setCursor(0, 56);
    oled.print("IP ");
    oled.print(WiFi.softAPIP());
    oled.print(" C:");
    oled.print(WiFi.softAPgetStationNum());

    // Status line
    oled.setCursor(0, 10);
    oled.print("Water:");
    oled.print(showWater ? "ON " : "OFF");
    oled.print(" Fire:");
    oled.print(showFire ? "ON" : "OFF");

    // Animations area
    if (showWater || showFire)
    {
      if (showWater && showFire)
      {
        // Both: faucet left, flame right
        drawFaucetAnim(2, 18, faucetFrame);
        drawFlameAnim(72, 18, flameFrame);
      }
      else if (showWater)
      {
        // Center faucet
        drawFaucetAnim(36, 18, faucetFrame);
      }
      else // showFire
      {
        // Center flame
        drawFlameAnim(40, 18, flameFrame);
      }
    }
    oled.display();
  }
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