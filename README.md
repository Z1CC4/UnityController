# UnityController

Firmware for an **Adafruit ESP32 Feather V2** that turns physical sensors and buttons into a wireless game controller for Unity.

Companion Unity project: [ProjectPPIE](https://github.com/Z1CC4/ProjectPPIE).

University project — Prototyping Physical Interactive Experiences, Free University of Bozen-Bolzano.

## What it does

The board opens a Wi-Fi access point and talks to Unity over **OSC** (Open Sound Control) on UDP. It sends live sensor values and movement buttons, and can show status on an SSD1306 OLED (minimap, water/fire state, connection).

Typical mapping:

| Hardware | OSC address | Role |
|----------|-------------|------|
| FSR (force) | `/fsr` | analogue pressure |
| Water sensor | `/water` | kitchen “water” interaction |
| Fire / flame sensor | `/fire` | kitchen “fire” interaction |
| Joystick X/Y + click | `/joystick` | look / extra input |
| Four move buttons | `/move` | WASD-style movement |
| Incoming `/heartbeat` | `/heartbeat_ack` | link watchdog |
| Incoming `/game` | — | selects display mode (`Andrea` / `elias`) |
| Incoming `/map/*`, `/state/*` | — | minimap and status from Unity |

## Hardware

- Adafruit Feather ESP32 V2 (`adafruit_feather_esp32_v2` in PlatformIO)
- SSD1306 OLED (I2C, address `0x3D` in the current firmware)
- FSR, water and fire sensors, analog joystick, four directional buttons, status LED

The board creates AP `ESP32-AP` (default password in firmware is for this local AP only). Unity should join that network and exchange OSC on ports **9999** (ESP → PC) and **6666** (PC → ESP).

## Build

[PlatformIO](https://platformio.org/) project:

```bash
git clone https://github.com/Z1CC4/UnityController.git
cd UnityController
pio run --target upload
pio device monitor
```

Libraries: Adafruit SSD1306 / GFX / BusIO, CNMAT OSC.

## Pairing with Unity

1. Flash this firmware and power the Feather.
2. Connect the PC to the ESP32 access point.
3. Open [ProjectPPIE](https://github.com/Z1CC4/ProjectPPIE) and start the scene that uses `ESP32OSCReceiver` / `ESP32MoveBridge`.
4. The OLED should leave the “No game connected” screen once Unity sends `/game`.

## Author

Andrea Zicarelli ([@Z1CC4](https://github.com/Z1CC4)), with collaborative display/input work in the same unibz PPIE course.
