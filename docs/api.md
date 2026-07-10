# BeerTrax — Web / Local API Notes

All endpoints are served over local HTTP on port `80`.

```text
http://<ESP32-IP>/
```

These endpoints are intended for trusted local networks only.

---

## Main Pages

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/` | Main dashboard with current totals and quick actions |
| `GET` | `/admin?code=<adminCode>` | Admin panel after PIN validation |
| `GET` | `/previousSaves` | Display saved end-of-day report files from SD card |

---

## Printing

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/printflowdata` | Print current totals and save a manual snapshot |
| `GET` | `/reprintNewest` | Reprint the newest saved EOD report |

---

## Calibration

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/calibrateMeter?meter=<0-7>` | Open calibration page for the selected meter |
| `POST` | `/startCalibration?meter=<0-7>` | Start calibration using the current pulse count as the baseline |
| `POST` | `/stopCalibration?mlPoured=<volume>` | Stop calibration and save the calculated mL-per-pulse factor |

Calibration endpoints require admin mode.

---

## Totalizer Control

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/resetTotalizer?resetTotalizer=<1-8>` | Reset one meter totalizer |
| `GET` | `/resetAll` | Reset all 8 meter totalizers |

Totalizer reset endpoints require admin mode.

---

## Configuration

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/setRTC` | Sync the DS3231 RTC from NTP time |
| `GET` | `/setTimeOfDay?timeInput=<HH:MM:SS>` | Set the daily reset/print time |
| `GET` | `/rename?renameMeter=<1-8>&newName=<name>` | Rename a flow meter |
| `GET` | `/toggleUnits` | Toggle display units between mL and fl oz |
| `GET` | `/changeWifi?newSSID=<ssid>&newPASS=<password>` | Update Wi-Fi credentials stored on SD card |
| `GET` | `/updateLicence?licence=<key>` | Update the licence key stored on SD card |

Configuration endpoints require admin mode.

---

## Notes

- Admin mode is enabled after visiting `/admin?code=<adminCode>` with the correct PIN.
- This is a local-device interface, not a public internet API.
- Query parameters are used for simplicity on an ESP32-hosted dashboard; do not expose the device on untrusted networks.
