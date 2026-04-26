# 🍺 Beer Trax — ESP32 Multi-Channel Flow Meter System

A real-world IoT deployment built for the bar and beverage industry, combining 8-channel hardware pulse counting, a live web dashboard, thermal printer reporting, and scheduled SD card logging — all running on a single ESP32.

---

## 📖 System Overview

Beer Trax is an ESP32-based beverage flow tracking system designed for real bar deployments. Up to 8 flow sensors are monitored simultaneously using the ESP32's hardware Pulse Counter (PCNT) peripheral, eliminating any software polling overhead. Totals are displayed on a live Wi-Fi web dashboard, printed on demand to a thermal receipt printer, saved to an SD card for historical review, and automatically reset on a configurable daily schedule.

---

## 🙋 What I Personally Built
This was a fully solo engineering project. I independently designed, specified, programmed, and deployed the entire system end-to-end, and provided full hardware assembly instructions to the client who carried out the physical build.

- Embedded firmware (ESP32) — Developed the complete main.ino system, including 8-channel hardware pulse counting via the ESP32 PCNT peripheral, a full async web server hosting the admin dashboard, per-meter flow calibration, watchdog timer fault recovery, RTC-scheduled daily resets, SD card logging with autosave crash recovery, NTP time synchronization, and thermal printer integration
- Embedded web interface — Built a full admin dashboard served directly from the ESP32, with dynamically generated HTML, CSS, and JavaScript. Includes live flow meter totals, per-meter calibration, totalizer resets, tap renaming, unit toggling, Wi-Fi configuration, daily reset scheduling, and an SD card save browser
- Hardware design — Selected all components and produced the complete wiring specification, pin mapping, and assembly instructions for the client to build from. Covers 8 pulse-output flow sensor inputs with hardware glitch filtering, SPI SD card module, I2C DS3231 RTC, and Adafruit-compatible thermal printer
- Deployment — Delivered as a production-ready IoT system for a real bar environment, including remote configuration support and documentation

---

## 📈 Impact

- Deployed in a live bar environment for continuous multi-tap beverage tracking across 8 independent flow lines.
- Eliminated manual pour tracking across 8 taps simultaneously
- Automated end-of-day reporting with printed receipts and timestamped SD card archives
- Enabled full remote configuration and monitoring through a browser on any device on the local network

---

## 🧠 Engineering Highlights

- Hardware PCNT pulse counting on all 8 channels — no interrupt contention or missed pulses
- Per-channel glitch filtering (~12 µs rejection) to suppress electrical noise on sensor lines
- Crash-safe autosave: pulse counts written to SD every 5 minutes and restored on reboot
- Async web server (ESPAsyncWebSrv) handles concurrent requests without blocking the main loop
- All credentials and configuration externalized to `secrets.h` and SD card files — nothing sensitive in firmware source
- Formatted `snprintf`-based timestamps throughout, replacing unreliable Arduino String concatenation for date/time

---

## ✨ Features

### 📊 8-Channel Hardware Flow Metering
- ESP32 hardware Pulse Counter (PCNT) peripheral — one dedicated unit per sensor
- Up to 8 simultaneously monitored flow sensors on GPIO 4, 0, 32, 33, 25, 26, 27, 14
- Per-meter calibration stored on SD card, configurable from the admin web interface
- Display units switchable between **mL** and **fl oz** at any time

### 🌐 Live Web Dashboard
- Async HTTP server runs entirely on the ESP32 — no external server required
- Real-time flow totals table with raw pulse counts and calibrated volumes
- Admin panel with PIN protection for all configuration changes
- Mobile-responsive dark theme UI served from generated HTML/CSS/JavaScript

### 🖨️ Thermal Printer Integration
- On-demand current totals printout triggered from the web UI
- Automatic end-of-day (EOD) receipt printed at the configured daily reset time
- Reprint of the most recent EOD report at any time

### 💾 SD Card Logging
- Autosave every 5 minutes for crash recovery
- Timestamped EOD report files saved to `/EOD_Saves/`
- On-demand snapshot saves stored in `/Manual_Saves/`
- Full save history browsable from the web interface

### ⏰ Scheduled Daily Reset
- Configurable reset time (HH:MM:SS) stored on SD card
- At the scheduled time: EOD report printed, data saved to SD, all totalizers cleared, ESP32 reboots
- Watchdog timer ensures recovery from any fault during normal operation

### 🔐 Licence Validation
- Encrypted licence key system with expiration date checking
- Warning printed on EOD reports when licence is within 15 days of expiry
- Licence manageable through the admin web interface

---

## 🏗️ Architecture

```
┌──────────────────────────────────────────────────────┐
│                        ESP32                         │
│                                                      │
│  ┌────────────────┐   ┌──────────────────────────┐   │
│  │  PCNT Units    │   │   AsyncWebServer         │   │
│  │  (×8 sensors)  │   │   Admin Dashboard        │   │
│  └───────┬────────┘   └────────────┬─────────────┘   │
│          │                         │                 │
│  ┌───────▼─────────────────────────▼─────────────┐   │
│  │              Main Control Logic               │   │
│  │  (Count → Accumulate → Display → Log → Reset) │   │
│  └───────┬─────────────┬─────────────┬───────────┘   │
│          │             │             │               │
│  ┌───────▼──┐  ┌───────▼──────┐  ┌───▼──────────┐    │
│  │ Thermal  │  │  DS3231 RTC  │  │   SD Card    │    │
│  │ Printer  │  │  NTP Sync    │  │  Autosave /  │    │
│  │ Reports  │  │  EOD Timer   │  │  EOD Saves   │    │
│  └──────────┘  └──────────────┘  └──────────────┘    │
└──────────────────────────────────────────────────────┘
                          │ WiFi
          ┌───────────────▼──────────────┐
          │     Browser (any device)     │
          │      http://<ESP32-IP>/      │
          └──────────────────────────────┘
```

---

## 📊 System Logic

```
┌──────────────┐
│     BOOT     │  ← Init PCNT, RTC, SD, printer, Wi-Fi
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  LOAD STATE  │  ← Read autosave, calibration, names,
│              │    credentials, licence from SD card
└──────┬───────┘
       │
       ▼
┌──────────────┐
│  MAIN LOOP   │  ← Every 10 seconds:
│              │    Drain PCNT counters into accumulators
│              │    Check daily reset schedule
│              │    Autosave if interval elapsed
└──────┬───────┘
       │ Daily reset time reached
       ▼
┌──────────────┐
│  EOD CYCLE   │  ← Print EOD report to thermal printer
│              │    Save timestamped file to SD card
│              │    Reset all 8 totalizers
│              │    ESP32 reboots
└──────────────┘
```

---

## 🔧 Hardware — Bill of Materials

| Component | Notes |
|---|---|
| ESP32 Dual Core Module | Main microcontroller |
| DS3231 RTC Module | Accurate timekeeping, I2C |
| Flow Sensors × 8 | Pulse-output, 3.3V compatible |
| Micro SD Card Module | SPI interface, any capacity |
| Adafruit-Compatible Thermal Printer | Receipt printing, 9600 baud |
| 5V Power Supply | USB or barrel jack |

---

## 📌 Pin Mapping

### Flow Sensor Inputs (PCNT)

| Meter | GPIO | PCNT Unit |
|---|---|---|
| Flow Meter 1 | 4 | Unit 0 |
| Flow Meter 2 | 0 | Unit 1 |
| Flow Meter 3 | 32 | Unit 2 |
| Flow Meter 4 | 33 | Unit 3 |
| Flow Meter 5 | 25 | Unit 4 |
| Flow Meter 6 | 26 | Unit 5 |
| Flow Meter 7 | 27 | Unit 6 |
| Flow Meter 8 | 14 | Unit 7 |

> Flow sensors must output 3.3V-compatible pulse signals. Use a voltage divider or level shifter for 5V sensors.

### SD Card Module (SPI)

| Signal | GPIO |
|---|---|
| CS | 15 |
| MOSI | 23 |
| MISO | 19 |
| SCK | 18 |

### DS3231 RTC (I2C)

| Signal | GPIO |
|---|---|
| SDA | 21 |
| SCL | 22 |

### Thermal Printer (Serial2)

| Signal | GPIO |
|---|---|
| TX → Printer RX | 17 |
| RX ← Printer TX | 16 |

---

## 📁 Repository Structure

```
beer-trax/
├── firmware/
│   ├── main.ino               # ESP32 firmware (Arduino IDE)
│   ├── pulse_cnt.h            # ESP-IDF PCNT driver header
│   └── secrets_template.h     # Copy to secrets.h and fill in your credentials
├── hardware/
│   └── wiring.md              # Full pin reference and wiring notes
├── docs/
│   └── api.md                 # Web endpoint documentation
├── photos/                    # System photos and wiring diagrams
└── README.md
```

---

## 🚀 Getting Started

### 1. Clone the repo

```bash
git clone https://github.com/YOUR_USERNAME/beer-trax.git
cd beer-trax
```

### 2. Configure credentials

Copy `secrets_template.h` and rename it to `secrets.h` in the same `firmware/` folder, then fill in your values:

```cpp
#define WIFI_SSID          "your-wifi-network-name"
#define WIFI_PASSWORD      "your-wifi-password"
#define ADMIN_CODE         "your-admin-pin"
#define GMT_OFFSET_SECONDS -18000   // UTC-5 (EST) — adjust for your timezone
#define DST_OFFSET_SECONDS  3600
#define DAILY_RESET_TIME   "02:30:00"
```

`secrets.h` is listed in `.gitignore` and will never be committed to the repository.

### 3. Install Arduino libraries

| Library | Purpose |
|---|---|
| `RTClib` | DS3231 RTC communication |
| `ESPAsyncWebSrv` | Async HTTP server |
| `Adafruit Thermal Printer Library` | Receipt printer control |
| `SD` (built-in ESP32) | SD card read/write |
| `WiFi` (built-in ESP32) | Network connectivity |

### 4. Flash the firmware

Open `firmware/main.ino` in Arduino IDE 2.x with the ESP32 board package installed. Select your board and COM port, then upload.

### 5. Access the web interface

After boot, the device IP is printed to Serial and on the thermal receipt. Open `http://<ESP32-IP>` in any browser on the same network.

---

## 🎛️ Admin Web Interface

Enter the admin PIN from the main page to access all configuration options.

| Option | Action |
|---|---|
| `1` | Update licence key |
| `2` | Change Wi-Fi credentials |
| `3` | Calibrate a flow sensor (1–8) |
| `4` | Reset an individual totalizer |
| `5` | Reset all totalizers |
| `6` | Sync RTC clock via NTP |
| `7` | Set daily reset/print time |
| `8` | Rename flow meters |
| `9` | Toggle mL / fl oz display units |
| `10` | Return to main page |

---

## 📐 Flow Meter Calibration

Each meter has an independent calibration factor (default `1.0 mL/pulse`). Volume is calculated as:

```
volume = pulse_count × calibration_factor × unit_multiplier
```

To calibrate:
1. Go to Admin → Option `3` and select the meter
2. Click **Start**, then pour a precisely measured volume from that tap
3. Click **Stop** and enter the exact volume poured in mL
4. The factor is computed and saved to the SD card automatically

---

## 💾 SD Card File Structure

```
/
├── ssid.txt               # Wi-Fi network name
├── password.txt           # Wi-Fi password
├── admincode.txt          # Admin panel PIN
├── config.txt             # Flow meter custom names (one per line)
├── calibration.txt        # Per-meter calibration factors (one per line)
├── mlOZ.txt               # Unit preference (0 = mL, 1 = fl oz)
├── time.txt               # Daily reset time (HH:MM:SS)
├── licence.txt            # Encrypted licence key
├── Autosave.txt           # Latest pulse counts (crash recovery)
├── EOD_Saves/             # Timestamped end-of-day report files
└── Manual_Saves/          # On-demand print snapshots
```

All files are created automatically on first boot if missing.

---

## 🛠️ Tech Stack

| Layer | Technology |
|---|---|
| Microcontroller | ESP32 (Arduino framework) |
| Pulse Counting | ESP32 hardware PCNT peripheral |
| Web Server | ESPAsyncWebServer |
| Real-Time Clock | DS3231 via RTClib |
| Time Sync | NTP (pool.ntp.org) |
| Data Storage | SD card (SPI) |
| Receipt Printing | Adafruit Thermal Printer Library |
| Fault Recovery | ESP32 Hardware Watchdog (WDT) |

---

## 🔒 Security Notes

- All credentials (Wi-Fi, admin PIN) are stored in `secrets.h`, which is never committed to the repo. Copy `secrets_template.h` to get started.
- At runtime, credentials are loaded from SD card files so they can be updated over-the-air through the admin interface without reflashing.
- The admin web interface has PIN protection but should be deployed on trusted local networks only.
- The licence encryption is a lightweight obfuscation scheme intended as a deployment gate, not cryptographic security.

---

## 📄 License

MIT License — free to use, modify, and deploy.
