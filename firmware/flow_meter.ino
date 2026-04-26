#include <Wire.h>
#include <RTClib.h>
#include <ESPAsyncWebSrv.h>
#include <WiFi.h>
#include <time.h>
#include <SPI.h>
#include <SD.h>
#include "Adafruit_Thermal.h"
#include "driver/pcnt.h"
//#include "soc/rtc_wdt.h"
#include <esp_task_wdt.h>
#include <Vector.h>
// Replace with your network credentials
String ssid = "Ultra Fast Super Sped Wifi";
String password = "9DC23F461EEE";

String licence = "";
String expiration = "";
bool licenceValid = false;
int whenExpired = 0;
bool soonExpire = false;

AsyncWebServer server(80);

#define PCNT_H_LIM_VAL      32767  // Maximum value for the high limit
#define PCNT_L_LIM_VAL     -32768  // Minimum value for the low limit
#define PCNT_FILTER_VAL     1023    // Glitch filter value in APB_CLK cycles

Adafruit_Thermal printer(&Serial2);

RTC_DS3231 rtc;

const char *ntpServer = "pool.ntp.org";
String timeInput = "02:30:00";
const long gmtOffset_sec = -25200;                                                                            
const int daylightOffset_sec = 3600;

const int sensorPins[] = {4, 0, 32, 33, 25, 26, 27, 14};
const int numSensors = sizeof(sensorPins) / sizeof(sensorPins[0]);

volatile long pulse1 = 0;
volatile long pulse2 = 0;
volatile long pulse3 = 0;
volatile long pulse4 = 0;
volatile long pulse5 = 0;
volatile long pulse6 = 0;
volatile long pulse7 = 0;
volatile long pulse8 = 0;

volatile long* pulses[] = {&pulse1, &pulse2, &pulse3, &pulse4, &pulse5, &pulse6, &pulse7, &pulse8};

bool adminMode = false;
const int adminCode = 1234;  // Replace with your admin code

// Calibration variables
int calibratingMeter = -1;  // -1 indicates no ongoing calibration
long calibrationPulses = 0;
long calibrationStart = 0;
float calibrationAmount = 0;
float mlPoured = 0;
float flowMeterCalibrations[8] = {1, 1, 1, 1, 1, 1, 1, 1}; // Calibration values for each flow meter


int sdPause = 0;
int sdAuto = 0;
int flowmeterRepeat = 0;

// Flow Meter names
String flowMeterNames[8] = { "Flow Meter 1", "Flow Meter 2", "Flow Meter 3", "Flow Meter 4", "Flow Meter 5", "Flow Meter 6", "Flow Meter 7", "Flow Meter 8" };
bool useOunces = false; // Default to milliliters
float mlOZ = 1.0; // Default to milliliters


void pcnt_init() {
    pcnt_config_t pcnt_config;

    for (int i = 0; i < numSensors; i++) {
        pcnt_config.pulse_gpio_num = sensorPins[i];
        pcnt_config.ctrl_gpio_num = PCNT_PIN_NOT_USED;
        pcnt_config.channel = PCNT_CHANNEL_0;
        pcnt_config.pos_mode = PCNT_COUNT_INC;
        pcnt_config.neg_mode = PCNT_COUNT_DIS;
        pcnt_config.unit = (pcnt_unit_t)i;
        pcnt_config.counter_h_lim = PCNT_H_LIM_VAL;
        pcnt_config.counter_l_lim = PCNT_L_LIM_VAL;

        pcnt_unit_config(&pcnt_config);

        // Set the glitch filter value
        pcnt_set_filter_value((pcnt_unit_t)i, PCNT_FILTER_VAL);
        pcnt_filter_enable((pcnt_unit_t)i);

        pcnt_counter_pause((pcnt_unit_t)i);
        pcnt_counter_clear((pcnt_unit_t)i);
        pcnt_counter_resume((pcnt_unit_t)i);
    }
}

String flowMeterData() {
  String unit = useOunces ? "Ounces" : "Milliliters";
  String data = "\
Flow Meter Data:<br>\
" + flowMeterNames[0]
                + ": " + String(pulse1 * flowMeterCalibrations[0] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[1]
                + ": " + String(pulse2 * flowMeterCalibrations[1] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[2]
                + ": " + String(pulse3 * flowMeterCalibrations[2] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[3]
                + ": " + String(pulse4 * flowMeterCalibrations[3] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[4]
                + ": " + String(pulse5 * flowMeterCalibrations[4] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[5]
                + ": " + String(pulse6 * flowMeterCalibrations[5] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[6]
                + ": " + String(pulse7 * flowMeterCalibrations[6] * mlOZ) + " " + unit + "<br>\
" + flowMeterNames[7]
                + ": " + String(pulse8 * flowMeterCalibrations[7] * mlOZ) + " " + unit + "<br><br>";
  return data;
}

String generateMainPage() {
  DateTime now = rtc.now();
  String expiry = expiration;
  String mainPage = "\
<html>\
<head>\
<style>\
  img {\
    max-width: 200px;\
    height: auto;\
    position: absolute;\
    top: 0;\
    right: 0;\
  }\
</style>\
</head>\
<body>\
<img src=\"https://scontent-ord5-2.xx.fbcdn.net/v/t39.30808-6/302413488_583729450120246_4153202502450297240_n.jpg?_nc_cat=103&ccb=1-7&_nc_sid=efb6e6&_nc_ohc=BclCSVOu1BMAX9Rex9b&_nc_ht=scontent-ord5-2.xx&oh=00_AfB2_5s_iwjily9KzjY8hREqyS7Dxz9x-nTym2Ch7puI6A&oe=65C8B875\" alt=\"New Image\">\
<center>\
<h1>Beer Trax</h1>\
<h3>v0.98.5f<br></h3>\
</center>\
<h2>Current totals</h2>\
<h4>" + flowMeterData() + "</h4>\
<h2>Menu Options</h2>\
<h3>1. Print Current Totals<br>\
2. Reprint Newest EOD<br>\
3. SD Saves<br>\
4. Access Admin Menu<br>\
<br></h3>\
<input type=\"text\" id=\"userInput\" onkeydown=\"checkEnter(event)\"><button onclick=\"submitUserInput()\">Submit</button>\ 
<p>IP Address: " + WiFi.localIP().toString()
                    + "</p>\
<p>RTC Time: " + formatTime(now)
                    + "</p>\
<p>Licence Expiration: " + expiry
                    + "</p>\
</body>\
</html>\
<script>\
function submitUserInput() {\
  var userInput = document.getElementById('userInput').value;\
  if (userInput == 1) {\
    alert('Printing Current Totals');\
    window.location.href = '/printflowdata';\
  } else if (userInput == 2) {\
    window.location.href = '/reprintNewest';\
  } else if (userInput == 3) {\
    window.location.href = '/previousSaves';\
  } else if (userInput == 4) {\
    var adminCode = prompt('Enter Admin Code:');\
    window.location.href = '/admin?code=' + adminCode;\
  }\
}\
function checkEnter(event) {\
  if (event.key === 'Enter') {\
    submitUserInput();\
  }\
}\
document.getElementById('userInput').focus();\
</script>";
  return mainPage;
}

String generateAdminPage() {
  DateTime now = rtc.now();
  String expiry = expiration;
  String unit = useOunces ? "Ounces" : "Milliliters";
  String adminPage = "\
<html>\
<head>\
<style>\
  img {\
    max-width: 200px;\
    height: auto;\
    position: absolute;\
    top: 0;\
    right: 0;\
  }\
</style>\
</head>\
<body>\
<img src=\"https://scontent-ord5-2.xx.fbcdn.net/v/t39.30808-6/302413488_583729450120246_4153202502450297240_n.jpg?_nc_cat=103&ccb=1-7&_nc_sid=efb6e6&_nc_ohc=BclCSVOu1BMAX9Rex9b&_nc_ht=scontent-ord5-2.xx&oh=00_AfB2_5s_iwjily9KzjY8hREqyS7Dxz9x-nTym2Ch7puI6A&oe=65C8B875\" alt=\"New Image\">\
<center>\
<h1>Beer Trax</h1>\
<h3>v0.98.5f<br></h3>\
</center>\
<h2>Current totals</h2>\
<h4>" + flowMeterData() + " </h4>\
<h2>Admin Menu Options</h2>\
<h3>1. Update Licence<br>\
2. Change Wifi Credentials<br>\
3. Calibrate Sensor 1-8<br>\
4. Reset Individual Totalizer 1-8<br>\
5. Reset All Totalizers<br>\
6. Set Date and Time<br>\
7. Set Time of Day for Reset & Print (Current Time: "
                     + timeInput + ")<br>\
8. Rename Flow Meters<br>\
9. Switch Units (Current Unit: " + unit + ")<br>\
10. Go back to Main Page<br> </h3>\
<br>\
<input type=\"text\" id=\"adminInput\" onkeydown=\"checkEnter(event)\"><button onclick=\"submitAdminInput()\">Submit</button>\
<br>\
<p>IP Address: " + WiFi.localIP().toString() + "</p>\
<p>RTC Time: " + formatTime(now) + "</p>\
<p>Licence Expiration: " + expiry + "</p>\
</body>\
</html>\
<script>\
function submitAdminInput() {\
  var adminInput = document.getElementById('adminInput').value;\
  if (adminInput == 3) {\
    var meterToCalibrate = prompt('Enter Flow Meter number to calibrate (1-8):');\
    if (meterToCalibrate != null && meterToCalibrate >= 1 && meterToCalibrate <= 8) {\
      meterToCalibrate = parseInt(meterToCalibrate) - 1;\
      window.location.href = '/calibrateMeter?meter=' + meterToCalibrate;\
    } else {\
      alert('Invalid Flow Meter number');\
    }\
  } else if (adminInput == 4) {\
    var totalizer = prompt('Enter Totalizer Number (1-8):');\
    window.location.href = '/resetTotalizer?resetTotalizer=' + totalizer;\
  } else if (adminInput == 5) {\
    if (confirm('Are you sure you want to reset all totalizers?')) {\
      window.location.href = '/resetAll';\
    }\
  } else if (adminInput == 6) {\
    alert('Setting RTC');\
    window.location.href = '/setRTC';\
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));\
  } else if (adminInput == 7) {\
    var timeInput = prompt('Enter Time of Day for Reset & Print (HH:MM:SS):');\
    window.location.href = '/setTimeOfDay?timeInput=' + timeInput;\
  } else if (adminInput == 8) {\
    var meterToRename = prompt('Select Flow Meter to Rename (1-8):');\
    var newName = prompt('Enter New Name:');\
    window.location.href = '/rename?renameMeter=' + meterToRename + '&newName=' + newName;\
  } else if (adminInput == 9) {\
    window.location.href = '/toggleUnits';\
  } else if (adminInput == 1) {\
    var newLicence = prompt('Enter Licence:');\
    window.location.href = '/updateLicence?licence=' + newLicence;\
  } else if (adminInput == 2) {\
    var newSSID = prompt('Enter New SSID');\
    var newPASS = prompt('Enter New Password');\
    var url = '/changeWifi?newSSID=' + encodeURIComponent(newSSID) + '&newPASS=' + encodeURIComponent(newPASS);\
  window.location.href = url;\
  } else if (adminInput == 10) {\
    window.location.href = '/';\
  }\
}\
function checkEnter(event) {\
  if (event.key === 'Enter') {\
    submitAdminInput();\
  }\
}\
document.getElementById('adminInput').focus();\
</script>";
  return adminPage;
}

String generateCalibratePage(int meterToCalibrate) {
  String calibratePage = "\
<html>\
<body>\
<h2>Calibrate Flow Meter " + String(meterToCalibrate + 1) + "</h2>\
Select START, Pour a metered quantity from Choosen tap. Select STOP and enter quantity dispensed.\
<form id=\"calibrateForm\">\
  <button type=\"button\" onclick=\"startCalibration(" + meterToCalibrate + ")\" id=\"startButton\">Start Calibration</button>\
  <button type=\"button\" onclick=\"stopCalibration(" + meterToCalibrate + ")\" id=\"stopButton\" disabled>Stop Calibration</button>\
</form>\
<script>\
var startButton = document.getElementById('startButton');\
var stopButton = document.getElementById('stopButton');\
\
function startCalibration(meterToCalibrate) {\
  startButton.disabled = true;\
  stopButton.disabled = false;\
  fetch('/startCalibration?meter=' + meterToCalibrate, {\
    method: 'POST'\
  });\
}\
\
function stopCalibration(meterToCalibrate) {\
  var mlPoured = prompt('Enter the amount of liquid poured (ml):');\
  if (mlPoured !== null) {\
    fetch('/stopCalibration?meter=' + meterToCalibrate + '&mlPoured=' + mlPoured, {\
      method: 'POST'\
    }).then(function(response) {\
      return response.text();\
    }).then(function(data) {\
      startButton.disabled = false;\
      stopButton.disabled = true;\
      window.location.href = '/admin?code=1234';\
    });\
  } else {\
    alert('Calibration aborted. Please enter a valid amount of liquid.');\
  }\
}\
\
document.addEventListener('keydown', function(event) {\
  if (event.key === 'Enter') {\
    if (startButton.disabled) {\
      stopCalibration(" + meterToCalibrate + ");\
    } else {\
      startCalibration(" + meterToCalibrate + ");\
    }\
  }\
});\
</script>\
</body>\
</html>";
  return calibratePage;
}

String generatePreviousSavesPage() {
  String previousSavesPage = "<html><body><h2>SD Saves:</h2>";

  // Open the "EOD_Saves" folder on the SD card
  File root = SD.open("/EOD_Saves");

  // Check if the folder opened successfully
  if (root) {
    // Create a vector to store the file names
    std::vector<String> fileNames;

    while (File entry = root.openNextFile()) {
      if (entry.isDirectory()) {
        // Skip directories
        continue;
      }

      // Add the file name to the vector
      fileNames.push_back(entry.name());
      entry.close();
    }

    // Reverse the order of the vector
    std::reverse(fileNames.begin(), fileNames.end());

    // Iterate over the vector in reverse order
    for (const String& fileName : fileNames) {
      previousSavesPage += "<h3>File Name: " + fileName + "</h3>";

      // Read the file content and append it to the HTML page
      String filePath = "/EOD_Saves/" + fileName;
      File file = SD.open(filePath.c_str(), FILE_READ);
      if (file) {
        while (file.available()) {
          previousSavesPage += file.readStringUntil('\n') + "<br>";
        }
        file.close();
      } else {
        previousSavesPage += "Error reading file: " + fileName + "<br>";
      }

      previousSavesPage += "<br>";
    }

    root.close();
  } else {
    previousSavesPage += "Error opening directory /EOD_Saves<br>";
  }

  previousSavesPage += "</body></html>";
  return previousSavesPage;
}


String formatTime(DateTime time) {
  return String(time.year(), DEC) + '/' + String(time.month(), DEC) + '/' + String(time.day(), DEC) + ' ' + String(time.hour(), DEC) + ':' + String(time.minute(), DEC) + ':' + String(time.second(), DEC);
}

void handleMainPage(AsyncWebServerRequest *request) {
    adminMode = false;
    request->send(200, "text/html", generateMainPage());
}

void handleAdminPage(AsyncWebServerRequest *request) {
  if (!adminMode) {
    String code = request->arg("code");
    if (code == String(adminCode)) {
      adminMode = true;
      request->send(200, "text/html", generateAdminPage());
    } else {
      request->send(403, "text/plain", "Access denied");
    }
  } else {
    String adminInput = request->arg("adminInput");
    if (adminInput == "8") {
    request->send(200, "text/html", generateMainPage());
      adminMode = false;
      request->send(200, "text/html", generateMainPage());
    } else {
      request->send(200, "text/html", generateAdminPage());
    }
  }
}

void handleCalibrateMeter(AsyncWebServerRequest *request) {
  int meterToCalibrate = request->arg("meter").toInt();
  if (request->method() == HTTP_GET) {
    startCalibration(request);
    String calibratePage = generateCalibratePage(meterToCalibrate);
    request->send(200, "text/html", calibratePage);
  } else if (request->method() == HTTP_POST) {
    float mlPoured = request->arg("mlPoured").toFloat();
    stopCalibration(request);
  } else {
    request->send(405, "text/plain", "Method Not Allowed");
  }
}

void handleResetTotalizer(AsyncWebServerRequest *request) { 
  if (adminMode) {
    int totalizer = request->arg("resetTotalizer").toInt();
    resetFlowMeter(totalizer - 1);
    request->send(200, "text/html", generateAdminPage());
  } else {
    request->send(403, "text/plain", "Access denied");
  }
}

void handleResetAll(AsyncWebServerRequest *request) {
  if (adminMode) {
    for (int i = 0; i < 8; i++) {
      resetFlowMeter(i);
    }
    request->send(200, "text/html", generateAdminPage());
  } else {
    request->send(403, "text/plain", "Access denied");
  }
}

void handleSetRTC(AsyncWebServerRequest *request) {
  if (adminMode) {
    struct tm timeinfo;
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    if (getLocalTime(&timeinfo)) {
      rtc.adjust(DateTime(timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec));
      request->send(200, "text/html", generateAdminPage());
    } else {
      request->send(500, "text/plain", "Failed to obtain time");
    }
  } else {
    request->send(403, "text/plain", "Access denied, Press Browser Back Button");
  }
}

void handleSetTimeOfDay(AsyncWebServerRequest *request) {
  if (adminMode) {
    timeInput = request->arg("timeInput");
    if (validateTimeInput(timeInput)) {
      // Set the time here
      writePrintTime();
      request->send(200, "text/html", generateAdminPage());
    } else {
      request->send(400, "text/plain", "Invalid time format");
    }
  } else {
    request->send(403, "text/plain", "Access denied");
  }
}

void handleFlowMeterRename(AsyncWebServerRequest *request) {
  String meterToRename = request->arg("renameMeter");
  String newName = request->arg("newName");
  int meterIndex = meterToRename.toInt() - 1;

  if (meterIndex >= 0 && meterIndex < 8) {
    flowMeterNames[meterIndex] = newName;
    writeFlowMeterNames();  // Call the function to write names to the SD card
    request->send(200, "text/html", generateAdminPage());
  } else {
    request->send(400, "text/plain", "Invalid Flow Meter number");
  }
}

void handleToggleUnits(AsyncWebServerRequest *request) {
  useOunces = !useOunces;
  mlOZ = useOunces ? 0.033814 : 1.0; // Update mlOZ based on the selected unit
  String unit = useOunces ? "Ounces" : "Milliliters";
  writemlOZ();
  adminMode = true;
  request->send(200, "text/html", generateAdminPage());
}

void handleUpdateLicence(AsyncWebServerRequest *request) {
  if (adminMode) {
  licence = request->arg("licence");
  writeLicence();
  adminMode = true;
  request->send(200, "text/html", generateAdminPage());
  } else {
    request->send(403, "text/plain", "Access denied");
  }
}

void handleReprintNewest(AsyncWebServerRequest *request) {
  reprintNewest();
  request->send(200, "text/html", generateMainPage());

}

void handlePrintFlowMeterData(AsyncWebServerRequest *request) {
  esp_task_wdt_init(55, false);
  // Get the current date and time
  DateTime now = rtc.now();

 
  Serial.println("Current totals here that dont blow up");
  String unit = useOunces ? "Ounces" : "Milliliters";



  String folderName = "/Manual_Saves";

  // Create the folder if it doesn't exist
  SD.mkdir(folderName.c_str());

  // Generate a filename based on the date and time within the folder
  String filename = folderName + "/" + String(now.year(), DEC) + String(now.month(), DEC) + String(now.day(), DEC) +
                    "_" + String(now.hour(), DEC) + String(now.minute(), DEC) + ".txt";

  // Open the file on the SD card for writing
  File dataFile = SD.open(filename, FILE_WRITE);

  // Check if the file opened successfully
  if (dataFile) {
    // Write flow meter data to the file
    dataFile.println(flowMeterDataForSD());
    // Close the file
    dataFile.close();
Serial.println("data saved in manual saves");
  printer.println("Current Totals");
  printer.feed(3);
  printer.println(String(now.year(), DEC) + String(now.month(), DEC) + String(now.day(), DEC) +
                    " " + String(now.hour(), DEC) + String(now.minute(), DEC));
                    
  printer.println(flowMeterNames[0] + ": " + String(pulse1 * flowMeterCalibrations[0] * mlOZ) + unit);
  printer.println(flowMeterNames[1] + ": " + String(pulse2 * flowMeterCalibrations[1] * mlOZ) + unit);
  printer.println(flowMeterNames[2] + ": " + String(pulse3 * flowMeterCalibrations[2] * mlOZ) + unit);
  printer.println(flowMeterNames[3] + ": " + String(pulse4 * flowMeterCalibrations[3] * mlOZ) + unit);
  printer.println(flowMeterNames[4] + ": " + String(pulse5 * flowMeterCalibrations[4] * mlOZ) + unit);
  printer.println(flowMeterNames[5] + ": " + String(pulse6 * flowMeterCalibrations[5] * mlOZ) + unit);
  printer.println(flowMeterNames[6] + ": " + String(pulse7 * flowMeterCalibrations[6] * mlOZ) + unit);
  printer.println(flowMeterNames[7] + ": " + String(pulse8 * flowMeterCalibrations[7] * mlOZ) + unit);
  printer.feed(2);
  delay(5);
    // Send a response to the client
    request->send(200, "text/html", generateMainPage());
  } else {
    // If the file couldn't be opened, send an error response
    request->send(500, "text/plain", "Error opening file for writing.");
  }
}

void handlePreviousSaves(AsyncWebServerRequest *request) {
  String response = generatePreviousSavesPage();
  request->send(200, "text/html", response);
}

void startCalibration(AsyncWebServerRequest *request) {
  Serial.println("Starting Calibration");
  int meterToCalibrate = request->arg("meter").toInt();
  calibratingMeter = meterToCalibrate;
  calibrationStart = *pulses[meterToCalibrate];
  Serial.println(meterToCalibrate);
  calibrationPulses = 0;

  String calibratePage = generateCalibratePage(meterToCalibrate);
  request->send(200, "text/html", calibratePage);
}

void stopCalibration(AsyncWebServerRequest *request) {
  if (calibratingMeter != -1) {
    int meterToCalibrate = calibratingMeter;
    calibrationPulses = *pulses[meterToCalibrate];

    // Retrieve the mlPoured value from the POST request
    String mlPouredStr = request->arg("mlPoured");
    mlPoured = mlPouredStr.toFloat();

    if (mlPoured > 0) {
      Serial.println(meterToCalibrate + 1);
      Serial.println(calibrationPulses);
      Serial.println(calibrationPulses - calibrationStart);
      calibrationAmount = mlPoured / (calibrationPulses - calibrationStart);
      flowMeterCalibrations[calibratingMeter] = calibrationAmount;
      writeCalibrationValues();
      Serial.println("Calibration for Flow Meter " + String(meterToCalibrate + 1) + " complete with " + flowMeterCalibrations[calibratingMeter] + " pulse/ml");
      request->send(200, "text/html", generateAdminPage());
    } else {
      Serial.println("Calibration for Flow Meter " + String(meterToCalibrate + 1) + " aborted");
      request->send(200, "text/html", generateAdminPage());
    }
    calibratingMeter = -1;  // Reset calibration state
    calibrationPulses = 0;
    calibrationStart = 0;
  } else {
    request->send(400, "text/plain", "No ongoing calibration");
  }
}

void handleChangeWifi(AsyncWebServerRequest *request) {
  String newSSID = String(request->arg("newSSID"));
  String newPASS = String(request->arg("newPASS"));
  if (newSSID != NULL && newPASS != NULL) {
  // Debugging output
  Serial.print("Received SSID: ");
  Serial.println(newSSID);
  Serial.print("Received Password: ");
  Serial.println(newPASS);

  ssid = newSSID;
  password = newPASS;

  writeCredentials();

  request->send(200, "text/html", generateAdminPage());
  }
}

void reprintNewest() {
  // Open the "EOD_Saves" folder on the SD card
  File root = SD.open("/EOD_Saves");

  // Check if the folder opened successfully
  if (!root) {
    printer.println("Error opening directory /EOD_Saves");
    return;
  }

  // Create a vector to store the file names
  std::vector<String> fileNames;

  // Open all the files in the directory
  while (File entry = root.openNextFile()) {
    if (entry.isDirectory()) {
      // Skip directories
      continue;
    }

    // Add the file name to the vector
    fileNames.push_back(entry.name());
    entry.close();
  }

  // Check if there are any files in the directory
  if (fileNames.empty()) {
    printer.println("No files in directory /EOD_Saves");
    return;
  }

  // Get the name of the last file
  String lastFileName = fileNames.back();

  // Print "Reprint Totals" and the filename
  printer.println("Reprint Totals");
  printer.println("Date: " + lastFileName);

  // Open the last file
  String filePath = "/EOD_Saves/" + lastFileName;
  File file = SD.open(filePath.c_str(), FILE_READ);
  if (!file) {
    printer.println("Error reading file: " + lastFileName);
    return;
  }

  // Print the first 9 lines of the file
  for (int i = 0; i < 9 && file.available(); i++) {
    printer.println(file.readStringUntil('\n'));
  }

  // Close the file
  file.close();

  // Close the directory
  root.close();
}





void resetFlowMeter(int meterIndex) {
  switch (meterIndex) {
    case 0:
      pulse1 = 0;
      break;
    case 1:
      pulse2 = 0;
      break;
    case 2:
      pulse3 = 0;
      break;
    case 3:
      pulse4 = 0;
      break;
    case 4:
      pulse5 = 0;
      break;
    case 5:
      pulse6 = 0;
      break;
    case 6:
      pulse7 = 0;
      break;
    case 7:
      pulse8 = 0;
      break;
  }
}

String flowMeterDataForSD() {
  String unit = useOunces ? "Ounces" : "Milliliters";
  String dataForSD = "Flow Meter Data:\n";

  // Flow Meter 1
  flowMeterNames[0].trim();
  dataForSD += flowMeterNames[0] + ": " + String(pulse1 * flowMeterCalibrations[0] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 2
  flowMeterNames[1].trim();
  dataForSD += flowMeterNames[1] + ": " + String(pulse2 * flowMeterCalibrations[1] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 3
  flowMeterNames[2].trim();
  dataForSD += flowMeterNames[2] + ": " + String(pulse3 * flowMeterCalibrations[2] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 4
  flowMeterNames[3].trim();
  dataForSD += flowMeterNames[3] + ": " + String(pulse4 * flowMeterCalibrations[3] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 5
  flowMeterNames[4].trim();
  dataForSD += flowMeterNames[4] + ": " + String(pulse5 * flowMeterCalibrations[4] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 6
  flowMeterNames[5].trim();
  dataForSD += flowMeterNames[5] + ": " + String(pulse6 * flowMeterCalibrations[5] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 7
  flowMeterNames[6].trim();
  dataForSD += flowMeterNames[6] + ": " + String(pulse7 * flowMeterCalibrations[6] * mlOZ, 2) + " " + unit + "\n";

  // Flow Meter 8
  flowMeterNames[7].trim();
  dataForSD += flowMeterNames[7] + ": " + String(pulse8 * flowMeterCalibrations[7] * mlOZ, 2) + " " + unit + "\n";

  return dataForSD;
}


bool validateTimeInput(String timeInput) {
  // Validate the time input in HH:MM:SS format
  int h, m, s;
  if (sscanf(timeInput.c_str(), "%d:%d:%d", &h, &m, &s) == 3) {
    return (h >= 0 && h < 24 && m >= 0 && m < 60 && s >= 0 && s < 60);
  }
  return false;
}

int getHourFromTimeInput() {
  int h, m, s;
  sscanf(timeInput.c_str(), "%d:%d:%d", &h, &m, &s);
  return h;
}

int getMinuteFromTimeInput() {
  int h, m, s;
  sscanf(timeInput.c_str(), "%d:%d:%d", &h, &m, &s);
  return m;
}

int getSecondFromTimeInput() {
  int h, m, s;
  sscanf(timeInput.c_str(), "%d:%d:%d", &h, &m, &s);
  return s;
}

void readFlowMeterNames() {
  File configFile = SD.open("/config.txt", FILE_READ);

  if (configFile) {
    for (int i = 0; i < 8; i++) {
      if (configFile.available()) {
        String line = configFile.readStringUntil('\n');
        flowMeterNames[i] = line;
      }
    }

    configFile.close();
  } else {
    Serial.println("Error opening config file for reading.");
  }
}

void readCalibrationValues() {
  File calibFile = SD.open("/calibration.txt", FILE_READ);

  if (calibFile) {
    for (int i = 0; i < 8; i++) {
      if (calibFile.available()) {
        String line = calibFile.readStringUntil('\n');
        flowMeterCalibrations[i] = line.toFloat();
      }
    }

    calibFile.close();
    Serial.println("Calibration values read from file.");
  } else {
    Serial.println("Calibration file not found. Using default calibration values.");
  }
}

void readmlOZ() {
  // Open the mlOZ file on the SD card for reading
  File mlOZFile = SD.open("/mlOZ.txt", FILE_READ);

  // Check if the file opened successfully
  if (mlOZFile) {
    // Read the mlOZ value from the file
    if (mlOZFile.available()) {
      String line = mlOZFile.readStringUntil('\n');
      useOunces = line.toInt(); // Convert the string to boolean (or use other conversion methods)
      mlOZ = useOunces ? 0.033814 : 1.0;
    }

    // Close the file
    mlOZFile.close();

    Serial.println("mlOZ value read from file.");
  } else {
    Serial.println("Error opening mlOZ file for reading. Using default mlOZ value.");
  }
}

void readPrintTime() {
  // Open the time file on the SD card for reading
  File timeFile = SD.open("/time.txt", FILE_READ);

  // Check if the file opened successfully
  if (timeFile) {
    // Read the time value from the file
    if (timeFile.available()) {
      String line = timeFile.readStringUntil('\n');
      timeInput = line; // Assign the read string to timeInput
    }

    // Close the file
    timeFile.close();

    Serial.println("Time value read from file.");
  } else {
    Serial.println("Error opening time file for reading. Using default time value.");
  }
}

void readAutosave() {
  File autosaveFile = SD.open("/Autosave.txt", FILE_READ);

  // Check if the file opened successfully
  if (autosaveFile) {
    // Read the pulse values from the file and set them to the respective variables
    for (int i = 0; i < 8; i++) {
      if (autosaveFile.available()) {
        String line = autosaveFile.readStringUntil('\n');
        *pulses[i] = line.toInt();
      }
    }

    // Close the file
    autosaveFile.close();

    Serial.println("Flow meter pulses read from Autosave file.");
  } else {
    Serial.println("Autosave file not found. Using default pulse values.");
  }
}

void readCredentials() {
  // Open the credentials files on the SD card for reading
  File ssidFile = SD.open("/ssid.txt", FILE_READ);
  File passwordFile = SD.open("/password.txt", FILE_READ);

  // Check if the files opened successfully
  if (ssidFile && passwordFile) {
    // Read the SSID and password values from their respective files
    if (ssidFile.available()) {
      ssid = ssidFile.readStringUntil('\n'); // Read until newline
    }
    if (passwordFile.available()) {
      password = passwordFile.readStringUntil('\n'); // Read until newline
    }

    // Close the files
    ssidFile.close();
    passwordFile.close();

    Serial.println("SSID and password read from files:");
    Serial.println("SSID: " + ssid);
    Serial.println("Password: " + password);
  } else {
    Serial.println("Error opening credentials files for reading. Using default SSID and password.");
  }
}

void readLicence() {
  DateTime now = rtc.now();
  // Open the credentials files on the SD card for reading
  File licenceFile = SD.open("/licence.txt", FILE_READ);

  // Check if the files opened successfully
  if (licenceFile) {
    // Read the licence file
    if (licenceFile.available()) {
      licence = licenceFile.readStringUntil('\n'); // Read until newline
      expiration = decrypt(licence);
    }
    // Close the files
    licenceFile.close();

    if (expiration.length() != 10) {
      expiration = "Invalid Licence";
      licenceValid = false;
    } else {
      int expiryYear = expiration.substring(0, 4).toInt();
      int expiryMonth = expiration.substring(5, 7).toInt();
      int expiryDay = expiration.substring(8, 10).toInt();

      DateTime expiryDate(expiryYear, expiryMonth, expiryDay, 0, 0, 0);
      TimeSpan timeToExpiry = expiryDate - now;

      // Calculate the number of days to expiry
      whenExpired = timeToExpiry.days();

      // Check if the licence will expire soon (in 15 or less days)
      soonExpire = whenExpired <= 15;
      Serial.println(whenExpired);
      if (expiryYear < now.year() || 
         (expiryYear == now.year() && expiryMonth < now.month()) || 
         (expiryYear == now.year() && expiryMonth == now.month() && expiryDay < now.day())) {
        expiration = "Expired";
        licenceValid = false;
      } else {
        licenceValid = true;
      }
    }
  }
}

void writeFlowMeterNames() {
  File configFile = SD.open("/config.txt", FILE_WRITE);

  if (configFile) {
    for (int i = 0; i < 8; i++) {
      configFile.println(flowMeterNames[i]);
    }

    configFile.close();
    Serial.println("Flow meter names written to config file.");
  } else {
    Serial.println("Error opening config file for writing.");
  }
}

void writeCalibrationValues() {
  File calibFile = SD.open("/calibration.txt", FILE_WRITE);

  if (calibFile) {
    for (int i = 0; i < 8; i++) {
      calibFile.println(flowMeterCalibrations[i]);
    }

    calibFile.close();
    Serial.println("Calibration values written to file.");
  } else {
    Serial.println("Error opening calibration file for writing.");
  }
}

void writemlOZ() {
  // Open the mlOZ file on the SD card for writing
  File mlOZFile = SD.open("/mlOZ.txt", FILE_WRITE);

  // Check if the file opened successfully
  if (mlOZFile) {
    // Write the current mlOZ value to the file
    mlOZFile.println(useOunces);

    // Close the file
    mlOZFile.close();

    Serial.println("mlOZ value saved to file.");
  } else {
    Serial.println("Error opening mlOZ file for writing.");
  }
}

void writePrintTime() {
  // Open the time file on the SD card for writing
  File timeFile = SD.open("/time.txt", FILE_WRITE);

  // Check if the file opened successfully
  if (timeFile) {
    // Write the current time value to the file
    timeFile.println(timeInput);

    // Close the file
    timeFile.close();

    Serial.println("Time value saved to file.");
  } else {
    Serial.println("Error opening time file for writing.");
  }
}

void writeCredentials() {
  // Open the credentials files on the SD card for writing
  File ssidFile = SD.open("/ssid.txt", FILE_WRITE);
  File passwordFile = SD.open("/password.txt", FILE_WRITE);

  // Check if the files opened successfully
  if (ssidFile && passwordFile) {
    // Write the current SSID and password to their respective files
    ssidFile.print(ssid); // Use print instead of println
    passwordFile.print(password); // Use print instead of println

    // Close the files
    ssidFile.close();
    passwordFile.close();

    Serial.println("SSID and password saved to files.");
  } else {
    Serial.println("Error opening credentials files for writing.");
  }
}

void writeLicence() {
  // Open the licence file on the SD card for writing
  File licenceFile = SD.open("/licence.txt", FILE_WRITE);

  // Check if the file opened successfully
  if (licenceFile) {
    // Write the licence to the file
    licenceFile.print(licence);

    // Close the file
    licenceFile.close();

    Serial.println("Licence written to file: " + licence);
  } else {
    Serial.println("Error opening licence file for writing.");
  }
}


void printFlowDataToFile() {
  // Get the current date and time
  DateTime now = rtc.now();
printer.println("End Of Day Report");
  printer.println(String(now.year(), DEC) + String(now.month(), DEC) + String(now.day(), DEC) +
                    " " + String(now.hour(), DEC) + String(now.minute(), DEC));
  printer.println(flowMeterDataForSD());
  if (soonExpire == true) {
    printer.println("LICENCE EXPIRING SOON, CONTACT DEALER");
    printer.println("Licence expires in " + String(whenExpired) + " days");
  }

  // Generate a filename based on the date and time within the "EOD_Saves" folder
  String folderName = "/EOD_Saves";
  SD.mkdir(folderName.c_str());  // Create the folder if it doesn't exist

  String filename = folderName + "/" + String(now.year(), DEC) + String(now.month(), DEC) + String(now.day(), DEC) +
                    "_" + String(now.hour(), DEC) + String(now.minute(), DEC) + ".txt";

  // Open the file on the SD card for writing
  File dataFile = SD.open(filename, FILE_WRITE);

  // Check if the file opened successfully
  if (dataFile) {
    // Write flow meter data to the file
    dataFile.println(flowMeterDataForSD());

    // Close the file
    dataFile.close();

    Serial.println("Flow meter data saved to file: " + filename);
    printer.feed(3);
   // ESP.restart();
  } else {
    Serial.println("Error opening file for writing.");
  }
}

void autoSavetoSD() {
  // Open the Autosave file on the SD card for writing
  File autosaveFile = SD.open("/Autosave.txt", FILE_WRITE);

  // Check if the file opened successfully
  if (autosaveFile) {
    // Write the current pulse values to the file
    for (int i = 0; i < 8; i++) {
      autosaveFile.println(*pulses[i]);
    }

    // Close the file
    autosaveFile.close();

    Serial.println("Flow meter pulses saved to Autosave file.");
  } else {
    Serial.println("Error opening Autosave file for writing.");
  }
}

void printFlowData() {
  Serial.print("Flow Meter 1: ");
  Serial.print(pulse1);
  Serial.println(" pulses");

  Serial.print("Flow Meter 2: ");
  Serial.print(pulse2);
  Serial.println(" pulses");

  Serial.print("Flow Meter 3: ");
  Serial.print(pulse3);
  Serial.println(" pulses");

  Serial.print("Flow Meter 4: ");
  Serial.print(pulse4);
  Serial.println(" pulses");

  Serial.print("Flow Meter 5: ");
  Serial.print(pulse5);
  Serial.println(" pulses");

  Serial.print("Flow Meter 6: ");
  Serial.print(pulse6);
  Serial.println(" pulses");

  Serial.print("Flow Meter 7: ");
  Serial.print(pulse7);
  Serial.println(" pulses");

  Serial.print("Flow Meter 8: ");
  Serial.print(pulse8);
  Serial.println(" pulses");

  Serial.println();
}

String encrypt(String date) {
    date.replace("/", ""); // Remove slashes
    long num = date.toInt() + 125345; // Add an offset
    num *= 43.53; // Multiply by a smaller encryption factor
    String encrypted = "";
    while (num > 0) {
        int digit = num % 10;
        switch (digit) {
            case 0: encrypted = "p" + encrypted; break;
            case 1: encrypted = "t" + encrypted; break;
            case 2: encrypted = "f" + encrypted; break;
            case 3: encrypted = "x" + encrypted; break;
            case 4: encrypted = "j" + encrypted; break;
            case 5: encrypted = "o" + encrypted; break;
            case 6: encrypted = "k" + encrypted; break;
            case 7: encrypted = "q" + encrypted; break;
            case 8: encrypted = "u" + encrypted; break;
            case 9: encrypted = "a" + encrypted; break;
        }
        num /= 10;
    }
    return encrypted;
}

String decrypt(String encrypted_date) {
    long num = 0;
    for (char c : encrypted_date) {
        switch (c) {
            case 'p': num = num * 10 + 0; break;
            case 't': num = num * 10 + 1; break;
            case 'f': num = num * 10 + 2; break;
            case 'x': num = num * 10 + 3; break;
            case 'j': num = num * 10 + 4; break;
            case 'o': num = num * 10 + 5; break;
            case 'k': num = num * 10 + 6; break;
            case 'q': num = num * 10 + 7; break;
            case 'u': num = num * 10 + 8; break;
            case 'a': num = num * 10 + 9; break;
        }
    }
    num /= 43.53; // Divide by the encryption factor
    num -= 125345; // Subtract the initial offset
    String originalDate = String(num);
    if (originalDate.length() < 8) originalDate = String("0") + originalDate; // Ensure leading zero is present if necessary
    originalDate = originalDate.substring(0, 4) + "/" + originalDate.substring(4, 6) + "/" + originalDate.substring(6, 8);
    return originalDate;
}

void setup() {
    Serial.begin(115200);
    pcnt_init();
    rtc.begin();
    Serial2.begin(9600);
    printer.begin();
    Wire.begin();
//rtc_wdt_protect_off();
//rtc_wdt_disable();
    if (!SD.begin(15)) {  // CS pin is connected to GPI15
    Serial.println("Error initializing SD card.");
    // return;
  }

    readCredentials();
    Serial.println(ssid);
    Serial.println(password);
    // Connect to Wi-Fi
    if (ssid == "JoeNitro" && password == "J0en1tr0") { 
      Serial.println("Wifi correct");
    } else {
      Serial.println("Wifi incorrect");
    }
  
    //Serial.println(ssid);
    //Serial.println(password);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.println("Connecting to WiFi...");
    }
    Serial.println(WiFi.localIP());

    // Route for root / web page
    server.on("/", HTTP_GET, handleMainPage);
    server.on("/admin", HTTP_GET, handleAdminPage);
    server.on("/calibrateMeter", HTTP_GET, handleCalibrateMeter);
    server.on("/startCalibration", HTTP_GET, startCalibration);
    server.on("/stopCalibration", HTTP_POST, stopCalibration);
    server.on("/resetTotalizer", HTTP_GET, handleResetTotalizer);
    server.on("/resetAll", HTTP_GET, handleResetAll);
    server.on("/setRTC", HTTP_GET, handleSetRTC);
    server.on("/setTimeOfDay", HTTP_GET, handleSetTimeOfDay);
    server.on("/rename", HTTP_GET, handleFlowMeterRename);
    server.on("/toggleUnits", HTTP_GET, handleToggleUnits);
    server.on("/printflowdata", HTTP_GET, handlePrintFlowMeterData);
    server.on("/previousSaves", HTTP_GET, handlePreviousSaves);
    server.on("/changeWifi", HTTP_GET, handleChangeWifi);
    server.on("/updateLicence", HTTP_GET, handleUpdateLicence);
    server.on("/reprintNewest", HTTP_GET, handleReprintNewest);
    // Start server
    server.begin();

  
  readLicence();

  if (licenceValid) {
  readFlowMeterNames();
  readCalibrationValues();
  readmlOZ();
  readPrintTime();
  readAutosave();
  }
  
  Serial.println("Connecting To Printer");
  printer.setFont('B');
  printer.println("");
  printer.println("Connected To Printer");
  printer.feed(1);
  printer.println("Power Failed Or System Rebooted");
  printer.println(WiFi.localIP());
  printer.println("Beer Trax v0.98.5f");
  printer.feed(2);
  if (!licenceValid){
    printer.println("Licence expired. Contact Dealer");
    Serial.println("Licence expired. Contact Dealer");
  }
}

void loop() {
    DateTime now = rtc.now();
    int16_t count = 0;
    for (int i = 0; i < numSensors; i++) {
        pcnt_get_counter_value((pcnt_unit_t)i, &count);
        pcnt_counter_clear((pcnt_unit_t)i);

        switch(i) {
            case 0: pulse1 += count; break;
            case 1: pulse2 += count; break;
            case 2: pulse3 += count; break;
            case 3: pulse4 += count; break;
            case 4: pulse5 += count; break;
            case 5: pulse6 += count; break;
            case 6: pulse7 += count; break;
            case 7: pulse8 += count; break;
        }
    }
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(" ");
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println(); 
    printFlowData();

  if (now.hour() == getHourFromTimeInput() && now.minute() == getMinuteFromTimeInput() && sdPause > 6) {
    // Perform the reset and print actions
    Serial.println("Resetting and Printing");
    sdPause = 0;
    printFlowDataToFile();
    for (int i = 0; i < 8; i++) {
      resetFlowMeter(i);
      Serial.println("zeroing totalizers");
      
    }

     autoSavetoSD();
     ESP.restart();    
  
  }
  if (sdAuto == 30) {
    Serial.println("Autosaving");
    sdAuto = 0;
    autoSavetoSD();
  }
  delay(10000);
  sdPause++;
  Serial.println(sdPause);
  sdAuto++;
}
