/**
 * Beer Trax — ESP32 Multi-Channel Flow Meter System
 * ====================================================
 * Tracks beverage volume across up to 8 flow meters using the ESP32
 * hardware Pulse Counter (PCNT) peripheral. Provides a Wi-Fi web
 * dashboard, thermal printer reports, and SD card data logging with
 * automatic scheduled end-of-day resets.
 *
 * Hardware:
 *   - ESP32 devkit
 *   - DS3231 RTC (I2C: SDA=21, SCL=22)
 *   - SD card module (SPI: CS=GPIO15)
 *   - Adafruit-compatible thermal printer (Serial2: TX=17, RX=16)
 *   - Up to 8 pulse-output flow sensors
 *
 * SD card files (created automatically on first boot if missing):
 *   /ssid.txt          - Wi-Fi network name
 *   /password.txt      - Wi-Fi password
 *   /admincode.txt     - Admin panel PIN (numeric)
 *   /config.txt        - Flow meter custom names (one per line)
 *   /calibration.txt   - Calibration factors (one float per line)
 *   /mlOZ.txt          - Unit preference (0=mL, 1=fl oz)
 *   /time.txt          - Daily reset time (HH:MM:SS)
 *   /licence.txt       - Encrypted licence key
 *   /Autosave.txt      - Last known pulse counts (crash recovery)
 *   /EOD_Saves/        - Timestamped end-of-day report files
 *   /Manual_Saves/     - On-demand print snapshots
 *
 * Author: Beer Trax Project
 * Version: 1.0.0
 */

// ─────────────────────────────────────────────
// Secrets — credentials, PIN, timezone
// Copy secrets_template.h → secrets.h and fill
// in your values. Never commit secrets.h.
// ─────────────────────────────────────────────
#include "secrets.h"

// ─────────────────────────────────────────────
// Includes
// ─────────────────────────────────────────────
#include <Wire.h>
#include <RTClib.h>
#include <ESPAsyncWebSrv.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include "Adafruit_Thermal.h"
#include "driver/pcnt.h"
#include <esp_task_wdt.h>
#include <vector>
#include <algorithm>

// ─────────────────────────────────────────────
// Hardware configuration constants
// ─────────────────────────────────────────────
#define NUM_SENSORS        8
#define SD_CS_PIN          15
#define PRINTER_BAUD       9600

// PCNT hardware limits and glitch filter
#define PCNT_H_LIM_VAL     32767
#define PCNT_L_LIM_VAL    -32768
#define PCNT_FILTER_VAL    1023   // ~12.8 us glitch rejection @ 80 MHz APB

// Flow sensor GPIO pins (indexed 0-7)
const int SENSOR_PINS[NUM_SENSORS] = { 4, 0, 32, 33, 25, 26, 27, 14 };

// NTP / timezone — values come from secrets.h
const char* NTP_SERVER       = "pool.ntp.org";
const long  GMT_OFFSET_SEC   = GMT_OFFSET_SECONDS;
const int   DST_OFFSET_SEC   = DST_OFFSET_SECONDS;

// Autosave interval (loop iterations x 10 s each = 300 s = 5 min)
#define AUTOSAVE_INTERVAL  30

// ─────────────────────────────────────────────
// Default fallback values — pulled from secrets.h
// Used only if SD card files are missing on first boot
// ─────────────────────────────────────────────
#define DEFAULT_SSID       WIFI_SSID
#define DEFAULT_PASSWORD   WIFI_PASSWORD
#define DEFAULT_ADMIN_CODE ADMIN_CODE
#define DEFAULT_RESET_TIME DAILY_RESET_TIME

// ─────────────────────────────────────────────
// Global state
// ─────────────────────────────────────────────
AsyncWebServer server(80);
Adafruit_Thermal printer(&Serial2);
RTC_DS3231 rtc;

// Runtime credentials (loaded from SD; never hardcoded)
String ssid      = DEFAULT_SSID;
String password  = DEFAULT_PASSWORD;
String adminCode = DEFAULT_ADMIN_CODE;

// Licence
String licence      = "";
String expiration   = "";
bool   licenceValid = false;
int    whenExpired  = 0;
bool   soonExpire   = false;

// Pulse accumulators - one per sensor, updated in loop()
volatile long pulseAccum[NUM_SENSORS] = {0, 0, 0, 0, 0, 0, 0, 0};

// Calibration & display settings
float  flowMeterCalibrations[NUM_SENSORS] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
String flowMeterNames[NUM_SENSORS] = {
  "Flow Meter 1", "Flow Meter 2", "Flow Meter 3", "Flow Meter 4",
  "Flow Meter 5", "Flow Meter 6", "Flow Meter 7", "Flow Meter 8"
};
bool  useOunces = false;
float mlOZ      = 1.0;

// Calibration session state
int   calibratingMeter = -1;
long  calibrationStart = 0;

// Admin session flag
bool adminMode = false;

// Scheduling counters
String dailyResetTime = DEFAULT_RESET_TIME;
int    sdPauseCount   = 0;  // guard against immediate re-trigger of daily reset
int    autosaveCount  = 0;

// ─────────────────────────────────────────────
// Forward declarations
// ─────────────────────────────────────────────
void   pcntInit();
float  getMeterVolume(int i);
String getUnitLabel();
String flowMeterDataHTML();
String flowMeterDataForSD();
String formatDateTime(DateTime dt);
bool   validateTimeInput(String t);
int    getHour(String t);
int    getMinute(String t);

void   readFlowMeterNames();
void   readCalibrationValues();
void   readmlOZ();
void   readPrintTime();
void   readAutosave();
void   readCredentials();
void   readLicence();
void   writeFlowMeterNames();
void   writeCalibrationValues();
void   writemlOZ();
void   writePrintTime();
void   writeCredentials();
void   writeLicence();

void   resetFlowMeter(int idx);
void   autoSaveToSD();
void   printFlowDataToFile();
void   reprintNewest();
void   firstBootInit();

String encrypt(String date);
String decrypt(String enc);

String generateCSS();
String generateHeader(String title);
String generateFooter();
String generateMainPage();
String generateAdminPage();
String generateCalibratePage(int meter);
String generatePreviousSavesPage();

void handleMainPage(AsyncWebServerRequest* req);
void handleAdminPage(AsyncWebServerRequest* req);
void handleCalibrateMeter(AsyncWebServerRequest* req);
void handleStartCalibration(AsyncWebServerRequest* req);
void handleStopCalibration(AsyncWebServerRequest* req);
void handleResetTotalizer(AsyncWebServerRequest* req);
void handleResetAll(AsyncWebServerRequest* req);
void handleSetRTC(AsyncWebServerRequest* req);
void handleSetTimeOfDay(AsyncWebServerRequest* req);
void handleFlowMeterRename(AsyncWebServerRequest* req);
void handleToggleUnits(AsyncWebServerRequest* req);
void handleUpdateLicence(AsyncWebServerRequest* req);
void handleReprintNewest(AsyncWebServerRequest* req);
void handlePrintFlowMeterData(AsyncWebServerRequest* req);
void handlePreviousSaves(AsyncWebServerRequest* req);
void handleChangeWifi(AsyncWebServerRequest* req);

// =============================================================
//  PCNT HARDWARE INITIALISATION
// =============================================================

/**
 * Initialises all 8 PCNT units.
 * Each unit counts rising edges from its flow sensor pin.
 * A glitch filter (~12 us) rejects electrical noise on the lines.
 */
void pcntInit() {
  for (int i = 0; i < NUM_SENSORS; i++) {
    pcnt_config_t cfg;
    cfg.pulse_gpio_num = SENSOR_PINS[i];
    cfg.ctrl_gpio_num  = PCNT_PIN_NOT_USED;
    cfg.channel        = PCNT_CHANNEL_0;
    cfg.unit           = (pcnt_unit_t)i;
    cfg.pos_mode       = PCNT_COUNT_INC;   // count on rising edge
    cfg.neg_mode       = PCNT_COUNT_DIS;   // ignore falling edge
    cfg.counter_h_lim  = PCNT_H_LIM_VAL;
    cfg.counter_l_lim  = PCNT_L_LIM_VAL;

    pcnt_unit_config(&cfg);
    pcnt_set_filter_value((pcnt_unit_t)i, PCNT_FILTER_VAL);
    pcnt_filter_enable((pcnt_unit_t)i);
    pcnt_counter_pause((pcnt_unit_t)i);
    pcnt_counter_clear((pcnt_unit_t)i);
    pcnt_counter_resume((pcnt_unit_t)i);
  }
}

// =============================================================
//  VOLUME CALCULATION HELPERS
// =============================================================

float getMeterVolume(int i) {
  return (float)pulseAccum[i] * flowMeterCalibrations[i] * mlOZ;
}

String getUnitLabel() {
  return useOunces ? "fl oz" : "mL";
}

// =============================================================
//  HTML PAGE GENERATORS
// =============================================================

/**
 * Shared CSS injected into every page.
 * Dark theme, clean layout, mobile-responsive.
 */
String generateCSS() {
  return
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0;}"
    "body{font-family:'Segoe UI',Arial,sans-serif;background:#1a1a2e;color:#e0e0e0;padding:20px;}"
    ".card{background:#16213e;border-radius:12px;padding:20px;margin:16px 0;border:1px solid #0f3460;}"
    "h1{color:#e94560;font-size:2rem;margin-bottom:4px;}"
    "h2{color:#0f9b8e;font-size:1.2rem;margin-bottom:12px;border-bottom:1px solid #0f3460;padding-bottom:6px;}"
    "h3{color:#a8dadc;font-size:0.95rem;font-weight:normal;margin-bottom:16px;}"
    "table{width:100%;border-collapse:collapse;margin-top:8px;}"
    "th{background:#0f3460;color:#a8dadc;padding:10px;text-align:left;font-size:0.85rem;text-transform:uppercase;letter-spacing:0.05em;}"
    "td{padding:10px 12px;border-bottom:1px solid #1a1a2e;font-size:0.95rem;}"
    "tr:hover td{background:#0f3460;}"
    ".volume{font-weight:bold;color:#e94560;font-size:1.05rem;}"
    "input[type=text],input[type=number]{"
    "background:#0f3460;border:1px solid #e94560;color:#e0e0e0;"
    "padding:10px 14px;border-radius:8px;font-size:1rem;width:100%;margin:8px 0;}"
    "button{background:#e94560;color:#fff;border:none;padding:10px 22px;"
    "border-radius:8px;font-size:1rem;cursor:pointer;margin:6px 4px 6px 0;}"
    "button:hover{background:#c73652;}"
    "button.secondary{background:#0f3460;}"
    "button.secondary:hover{background:#1a4a80;}"
    ".badge{display:inline-block;padding:3px 10px;border-radius:20px;font-size:0.8rem;}"
    ".badge-ok{background:#0f9b8e;color:#fff;}"
    ".badge-warn{background:#e94560;color:#fff;}"
    ".info{font-size:0.82rem;color:#888;margin-top:12px;}"
    "a{color:#a8dadc;text-decoration:none;}"
    "a:hover{color:#e94560;}"
    ".logo{font-size:0.8rem;color:#555;text-align:right;margin-top:20px;}"
    "</style>";
}

String generateHeader(String title) {
  return
    "<html><head><meta charset='UTF-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>" + title + " - Beer Trax</title>" +
    generateCSS() +
    "</head><body>";
}

String generateFooter() {
  DateTime now = rtc.now();
  return
    "<div class='info' style='margin-top:24px;'>"
    "IP: " + WiFi.localIP().toString() +
    " | RTC: " + formatDateTime(now) +
    " | Licence: " + expiration +
    "</div>"
    "<div class='logo'>Beer Trax v1.0.0</div>"
    "</body>";
}

/**
 * Builds the HTML table rows for all 8 flow meters.
 */
String flowMeterDataHTML() {
  String rows = "";
  for (int i = 0; i < NUM_SENSORS; i++) {
    float vol = getMeterVolume(i);
    rows += "<tr><td>" + String(i + 1) + "</td>";
    rows += "<td>" + flowMeterNames[i] + "</td>";
    rows += "<td class='volume'>" + String(vol, 2) + " " + getUnitLabel() + "</td>";
    rows += "<td>" + String(pulseAccum[i]) + "</td></tr>";
  }
  return rows;
}

String generateMainPage() {
  String licBadge = licenceValid
    ? "<span class='badge badge-ok'>Valid</span>"
    : "<span class='badge badge-warn'>Invalid / Expired</span>";

  String page = generateHeader("Dashboard");
  page += "<div class='card'>";
  page += "<h1>Beer Trax</h1>";
  page += "<h3>Real-Time Beverage Flow Monitoring</h3>";
  page += "</div>";

  page += "<div class='card'>";
  page += "<h2>Current Totals &nbsp;" + licBadge + "</h2>";
  page += "<table><tr><th>#</th><th>Tap Name</th><th>Volume</th><th>Raw Pulses</th></tr>";
  page += flowMeterDataHTML();
  page += "</table></div>";

  page += "<div class='card'>";
  page += "<h2>Quick Actions</h2>";
  page +=
    "<input type='text' id='cmd' placeholder='Enter option number (1-4)' "
    "onkeydown='if(event.key==\"Enter\")go()'>";
  page +=
    "<p style='margin:10px 0 6px;color:#888;font-size:0.85rem;'>"
    "1 - Print current totals &nbsp; "
    "2 - Reprint newest EOD &nbsp; "
    "3 - View SD saves &nbsp; "
    "4 - Admin menu</p>";
  page += "<button onclick='go()'>Go</button>";
  page += "</div>";

  page += generateFooter();
  page +=
    "<script>"
    "document.getElementById('cmd').focus();"
    "function go(){"
    "  var v=document.getElementById('cmd').value.trim();"
    "  if(v=='1'){window.location='/printflowdata';}"
    "  else if(v=='2'){window.location='/reprintNewest';}"
    "  else if(v=='3'){window.location='/previousSaves';}"
    "  else if(v=='4'){"
    "    var code=prompt('Enter admin code:');"
    "    if(code)window.location='/admin?code='+code;"
    "  }"
    "}"
    "</script></html>";
  return page;
}

String generateAdminPage() {
  String unit = useOunces ? "fl oz" : "mL";

  String page = generateHeader("Admin Panel");
  page += "<div class='card'>";
  page += "<h1>Admin Panel</h1>";
  page += "<h3>Beer Trax v1.0.0</h3>";
  page += "</div>";

  page += "<div class='card'>";
  page += "<h2>Current Totals</h2>";
  page += "<table><tr><th>#</th><th>Tap Name</th><th>Volume</th><th>Raw Pulses</th></tr>";
  page += flowMeterDataHTML();
  page += "</table></div>";

  page += "<div class='card'>";
  page += "<h2>Admin Options</h2>";
  page +=
    "<input type='text' id='adminInput' placeholder='Enter option (1-10)' "
    "onkeydown='if(event.key==\"Enter\")go()'>";
  page +=
    "<p style='margin:10px 0 6px;color:#888;font-size:0.85rem;'>"
    "1 - Update Licence &nbsp; "
    "2 - Change Wi-Fi &nbsp; "
    "3 - Calibrate Sensor (1-8) &nbsp; "
    "4 - Reset Totalizer &nbsp; "
    "5 - Reset All &nbsp; "
    "6 - Sync RTC via NTP &nbsp; "
    "7 - Set Daily Reset Time (now: " + dailyResetTime + ") &nbsp; "
    "8 - Rename Tap &nbsp; "
    "9 - Toggle Units (now: " + unit + ") &nbsp; "
    "10 - Back to Main</p>";
  page += "<button onclick='go()'>Go</button>";
  page += "<button class='secondary' onclick=\"window.location='/'\">Back to Main</button>";
  page += "</div>";

  page += generateFooter();
  page +=
    "<script>"
    "document.getElementById('adminInput').focus();"
    "function go(){"
    "  var v=document.getElementById('adminInput').value.trim();"
    "  if(v=='1'){var k=prompt('Enter licence key:');if(k)window.location='/updateLicence?licence='+k;}"
    "  else if(v=='2'){"
    "    var s=prompt('New SSID:');var p=prompt('New Password:');"
    "    if(s&&p)window.location='/changeWifi?newSSID='+encodeURIComponent(s)+'&newPASS='+encodeURIComponent(p);"
    "  }"
    "  else if(v=='3'){"
    "    var m=prompt('Meter to calibrate (1-8):');"
    "    if(m&&m>=1&&m<=8)window.location='/calibrateMeter?meter='+(parseInt(m)-1);"
    "    else alert('Invalid meter number');"
    "  }"
    "  else if(v=='4'){"
    "    var t=prompt('Totalizer to reset (1-8):');"
    "    if(t&&t>=1&&t<=8)window.location='/resetTotalizer?resetTotalizer='+t;"
    "  }"
    "  else if(v=='5'){if(confirm('Reset ALL totalizers?'))window.location='/resetAll';}"
    "  else if(v=='6'){if(confirm('Sync RTC to NTP time now?'))window.location='/setRTC';}"
    "  else if(v=='7'){var t=prompt('Daily reset time (HH:MM:SS):');if(t)window.location='/setTimeOfDay?timeInput='+t;}"
    "  else if(v=='8'){"
    "    var m=prompt('Tap to rename (1-8):');var n=prompt('New name:');"
    "    if(m&&n)window.location='/rename?renameMeter='+m+'&newName='+encodeURIComponent(n);"
    "  }"
    "  else if(v=='9'){window.location='/toggleUnits';}"
    "  else if(v=='10'){window.location='/';}"
    "}"
    "</script></html>";
  return page;
}

String generateCalibratePage(int meter) {
  String page = generateHeader("Calibrate Meter " + String(meter + 1));
  page += "<div class='card'>";
  page += "<h1>Calibrate Tap " + String(meter + 1) + "</h1>";
  page += "<h3>" + flowMeterNames[meter] + "</h3>";
  page += "</div>";

  page += "<div class='card'>";
  page += "<h2>Calibration Procedure</h2>";
  page +=
    "<p style='margin-bottom:16px;'>"
    "1. Click <strong>Start</strong>, then pour a precisely measured volume from this tap.<br>"
    "2. Click <strong>Stop</strong> and enter the exact volume poured in mL.<br>"
    "3. The calibration factor is calculated and saved automatically.</p>";
  page += "<button id='startBtn' onclick='startCal()'>Start</button>";
  page += "<button id='stopBtn' onclick='stopCal()' disabled style='opacity:0.5'>Stop</button>";
  page += "<p id='status' style='margin-top:12px;color:#0f9b8e;'></p>";
  page +=
    "<button class='secondary' style='margin-top:16px;' "
    "onclick=\"window.location='/'\">Back to Main</button>";
  page += "</div>";

  page += generateFooter();
  page +=
    "<script>"
    "var startBtn=document.getElementById('startBtn');"
    "var stopBtn=document.getElementById('stopBtn');"
    "var status=document.getElementById('status');"
    "function startCal(){"
    "  startBtn.disabled=true;startBtn.style.opacity='0.5';"
    "  stopBtn.disabled=false;stopBtn.style.opacity='1';"
    "  status.textContent='Counting pulses...';"
    "  fetch('/startCalibration?meter=" + String(meter) + "',{method:'POST'});"
    "}"
    "function stopCal(){"
    "  var ml=prompt('Enter volume poured (mL):');"
    "  if(ml&&parseFloat(ml)>0){"
    "    status.textContent='Saving calibration...';"
    "    fetch('/stopCalibration?meter=" + String(meter) + "&mlPoured='+ml,{method:'POST'})"
    "    .then(r=>r.text()).then(()=>{"
    "      status.textContent='Calibration saved successfully!';"
    "      startBtn.disabled=false;startBtn.style.opacity='1';"
    "      stopBtn.disabled=true;stopBtn.style.opacity='0.5';"
    "    });"
    "  } else { alert('Invalid amount. Calibration aborted.'); }"
    "}"
    "</script></html>";
  return page;
}

String generatePreviousSavesPage() {
  String page = generateHeader("SD Saves");
  page += "<div class='card'><h1>SD Card Saves</h1></div>";
  page += "<div class='card'><h2>End-of-Day Reports</h2>";

  File root = SD.open("/EOD_Saves");
  if (!root) {
    page += "<p style='color:#e94560;'>Could not open /EOD_Saves directory.</p>";
  } else {
    std::vector<String> names;
    while (File entry = root.openNextFile()) {
      if (!entry.isDirectory()) names.push_back(String(entry.name()));
      entry.close();
    }
    root.close();
    std::sort(names.begin(), names.end());
    std::reverse(names.begin(), names.end());

    if (names.empty()) {
      page += "<p style='color:#888;'>No saves found yet.</p>";
    } else {
      for (const String& name : names) {
        page += "<div style='margin-bottom:20px;'>";
        page += "<h3 style='color:#a8dadc;'>" + name + "</h3>";
        page +=
          "<pre style='background:#0f3460;padding:12px;border-radius:8px;"
          "font-size:0.85rem;white-space:pre-wrap;'>";
        String path = "/EOD_Saves/" + name;
        File f = SD.open(path.c_str(), FILE_READ);
        if (f) {
          while (f.available()) page += (char)f.read();
          f.close();
        } else {
          page += "(Error reading file)";
        }
        page += "</pre></div>";
      }
    }
  }

  page +=
    "<button class='secondary' onclick=\"window.location='/'\">Back to Main</button>";
  page += "</div>" + generateFooter() + "</html>";
  return page;
}

// =============================================================
//  HTTP REQUEST HANDLERS
// =============================================================

void handleMainPage(AsyncWebServerRequest* req) {
  adminMode = false;
  req->send(200, "text/html", generateMainPage());
}

void handleAdminPage(AsyncWebServerRequest* req) {
  String code = req->arg("code");
  if (code == adminCode) {
    adminMode = true;
    req->send(200, "text/html", generateAdminPage());
  } else {
    req->send(403, "text/html",
      "<html><body style='background:#1a1a2e;color:#e94560;font-family:sans-serif;"
      "text-align:center;padding:60px;'>"
      "<h2>Access Denied</h2><p>Incorrect admin code.</p>"
      "<a href='/' style='color:#a8dadc;'>Back to main page</a>"
      "</body></html>");
  }
}

void handleCalibrateMeter(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  int meter = req->arg("meter").toInt();
  if (meter < 0 || meter >= NUM_SENSORS) {
    req->send(400, "text/plain", "Invalid meter index"); return;
  }
  req->send(200, "text/html", generateCalibratePage(meter));
}

void handleStartCalibration(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  int meter = req->arg("meter").toInt();
  if (meter < 0 || meter >= NUM_SENSORS) {
    req->send(400, "text/plain", "Invalid meter"); return;
  }
  calibratingMeter = meter;
  calibrationStart = pulseAccum[meter];
  Serial.println("[CAL] Start on meter " + String(meter + 1) +
                 " | baseline = " + String(calibrationStart));
  req->send(200, "text/plain", "OK");
}

void handleStopCalibration(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  if (calibratingMeter == -1) {
    req->send(400, "text/plain", "No calibration in progress"); return;
  }
  float ml = req->arg("mlPoured").toFloat();
  if (ml <= 0) {
    req->send(400, "text/plain", "Invalid volume");
    calibratingMeter = -1;
    return;
  }
  long pulsesDelta = pulseAccum[calibratingMeter] - calibrationStart;
  if (pulsesDelta <= 0) {
    req->send(400, "text/plain", "No pulses detected during calibration");
    calibratingMeter = -1;
    return;
  }
  float factor = ml / (float)pulsesDelta;
  flowMeterCalibrations[calibratingMeter] = factor;
  writeCalibrationValues();
  Serial.println("[CAL] Meter " + String(calibratingMeter + 1) +
                 " calibrated: " + String(factor, 6) + " mL/pulse | " +
                 String(pulsesDelta) + " pulses for " + String(ml) + " mL");
  calibratingMeter = -1;
  req->send(200, "text/plain", "Calibration saved");
}

void handleResetTotalizer(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  int t = req->arg("resetTotalizer").toInt();
  if (t < 1 || t > NUM_SENSORS) {
    req->send(400, "text/plain", "Invalid totalizer number"); return;
  }
  resetFlowMeter(t - 1);
  autoSaveToSD();
  req->send(200, "text/html", generateAdminPage());
}

void handleResetAll(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  for (int i = 0; i < NUM_SENSORS; i++) resetFlowMeter(i);
  autoSaveToSD();
  req->send(200, "text/html", generateAdminPage());
}

void handleSetRTC(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  struct tm ti;
  configTime(GMT_OFFSET_SEC, DST_OFFSET_SEC, NTP_SERVER);
  delay(2000);
  if (getLocalTime(&ti)) {
    rtc.adjust(DateTime(
      ti.tm_year + 1900, ti.tm_mon + 1, ti.tm_mday,
      ti.tm_hour, ti.tm_min, ti.tm_sec));
    Serial.println("[RTC] Synced via NTP");
    req->send(200, "text/html", generateAdminPage());
  } else {
    req->send(500, "text/plain", "NTP sync failed - check Wi-Fi connection");
  }
}

void handleSetTimeOfDay(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  String t = req->arg("timeInput");
  if (!validateTimeInput(t)) {
    req->send(400, "text/plain", "Invalid format - use HH:MM:SS"); return;
  }
  dailyResetTime = t;
  writePrintTime();
  req->send(200, "text/html", generateAdminPage());
}

void handleFlowMeterRename(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  int idx = req->arg("renameMeter").toInt() - 1;
  String newName = req->arg("newName");
  if (idx < 0 || idx >= NUM_SENSORS || newName.length() == 0) {
    req->send(400, "text/plain", "Invalid parameters"); return;
  }
  flowMeterNames[idx] = newName;
  writeFlowMeterNames();
  req->send(200, "text/html", generateAdminPage());
}

void handleToggleUnits(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  useOunces = !useOunces;
  mlOZ = useOunces ? 0.033814f : 1.0f;
  writemlOZ();
  req->send(200, "text/html", generateAdminPage());
}

void handleUpdateLicence(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  licence = req->arg("licence");
  writeLicence();
  readLicence();  // re-validate immediately
  req->send(200, "text/html", generateAdminPage());
}

void handleReprintNewest(AsyncWebServerRequest* req) {
  reprintNewest();
  req->send(200, "text/html", generateMainPage());
}

void handlePrintFlowMeterData(AsyncWebServerRequest* req) {
  esp_task_wdt_init(55, false);
  DateTime now = rtc.now();

  // Save snapshot to /Manual_Saves
  SD.mkdir("/Manual_Saves");
  char fname[48];
  snprintf(fname, sizeof(fname), "/Manual_Saves/%04d%02d%02d_%02d%02d.txt",
           now.year(), now.month(), now.day(), now.hour(), now.minute());
  File f = SD.open(fname, FILE_WRITE);
  if (f) {
    f.println("Manual Print - " + formatDateTime(now));
    f.println(flowMeterDataForSD());
    f.close();
  }

  // Send to thermal printer
  printer.println("--- Beer Trax ---");
  printer.println("Current Totals");
  printer.println(formatDateTime(now));
  printer.feed(1);
  for (int i = 0; i < NUM_SENSORS; i++) {
    String name = flowMeterNames[i];
    name.trim();
    printer.println(name + ": " + String(getMeterVolume(i), 2) + " " + getUnitLabel());
  }
  printer.feed(2);

  req->send(200, "text/html", generateMainPage());
}

void handlePreviousSaves(AsyncWebServerRequest* req) {
  req->send(200, "text/html", generatePreviousSavesPage());
}

void handleChangeWifi(AsyncWebServerRequest* req) {
  if (!adminMode) { req->send(403, "text/plain", "Access denied"); return; }
  String newSSID = req->arg("newSSID");
  String newPass = req->arg("newPASS");
  if (newSSID.length() == 0 || newPass.length() == 0) {
    req->send(400, "text/plain", "SSID and password cannot be empty"); return;
  }
  ssid = newSSID;
  password = newPass;
  writeCredentials();
  Serial.println("[WiFi] Credentials updated. Reboot to connect to the new network.");
  req->send(200, "text/html", generateAdminPage());
}

// =============================================================
//  FLOW METER CONTROL
// =============================================================

void resetFlowMeter(int idx) {
  if (idx >= 0 && idx < NUM_SENSORS) {
    pulseAccum[idx] = 0;
    Serial.println("[RESET] Meter " + String(idx + 1) + " cleared");
  }
}

// =============================================================
//  DATA FORMATTING
// =============================================================

String formatDateTime(DateTime dt) {
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d/%02d/%02d %02d:%02d:%02d",
           dt.year(), dt.month(), dt.day(),
           dt.hour(), dt.minute(), dt.second());
  return String(buf);
}

String flowMeterDataForSD() {
  String unit = getUnitLabel();
  String out  = "Flow Meter Data (" + unit + "):\n";
  for (int i = 0; i < NUM_SENSORS; i++) {
    String name = flowMeterNames[i];
    name.trim();
    out += name + ": " + String(getMeterVolume(i), 2) + " " + unit + "\n";
  }
  return out;
}

bool validateTimeInput(String t) {
  int h, m, s;
  return (sscanf(t.c_str(), "%d:%d:%d", &h, &m, &s) == 3
          && h >= 0 && h < 24
          && m >= 0 && m < 60
          && s >= 0 && s < 60);
}

int getHour(String t)   { int h, m, s; sscanf(t.c_str(), "%d:%d:%d", &h, &m, &s); return h; }
int getMinute(String t) { int h, m, s; sscanf(t.c_str(), "%d:%d:%d", &h, &m, &s); return m; }

// =============================================================
//  PRINTER FUNCTIONS
// =============================================================

void printFlowDataToFile() {
  DateTime now = rtc.now();

  // Thermal printer - EOD report
  printer.println("--- Beer Trax ---");
  printer.println("END OF DAY REPORT");
  printer.println(formatDateTime(now));
  printer.feed(1);
  for (int i = 0; i < NUM_SENSORS; i++) {
    String name = flowMeterNames[i];
    name.trim();
    printer.println(name + ": " + String(getMeterVolume(i), 2) + " " + getUnitLabel());
  }
  if (soonExpire) {
    printer.feed(1);
    printer.println("!! LICENCE EXPIRING SOON !!");
    printer.println("Expires in " + String(whenExpired) + " days");
  }
  printer.feed(3);

  // Save to /EOD_Saves
  SD.mkdir("/EOD_Saves");
  char fname[48];
  snprintf(fname, sizeof(fname), "/EOD_Saves/%04d%02d%02d_%02d%02d.txt",
           now.year(), now.month(), now.day(), now.hour(), now.minute());
  File f = SD.open(fname, FILE_WRITE);
  if (f) {
    f.println("END OF DAY - " + formatDateTime(now));
    f.println(flowMeterDataForSD());
    f.close();
    Serial.println("[EOD] Report saved: " + String(fname));
  } else {
    Serial.println("[EOD] ERROR: Could not write " + String(fname));
  }
}

void reprintNewest() {
  File root = SD.open("/EOD_Saves");
  if (!root) { printer.println("No EOD_Saves folder found"); return; }

  std::vector<String> names;
  while (File e = root.openNextFile()) {
    if (!e.isDirectory()) names.push_back(String(e.name()));
    e.close();
  }
  root.close();

  if (names.empty()) { printer.println("No EOD saves found"); return; }

  std::sort(names.begin(), names.end());
  String newest = names.back();
  printer.println("--- Beer Trax ---");
  printer.println("REPRINT: " + newest);
  printer.feed(1);
  String path = "/EOD_Saves/" + newest;
  File f = SD.open(path.c_str(), FILE_READ);
  if (!f) { printer.println("Error reading file"); return; }
  while (f.available()) {
    printer.println(f.readStringUntil('\n'));
  }
  f.close();
  printer.feed(2);
}

// =============================================================
//  SD CARD - READ FUNCTIONS
// =============================================================

void readFlowMeterNames() {
  File f = SD.open("/config.txt", FILE_READ);
  if (!f) { Serial.println("[SD] config.txt not found, using defaults"); return; }
  for (int i = 0; i < NUM_SENSORS && f.available(); i++) {
    String line = f.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) flowMeterNames[i] = line;
  }
  f.close();
}

void readCalibrationValues() {
  File f = SD.open("/calibration.txt", FILE_READ);
  if (!f) { Serial.println("[SD] calibration.txt not found, using defaults"); return; }
  for (int i = 0; i < NUM_SENSORS && f.available(); i++) {
    float v = f.readStringUntil('\n').toFloat();
    if (v > 0) flowMeterCalibrations[i] = v;
  }
  f.close();
  Serial.println("[SD] Calibration values loaded");
}

void readmlOZ() {
  File f = SD.open("/mlOZ.txt", FILE_READ);
  if (!f) { Serial.println("[SD] mlOZ.txt not found, defaulting to mL"); return; }
  if (f.available()) {
    useOunces = (f.readStringUntil('\n').toInt() == 1);
    mlOZ = useOunces ? 0.033814f : 1.0f;
  }
  f.close();
}

void readPrintTime() {
  File f = SD.open("/time.txt", FILE_READ);
  if (!f) {
    Serial.println("[SD] time.txt not found, using default: " + dailyResetTime);
    return;
  }
  if (f.available()) {
    String t = f.readStringUntil('\n');
    t.trim();
    if (validateTimeInput(t)) dailyResetTime = t;
  }
  f.close();
}

void readAutosave() {
  File f = SD.open("/Autosave.txt", FILE_READ);
  if (!f) {
    Serial.println("[SD] Autosave.txt not found, starting from zero");
    return;
  }
  for (int i = 0; i < NUM_SENSORS && f.available(); i++) {
    long v = f.readStringUntil('\n').toInt();
    if (v >= 0) pulseAccum[i] = v;
  }
  f.close();
  Serial.println("[SD] Autosave restored");
}

void readCredentials() {
  // Wi-Fi SSID
  File sf = SD.open("/ssid.txt", FILE_READ);
  if (sf) {
    String s = sf.readStringUntil('\n');
    s.trim();
    if (s.length() > 0) ssid = s;
    sf.close();
  }
  // Wi-Fi password
  File pf = SD.open("/password.txt", FILE_READ);
  if (pf) {
    String p = pf.readStringUntil('\n');
    p.trim();
    if (p.length() > 0) password = p;
    pf.close();
  }
  // Admin code - stored separately, never in source code
  File af = SD.open("/admincode.txt", FILE_READ);
  if (af) {
    String a = af.readStringUntil('\n');
    a.trim();
    if (a.length() > 0) adminCode = a;
    af.close();
    Serial.println("[SD] Admin code loaded");
  } else {
    Serial.println("[SD] admincode.txt not found - using fallback " DEFAULT_ADMIN_CODE);
  }
  Serial.println("[SD] Credentials loaded (SSID: " + ssid + ")");
}

void readLicence() {
  DateTime now = rtc.now();
  File f = SD.open("/licence.txt", FILE_READ);
  if (!f) {
    licenceValid = false;
    expiration = "No licence file";
    return;
  }
  if (f.available()) {
    licence = f.readStringUntil('\n');
    licence.trim();
    expiration = decrypt(licence);
  }
  f.close();

  if (expiration.length() != 10) {
    expiration = "Invalid licence";
    licenceValid = false;
    return;
  }

  int ey = expiration.substring(0, 4).toInt();
  int em = expiration.substring(5, 7).toInt();
  int ed = expiration.substring(8, 10).toInt();
  DateTime expiryDt(ey, em, ed, 0, 0, 0);
  TimeSpan remaining = expiryDt - now;
  whenExpired = remaining.days();
  soonExpire  = (whenExpired >= 0 && whenExpired <= 15);

  licenceValid = !(ey < now.year() ||
                  (ey == now.year() && em < now.month()) ||
                  (ey == now.year() && em == now.month() && ed < now.day()));

  if (!licenceValid) expiration = "Expired (" + expiration + ")";
  Serial.println("[LIC] Valid=" + String(licenceValid) + " | Expiry: " + expiration);
}

// =============================================================
//  SD CARD - WRITE FUNCTIONS
// =============================================================

void writeFlowMeterNames() {
  File f = SD.open("/config.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing config.txt"); return; }
  for (int i = 0; i < NUM_SENSORS; i++) f.println(flowMeterNames[i]);
  f.close();
}

void writeCalibrationValues() {
  File f = SD.open("/calibration.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing calibration.txt"); return; }
  for (int i = 0; i < NUM_SENSORS; i++) f.println(String(flowMeterCalibrations[i], 8));
  f.close();
}

void writemlOZ() {
  File f = SD.open("/mlOZ.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing mlOZ.txt"); return; }
  f.println(useOunces ? "1" : "0");
  f.close();
}

void writePrintTime() {
  File f = SD.open("/time.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing time.txt"); return; }
  f.println(dailyResetTime);
  f.close();
}

void writeCredentials() {
  File sf = SD.open("/ssid.txt", FILE_WRITE);
  if (sf) { sf.print(ssid); sf.close(); }
  File pf = SD.open("/password.txt", FILE_WRITE);
  if (pf) { pf.print(password); pf.close(); }
}

void writeLicence() {
  File f = SD.open("/licence.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing licence.txt"); return; }
  f.print(licence);
  f.close();
}

void autoSaveToSD() {
  File f = SD.open("/Autosave.txt", FILE_WRITE);
  if (!f) { Serial.println("[SD] Error writing Autosave.txt"); return; }
  for (int i = 0; i < NUM_SENSORS; i++) f.println(pulseAccum[i]);
  f.close();
  Serial.println("[SD] Autosave written");
}

/**
 * Creates required folders and default config files if missing.
 * Runs once on very first boot (or after SD card replacement).
 */
void firstBootInit() {
  SD.mkdir("/EOD_Saves");
  SD.mkdir("/Manual_Saves");

  if (!SD.exists("/admincode.txt")) {
    File f = SD.open("/admincode.txt", FILE_WRITE);
    if (f) { f.println(DEFAULT_ADMIN_CODE); f.close(); }
    Serial.println("[BOOT] Created admincode.txt with default PIN " DEFAULT_ADMIN_CODE
                   " - CHANGE THIS before deploying!");
  }
  if (!SD.exists("/time.txt")) {
    File f = SD.open("/time.txt", FILE_WRITE);
    if (f) { f.println(DEFAULT_RESET_TIME); f.close(); }
  }
  if (!SD.exists("/mlOZ.txt")) {
    File f = SD.open("/mlOZ.txt", FILE_WRITE);
    if (f) { f.println("0"); f.close(); }
  }
}

// =============================================================
//  LICENCE ENCRYPTION
//  Simple symmetric obfuscation. Format: "YYYY/MM/DD" <-> token.
//  Not cryptographic — intended as a basic deployment gate.
// =============================================================

static const char ENC_MAP[] = "ptfxjokqua";  // maps digits 0-9 to letters

String encrypt(String date) {
  date.replace("/", "");
  long num = (date.toInt() + 125345L) * 4353L;
  if (num <= 0) return "";
  String out = "";
  while (num > 0) {
    out = String(ENC_MAP[num % 10]) + out;
    num /= 10;
  }
  return out;
}

String decrypt(String enc) {
  long num = 0;
  for (unsigned int i = 0; i < enc.length(); i++) {
    const char* p = strchr(ENC_MAP, enc[i]);
    if (!p) return "";
    num = num * 10 + (int)(p - ENC_MAP);
  }
  num /= 4353L;
  num -= 125345L;
  if (num < 0) return "";
  char buf[12];
  snprintf(buf, sizeof(buf), "%08ld", num);
  return String(buf).substring(0, 4) + "/" +
         String(buf).substring(4, 6) + "/" +
         String(buf).substring(6, 8);
}

// =============================================================
//  SETUP
// =============================================================

void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Beer Trax v1.0.0 starting...");

  pcntInit();
  Serial.println("[BOOT] PCNT initialised (" + String(NUM_SENSORS) + " channels)");

  Wire.begin();
  if (!rtc.begin()) {
    Serial.println("[BOOT] ERROR: DS3231 RTC not found on I2C bus!");
  } else {
    Serial.println("[BOOT] RTC OK - " + formatDateTime(rtc.now()));
  }

  Serial2.begin(PRINTER_BAUD);
  printer.begin();
  Serial.println("[BOOT] Thermal printer ready");

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("[BOOT] ERROR: SD card init failed (CS=GPIO" + String(SD_CS_PIN) + ")");
  } else {
    Serial.println("[BOOT] SD card OK");
    firstBootInit();
    readCredentials();
  }

  // Connect to Wi-Fi with a 20-second timeout
  Serial.println("[WiFi] Connecting to: " + ssid);
  WiFi.begin(ssid, password);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 20000) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected - IP: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n[WiFi] WARNING: Could not connect. "
                   "Update ssid.txt and password.txt on the SD card.");
  }

  // Register all HTTP routes
  server.on("/",                HTTP_GET,  handleMainPage);
  server.on("/admin",           HTTP_GET,  handleAdminPage);
  server.on("/calibrateMeter",  HTTP_GET,  handleCalibrateMeter);
  server.on("/startCalibration",HTTP_POST, handleStartCalibration);
  server.on("/stopCalibration", HTTP_POST, handleStopCalibration);
  server.on("/resetTotalizer",  HTTP_GET,  handleResetTotalizer);
  server.on("/resetAll",        HTTP_GET,  handleResetAll);
  server.on("/setRTC",          HTTP_GET,  handleSetRTC);
  server.on("/setTimeOfDay",    HTTP_GET,  handleSetTimeOfDay);
  server.on("/rename",          HTTP_GET,  handleFlowMeterRename);
  server.on("/toggleUnits",     HTTP_GET,  handleToggleUnits);
  server.on("/printflowdata",   HTTP_GET,  handlePrintFlowMeterData);
  server.on("/previousSaves",   HTTP_GET,  handlePreviousSaves);
  server.on("/changeWifi",      HTTP_GET,  handleChangeWifi);
  server.on("/updateLicence",   HTTP_GET,  handleUpdateLicence);
  server.on("/reprintNewest",   HTTP_GET,  handleReprintNewest);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");

  // Load all persistent configuration from SD
  readLicence();
  if (licenceValid) {
    readFlowMeterNames();
    readCalibrationValues();
    readmlOZ();
    readPrintTime();
    readAutosave();
  }

  // Boot confirmation on thermal printer
  printer.setFont('B');
  printer.println("--- Beer Trax ---");
  printer.println("System Started");
  printer.println(formatDateTime(rtc.now()));
  printer.println("IP: " + WiFi.localIP().toString());
  printer.println("v1.0.0");
  if (!licenceValid) {
    printer.feed(1);
    printer.println("!! LICENCE INVALID !!");
    printer.println("Contact your dealer.");
  }
  printer.feed(2);

  Serial.println("[BOOT] Startup complete.\n");
}

// =============================================================
//  MAIN LOOP  (runs every ~10 seconds)
// =============================================================

void loop() {
  // Drain hardware PCNT counters into software accumulators.
  // The PCNT register is 16-bit and wraps; we read and clear atomically.
  int16_t count = 0;
  for (int i = 0; i < NUM_SENSORS; i++) {
    pcnt_get_counter_value((pcnt_unit_t)i, &count);
    pcnt_counter_clear((pcnt_unit_t)i);
    pulseAccum[i] += count;
  }

  // Serial debug log
  DateTime now = rtc.now();
  Serial.print("[LOOP] " + formatDateTime(now) + " | ");
  for (int i = 0; i < NUM_SENSORS; i++) {
    Serial.print("M" + String(i + 1) + "=" + String(pulseAccum[i]));
    if (i < NUM_SENSORS - 1) Serial.print(" ");
  }
  Serial.println();

  // Scheduled daily reset.
  // sdPauseCount > 6 guard prevents re-triggering for ~60 seconds after reset.
  if (now.hour()   == getHour(dailyResetTime) &&
      now.minute() == getMinute(dailyResetTime) &&
      sdPauseCount > 6) {
    Serial.println("[EOD] Daily reset triggered at " + formatDateTime(now));
    sdPauseCount = 0;
    printFlowDataToFile();
    for (int i = 0; i < NUM_SENSORS; i++) resetFlowMeter(i);
    autoSaveToSD();
    delay(500);
    ESP.restart();
  }

  // Periodic autosave every AUTOSAVE_INTERVAL iterations (~5 minutes)
  autosaveCount++;
  if (autosaveCount >= AUTOSAVE_INTERVAL) {
    autosaveCount = 0;
    autoSaveToSD();
  }

  sdPauseCount++;
  delay(10000);  // 10-second polling interval
}
