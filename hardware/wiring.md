# Beer Trax — Wiring Reference

## ESP32 Pin Assignments

### Flow Sensor Inputs (PCNT)

| Flow Meter | GPIO | PCNT Unit |
|---|---|---|
| Meter 1 | GPIO 4 | Unit 0 |
| Meter 2 | GPIO 0 | Unit 1 |
| Meter 3 | GPIO 32 | Unit 2 |
| Meter 4 | GPIO 33 | Unit 3 |
| Meter 5 | GPIO 25 | Unit 4 |
| Meter 6 | GPIO 26 | Unit 5 |
| Meter 7 | GPIO 27 | Unit 6 |
| Meter 8 | GPIO 14 | Unit 7 |

> Flow sensors must output 3.3V-compatible pulse signals. Use a voltage divider or level shifter if your sensors output 5V.

### SD Card Module (SPI)

| Signal | ESP32 GPIO |
|---|---|
| CS (Chip Select) | GPIO 15 |
| MOSI | GPIO 23 (default SPI) |
| MISO | GPIO 19 (default SPI) |
| SCK | GPIO 18 (default SPI) |

### DS3231 RTC (I²C)

| Signal | ESP32 GPIO |
|---|---|
| SDA | GPIO 21 (default I²C) |
| SCL | GPIO 22 (default I²C) |

### Thermal Printer (Serial2)

| Signal | ESP32 GPIO |
|---|---|
| TX → Printer RX | GPIO 17 (Serial2 TX) |
| RX ← Printer TX | GPIO 16 (Serial2 RX) |

Printer baud rate: **9600**

---

## PCNT Configuration

| Parameter | Value |
|---|---|
| High Limit | 32767 |
| Low Limit | -32768 |
| Glitch Filter | 1023 APB_CLK cycles |
| Count Mode | Increment on rising edge |

The glitch filter suppresses pulses shorter than ~10 µs at 80 MHz APB clock, helping reject electrical noise on sensor lines.
