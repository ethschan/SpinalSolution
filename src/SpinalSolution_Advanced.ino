/*
 * File Name: SpinalSolution_Advanced.ino
 * 
 * Description:
 * This Arduino sketch is designed to detect cerebrospinal fluid leakage using optical sensors.
 * The device uses a series of spectral sensors to measure red, orange, yellow, green, blue, and violet light, along with a UV sensor.
 * 
 * This advanced version includes graphical plotting on the TFT display.
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
#define TFT_CS 10       // TFT Chip Select pin
#define TFT_RST 9       // TFT Reset pin
#define TFT_DC 8        // TFT Data/Command pin
#define SD_RX 3         // OpenLog SD RX Pin
#define SD_TX 4         // OpenLog SD TX Pin
#define SD_RST 5        // OpenLog SD Reset Pin
#define selectPin 7     // Select button input pin
#define switchPin 6     // Switch button input pin
#define uvSensorPin A2  // UV sensor analog pin
#define ledPin 2        // LED pin to indicate measurement process

// Constant Definitions
#define configName "CONFIG.TXT"  // Configuration file name
#define configHeader "index,r,o,y,g,b,v,uv"  // Header format for configuration file

// Color Definitions for TFT Display
#define black ST77XX_BLACK
#define green ST77XX_GREEN
#define white ST77XX_WHITE
#define blue ST77XX_BLUE
#define red ST77XX_RED
#define orange ST77XX_ORANGE
#define yellow ST77XX_YELLOW
#define dark_green 0x1D80
#define violet 0xD81F

// Dynamic Variables to Manage State and Configuration
int8_t selectState = 0;  // Current state of the select button
int8_t switchState = 0;  // Current state of the switch button
int8_t currentMenu = 0;  // Current menu being displayed
int8_t currentSelection = 0;  // Current menu selection
int index = 0;  // Index to track measurements and logs
float calibration[6] = {0, 0, 0, 0, 0, 0};  // Calibration data for each spectral channel
int uvCalibration = 0;  // Calibration data for UV sensor

// Arrays to Store Graph Data for Each Spectral Channel and UV Sensor
int8_t rX[46], oX[47], yX[46], gX[47], bX[46], vX[47], uvX[95];
int8_t rXLen = 0, oXLen = 0, yXLen = 0, gXLen = 0, bXLen = 0, vXLen = 0, uvXLen = 0;
float rXMax = -1, oXMax = -1, yXMax = -1, gXMax = -1, bXMax = -1, vXMax = -1, uvXMax = -1;
float rXMin = -1, oXMin = -1, yXMin = -1, gXMin = -1, bXMin = -1, vXMin = -1, uvXMin = -1;

// Additional Variables for Measurement Calculation
float timeAverages[6][10] = {0};  // Array to hold averaged values over time
int sampleCounter = 0;  // Counter for samples
int averageIndex = 0;  // Index for averaging

// Enumeration for result types
enum TestResult { NO_LEAK, LEAK_DETECTED, RECOMMEND_FURTHER_TESTING };

// Object Definitions for TFT Display, Spectral Sensor, and OpenLog
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); // TFT object
AS726X spectralSensor; // Spectral sensor object
SoftwareSerial OpenLog(SD_RX, SD_TX); // SoftwareSerial for logging to OpenLog

// Function to Display the Measuring Menu Header with Graph Structure
void measuringMenuHeader(void) {
  tft.fillScreen(black);  // Clear the screen
  tft.drawRect(6, 6, 116, 82, green);  // Outer graph box
  tft.drawRect(26, 6, 96, 82, green);  // Inner graph box
  tft.drawLine(27, 25, 120, 25, green);  // Horizontal lines within graph box
  tft.drawLine(27, 43, 120, 43, green);
  tft.drawLine(6, 61, 121, 61, green);
  tft.drawLine(12, 106, 56, 106, green);  // Time label underline
  tft.drawLine(73, 6, 73, 61, green);  // Vertical divider for graph sections
  tft.drawRect(68, 100, 52, 20, green);  // Stop button outline
  tft.fillRect(70, 102, 48, 16, green);  // Stop button fill

  tft.setTextSize(1);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);

  tft.setCursor(11, 84);  // Labels for spectral channels and UV
  tft.print(F("V"));
  
  tft.setCursor(11, 73);
  tft.print(F("U"));

  tft.setCursor(11, 19+6);
  tft.print(F("V"));
  tft.setCursor(11, 31+6);
  tft.print(F("I"));
  tft.setCursor(11, 43+6);
  tft.print(F("S"));

  tft.setCursor(12, 102);
  tft.print(F("TIME"));

  tft.setTextColor(black);
  tft.setCursor(72, 114);
  tft.print(F("STOP"));
}

// Function to Plot Spectral Data on the TFT Display
void graphColor(int8_t x, int8_t y, int8_t w, int8_t h, float *xMin, float *xMax, int8_t *xLen, float measurement, int color, int8_t xAr[]) {
    // Update min/max values for scaling
    if (measurement > *xMax || *xMax == -1) {
        *xMax = measurement;
    }
    if (measurement < *xMin || *xMin == -1) {
        *xMin = measurement;
    }

    // Calculate the y-position for the graph based on the measurement
    int8_t result = calculateGraphPosition(*xMin, *xMax, h-1, measurement);

    // Add the new value to the array, and shift the old values if necessary
    if (*xLen < w) {
      xAr[*xLen] = result;
      *xLen += 1;
    } else {
      for (int8_t i = 1; i < *xLen-1; i++) {
        xAr[i-1] = xAr[i];
      }
      xAr[w-1] = result;
    }

    // Clear the previous graph area and draw the new graph
    tft.fillRect(x, y, w, h, black);
    tft.drawPixel(x, y+h-1-xAr[0], color);
    if (*xLen > 1) {
      for (int8_t i = 0; i < *xLen-1; i++) {
        tft.drawLine(x+i, y+h-1-xAr[i], x+i+1, y+h-1-xAr[i+1], color);
      }
    }
}

// Function to Calculate the Graph Position for a Measurement
int calculateGraphPosition(float minX, float maxX, int height, float measurement) {
  if (minX != maxX){  
    return ((measurement - minX)/(maxX-minX)) * height;
  }
  return 0;
}

// Function to Handle the Main Measuring Process with Graph Plotting and Calculation
void measuringMenu1(void) {
  measuringMenuHeader();  // Display the header with graph structure
  
  tft.setTextSize(1);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);

  float specReadings[6];  // Array to store spectral sensor readings
  double xy[6] = {0, 0, 0, 0, 0, 0};  // Array for storing the product of time and readings
  double x2 = 0;  // Sum of squared time values
  double slopes[6];  // Array to store calculated slopes for each channel

  int8_t historicSelectState = digitalRead(selectPin);  // Initial state of the select button
  unsigned long start_time = millis();  // Start time for the measurement
  unsigned long current_time;

  int datalen = 0;  // Data length counter
  int maxlen = 50;  // Maximum data points to consider

  boolean initialTaken = false;  // Flag to check if initial readings have been taken
  double initial[6];  // Array to store initial readings

  int signal_resolution_multiplier = 1000000;
  int time_resolution_multiplier = 1000;
  int ms_time = 300000 / maxlen;  // Milliseconds between data points

  char sampleName[16];
  sprintf(sampleName, "LOG%d.CSV", index++);  // Generate a unique filename for the log

  writeConfig();  // Save the current configuration to SD card
  sdCommand("rm", sampleName, '>');  // Remove existing log file if it exists
  sdCommand("new", sampleName, '>');  // Create a new log file
  delay(10);
  OpenLog.print("append ");
  delay(10);
  OpenLog.print(sampleName);
  delay(10);
  OpenLog.write(13);
  delay(15);
  OpenLog.print(F("time,red,orange,yellow,green,blue,violet,uv"));  // Write the header to the log file
  delay(15);
  OpenLog.write(26);
  delay(10);
  OpenLog.write(26);
  delay(10);
  OpenLog.write(26);
  delay(15);

  while (true) {
    digitalWrite(ledPin, HIGH);  // Turn on the LED to indicate measurement
    delay(200);
    int uvReading = analogRead(uvSensorPin);  // Read the UV sensor
    spectralSensor.takeMeasurements();  // Take readings from the spectral sensor
    delay(50);
    digitalWrite(ledPin, LOW);  // Turn off the LED
    
    current_time = millis() - start_time;
    int8_t current_time_minutes = (current_time / 1000) / 60;
    int8_t current_time_seconds = (current_time / 1000) % 60;

    // Get the calibrated spectral readings
    specReadings[0] = spectralSensor.getCalibratedRed();
    specReadings[1] = spectralSensor.getCalibratedOrange();
    specReadings[2] = spectralSensor.getCalibratedYellow();
    specReadings[3] = spectralSensor.getCalibratedGreen();
    specReadings[4] = spectralSensor.getCalibratedBlue();
    specReadings[5] = spectralSensor.getCalibratedViolet();

    if (!initialTaken) {
      for (int i = 0; i < 6; i++) {
        initial[i] = specReadings[i];  // Store initial readings
      }
      initialTaken = true;
    }

    if (datalen < maxlen && (current_time - (datalen * ms_time)) > ms_time) {
      double current_time_total_minutes = current_time / (60000.0 / time_resolution_multiplier);
      x2 += current_time_total_minutes * current_time_total_minutes;

      for (int i = 0; i < 6; i++) {
        xy[i] += (((signal_resolution_multiplier * specReadings[i]) / initial[i]) - 1) * current_time_total_minutes;
        updateTimeAverages(specReadings[i], i);
      }
      datalen++;
    }

    // Update the display with the current time and plot the graphs
    tft.fillRect(0, 107, 67, 20, black);
    tft.setCursor(11, 120);
    tft.print(current_time_minutes < 10 ? "0" : "");
    tft.print(current_time_minutes);
    tft.print(":");
    tft.print(current_time_seconds < 10 ? "0" : "");
    tft.print(current_time_seconds);

    graphColor(27, 62, 94, 25, &uvXMin, &uvXMax, &uvXLen, uvReading, white, uvX);
    graphColor(27, 7, 46, 18, &rXMin, &rXMax, &rXLen, specReadings[0], red, rX);
    graphColor(74, 7, 47, 18, &oXMin, &oXMax, &oXLen, specReadings[1], orange, oX);
    graphColor(27, 26, 46, 17, &yXMin, &yXMax, &yXLen, specReadings[2], yellow, yX);
    graphColor(74, 26, 47, 17, &gXMin, &gXMax, &gXLen, specReadings[3], dark_green, gX);
    graphColor(27, 44, 46, 17, &bXMin, &bXMax, &bXLen, specReadings[4], blue, bX);
    graphColor(74, 44, 47, 17, &vXMin, &vXMax, &vXLen, specReadings[5], violet, vX);

    // Log the current readings to the OpenLog
    OpenLog.print("append ");
    delay(10);
    OpenLog.print(sampleName);
    delay(10);
    OpenLog.write(13);
    delay(15);
    OpenLog.print(current_time);
    for (uint8_t i = 0; i < 6; i++) {
      OpenLog.print(",");
      delay(10);
      OpenLog.print(specReadings[i]);
      delay(10);
    }
    OpenLog.print(",");
    delay(10);
    OpenLog.print(uvReading);
    delay(10);
    OpenLog.write(26);
    delay(10);
    OpenLog.write(26);
    delay(10);
    OpenLog.write(26);
    delay(15);

    // Exit if the measurement has been running for more than 60 minutes
    if (current_time_minutes >= 60) {
      goto escape;
    }

    // Check if the select button has been pressed
    if (checkForButtonPress(historicSelectState)) {
        goto escape;
    }
  }
  escape:
  OpenLog.write(13);  // End the log entry

  // Process and calculate slopes after measurement loop
  processSlopes(xy, x2, slopes);

  // Calculate concentration and result
  double concentration = calculateConcentration(slopes, timeAverages);
  TestResult result = calculateResult(concentration);

  // Display the results
  displayResults(concentration, result);

  setHomeMenu();  // Return to the home menu
}

// Function to Update the Time-Averaged Data
void updateTimeAverages(float reading, int channel) {
  timeAverages[channel][averageIndex] += reading;
  sampleCounter++;

  if (sampleCounter >= 10) {  // Averaging every 10 samples
    timeAverages[channel][averageIndex] /= 10;
    averageIndex = (averageIndex + 1) % 10;  // Fixed-size array, loop back to 0
    sampleCounter = 0;
  }
}

// Function to Process Slopes for Each Channel
void processSlopes(double xy[], double x2, double slopes[]) {
  for (int i = 0; i < 6; i++) {
    slopes[i] = xy[i] / x2;
  }
}

// Function to Calculate the Concentration Based on Slopes and Time-Averages
double calculateConcentration(double slopes[], float timeAverages[][10]) {
  // Placeholder function for concentration calculation
  // TODO: Implement actual calibration curves and calculations here for device

  // Just returning a dummy value for now...
  return 0.0;
}

// Function to Determine the Test Result Based on Concentration
TestResult calculateResult(double concentration) {
  // Placeholder function for result calculation
  // TODO: Implement actual logic to determine if there is a leak, no leak, or need for further testing

  // Just returning a dummy value for now...
  return NO_LEAK;
}

// Function to Display the Results on the TFT Screen
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

// Function to Display the Home Menu Header
void homeMenuHeader(void) {
  tft.fillScreen(black);
  tft.drawRect(6, 6, 116, 70, green); // top box
  tft.drawRect(6, 82, 116, 40, green); // bottom box

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

// Function to Display Home Menu Option 1
void homeMenu1(void) {
  homeMenuHeader();
  tft.fillRect(8, 8, 112, 66, green); // top box
  tft.setCursor(44, 34);
  tft.print(F("NEW"));
  tft.setCursor(28, 58);
  tft.print(F("SAMPLE"));
}

// Function to Display Home Menu Option 2
void homeMenu2(void) {
  homeMenuHeader();
  tft.fillRect(8, 84, 112, 36, green); // bottom box
  tft.setCursor(14, 108);
  tft.print(F("CALIBRATE"));
}

// Function to Display a Button Menu Header
void buttonMenuHeader(char *button_text, uint8_t x, uint8_t y) {
  tft.fillScreen(black);
  tft.drawRect(18, 82, 92, 40, green); // bottom box
  tft.fillRect(20, 84, 88, 36, green); // fill box

  setHeaderText();
  tft.setTextColor(black);
  tft.setCursor(x, y);
  tft.print(button_text);
  setNormalText();
}

// Function to Display Confirmation Menu 1
void confirmationMenu1(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Confirm the"));
  tft.setCursor(13, 46);
  tft.print(F("sample is"));
  tft.setCursor(33, 66);
  tft.print(F("ready."));
}

// Function to Display Confirmation Menu 2
void confirmationMenu2(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Prepare the"));
  tft.setCursor(17, 46);
  tft.print(F("water to"));
  tft.setCursor(9, 66);
  tft.print(F("calibrate."));
}

// Function to Display the Calibration Menu
void calibrationMenu() {
  tft.fillScreen(black);
  setHeaderText();
  tft.setTextColor(green);
  tft.setCursor(4, 50);
  tft.print(F("CALIBRATING"));
  tft.drawRect(14, 70, 100, 20, green);  // Progress bar outline

  int uvReadings = 0;
  float specReadings[6] = {0, 0, 0, 0, 0, 0};
  
  // Take 20 samples to calibrate the sensors
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
    tft.fillRect(14, 70, 5*(i+1), 20, green);  // Update progress bar
  }

  // Calculate average calibration values
  uvCalibration = uvReadings / 20;
  for (uint8_t i = 0; i < 6; i++) {
    calibration[i] = specReadings[i] / 20;
    if (calibration[i] < 0) {
      calibration[i] = 0;
    }
  }

  writeConfig();  // Save the calibration data to SD card
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

// Function to Handle Button Presses for Menu Navigation
int8_t buttons() {
  int8_t historicSelectState = digitalRead(selectPin);  // Initial state of the select button
  int8_t historicSwitchState = digitalRead(switchPin);  // Initial state of the switch button

  while (true) {
    delay(20);  // Debounce delay

    selectState = digitalRead(selectPin);  // Read the select button state
    switchState = digitalRead(switchPin);  // Read the switch button state

    if (selectState == 1 && historicSelectState == 0) {
      return 1;  // Select button pressed
    }

    if (switchState == 1 && historicSwitchState == 0) {
      return 0;  // Switch button pressed
    }

    historicSelectState = selectState;  // Update historical states
    historicSwitchState = switchState;
  }
}

// Function to Check for Button Press (Simplified version)
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

// Function to Send a Command to OpenLog and Wait for a Response
void sdCommand(char* command, char* filename, char confirmation) {
  OpenLog.print(command);
  OpenLog.print(" ");
  OpenLog.print(filename);
  OpenLog.write(13);  // Send carriage return
  waitForChar(confirmation);  // Wait for confirmation character from OpenLog
}

// Function to Enter OpenLog Command Mode
void gotoCommandMode(void) {
  OpenLog.write(26);
  OpenLog.write(26);
  OpenLog.write(26);
  waitForChar('>');  // Wait for OpenLog to be ready
}

// Function to Wait for a Specific Character from OpenLog
void waitForChar(char c) {
  while(1) {
    if(OpenLog.available())
      if(OpenLog.read() == c) break;
  }  
}

// Function to Wait for a Non-Whitespace Character from OpenLog
char waitForRealChar() {
  char c;
  while(1) {
    if(OpenLog.available()) {
      c = (char) OpenLog.read();
      if(c != '\n' && c != '\r') return c;
    }
  }
}

// Function to Write a File to the SD Card via OpenLog
void writeFile(char* filename) {
  sdCommand("rm", filename, '>');
  sdCommand("new", filename, '>');
  sdCommand("write", filename, '<');
}

// Function to Update the Global Configuration Based on the Read Data
void updateGlobalConfig(uint8_t len, char *ar, uint8_t config_index) {
  if (config_index == 0) {
    index = atoi(ar);
  } else if (config_index > 0 && config_index < 7) {
    calibration[config_index-1] = atof(ar);
  } else if (config_index == 7) {
    uvCalibration = atoi(ar);
  }
}

// Function to Write the Configuration Data to the SD Card
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

// Function to Read Configuration Data from the SD Card
void readConfigData() {
  char c;
  byte config_index = 0;
  const uint8_t dataBufLen = 10;
  char dataBuf[dataBufLen];
  byte nData = 1;
  dataBuf[0] = waitForRealChar();

  for(int timeOut = 0 ; timeOut < 1000 ; timeOut++) {
    while((c = (char) OpenLog.read()) > 0) {
      if (c == ',' || c == '\r' || c == '\n') {
        dataBuf[nData]  = '\0';
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

// Function to Check if the Configuration File Header Matches
boolean checkHeader(char* header, uint16_t headerlen) {
  int filePosition = 0;
  int16_t c;

  waitForChar('\r');
  waitForChar('\n');
  
  for(int timeOut = 0 ; timeOut < 1000 ; timeOut++) {
    while((c = (char) OpenLog.read()) > 0) {
      if (c == header[filePosition]) {
        if (filePosition == headerlen-1) {
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

// Function to Load the Configuration from SD Card
void loadConfig() {
  sdCommand("read", "config.csv", '\r');
  bool header = checkHeader(configHeader, 20);
  if (header) {
    readConfigData();
  } else {
    writeConfig();
  }
}

// Function to Set the Home Menu
void setHomeMenu() {
  currentMenu = 0;
  currentSelection = 0;
  homeMenu1();
}

// Function to Set Header Text Style
void setHeaderText() {
  tft.setFont(&FreeSans9pt7b);
}

// Function to Set Normal Text Style
void setNormalText() {
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);
}

// Setup function - initializes the system
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

// Main loop function - handles menu navigation and measurements
void loop() {
  while (true) {
    int8_t button_pressed = buttons();  // Check button presses
    if (button_pressed == 0) {  // Switch button pressed
      if (currentMenu == 0) {
        if (currentSelection == 1) {
          currentSelection = 0;
          homeMenu1();
        } else if (currentSelection == 0){
          currentSelection = 1;
          homeMenu2();
        }
      }
    } else if (button_pressed == 1) {  // Select button pressed
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
        measuringMenu1();  // Start the measurement process
      } else if (currentMenu == 3) {
        currentMenu = 5;
        calibrationMenu();  // Start the calibration process
      } else if (currentMenu == 5) {
        setHomeMenu();  // Return to the home menu
      }
    }
  }
}

// Function to Setup Pins
void setupPins() {
  pinMode(selectPin, INPUT);
  pinMode(switchPin, INPUT);
  pinMode(uvSensorPin, INPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_RST, OUTPUT);
  pinMode(ledPin, OUTPUT);
}

// Function to Setup TFT Display
void setupTFT() {
  tft.initR(INITR_144GREENTAB); // 1.44" green tab TFT LCD
  tft.setSPISpeed(40000000); // TFT refresh rate
  tft.setRotation(2); // Flip TFT screen
  tft.setTextSize(1);
  tft.fillScreen(black);
}

// Function to Setup Sensors
void setupSensors() {
  Wire.begin();
  spectralSensor.begin();
}

// Function to Setup SD Logging
void setupSD() {
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
