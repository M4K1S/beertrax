# 🍺 Beer Trax — ESP32 Flow Meter System

Beer Trax is an ESP32-based beverage flow tracking system that monitors up to **8 flow meters** simultaneously, logs data to an SD card, prints reports via a thermal printer, and exposes a local web UI for real-time monitoring and configuration.

---

## Features

- **8-channel flow metering** using the ESP32 hardware Pulse Counter (PCNT) peripheral
- **Real-time web dashboard** (served over Wi-Fi) for viewing totals and accessing admin controls
- **Thermal printer** support for end-of-day (EOD) reports
- **SD card logging** with autosave and timestamped EOD files
- **RTC (DS3231)** for accurate timekeeping and scheduled daily resets
- **Per-meter calibration** with support for mL or fl oz units
- **Licence validation** system with encrypted expiration dates
- **Admin panel** with password protection for configuration changes

---

## Hardware Requirements

| Component | Details |
|---|---|
| Microcontroller | ESP32 (any standard devkit) |
| RTC Module | DS3231 (I²C) |
| SD Card Module | SPI, CS on GPIO 15 |
| Thermal Printer | Adafruit-compatible, connected to Serial2 |
| Flow Sensors | Up to 8 pulse-output sensors |
| Power | 5V via USB or external supply |

### Flow Sensor GPIO Pins

| Meter | GPIO |
|---|---|
| Flow Meter 1 | 4 |
| Flow Meter 2 | 0 |
| Flow Meter 3 | 32 |
| Flow Meter 4 | 33 |
| Flow Meter 5 | 25 |
| Flow Meter 6 | 26 |
| Flow Meter 7 | 27 |
| Flow Meter 8 | 14 |

---

## Software Dependencies

Install the following libraries via the Arduino Library Manager or manually:

| Library | Purpose |
|---|---|
| `RTClib` | DS3231 RTC communication |
| `ESPAsyncWebSrv` | Async HTTP server |
| `Adafruit Thermal Printer` | Receipt printer control |
| `Vector` | Dynamic array utility |
| `SD` (built-in) | SD card read/write |
| `WiFi` (built-in ESP32) | Network connectivity |
| `esp_task_wdt` (built-in ESP32) | Watchdog timer |
| `driver/pcnt` (built-in ESP32) | Hardware pulse counter |

---

## Getting Started

### 1. Clone the repo

```bash
git clone https://github.com/YOUR_USERNAME/beer-trax.git
cd beer-trax
```

### 2. Open in Arduino IDE

Open `src/flow_meter.ino` in the Arduino IDE (2.x recommended). Make sure the **ESP32 board package** is installed.

### 3. Configure Wi-Fi

Wi-Fi credentials are stored on the SD card and loaded at boot via `readCredentials()`. On first use, the default credentials in the sketch are:

```cpp
String ssid     = "Ultra Fast Super Sped Wifi";
String password = "9DC23F461EEE";
```

Update these or use the admin web panel → **Change Wifi Credentials** to set them on the SD card.

### 4. Flash the device

Select your ESP32 board and COM port, then upload.

### 5. Access the web interface

After boot, the IP address is printed to Serial and on the thermal receipt. Navigate to it in a browser on the same network.

---

## Web Interface

### Main Menu

| Option | Action |
|---|---|
| `1` | Print current totals to thermal printer |
| `2` | Reprint newest EOD report |
| `3` | Browse SD card saves |
| `4` | Enter Admin Menu |

### Admin Menu (code required)

> Default admin code: `1234` — change this in the source before deploying.

| Option | Action |
|---|---|
| `1` | Update licence key |
| `2` | Change Wi-Fi credentials |
| `3` | Calibrate a sensor (1–8) |
| `4` | Reset individual totalizer |
| `5` | Reset all totalizers |
| `6` | Set RTC date/time |
| `7` | Set daily reset/print time |
| `8` | Rename flow meters |
| `9` | Toggle mL / fl oz units |
| `10` | Return to main page |

---

## SD Card File Structure

```
/
├── Autosave.txt          # Latest pulse counts (written every ~5 min)
├── credentials.txt       # Wi-Fi SSID and password
├── calibration.txt       # Per-meter calibration factors
├── flowMeterNames.txt    # Custom names for each meter
├── licence.txt           # Encrypted licence key
├── printTime.txt         # Scheduled EOD reset time (HH:MM:SS)
├── mlOZ.txt              # Unit preference
└── YYYY-MM-DD_HH-MM.txt # Timestamped EOD save files
```

---

## Calibration

Each meter has an independent calibration factor (default `1.0`). Volume is calculated as:

```
volume = pulse_count × calibration_factor × unit_multiplier
```

To calibrate:
1. Go to Admin → Calibrate Sensor
2. Select the meter number
3. Enter the known pour volume when prompted
4. The system calculates and saves the new factor

---

## Scheduled Daily Reset

The system performs an automatic EOD cycle at the configured time:
1. Saves flow data to a timestamped file on the SD card
2. Prints an EOD report to the thermal printer
3. Resets all totalizers to zero
4. Reboots the ESP32

Configure the reset time via Admin → Option `7`.

---

## Project Structure

```
beer-trax/
├── src/
│   ├── flow_meter.ino   # Main Arduino sketch
│   └── pulse_cnt.h      # ESP-IDF PCNT driver header
├── hardware/
│   └── wiring.md        # Pin reference and wiring notes
├── docs/
│   └── api.md           # Web endpoint documentation
├── .gitignore
└── README.md
```

---

## Security Notes

- The admin code is hardcoded as `1234` — **change it before deployment**.
- Wi-Fi credentials are stored in plaintext on the SD card.
- The licence encryption is a simple obfuscation scheme, not cryptographic security.

---

## Licence

This project is proprietary. All rights reserved.
