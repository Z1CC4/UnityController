#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Arduino.h>
#include <Wire.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3D

const int fsrPin = A3;
const int waterPin = A2;
const int digitalFirePin = 15;
const int digitalKnockPin = 32;
const int buttonPinJoyStick = 13;
const int joyStickXPin = A5;
const int joyStickYPin = A4;
unsigned long lastKnock = 0;
const unsigned long debounce = 100;

Adafruit_SSD1306 oled(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

void setup() {
  Serial.begin(115200);

  // display works
  if (!oled.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 allocation failed");
    while (true) {
    } // stop when not found
  }
  Serial.println("SSD1306 initialized");
  pinMode(digitalFirePin, INPUT);
  pinMode(fsrPin, INPUT);
  pinMode(waterPin, INPUT);
  pinMode(digitalKnockPin, INPUT_PULLDOWN);
  pinMode(buttonPinJoyStick, INPUT_PULLUP);
  pinMode(joyStickXPin, INPUT);
  pinMode(joyStickYPin, INPUT);

  oled.clearDisplay();
  oled.display();
}

void loop() {
  // code display
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);
  oled.setCursor(0, 0);
  // Serial.println("Displaying Hello, world!");
  oled.print("Hello, world!");
  oled.display();

  // code fsr - works!
  int fsrReading = analogRead(fsrPin);
  // Serial.print("Analog reading = ");
  // Serial.println(fsrReading);
  // implement filter here

  // code water - works!
  int waterReading = analogRead(waterPin);
  // Serial.print("Water reading = ");
  // Serial.println(waterReading);
  // implement filter here

  // code fire - works!
  int digitalFireReading = digitalRead(digitalFirePin);
  // Serial.print("Digital Fire reading = ");
  // Serial.println(digitalFireReading);
  // would be also analogRead option - to implement and test if necessary
  // implement filter here (for analog reading)

  // code knock
  int reading = digitalRead(digitalKnockPin);
  unsigned long now = millis();

  if (reading == HIGH && (now - lastKnock > debounce)) {
    Serial.println("Klopfen erkannt");
    lastKnock = now;
  }
  // implement filter here

  // joy stick position
  float x, y;
  int Knopf;
  // Current values will be read and converted to the right voltage
  x = analogRead(joyStickXPin) * (5.0 / 1023.0);
  y = analogRead(joyStickYPin) * (5.0 / 1023.0);
  Knopf = digitalRead(buttonPinJoyStick);
  //... and outputted here
  // Serial.print("X-axis:");
  // Serial.print(x, 4);
  // Serial.print("V, ");
  // Serial.print("Y-axis:");
  // Serial.print(y, 4);
  // Serial.print("V, ");
  // Serial.print("Button:");
  if (Knopf == 1) {
    // Serial.println("not pushed");
  } else {
    // Serial.println("pushed");
  }

  delay(500);
}

/*
int led = 13; // define the LED pin
int digitalPin = 2; // KY-026 digital interface
int analogPin = A0; // KY-026 analog interface
int digitalVal; // digital readings
int analogVal; //analog readings

void setup()
{
  pinMode(led, OUTPUT);
  pinMode(digitalPin, INPUT);
  //pinMode(analogPin, OUTPUT);
  Serial.begin(9600);
}

void loop()
{
  // Read the digital interface
  digitalVal = digitalRead(digitalPin);
  if(digitalVal == HIGH) // if flame is detected
  {
    digitalWrite(led, HIGH); // turn ON Arduino's LED
  }
  else
  {
    digitalWrite(led, LOW); // turn OFF Arduino's LED
  }

  // Read the analog interface
  analogVal = analogRead(analogPin);
  Serial.println(analogVal); // print analog value to serial

  delay(100);
}
*/

/*
// Joy stick
// Declaration and initialization of the input pin
int JoyStick_X = A0; // X-axis-signal
int JoyStick_Y = A1; // Y-axis-signal
int Button = 3;
void setup ()
{
pinMode (JoyStick_X, INPUT);
pinMode (JoyStick_Y, INPUT);
pinMode (Button, INPUT);
// pushing the button leads to
// power up the pullup-resistor
digitalWrite(Button, HIGH);
Serial.begin (9600); // serial output with 9600 bps
}
// The program reads the current values of the input pins
// and outputs them at the serial output
void loop ()
{
float x, y;
int Knopf;
// Current values will be read and converted to the right voltage
x = analogRead (JoyStick_X) * (5.0 / 1023.0);
y = analogRead (JoyStick_Y) * (5.0 / 1023.0);
Knopf = digitalRead (Button);
//... and outputted here
Serial.print ("X-axis:"); Serial.print (x, 4); Serial.print ("V, ");
Serial.print ("Y-axis:"); Serial.print (y, 4); Serial.print ("V, ");
Serial.print ("Button:");
if(Knopf==1)
{
Serial.println ("not pushed");
}
else
{
Serial.println ("pushed");
}
delay (200);
}



*/