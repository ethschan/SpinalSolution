/*
 * File Name: SpinalSolution_Basic.ino
 * 
 * Description:
 * This Arduino sketch is designed to detect cerebrospinal fluid leakage using optical sensors.
 * The device uses a series of spectral sensors to measure red, orange, yellow, green, blue, and violet light, along with a UV sensor.
 * 
 * NOTE: This sketch takes care of calibration, processing samples, and displaying results.
 * The current implementation includes placeholder calculations. You MUST build and calibrate the system yourself to obtain accurate results.
 * 
 * Hardware:
 * - 1.44" TFT LCD display connected to pins 10 (CS), 9 (RST), 8 (DC)
 * - AS726X spectral sensor connected via I2C
 * - UV sensor connected to analog pin A2
 * - Button inputs on pins 6 and 7
 * - OpenLog SD card module connected to pins 3 (RX), 4 (TX), 5 (RST)
 * 
 * Usage:
 * - Insert the sample into a cuvette and place it in the device.
 * - Push a reagent cartridge onto the spring valve.
 * - Press the button to start the measurement.
 * - Wait for results.
 * 
 * History:
 * - Original Code: March 2021
 * - Updated: August 2024 for code readability
 */

// Import Statements
#include <Adafruit_GFX.h> // Adafruit Graphics library for TFT display
#include <Adafruit_ST7735.h> // 1.44" TFT LCD library
#include <Fonts/FreeSans9pt7b.h> // Header font library
#include <Fonts/FreeMono9pt7b.h> // Text font library
#include "AS726X.h" // AS726x spectral sensor library
#include <SoftwareSerial.h> // SoftwareSerial library for serial communication with OpenLog


// Pin Definitions
#define TFT_CS 10       // TFT CS pin
#define TFT_RST 9       // TFT Reset pin
#define TFT_DC 8        // TFT DC pin
#define SD_RX 3         // SD RX Pin
#define SD_TX 4         // SD TX Pin
#define SD_RST 5        // SD RST Pin
#define selectPin 7     // Select button input pin
#define switchPin 6     // Switch button input pin
#define uvSensorPin A2  // UV sensor analog pin
#define ledPin 2        // LED pin

// Constant Definitions
#define configName "CONFIG.TXT"
#define configHeader "index,r,o,y,g,b,v,uv"

// Color Definitions
#define black ST77XX_BLACK
#define green ST77XX_GREEN
#define white ST77XX_WHITE
#define blue ST77XX_BLUE
#define red ST77XX_RED
#define orange ST77XX_ORANGE
#define yellow ST77XX_YELLOW
#define dark_green 0x1D80
#define violet 0xD81F

// State Variables
int8_t selectState = 0;
int8_t switchState = 0;
int8_t currentMenu = 0;
int8_t currentSelection = 0;

int index = 0;
float calibration[6] = {0, 0, 0, 0, 0, 0};
int uvCalibration = 0;
float timeAverages[6][10] = {0};  // Array to hold averaged values over time

// Enumeration for result types
enum TestResult { NO_LEAK, LEAK_DETECTED, RECOMMEND_FURTHER_TESTING };

// Object Definitions
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); // TFT object
AS726X spectralSensor; // Spectral sensor object
SoftwareSerial OpenLog(SD_RX, SD_TX); // SoftwareSerial for logging

// Function to display the measuring menu header
void measuringMenuHeader(void) {
  tft.fillScreen(black);
  tft.drawRect(6, 6, 116, 116, green);
}

// Function to handle the main measuring process
void measuringMenu1(void) {
  measuringMenuHeader();
  setupTextDisplay();

  float specReadings[6];
  int8_t historicSelectState = digitalRead(selectPin);
  unsigned long start_time = millis();
  unsigned long current_time;
  unsigned long past_time = 0;

  int maxlen = 50;
  double x2 = 0;
  double xy[6] = {0, 0, 0, 0, 0, 0};
  int datalen = 0;
  double initial[6];
  boolean initialTaken = false;
  int signal_resolution_multiplier = 1000000;
  int time_resolution_multiplier = 1000;
  int ms_time = 300000 / maxlen;

  startNewLog();

  while (true) {
    takeMeasurement(specReadings);
    current_time = millis() - start_time;

    double current_time_total_minutes = current_time / (60000.0 / time_resolution_multiplier);
    logData(current_time, specReadings, analogRead(uvSensorPin));

    if (!initialTaken) {
      for (int i = 0; i < 6; i++) {
        initial[i] = specReadings[i];
      }
      initialTaken = true;
    }

    if (datalen < maxlen && current_time - past_time > ms_time) {
      x2 += current_time_total_minutes * current_time_total_minutes;

      for (int i = 0; i < 6; i++) {
        xy[i] += (((signal_resolution_multiplier * specReadings[i]) / initial[i]) - 1) * current_time_total_minutes;
        updateTimeAverages(specReadings[i], i);
      }
      past_time = current_time;
      datalen++;
    }

    updateProgress(current_time_total_minutes, time_resolution_multiplier);

    if (current_time_total_minutes >= 5 || checkForButtonPress(historicSelectState)) {
      break;
    }
  }

  // Process and calculate slopes after measurement loop
  double slopes[6];
  processSlopes(xy, x2, slopes);

  // Calculate concentration and result
  double concentration = calculateConcentration(slopes, timeAverages);
  TestResult result = calculateResult(concentration);

  // Display the results
  displayResults(concentration, result);

  setHomeMenu();
}

// Function to update the time-averaged data
void updateTimeAverages(float reading, int channel) {
  static int sampleCounter = 0;
  static int averageIndex = 0;

  timeAverages[channel][averageIndex] += reading;
  sampleCounter++;

  if (sampleCounter >= 10) {  // Averaging every 10 samples
    timeAverages[channel][averageIndex] /= 10;
    averageIndex = (averageIndex + 1) % 10;  // Fixed-size array, loop back to 0
    sampleCounter = 0;
  }
}

// Function to process slopes for each channel
void processSlopes(double xy[], double x2, double slopes[]) {
  for (int i = 0; i < 6; i++) {
    slopes[i] = xy[i] / x2;
  }
}

// Function to calculate the concentration based on slopes and time-averages
double calculateConcentration(double slopes[], float timeAverages[][10]) {
  // Placeholder function for concentration calculation
  // TODO: Implement actual calibration curves and calculations here for device

  // Just returning a dummy value for now...
  return 0.0;
}

// Function to determine the test result based on concentration
TestResult calculateResult(double concentration) {
  // Placeholder function for result calculation
  // TODO: Implement actual logic to determine if there is a leak, no leak, or need for further testing

  // Just returning a dummy value for now...
  return NO_LEAK;
}

// Function to display the results on the TFT screen
void displayResults(double concentration, TestResult result) {
  tft.fillScreen(black);
  tft.setCursor(0, 0);
  tft.setTextColor(green);
  tft.setTextSize(1);
  
  tft.print("Concentration: ");
  tft.print(concentration, 2);
  tft.println(" mg/L");

  tft.setCursor(0, 20);
  tft.print("Result: ");
  switch (result) {
    case NO_LEAK:
      tft.print("No Leak");
      break;
    case LEAK_DETECTED:
      tft.print("Leak Detected");
      break;
    case RECOMMEND_FURTHER_TESTING:
      tft.print("Recommend Further Testing");
      break;
  }
}

// Function to start a new log file
void startNewLog() {
  tft.drawRect(14, 70, 100, 20, green);
  sendSDCommand("rm", "LOG.CSV", '>');
  sendSDCommand("new", "LOG.CSV", '>');
  OpenLog.print("append LOG.CSV");
  OpenLog.write(13);
  OpenLog.print(F("time,red,orange,yellow,green,blue,violet,uv"));
  OpenLog.write(26);
  OpenLog.write(26);
  OpenLog.write(26);
}

// Function to take a measurement
void takeMeasurement(float specReadings[]) {
  digitalWrite(ledPin, HIGH);
  delay(200);
  spectralSensor.takeMeasurements();
  delay(50);
  digitalWrite(ledPin, LOW);

  specReadings[0] = spectralSensor.getCalibratedRed();
  specReadings[1] = spectralSensor.getCalibratedOrange();
  specReadings[2] = spectralSensor.getCalibratedYellow();
  specReadings[3] = spectralSensor.getCalibratedGreen();
  specReadings[4] = spectralSensor.getCalibratedBlue();
  specReadings[5] = spectralSensor.getCalibratedViolet();
}

// Function to log data to OpenLog
void logData(unsigned long current_time, float specReadings[], int uvReading) {
  OpenLog.print("append LOG.CSV");
  OpenLog.write(13);

  OpenLog.print(current_time);
  for (uint8_t i = 0; i < 6; i++) {
    OpenLog.print(",");
    OpenLog.print(specReadings[i]);
  }
  OpenLog.print(",");
  OpenLog.print(uvReading);
  OpenLog.write(26);
  OpenLog.write(26);
  OpenLog.write(26);
}

// Function to update progress on TFT
void updateProgress(double current_time_total_minutes, int time_resolution_multiplier) {
  int progress = 20 * current_time_total_minutes / time_resolution_multiplier;
  tft.fillRect(14, 70, progress, 20, green);
}

// Function to check for button press
bool checkForButtonPress(int8_t historicSelectState) {
  for (int8_t i = 0; i < 80; i++) {
    delay(5);
    selectState = digitalRead(selectPin);
    if (selectState == 1 && historicSelectState == 0) {
      return true;
    }
    historicSelectState = selectState;
  }
  return false;
}

// Setup the text display settings
void setupTextDisplay() {
  tft.setTextSize(1);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);
}

// Send a command to the SD card via OpenLog
void sendSDCommand(char* command, char* filename, char confirmation) {
  OpenLog.print(command);
  OpenLog.print(" ");
  OpenLog.print(filename);
  OpenLog.write(13);
  waitForChar(confirmation);
}

// Wait for a specific character from OpenLog
void waitForChar(char c) {
  while (1) {
    if (OpenLog.available()) {
      if (OpenLog.read() == c) break;
    }
  }
}

// Set the home menu
void setHomeMenu() {
  currentMenu = 0;
  currentSelection = 0;
  homeMenu1();
}

// Home Menu Header
void homeMenuHeader(void) {
  tft.fillScreen(black);
  tft.drawRect(6, 6, 116, 70, green); // top
  tft.drawRect(6, 82, 116, 40, green); // bottom

  setHeaderText();
  tft.setTextColor(green);
  tft.setCursor(44, 34);
  tft.print(F("NEW"));

  tft.setCursor(28, 58);
  tft.print(F("SAMPLE"));

  tft.setCursor(14, 108);
  tft.print(F("CALIBRATE"));

  tft.setTextColor(black);
}

// Home Menu 1
void homeMenu1(void) {
  homeMenuHeader();
  tft.fillRect(8, 8, 112, 66, green); // top box
  tft.setCursor(44, 34);
  tft.print(F("NEW"));
  tft.setCursor(28, 58);
  tft.print(F("SAMPLE"));
}

// Home Menu 2
void homeMenu2(void) {
  homeMenuHeader();
  tft.fillRect(8, 84, 112, 36, green); // bottom box
  tft.setCursor(14, 108);
  tft.print(F("CALIBRATE"));
}

// Set header text style
void setHeaderText(void) {
  tft.setFont(&FreeSans9pt7b);
}

// Set normal text style
void setNormalText(void) {
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);
}

// Button Menu Header
void buttonMenuHeader(char *button_text, uint8_t x, uint8_t y) {
  tft.fillScreen(black);
  tft.drawRect(18, 82, 92, 40, green); // bottom start
  tft.fillRect(20, 84, 88, 36, green); // bottom start

  setHeaderText();
  tft.setTextColor(black);
  tft.setCursor(x, y);
  tft.print(button_text);
  setNormalText();
}

// Confirmation Menu 1
void confirmationMenu1(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Confirm the"));
  tft.setCursor(13, 46);
  tft.print(F("sample is"));
  tft.setCursor(33, 66);
  tft.print(F("ready."));
}

// Confirmation Menu 2
void confirmationMenu2(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Prepare the"));
  tft.setCursor(17, 46);
  tft.print(F("water to"));
  tft.setCursor(9, 66);
  tft.print(F("calibrate."));
}

// Calibration Menu
void calibrationMenu() {
  tft.fillScreen(black);
  setHeaderText();
  tft.setTextColor(green);
  tft.setCursor(4, 50);
  tft.print(F("CALIBRATING"));
  tft.drawRect(14, 70, 100, 20, green);

  int uvReadings;
  float specReadings[6] = {0, 0, 0, 0, 0, 0};
  
  for (uint8_t i = 0; i < 20; i++) {
    uvReadings += analogRead(uvSensorPin);
    spectralSensor.takeMeasurements();

    specReadings[0] += spectralSensor.getCalibratedRed();
    specReadings[1] += spectralSensor.getCalibratedOrange();
    specReadings[2] += spectralSensor.getCalibratedYellow();
    specReadings[3] += spectralSensor.getCalibratedGreen();
    specReadings[4] += spectralSensor.getCalibratedBlue();
    specReadings[5] += spectralSensor.getCalibratedViolet();
    delay(100);
    tft.fillRect(14, 70, 5*(i+1), 20, green);
  }

  uvCalibration = uvReadings / 20;

  for (uint8_t i = 0; i < 6; i++) {
    calibration[i] = specReadings[i] / 20;
    if (calibration[i] < 0) {
      calibration[i] = 0;
    }
  }

  writeConfig();
  OpenLog.write('\r');
  waitForChar('>');
  buttonMenuHeader("DONE", 39, 108);
  setHeaderText();
  tft.setTextColor(green);
  tft.setCursor(4, 40);
  tft.print(F("CALIBRATION"));
  tft.setCursor(16, 60);
  tft.print(F("COMPLETE"));
}

// Setup functions for TFT, Pins, Sensors, SD, etc.
void setupTFT(void) {
  tft.initR(INITR_144GREENTAB); // 1.44" green tab TFT LCD
  tft.setSPISpeed(40000000); // TFT refresh rate
  tft.setRotation(2); // Flip TFT screen
  tft.setTextSize(1);
  tft.fillScreen(black);
}

void setupPins(void) {
  pinMode(selectPin, INPUT);
  pinMode(switchPin, INPUT);
  pinMode(uvSensorPin, INPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_RST, OUTPUT);
  pinMode(ledPin, OUTPUT);
}

void setupSensors(void) {
  Wire.begin();
  spectralSensor.begin();
}

void setupSD(void) {
  OpenLog.begin(9600);

  // Reset OpenLog
  digitalWrite(SD_RST, LOW);
  delay(100);
  digitalWrite(SD_RST, HIGH);

  // Wait for OpenLog to respond with '<' to indicate it is alive and recording to a file
  while (1) {
    if (OpenLog.available()) {
      if (OpenLog.read() == '<') break;
    }
  }
}

// Setup function
void setup(void) {
  Serial.begin(9600);
  
  setupPins();
  setupTFT();
  setupSensors();
  setupSD();
  gotoCommandMode(); 
  loadConfig();
  setHomeMenu();
}

// Enter OpenLog command mode
void gotoCommandMode(void) {
  OpenLog.write(26);
  OpenLog.write(26);
  OpenLog.write(26);
  waitForChar('>');
}

// Loop function for menu navigation
void loop() {
  while (true) {
    int8_t button_pressed = buttons();
    if (button_pressed == 0) {
      navigateMenu();
    } else if (button_pressed == 1) {
      executeSelection();
    }
  }
  delay(100);
}

// Handle menu navigation
void navigateMenu() {
  if (currentMenu == 0) {
    if (currentSelection == 1) {
      currentSelection = 0;
      homeMenu1();
    } else if (currentSelection == 0) {
      currentSelection = 1;
      homeMenu2();
    }
  }
}

// Execute the selected menu option
void executeSelection() {
  if (currentMenu == 0) {
    if (currentSelection == 0) {
      currentMenu = 1;
      confirmationMenu1();
    } else {
      currentMenu = 3;
      confirmationMenu2();
    }
  } else if (currentMenu == 1) {
    currentMenu = 2;
    measuringMenu1();
  } else if (currentMenu == 3) {
    currentMenu = 5;
    calibrationMenu();
  } else if (currentMenu == 5) {
    setHomeMenu();
  }
}

// Handle button presses for menu selection
int8_t buttons() {
  int8_t historicSelectState = digitalRead(selectPin);
  int8_t historicSwitchState = digitalRead(switchPin);

  while (true) {
    delay(20);

    selectState = digitalRead(selectPin);
    switchState = digitalRead(switchPin);

    if (selectState == 1 && historicSelectState == 0) {
      return 1;
    }

    if (switchState == 1 && historicSwitchState == 0) {
      return 0;
    }

    historicSelectState = selectState;
    historicSwitchState = switchState;
  }
}

// Load configuration from SD card
void loadConfig() {
  sendSDCommand("read", "config.csv", '\r');
  if (checkHeader(configHeader, 20)) {
    readConfigData();
  } else {
    writeConfig();
  }
}

// Check if the configuration file header matches
bool checkHeader(char* header, uint16_t headerlen) {
  int filePosition = 0;
  int16_t c;

  waitForChar('\r');
  waitForChar('\n');

  for (int timeOut = 0 ; timeOut < 1000 ; timeOut++) {
    while ((c = (char) OpenLog.read()) > 0) {
      if (c == header[filePosition]) {
        if (filePosition == headerlen - 1) {
          return true;
        }
      } else {
        return false;
      }

      filePosition++;
      timeOut = 0;
    }
    delay(1);
  }
  return false;
}

// Read configuration data from the SD card
void readConfigData() {
  char c;
  byte config_index = 0;
  const uint8_t dataBufLen = 10;
  char dataBuf[dataBufLen];
  byte nData = 1;
  dataBuf[0] = waitForRealChar();

  for (int timeOut = 0 ; timeOut < 1000 ; timeOut++) {
    while ((c = (char) OpenLog.read()) > 0) {
      if (c == ',' || c == '\r' || c == '\n') {
        dataBuf[nData] = '\0';
        updateGlobalConfig(nData, dataBuf, config_index);
        config_index++;
        nData = 0;
        if (c == '\r' || c == '\n') {
          timeOut = 1000;
          break;
        }
      } else {
        dataBuf[nData++] = c;
      }
      timeOut = 0;
    }
    delay(1);
  }
  waitForChar('>');
}

// Update the global configuration based on the read data
void updateGlobalConfig(uint8_t len, char *ar, uint8_t config_index) {
  if (config_index == 0) {
    index = atoi(ar);
  } else if (config_index > 0 && config_index < 7) {
    calibration[config_index - 1] = atof(ar);
  } else if (config_index == 7) {
    uvCalibration = atoi(ar);
  }
}

// Wait for a non-whitespace character from OpenLog
char waitForRealChar() {
  char c;
  while (1) {
    if (OpenLog.available()) {
      c = (char) OpenLog.read();
      if (c != '\n' && c != '\r') return c;
    }
  }
}

// Write configuration data to the SD card
void writeConfig() {
  writeFile("config.csv");

  OpenLog.print(configHeader);
  OpenLog.write('\r');
  OpenLog.print(index);
  for (uint16_t i = 0; i < 6; i++) {
    OpenLog.print(',');
    OpenLog.print(calibration[i]);
  }
  OpenLog.print(',');
  OpenLog.print(uvCalibration);
  OpenLog.write('\r');
  OpenLog.write('\r');
  waitForChar('>');
}

// Write a file to the SD card
void writeFile(char* filename) {
  sendSDCommand("rm", filename, '>');
  sendSDCommand("new", filename, '>');
  sendSDCommand("write", filename, '<');
}
