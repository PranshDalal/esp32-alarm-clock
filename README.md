# ESP32 Alarm Clock

A Wi-Fi alarm clock built on an ESP32. It gets the time from NTP, with automatic US Pacific daylight-saving changes, shows it on a 128×64 OLED, and lets you set the alarm with two buttons. The alarm is saved in flash, so it survives power cuts. It runs on a custom two-layer PCB I designed in KiCad.

## Features

- **NTP time** from `pool.ntp.org` / `time.nist.gov`, with the time zone `PST8PDT,M3.2.0,M11.1.0`
- **Two-button interface:**
  - SET steps through hour → minute → save
  - \+ changes the value, or toggles the alarm on/off from the clock screen
  - Hold + to auto-repeat (after 500 ms, then every 120 ms)
- **Debounced buttons** (30 ms), timed with `millis()`
- **Alarm saved** in ESP32 flash (NVS) using `Preferences`
- **Ringing:** SET stops it, + snoozes for 5 minutes, and it stops by itself after 3 minutes
- **Rings exactly once per alarm minute.** The firing minute is stamped, so stopping the alarm can't re-trigger it within the same minute.

## How it works

```
NTP servers ──Wi-Fi──► ESP32 ──I2C (GPIO 21/22)──► SSD1306 OLED
                        │ ▲
          GPIO 18 ──────┘ └────── GPIO 19 / 23
          Buzzer                   SET / + buttons
                        │
                        ▼
                  NVS flash (saved alarm)
```

- **Timekeeping:** on boot the ESP32 joins Wi-Fi and calls `configTzTime()`. After that, the ESP32's clock keeps time and NTP keeps it in sync, including daylight-saving changes.
- **Main loop:** timed with `millis()`. Each pass reads and debounces the buttons, updates the menu state, redraws the OLED, and checks whether the current minute matches the alarm.

## Hardware

| Part | Connection |
|---|---|
| ESP32 dev board (on 2× 1×19 pin sockets) | n/a |
| SSD1306 128×64 I2C OLED (address `0x3C`) | SDA → GPIO 21, SCL → GPIO 22 |
| SET button | GPIO 19 (internal pull-up, active low) |
| \+ button | GPIO 23 (internal pull-up, active low) |
| Buzzer | GPIO 18 (active low; change `BUZZER_ACTIVE_LOW` if yours is active high) |

The PCB has headers for the OLED (4-pin) and buzzer (3-pin), screw terminals for the two buttons, and four M3 mounting holes.

## Repository layout

```
firmware/esp_alarm_clock/
  esp_alarm_clock.ino     Arduino sketch
pcb/
  alarm_clock_pcb.kicad_pro / .kicad_sch / .kicad_pcb   KiCad 10 project
  gerbers/                Fabrication files (Gerbers + drill)
  alarm_clock_pcb.step    3D model of the board
  *.pdf                   Copper and board-outline plots
```

## Getting started

### Firmware

1. Install the **ESP32 board package** in the Arduino IDE (Boards Manager → "esp32" by Espressif).
2. Install the libraries **Adafruit SSD1306** and **Adafruit GFX** (Library Manager).
3. Create `firmware/esp_alarm_clock/secrets.h` with your Wi-Fi details. The file is git-ignored.
   ```cpp
   #pragma once
   const char* WIFI_SSID = "your-network";
   const char* WIFI_PASS = "your-password";
   ```
4. Open `esp_alarm_clock.ino`, select your ESP32 board and port, and upload.

To change the time zone, edit `TIMEZONE` near the top of the sketch. It takes a [POSIX TZ string](https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv).

### PCB

Zip the contents of `pcb/gerbers/` and upload the zip to a PCB fab (JLCPCB, PCBWay, OSH Park and others). The board is two layers, 1.6 mm thick.

## Tech stack

**Firmware:** Arduino (C++), Espressif arduino-esp32 core (`WiFi`, `Preferences`, SNTP), Adafruit SSD1306, Adafruit GFX
**PCB:** KiCad 10

## Credits

- **Adafruit SSD1306** and **Adafruit GFX** libraries by Adafruit (BSD License).
- Schematic symbols and footprints from KiCad's standard libraries.

## AI disclosure

AI assistance (Claude) was used in this project for documentation, testing, and debugging.
