/*
NEED TO CALIBARTE BEFORE USE:
- coefficentsA
- coefficentsB
- thresholds
- offsets
*/ 
// set to nominal value of "0.0" to indicate calibration required
double coefficentsA[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
double coefficentsB[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
double thresholds[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
double offsets[2] = {0.0, 0.0};

/* 
 * DO NOT MODIFY ANYTHING BELOW HERE (Unless you know what you're doing!)
 */

//Import Statements
#include <Adafruit_GFX.h> //Adafruit Graphcis library
#include <Adafruit_ST7735.h> //1.44" tft lcd library
#include <Fonts/FreeSans9pt7b.h> //Header font library
#include <Fonts/FreeMono9pt7b.h> //Text font library
#include "AS726X.h" //AS726x spectral sensor library
#include <SoftwareSerial.h>

//Pin Definitions
#define TFT_CS 10 //TFT CS pin
#define TFT_RST 9 //TFT Reset pin
#define TFT_DC 8 //TFT DC pin
#define SD_RX 3 //SD RX Pin
#define SD_TX 4 //SD TX Pin
#define SD_RST 5 //SD RST Pin
#define selectPin 7 //Select button input pin
#define switchPin 6 //Switch button input pin
#define uvSensorPin A2 //UV sensor analog pin
#define ledPin 2

//Constant Definitions
#define configName "CONFIG.TXT"
#define configHeader "index,r,o,y,g,b,v,uv"

//Color Definitions
#define black ST77XX_BLACK //black
#define green ST77XX_GREEN //green
#define white ST77XX_WHITE //white
#define blue ST77XX_BLUE //blue
#define red ST77XX_RED //red
#define orange ST77XX_ORANGE //orange
#define yellow ST77XX_YELLOW //yellow
#define dark_green 0x1D80 //dark green
#define violet 0xD81F //violet

//Dynamic Variables
int8_t selectState = 0;
int8_t switchState = 0;
int8_t currentMenu = 0;
int8_t currentSelection = 0;

int index = 0;
float calibration[6] = {0, 0, 0, 0, 0, 0};
int uvCalibration = 0;


//Object Definitions
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST); //tft object
AS726X spectralSensor; //spectral sensor object
SoftwareSerial OpenLog(SD_RX, SD_TX);



//measuringMenuHeader
void measuringMenuHeader(void) {
  tft.fillScreen(black);

  //graph boxes
  tft.drawRect(6, 6, 116, 116, green);
}

void graphColor(int8_t x, int8_t y, int8_t w, int8_t h, float *xMin, float *xMax, int8_t *xLen, float measurement, int color, int8_t xAr[]) {
    //check for min/max
    if (measurement > *xMax || *xMax == -1) {
        *xMax = measurement;
    }

    if (measurement < *xMin || *xMin == -1) {
        *xMin = measurement;
    }

    int8_t result = calculateGraphPosition(*xMin, *xMax, h-1, measurement);

    //add to array
    if (*xLen < w) {
      xAr[*xLen] = result;
      *xLen += 1;
    } else {
      for (int8_t i = 1; i < *xLen-1; i++) {
        xAr[i-1] = xAr[i];
      }
      xAr[w-1] = result;
    }

    //display
    tft.fillRect(x, y, w, h, black);
    tft.drawPixel(x, y+h-1-xAr[0], color);
    if (*xLen > 1) {
      for (int8_t i = 0; i < *xLen-1; i++) {
        tft.drawLine(x+i, y+h-1-xAr[i], x+i+1, y+h-1-xAr[i+1], color);
      }
    }
}


//returns the size of a character array using a pointer to the first element of the character array
int size(char *ptr)
{
    //variable used to access the subsequent array elements.
    int offset = 0;
    //variable that counts the number of elements in your array
    int count = 0;

    //While loop that tests whether the end of the array has been reached
    while (*(ptr + offset) != '\0')
    {
        //increment the count variable
        ++count;
        //advance to the next element of the array
        ++offset;
    }
    //return the size of the array
    return count;
}


void flushOpenLog() {
  char c;
  
  for (int i = 0; i < 1000; i++) {
    if (OpenLog.available() > 0) {
      c = OpenLog.read();
      i = 0;
    }
    delay(1);
  }
}


//measuringMenu1
void measuringMenu1(void) {
  
  
  char indexNum[5];
  itoa(index, indexNum, 10);
  int8_t index_size = size(&indexNum[0]);
  
  char sampleName[7+index_size+1] = "LOG";

  for (int i = 0; i < index_size; i++) {
    sampleName[3+i] = indexNum[i];
  }
  sampleName[3+index_size] = (char) 46;
  sampleName[4+index_size] = (char) 67;
  sampleName[5+index_size] = (char) 83;
  sampleName[6+index_size] = (char) 86;
  sampleName[7+index_size] = '\0';
  

  index++;
  writeConfig();  
  
  measuringMenuHeader();

  
  tft.setTextSize(1);
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);

  float specReadings[6];

  int8_t historicSelectState = digitalRead(selectPin);
  
  unsigned long start_time = millis();
  unsigned long current_time;
  unsigned long past_time = 0;

  int maxlen = 50;
  double x2 = 0;
  double xy[6] = {0, 0, 0, 0, 0, 0};
  int datalen = 0;
  double inital[6];
  boolean initalTaken = false;
  int signal_resolution_multiplier = 1000000;
  int time_resolution_multiplier = 1000;
  int ms_time = 300000/maxlen;


  tft.drawRect(14, 70, 100, 20, green);
  sdCommand("rm", sampleName, '>');
  sdCommand("new", sampleName, '>');
  delay(10);
  OpenLog.print("append");
  delay(10);
  OpenLog.print(" ");
  delay(10);
  OpenLog.print(sampleName);
  delay(10);
  OpenLog.write(13);
  delay(15);
  OpenLog.print(F("time,red,orange,yellow,green,blue,violet,uv"));
  delay(15);
  OpenLog.write(26);
  delay(10);
  OpenLog.write(26);
  delay(10);
  OpenLog.write(26);
  delay(15);

  while (true) {
    digitalWrite(ledPin, HIGH);
    delay(200);
    int uvReading = analogRead(uvSensorPin);
    spectralSensor.takeMeasurements();
    delay(50);
    digitalWrite(ledPin, LOW);
    
    current_time = millis()- start_time;
    
    specReadings[0] = spectralSensor.getCalibratedRed();
    specReadings[1] = spectralSensor.getCalibratedOrange();
    specReadings[2] = spectralSensor.getCalibratedYellow();
    specReadings[3] = spectralSensor.getCalibratedGreen();
    specReadings[4] = spectralSensor.getCalibratedBlue();
    specReadings[5] = spectralSensor.getCalibratedViolet();
    double current_time_total_minutes = current_time/(60000.0/time_resolution_multiplier);
    Serial.println(current_time_total_minutes);
    int8_t current_time_minutes = current_time/60000;
    int8_t current_time_seconds = (current_time/1000)%60;
    tft.fillRect(0, 107, 67, 20, black);
    tft.setCursor(11, 120);
    if (current_time_minutes > 9) {
      tft.print(current_time_minutes);
    } else {
      tft.print("0");
      tft.setCursor(21, 120);
      tft.print(current_time_minutes);
    }
    
    tft.setCursor(29, 120);
    tft.print(":");
    tft.setCursor(36, 120);
    if (current_time_seconds > 9) {
      tft.print(current_time_seconds);
    } else {
      tft.print("0");
      tft.setCursor(46, 120);
      tft.print(current_time_seconds);
    }
    
    OpenLog.print("append");
    delay(10);
    OpenLog.print(" ");
    delay(10);
    OpenLog.print(sampleName);
    delay(10);
    OpenLog.write(13);
    delay(15);
    OpenLog.write(13);
    delay(10);
    
    OpenLog.print(current_time);
    delay(15);
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


    if (!initalTaken) {
      for (int i = 0; i < 6; i++) {
        inital[i] = specReadings[i];
      }
      initalTaken = true;
    }
    
    if (datalen < maxlen && current_time-past_time > ms_time) {
      x2 += current_time_total_minutes*current_time_total_minutes;
      
      for (int i = 0; i < 6; i++) {
        xy[i] = (((signal_resolution_multiplier*specReadings[i])/inital[i])-1)*current_time_total_minutes;  
      }
      past_time = current_time;
      datalen++;
    }


    if (current_time_minutes >= 5) {
      goto escape;
    }

    for (int8_t i = 0; i < 80; i++) {
      delay(5);
  
      selectState = digitalRead(selectPin);
  
  
      if (selectState == 1 && historicSelectState == 0) {
        goto escape;
      }//end of if
  
      historicSelectState = selectState;
    }

    int progress = 20*current_time_total_minutes/time_resolution_multiplier;
    tft.fillRect(14, 70, progress, 20, green);
    

  }
  escape:
  OpenLog.write(13);
  Serial.println(x2);
  Serial.println(xy[0]);
  double result = 0;
 
  double slopes[6];

  for (int i = 0; i < 6; i++) {
      slopes[i] = xy[i]/x2;
  }

  if (slopes[0] >= thresholds[0] && slopes[1] >= thresholds[1] && slopes[2] >= thresholds[2] && slopes[3] >= thresholds[3] && slopes[4] >= thresholds[4] && slopes[5] >= thresholds[5]) {
    result += offsets[0];
    for (int i = 0; i < 6; i++) {
      result += (coefficentsA[i]*(xy[i]/(x2)))/100;
    }
  } else {
    result += offsets[1];
    for (int i = 0; i < 6; i++) {
      result += (coefficentsB[i]*(xy[i]/(x2)))/100;
    }
  }
  

  
  Serial.println(result);
  setHomeMenu();
}

//homeMenuHeader
void homeMenuHeader(void) {
  tft.fillScreen(black);

  tft.drawRect(6, 6, 116, 70, green); //top
  tft.drawRect(6, 82, 116, 40, green); //bottom

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


//homeMenu1
void homeMenu1(void) {
  homeMenuHeader();
  tft.fillRect(8, 8, 112, 66, green); //top box
  tft.setCursor(44, 34);
  tft.print(F("NEW"));
  tft.setCursor(28, 58);
  tft.print(F("SAMPLE"));
}

//homeMenu2
void homeMenu2(void) {
  homeMenuHeader();
  tft.fillRect(8, 84, 112, 36, green); //top box
  tft.setCursor(14, 108);
  tft.print(F("CALIBRATE"));
}

void buttonMenuHeader(char *button_text, uint8_t x, uint8_t y) {
  tft.fillScreen(black);
 
  tft.drawRect(18, 82, 92, 40, green); //bottom start
  tft.fillRect(20, 84, 88, 36, green); //bottom start

  setHeaderText();
  tft.setTextColor(black);
  tft.setCursor(x, y);
  tft.print(button_text);
  setNormalText();
}

//confirmationMenu1
void confirmationMenu1(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Confirm the"));
  tft.setCursor(13, 46);
  tft.print(F("sample is"));
  tft.setCursor(33, 66);
  tft.print(F("ready."));
}

//confirmationMenu2
void confirmationMenu2(void) {
  buttonMenuHeader("START", 34, 108);
  tft.setCursor(4, 26);
  tft.print(F("Prepare the"));
  tft.setCursor(17, 46);
  tft.print(F("water to"));
  tft.setCursor(9, 66);
  tft.print(F("calibrate."));
}

//calibrationMenu
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

  uvCalibration = uvReadings/20;

  for (uint8_t i = 0; i < 6; i++) {
    calibration[i] = specReadings[i]/20;
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

int calculateGraphPosition(float minX, float maxX, int height, float measurement) {
  if (minX != maxX){  
    return ((measurement - minX)/(maxX-minX))*height;
  }
  return 0;
}

//setNormalText
void setNormalText(void) {
  tft.setFont(&FreeMono9pt7b);
  tft.setTextColor(green);
}

//setHeaderText
void setHeaderText(void) {
  tft.setFont(&FreeSans9pt7b);
}

//setupTFT
void setupTFT(void) {
  
  tft.initR(INITR_144GREENTAB); //1.44" green tab tft lcd
  tft.setSPISpeed(40000000); //tft refresh rate
  tft.setRotation(2); //flip tft screen
  tft.setTextSize(1);
  tft.fillScreen(black);
}

//setupPins
void setupPins(void) {
  pinMode(selectPin, INPUT);
  pinMode(switchPin, INPUT);
  pinMode(uvSensorPin, INPUT);
  pinMode(TFT_CS, OUTPUT);
  pinMode(SD_RST, OUTPUT);
  pinMode(ledPin, OUTPUT);
}

//setupSensors
void setupSensors(void) {
  Wire.begin();
  spectralSensor.begin();
  
}


//setupSD
void setupSD(void) {
  OpenLog.begin(9600);

  //Reset OpenLog
  digitalWrite(SD_RST, LOW);
  delay(100);
  digitalWrite(SD_RST, HIGH);

  //Wait for OpenLog to respond with '<' to indicate it is alive and recording to a file
  while(1) {
    if(OpenLog.available())
      if(OpenLog.read() == '<') break;
  }
}

void updateGlobalConfig(uint8_t len, char *ar, uint8_t config_index) {

  if (config_index == 0) {
    index = atoi(ar);
  }  else if (config_index > 0 && config_index < 7) {
    calibration[config_index-1] = atof(ar);
  } else if (config_index == 7) {
    uvCalibration = atoi(ar);
  }
   
}

void waitForChar(char c) {
  while(1) {
    if(OpenLog.available())
      if(OpenLog.read() == c) break;
  }  
}

void sdCommand(char* command, char* filename, char confirmation) {
  OpenLog.print(command);
  OpenLog.print(" ");
  OpenLog.print(filename);
  OpenLog.write(13);
  waitForChar(confirmation);
}

void gotoCommandMode(void) {
  OpenLog.write(26);
  OpenLog.write(26);
  OpenLog.write(26);

  waitForChar('>');
}

char waitForRealChar() {
  char c;
  while(1) {
    if(OpenLog.available()) {
      c = (char) OpenLog.read();
      if(c != '\n' && c != '\r') return c;
    }
  }
}

void writeFile(char* filename) {
  sdCommand("rm", filename, '>');
  sdCommand("new", filename, '>');
  sdCommand("write", filename, '<');
}

float linearRegression(float *x, float *y, int len) {
  float xySum = 0;
  float x2Sum = 0;

  for (int i = 0; i < len; i++) {
    xySum += x[i] + y[i];
    x2Sum += x[i] + x[i];
  }

  return xySum/x2Sum;
}

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
  
}

void loadConfig() {
  sdCommand("read", "config.csv", '\r');
  bool header = checkHeader(configHeader, 20);
  if (header) {
    readConfigData();
  } else {

    writeConfig();
  }
}

void setHomeMenu() {
  currentMenu = 0;
  currentSelection = 0;
  homeMenu1();
}


//buttons
int8_t buttons() {

  int8_t historicSelectState = digitalRead(selectPin);
  int8_t historicSwitchState = digitalRead(switchPin);

  while (true) {

    delay(20);

    selectState = digitalRead(selectPin);
    switchState = digitalRead(switchPin);

    if (selectState == 1 && historicSelectState == 0) {
      return 1;
    }//end of if

    if (switchState == 1 && historicSwitchState == 0) {
      return 0;
    }//end of if

    historicSelectState = selectState;
    historicSwitchState = switchState;
  }//end of while
}//end of button

//setup
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

int ledState = 0;

//loop
void loop() {
  
  while (true) {
    int8_t button_pressed = buttons();
    if (button_pressed == 0) {
      
      if (currentMenu == 0) {
        if (currentSelection == 1) {
          currentSelection = 0;
          homeMenu1();
        } else if (currentSelection == 0){
          currentSelection = 1;
          homeMenu2();
        }
      }

   
    } else if (button_pressed == 1) {
      
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
    }//end of else if
  }
  delay(100);
  
}
