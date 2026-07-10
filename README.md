# BeerTrax — ESP32 8-Channel Flow Monitoring System

> A real-world embedded IoT flow-monitoring system for bar and beverage environments, using an ESP32, 8 hardware pulse-counter channels, a local web dashboard, SD card logging, RTC scheduling, and thermal printer reports.

BeerTrax was designed to monitor multiple beverage lines at once. Each flow sensor is counted through the ESP32 PCNT hardware peripheral, so the system can track pulses across 8 taps without relying on slow polling loops. Totals are displayed in a browser, saved to SD card, and printed as end-of-day reports.

---

## Project Snapshot

| Area | Details |
|---|---|
| Project type | Embedded data acquisition + beverage monitoring |
| Main MCU | ESP32 using the Arduino framework |
| Core inputs | 8 pulse-output flow sensors, RTC, web UI commands |
| Core outputs | Live web dashboard, SD card files, thermal printer reports |
| Storage | SD card autosave + timestamped report files |
| Timing | DS3231 RTC + NTP synchronization |
| Main skills shown | Hardware pulse counting, embedded web UI, SD logging, scheduled automation, firmware reliability design |

---

## System Overview

BeerTrax continuously monitors up to 8 flow sensors connected to beverage lines.

1. Each flow sensor produces pulses as liquid passes through it.
2. The ESP32 PCNT hardware peripheral counts pulses for each sensor.
3. The firmware periodically drains PCNT counters into software accumulators.
4. Calibration factors convert raw pulse counts into mL or fl oz.
5. A browser dashboard displays live totals and configuration options.
6. The system autosaves counts to SD card for crash recovery.
7. At the configured daily reset time, the system prints an end-of-day report, saves it to SD, resets totals, and reboots.

---

## What I Built

This was a solo engineering project. I developed the firmware, web dashboard, pin mapping, hardware specification, SD file format, printer reporting flow, and deployment documentation.

### Embedded Firmware

- Implemented ESP32 firmware in Arduino/C++.
- Configured 8 ESP32 PCNT hardware units for simultaneous pulse counting.
- Added per-channel glitch filtering to reject short electrical noise pulses.
- Built volume calculation using per-meter calibration factors.
- Added mL / fl oz unit switching.
- Added SD card autosave and restore on reboot.
- Added timestamped end-of-day report generation.
- Added scheduled daily reset using a DS3231 RTC.
- Added NTP-based RTC synchronization.
- Integrated an Adafruit-compatible thermal receipt printer over Serial2.

### Web Dashboard

- Built an ESP32-hosted web interface with no external server required.
- Added live total viewing for all 8 meters.
- Added admin functions for calibration, totalizer reset, Wi-Fi changes, unit switching, tap renaming, RTC sync, daily reset time, and licence updates.
- Added SD save browsing from the web UI.

### Hardware Integration

- Designed the full pin map for 8 flow sensors, SD card SPI, DS3231 I2C, and Serial2 printer communication.
- Included wiring documentation for hardware assembly.
- Designed the project around real bar deployment needs: multiple taps, daily reporting, local network access, and recoverable state.

---

## Engineering Highlights

- 8 independent flow sensor channels using ESP32 PCNT hardware.
- Timer-like fixed interval accumulation loop that avoids expensive software polling.
- Glitch filtering on PCNT inputs to reduce false counts from electrical noise.
- Crash-recovery autosave system using `/Autosave.txt` on SD card.
- End-of-day reporting pipeline: print report → save report → reset totals → reboot.
- Browser-based dashboard served directly from the ESP32.
- SD card file structure for credentials, calibration, tap names, unit preferences, autosave, manual snapshots, and EOD reports.
- Admin-gated configuration actions for sensitive operations.

---

## Architecture

```text
┌──────────────────────────────────────────────────────────┐
│                          ESP32                           │
│                                                          │
│  ┌────────────────┐   ┌───────────────────────────────┐  │
│  │  PCNT Units    │   │      Async Web Dashboard       │  │
│  │  8 Flow Inputs │   │  View / Calibrate / Configure  │  │
│  └───────┬────────┘   └───────────────┬───────────────┘  │
│          │                            │                  │
│          ▼                            ▼                  │
│  ┌────────────────────────────────────────────────────┐  │
│  │              Main Firmware Control Loop            │  │
│  │ Count → Accumulate → Convert → Display → Save      │  │
│  └──────────────┬──────────────┬──────────────┬───────┘  │
│                 │              │              │          │
│                 ▼              ▼              ▼          │
│           SD Card Files    DS3231 RTC    Thermal Printer │
│           Autosave/Logs    Daily Reset   Reports         │
└─────────────────┬────────────────────────────────────────┘
                  │ Wi-Fi
                  ▼
        ┌─────────────────────────┐
        │ Browser on Local Network │
        │   http://<ESP32-IP>/     │
        └─────────────────────────┘
```

---

## System Flow

```text
BOOT
  ↓
INIT PCNT + RTC + SD + PRINTER + WI-FI
  ↓
LOAD CONFIGURATION FROM SD
  ↓
RESTORE AUTOSAVED TOTALS
  ↓
MAIN LOOP EVERY ~10 SECONDS
  ↓
DRAIN PCNT COUNTERS INTO ACCUMULATORS
  ↓
UPDATE LIVE TOTALS FOR WEB UI
  ↓
AUTOSAVE EVERY ~5 MINUTES
  ↓
DAILY RESET TIME REACHED?
  ↓
PRINT EOD REPORT + SAVE TO SD
  ↓
RESET TOTALS + REBOOT
```

---

## Hardware

| Component | Purpose |
|---|---|
| ESP32 dual-core module | Main controller |
| Flow sensor × 8 | Pulse-output beverage flow measurement |
| DS3231 RTC module | Accurate local timekeeping |
| MicroSD card module | Persistent config, autosave, and reports |
| Adafruit-compatible thermal printer | Current/EOD receipts |
| 5V power supply | System power |

---

## Pin Mapping

### Flow Sensor Inputs

| Meter | GPIO | PCNT Unit |
|---|---|---|
| 1 | 4 | Unit 0 |
| 2 | 0 | Unit 1 |
| 3 | 32 | Unit 2 |
| 4 | 33 | Unit 3 |
| 5 | 25 | Unit 4 |
| 6 | 26 | Unit 5 |
| 7 | 27 | Unit 6 |
| 8 | 14 | Unit 7 |

> Flow sensors must output 3.3V-compatible pulse signals. Use a voltage divider or level shifter for 5V pulse outputs.

### SD Card Module

| Signal | GPIO |
|---|---|
| CS | 15 |
| MOSI | 23 |
| MISO | 19 |
| SCK | 18 |

### DS3231 RTC

| Signal | GPIO |
|---|---|
| SDA | 21 |
| SCL | 22 |

### Thermal Printer

| Signal | GPIO |
|---|---|
| ESP32 TX → Printer RX | 17 |
| ESP32 RX ← Printer TX | 16 |

---

## Repository Structure

```text
beertrax/
├── firmware/
│   ├── main.ino              # ESP32 firmware
│   ├── pulse_cnt.h           # PCNT API reference/compatibility header
│   └── secrets_template.h    # Safe config template; copy to secrets.h
├── docs/
│   └── api.md                # Web endpoint notes
├── hardware/
│   └── wiring.md             # Pin map and wiring reference
├── LICENSE
├── .gitignore
└── README.md
```

---

## Setup

### 1. Firmware

Install Arduino IDE or PlatformIO with ESP32 board support.

Required Arduino libraries:

| Library | Purpose |
|---|---|
| `RTClib` | DS3231 RTC |
| `ESPAsyncWebSrv` | ESP32-hosted web dashboard |
| `Adafruit Thermal Printer Library` | Thermal printer output |
| `SD` / `SPI` | SD card storage |
| `WiFi` | Local network access |

Copy the template credentials file:

```bash
cp firmware/secrets_template.h firmware/secrets.h
```

Fill in your local values:

```cpp
#define WIFI_SSID          "your-wifi-network-name"
#define WIFI_PASSWORD      "your-wifi-password"
#define ADMIN_CODE         "your-admin-pin"
#define GMT_OFFSET_SECONDS -18000
#define DST_OFFSET_SECONDS  3600
#define DAILY_RESET_TIME   "02:30:00"
```

Then flash `firmware/main.ino` to the ESP32.

### 2. SD Card Runtime Files

The firmware creates required files/folders on first boot if they are missing:

```text
/
├── ssid.txt
├── password.txt
├── admincode.txt
├── config.txt
├── calibration.txt
├── mlOZ.txt
├── time.txt
├── licence.txt
├── Autosave.txt
├── EOD_Saves/
└── Manual_Saves/
```

These files allow the device to recover counts, store calibration, update credentials, and save reports without reflashing firmware.

### 3. Web Dashboard

After boot, open the ESP32 IP address in a browser on the same local network:

```text
http://<ESP32-IP>/
```

---

## Interface / API

Main dashboard actions:

| Area | Examples |
|---|---|
| Live totals | View all 8 meters, calibrated volume, and raw pulses |
| Printing | Print current totals or reprint the newest EOD report |
| SD browsing | View saved end-of-day reports from the web UI |
| Admin configuration | Calibration, tap renaming, resets, units, Wi-Fi, RTC, daily reset time, licence |

Detailed endpoint notes are in [`docs/api.md`](docs/api.md).

---

## Security Notes

- `firmware/secrets.h` is intentionally ignored and should never be committed.
- Wi-Fi credentials, admin PIN, and licence values are deployment-specific.
- Admin actions require the configured PIN, but this is still intended for trusted local networks.
- Licence encoding is lightweight obfuscation, not cryptographic security.
- GPIO 0 is a bootstrapping pin on ESP32 boards; ensure the connected flow sensor does not hold it in an invalid boot state.

---

## Future Improvements

- Replace query-string admin PIN with a stronger session/token mechanism.
- Add HTTPS or run behind a trusted local gateway for stronger network security.
- Add OTA firmware updates.
- Add a custom PCB or shield for cleaner wiring.
- Add formal calibration test logs for each flow sensor.
- Add CSV export from the SD card reports.
- Add automated firmware tests for parsing, date/time validation, and SD file formatting.

---

## Skills Demonstrated

- Embedded C++ / Arduino on ESP32
- ESP32 PCNT hardware peripheral
- Multi-channel sensor acquisition
- Glitch filtering
- SPI SD card storage
- I2C RTC integration
- Serial thermal printer integration
- ESP32-hosted web server development
- Persistent configuration files
- Calibration workflow design
- Scheduled automation
- Hardware/software integration
- Debugging real deployment constraints
- Project documentation and deployment planning

---

## License

MIT License.

---

## Author

Built by Maxim Kisselev as a real-world embedded monitoring system for beverage flow tracking and reporting.
