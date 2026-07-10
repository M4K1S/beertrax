# BeerTrax — Wiring Reference

## ESP32 Pin Assignments

### Flow Sensor Inputs (PCNT)

| Flow Meter | ESP32 GPIO | PCNT Unit | Notes |
|---|---|---|---|
| Meter 1 | GPIO 4 | Unit 0 | Pulse input |
| Meter 2 | GPIO 0 | Unit 1 | Bootstrapping pin; avoid holding LOW during reset |
| Meter 3 | GPIO 32 | Unit 2 | Pulse input |
| Meter 4 | GPIO 33 | Unit 3 | Pulse input |
| Meter 5 | GPIO 25 | Unit 4 | Pulse input |
| Meter 6 | GPIO 26 | Unit 5 | Pulse input |
| Meter 7 | GPIO 27 | Unit 6 | Pulse input |
| Meter 8 | GPIO 14 | Unit 7 | Pulse input |

> Flow sensors must output 3.3V-compatible pulse signals. Use a voltage divider or level shifter if the sensor outputs 5V pulses.

### SD Card Module (SPI)

| Signal | ESP32 GPIO |
|---|---|
| CS | GPIO 15 |
| MOSI | GPIO 23 |
| MISO | GPIO 19 |
| SCK | GPIO 18 |

### DS3231 RTC (I2C)

| Signal | ESP32 GPIO |
|---|---|
| SDA | GPIO 21 |
| SCL | GPIO 22 |

### Thermal Printer (Serial2)

| Signal | ESP32 GPIO |
|---|---|
| ESP32 TX → Printer RX | GPIO 17 |
| ESP32 RX ← Printer TX | GPIO 16 |

Printer baud rate: `9600`.

---

## PCNT Configuration

| Parameter | Value |
|---|---|
| High limit | `32767` |
| Low limit | `-32768` |
| Glitch filter | `1023` APB clock cycles |
| Count mode | Increment on rising edge |

The glitch filter rejects very short pulses, helping reduce false counts from noise on long sensor wires.

---

## Electrical Notes

- Keep flow sensor signal wires away from high-current wiring when possible.
- Add common ground between the ESP32 and all sensor modules.
- Confirm every sensor output is 3.3V-safe before connecting to the ESP32.
- Use stable power for the ESP32, SD card module, RTC, and thermal printer.
- Thermal printers can draw high peak current; power them from a suitable supply rather than the ESP32 3.3V rail.
