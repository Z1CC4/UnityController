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

const int moveLeftButton = 12;
const int moveRightButton = 13;
const int moveBackButton = 33;
const int moveForwardButton = 27;


const int statusLedPin = 15;
unsigned long lastHeartbeat = 0;
const unsigned long heartbeatTimeout = 1000;
int ledState = LOW;


const unsigned long sensorInterval = 62; // ~16 times per second (1000/62 ≈ 16)
unsigned long lastSensorTime = 0;
unsigned long lastDisplayUpdate = 0;
const unsigned long displayInterval = 500; // Update display every 500ms

String game;

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- Move buttons (WASD-like) ---
constexpr unsigned long kMoveButtonDebounceMs = 25;
constexpr unsigned long kMoveResendIntervalMs = 80; 


struct DebouncedMoveButton
{
  uint8_t pin;
  bool stablePressed;  // debounced state (true = pressed)
  bool lastRawPressed; // last raw read
  unsigned long lastEdgeMs;
};

static bool updateDebounced(DebouncedMoveButton &b, unsigned long nowMs)
{
  const bool rawPressed = (digitalRead(b.pin) == LOW);
  bool changed = false;

  if (rawPressed != b.lastRawPressed)
  {
    b.lastRawPressed = rawPressed;
    b.lastEdgeMs = nowMs;
  }

  if ((nowMs - b.lastEdgeMs) >= kMoveButtonDebounceMs && b.stablePressed != rawPressed)
  {
    b.stablePressed = rawPressed;
    changed = true;
  }
  return changed;
}

static void handleMoveButtons(unsigned long nowMs)
{
  static bool inited = false;
  static unsigned long lastSendMs = 0;

  static DebouncedMoveButton left{(uint8_t)moveLeftButton, false, false, 0};
  static DebouncedMoveButton right{(uint8_t)moveRightButton, false, false, 0};
  static DebouncedMoveButton forward{(uint8_t)moveForwardButton, false, false, 0};
  static DebouncedMoveButton back{(uint8_t)moveBackButton, false, false, 0};

  if (!inited)
  {
    left.lastRawPressed = left.stablePressed = (digitalRead(left.pin) == LOW);
    right.lastRawPressed = right.stablePressed = (digitalRead(right.pin) == LOW);
    forward.lastRawPressed = forward.stablePressed = (digitalRead(forward.pin) == LOW);
    back.lastRawPressed = back.stablePressed = (digitalRead(back.pin) == LOW);

    left.lastEdgeMs = right.lastEdgeMs = forward.lastEdgeMs = back.lastEdgeMs = nowMs;
    lastSendMs = 0;
    inited = true;

    sendMove((int)left.stablePressed, (int)right.stablePressed, (int)forward.stablePressed, (int)back.stablePressed);
    lastSendMs = nowMs;
    return;
  }

  const bool changed =
      updateDebounced(left, nowMs) |
      updateDebounced(right, nowMs) |
      updateDebounced(forward, nowMs) |
      updateDebounced(back, nowMs);

  if (changed || (nowMs - lastSendMs) >= kMoveResendIntervalMs)
  {
    sendMove((int)left.stablePressed, (int)right.stablePressed, (int)forward.stablePressed, (int)back.stablePressed);
    lastSendMs = nowMs;
  }
}

// --- Display animations --- Game Andrea ---
static void drawFaucetAnim(int x, int y, uint8_t frame)
{
  oled.drawRoundRect(x + 6, y + 8, 26, 12, 2, SSD1306_WHITE); 
  oled.fillRect(x + 10, y + 18, 10, 6, SSD1306_WHITE);        

  oled.fillRect(x + 14, y + 2, 6, 6, SSD1306_WHITE);
  oled.drawRect(x + 12, y + 0, 10, 4, SSD1306_WHITE);

  oled.fillRect(x + 30, y + 10, 16, 5, SSD1306_WHITE);
  oled.fillRect(x + 44, y + 12, 4, 10, SSD1306_WHITE);
  oled.drawRect(x + 42, y + 11, 8, 12, SSD1306_WHITE);

  const int dy = (frame % 3) * 3;
  const int dx = (frame % 2) ? 1 : 0;
  oled.fillCircle(x + 46 + dx, y + 26 + dy, 2, SSD1306_WHITE);
  oled.fillCircle(x + 42 - dx, y + 32 + ((dy + 2) % 9), 1, SSD1306_WHITE);
  oled.fillCircle(x + 49, y + 35 + ((dy + 4) % 9), 1, SSD1306_WHITE);
}

static void drawFlameAnim(int x, int y, uint8_t frame)
{
  const int shift = (frame == 1) ? -2 : (frame == 2) ? 2
                                                     : 0;
  const int topY = y + ((frame == 1) ? 2 : (frame == 2) ? 0
                                                        : 1);

  oled.fillTriangle(x + 24 + shift, topY, x + 6, y + 28, x + 42, y + 28, SSD1306_WHITE);
  oled.fillTriangle(x + 24 - shift, topY + 10, x + 10, y + 34, x + 38, y + 34, SSD1306_WHITE);

  oled.fillTriangle(x + 24 + shift, topY + 12, x + 16, y + 28, x + 32, y + 28, SSD1306_BLACK);

  if (frame == 2)
  {
    oled.drawPixel(x + 10, y + 10, SSD1306_WHITE);
    oled.drawPixel(x + 38, y + 12, SSD1306_WHITE);
  }
}

// --- Display logic --- Game Elias ---

// --- Minimap data (from Unity) ---
float g_px = 0, g_pz = 0, g_yaw = 0;
float g_hx = 0, g_hz = 0;
int g_stateWater = 0;
int g_stateFire = 0;

// map drawing helpers
static void drawPlayerCircle(int cx, int cy)
{
  oled.fillCircle(cx, cy, 2, SSD1306_WHITE);
  oled.drawCircle(cx, cy, 2, SSD1306_WHITE); 
}


static int worldToMini(float d, float range, int halfSize)
{
  // clamp to [-range, +range], map to [-halfSize, +halfSize]
  if (d > range)
    d = range;
  if (d < -range)
    d = -range;
  float k = (float)halfSize / range;
  return (int)(d * k);
}

static void drawMinimap64()
{
  const int mapX = 0, mapY = 0;
  const int mapS = 64;
  const float worldRange = 100.0f; // show 100m x 100m world (handles coordinates from -50 to +50)

  // Helper to convert world coordinates to screen coordinates
  auto worldToScreen = [&](float wx) -> int
  {
    // Map from [-worldRange/2, +worldRange/2] to [0, mapS]
    float screenX = ((wx + worldRange / 2.0f) / worldRange) * mapS;
    return (int)screenX;
  };

  auto worldToScreenZ = [&](float wz) -> int
  {
    float screenZ = ((wz + worldRange / 2.0f) / worldRange) * mapS;
    return (int)screenZ;
  };

  // border
  oled.drawRect(mapX, mapY, mapS, mapS, SSD1306_WHITE);

  // House (fixed position on map)
  int hx = worldToScreen(g_hx);
  int hz = worldToScreenZ(g_hz);
  oled.drawRect(hx - 3, hz - 3, 6, 6, SSD1306_WHITE);

  // Player (moves on map)
  int px = worldToScreen(g_px);
  int pz = worldToScreenZ(g_pz);
  drawPlayerCircle(px, pz);
}

// right part of display drawing helpers
static void drawWaterDropIcon(int x, int y)
{
  // Water drop shape - oval on top, point on bottom
  oled.drawCircle(x + 11, y + 14, 4, SSD1306_WHITE);                               // top circle
  oled.fillTriangle(x + 7, y + 16, x + 15, y + 16, x + 11, y + 22, SSD1306_WHITE); // bottom point
  oled.fillCircle(x + 11, y + 14, 3, SSD1306_WHITE);                               // fill top part
}

static void drawFlameIcon(int x, int y)
{
  // Outer flame
  oled.fillTriangle(x + 16, y + 6, x + 8, y + 20, x + 24, y + 20, SSD1306_WHITE);
  oled.fillTriangle(x + 16, y + 12, x + 12, y + 18, x + 20, y + 18, SSD1306_BLACK);
  // Small spark
  oled.drawPixel(x + 6, y + 14, SSD1306_WHITE);
  oled.drawPixel(x + 26, y + 15, SSD1306_WHITE);
}

static void drawFlameIconDetailed(int x, int y, uint8_t frame)
{
  // Dynamic flame animation
  int wobble = (frame == 0) ? -1 : (frame == 2) ? 1
                                                : 0;
  int topShift = y + (frame == 1 ? 1 : 0);

  // Main flame body - outer
  oled.fillTriangle(x + 16 + wobble, topShift, x + 8, y + 18, x + 24, y + 18, SSD1306_WHITE);
  // Secondary flame
  oled.fillTriangle(x + 16 - wobble, topShift + 4, x + 10, y + 20, x + 22, y + 20, SSD1306_WHITE);
  // Inner hollow
  oled.fillTriangle(x + 16 + wobble, topShift + 6, x + 12, y + 16, x + 20, y + 16, SSD1306_BLACK);
  // Sparks on side
  if (frame > 0)
  {
    oled.drawPixel(x + 4 + frame, y + 10, SSD1306_WHITE);
    oled.drawPixel(x + 28 - frame, y + 12, SSD1306_WHITE);
  }
}

static void drawRightStatus(uint8_t frame)
{
  const int xCenter = 96; // Center of right half (70 + 64/2)
  const int yWater = 8;   // Water in upper half
  const int yFire = 40;   // Fire in lower half

  if (g_stateWater)
    drawFaucetAnim(xCenter - 18, yWater, frame % 3);
  else
  {
    drawWaterDropIcon(xCenter - 13, yWater);
  }

  if (g_stateFire)
    drawFlameIconDetailed(xCenter - 16, yFire, frame);
  else
  {
    drawFlameIcon(xCenter - 16, yFire);
  }
}

void drawDisplayElias(unsigned long currentMillis)
{
  static uint8_t uiFrame = 0;
  static unsigned long lastDisplayUpdateElias = 0;

  if (currentMillis - lastDisplayUpdateElias >= 120) // ~8 FPS
  {
    lastDisplayUpdateElias = currentMillis;
    uiFrame = (uiFrame + 1) % 3;

    oled.clearDisplay();
    drawMinimap64();
    drawRightStatus(uiFrame);
    oled.display();
  }
}

// OSC sending functions
void sendHeartbeatAck()
{
  OSCMessage msg("/heartbeat_ack");
  msg.add(1);
  outUDP.beginPacket(dstIP, outPort);
  msg.send(outUDP);
  outUDP.endPacket();
}

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
    while (true)
      ;
  }

  // WiFi Setup
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

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
  pinMode(moveLeftButton, INPUT_PULLUP);
  pinMode(moveRightButton, INPUT_PULLUP);
  pinMode(moveForwardButton, INPUT_PULLUP);
  pinMode(moveBackButton, INPUT_PULLUP);
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

  // display logic:
  drawDisplayElias(currentMillis);

  //  Read and send sensor data at ~16Hz
  if (currentMillis - lastSensorTime >= sensorInterval)
  {
    lastSensorTime = currentMillis;

    // Read all sensors
    int fsrReading = analogRead(fsrPin);
    int waterReading = analogRead(waterPin);
    int fireReading = analogRead(firePin);

    // Joystick
    float rawJoyX = analogRead(joyStickXPin);
    float rawJoyY = analogRead(joyStickYPin);

    // Normalize to -1.0 to +1.0
    float joyX = (rawJoyX - 2047.5) / 2047.5; // 4095/2 = 2047.5
    float joyY = (rawJoyY - 2047.5) / 2047.5;
    int joyButton = digitalRead(buttonPinJoyStick);

    // Send all data via OSC
    sendFSR(fsrReading);
    sendWater(waterReading);
    sendFire(fireReading);
    sendJoystick(joyX, joyY, joyButton);
  }
  // Move buttons (WASD-like)
  handleMoveButtons(currentMillis);
}

void loopAndrea()
{
  unsigned long currentMillis = millis();
  static int waterState = 0;
  static int fireState = 0;
  static unsigned long lastDisplayUpdateAndrea = 0;
  static uint8_t faucetFrame = 0;
  static uint8_t flameFrame = 0;

  if (millis() - lastHeartbeat > heartbeatTimeout)
  {
    ledState = LOW;
  }
  digitalWrite(statusLedPin, ledState);

  if (currentMillis - lastSensorTime >= sensorInterval)
  {
    lastSensorTime = currentMillis;

    const int fsrReading = analogRead(fsrPin);
    const int waterRaw = analogRead(waterPin);
    const int fireRaw = analogRead(firePin);

    static int waterBaseline = -1;
    if (waterBaseline < 0)
    {
      waterBaseline = waterRaw;
    }
    const int waterDiff = abs(waterRaw - waterBaseline);
    constexpr int WATER_DELTA_ON = 300;
    constexpr int WATER_DELTA_OFF = 200;
    if (waterState == 0)
    {
      waterBaseline = (waterBaseline * 15 + waterRaw) / 16;
      if (waterDiff > WATER_DELTA_ON)
      {
        waterState = 1;
      }
    }
    else
    {
      if (waterDiff < WATER_DELTA_OFF)
      {
        waterState = 0;
      }
    }

    constexpr bool FIRE_ACTIVE_LOW = true;
    const int fireEffRaw = FIRE_ACTIVE_LOW ? (4095 - fireRaw) : fireRaw;

    constexpr int FIRE_TH_ON = 2500;
    constexpr int FIRE_TH_OFF = 2200;

    auto hysteresis01 = [](int raw, int prevState, int thOn, int thOff) -> int
    {
      if (prevState == 0)
      {
        return (raw > thOn) ? 1 : 0;
      }
      return (raw < thOff) ? 0 : 1;
    };

    fireState = hysteresis01(fireEffRaw, fireState, FIRE_TH_ON, FIRE_TH_OFF);

    sendWater(waterRaw);
    sendFire(fireState);
    sendFSR(fsrReading);

    const float rawJoyX = (float)analogRead(joyStickXPin);
    const float rawJoyY = (float)analogRead(joyStickYPin);
    const float joyX = (rawJoyX - 2047.5f) / 2047.5f; // 4095/2 = 2047.5
    const float joyY = (rawJoyY - 2047.5f) / 2047.5f;
    const int joyButton = digitalRead(buttonPinJoyStick);
    sendJoystick(joyX, joyY, joyButton);
  }

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

    oled.setCursor(0, 56);
    oled.print("IP ");
    oled.print(WiFi.softAPIP());
    oled.print(" C:");
    oled.print(WiFi.softAPgetStationNum());

    oled.setCursor(0, 10);
    oled.print("Water:");
    oled.print(showWater ? "ON " : "OFF");
    oled.print(" Fire:");
    oled.print(showFire ? "ON" : "OFF");

    if (showWater || showFire)
    {
      if (showWater && showFire)
      {
        drawFaucetAnim(2, 18, faucetFrame);
        drawFlameAnim(72, 18, flameFrame);
      }
      else if (showWater)
      {
        drawFaucetAnim(36, 18, faucetFrame);
      }
      else // showFire
      {
        drawFlameAnim(40, 18, flameFrame);
      }
    }
    oled.display();
  }

  handleMoveButtons(currentMillis);
};

void handleIncomingOSC()
{
  int packetSize;
  while ((packetSize = inUDP.parsePacket()) > 0)
  {
    OSCMessage msg;
    while (packetSize--)
      msg.fill(inUDP.read());

    if (msg.hasError())
      continue;

    if (msg.fullMatch("/heartbeat"))
    {
      onHeartbeat();
      sendHeartbeatAck();
      continue;
    }

    if (msg.fullMatch("/game"))
    {
      if (msg.isString(0))
      {
        char gameName[32];
        msg.getString(0, gameName, sizeof(gameName));
        game = String(gameName);
      }
      continue;
    }

    // --- minimap / states ---
    if (msg.fullMatch("/map/player"))
    {
      g_px = msg.getFloat(0);
      g_pz = msg.getFloat(1);
      continue;
    }

    if (msg.fullMatch("/map/house"))
    {
      g_hx = msg.getFloat(0);
      g_hz = msg.getFloat(1);
      continue;
    }

    if (msg.fullMatch("/state/water"))
    {
      g_stateWater = msg.getInt(0);
      continue;
    }

    if (msg.fullMatch("/state/fire"))
    {
      g_stateFire = msg.getInt(0);
      continue;
    }
  }
}

void loop()
{
  handleIncomingOSC();

  if (millis() - lastHeartbeat > heartbeatTimeout)
  {
    ledState = LOW;
  }

  digitalWrite(statusLedPin, ledState);

  if (game == "elias")
  {
    loopElias();
  }
  else if (game == "Andrea")
  {
    loopAndrea();
  }
  else
  {
    oled.clearDisplay();
    oled.setCursor(0, 0);
    oled.println("No game connected");
    oled.display();
  }
}