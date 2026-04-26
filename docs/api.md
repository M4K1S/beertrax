# Beer Trax — Web API Endpoints

All endpoints are served on port **80** over HTTP. Replace `<IP>` with the device's local IP address shown on the main page or serial monitor.

---

## Main Interface

### `GET /`
Returns the main web dashboard with current flow totals and menu.

### `GET /admin?code=<adminCode>`
Returns the admin panel. Requires the correct admin code (default: `1234`).

---

## Printing

### `GET /printflowdata`
Prints the current flow meter totals to the thermal printer.

### `GET /reprintNewest`
Reprints the most recent EOD report from the SD card.

---

## SD Card

### `GET /previousSaves`
Lists all saved EOD files on the SD card with links to view each one.

---

## Calibration

### `GET /calibrateMeter?meter=<0-7>`
Opens the calibration page for the specified meter (0-indexed).

### `GET /startCalibration?meter=<0-7>&amount=<ml>`
Starts a calibration run for the specified meter with the target volume in mL.

### `POST /stopCalibration`
Stops the active calibration run, calculates the calibration factor, and saves it.

---

## Totalizer Control

### `GET /resetTotalizer?resetTotalizer=<1-8>`
Resets the specified flow meter totalizer to zero (1-indexed).

### `GET /resetAll`
Resets all 8 flow meter totalizers to zero.

---

## Configuration

### `GET /setRTC`
Sets the RTC clock to the compile-time timestamp (`__DATE__` / `__TIME__`).

### `GET /setTimeOfDay?time=<HH:MM:SS>`
Sets the scheduled daily reset and print time.

### `GET /rename?meter=<0-7>&name=<name>`
Renames the specified flow meter.

### `GET /toggleUnits`
Toggles the display unit between **milliliters** and **fluid ounces**.

### `GET /changeWifi?ssid=<ssid>&password=<password>`
Updates the Wi-Fi credentials stored on the SD card. Takes effect after reboot.

### `GET /updateLicence?key=<key>`
Updates the licence key stored on the SD card.
