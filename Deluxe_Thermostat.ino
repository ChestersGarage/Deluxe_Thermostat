/*
  Deluxe Thermostat program by Mark Chester
  
  Hardware:
    DS18B20 or LM335 for the inside temperature (uncomment only one 'define' below)
    Outside temperature and humidity via DHT22 (actually a RHT03)
    U8GLIB LCD library
    DS1307 RTC
*/

#define DS        // DS18B20 for inside temperature
#define DH          // DHT22 (RHT03) for outside temperature and humidity
#define DEBUGS      // Serial debugging

//#include <EEPROM.h>
#include <U8glib.h>
#include <Time.h>
#include <Wire.h>
#include <DS1307RTC.h>

#ifdef DS
#include <DallasTemperature.h>    
#include <OneWire.h>
#endif

#ifdef DH
#include "DHT.h"
#endif

// The screen is monochrome, 240 W x 128 H. Pixels are zero-indexed (0-239 and 0-127).
// 8Bit Com: D0..D7:  lcd(D0,D1,D2,D3,D4,D5,D6,D7,en,cs,rs,rw,[reset])
U8GLIB_LC7981_240X128 u8g(13,12,11,10, 9, 8, 7, 6, 3, 2, 5, 4);

#ifdef DS
#define tempInPinDS 14                   // DS18B20 on digital pin D14 (A0)
OneWire tempSensor(tempInPinDS);          // Initialize the OneWire (temperature sensor) library
DallasTemperature sensors(&tempSensor);  // Initialize DallasTemperature library
#endif

#ifdef DH
#define DHTPIN 15                     // RHT03 on digital pin D15 (A1)
//#define DHTTYPE DHT11   // DHT 11 
#define DHTTYPE DHT22   // DHT 22  (AM2302)
//#define DHTTYPE DHT21   // DHT 21 (AM2301)
DHT dht(DHTPIN, DHTTYPE);
#endif

boolean format12Hour = true;

int timePos, tempIn, tempOut, tempInMax, tempOutMax, tempInWidth, tempOutWidth, labelInWidth, labelOutWidth, labelInPos, labelOutPos, tempInPos, tempOutPos;
char tempInHiAP[3], tempInLoAP[3], tempOutHiAP[3], tempOutLoAP[3], tempInStr[4], tempOutStr[4]; 
char tempInHiTime[9], tempInLoTime[9], tempOutHiTime[9], tempOutLoTime[9];
int tempInHi=-999, tempInLo=999, tempOutHi=-999, tempOutLo=999;
long int tempLimitTimes[5], lastScreenDraw;
char* AMPM;
char* mZero;
char* sZero;
char dyStr[3], yearStr[5], hourStr[3], minuteStr[3], tempStr[4];
int hours, minutes, seconds, timeWidth, tPos, tempWidth, dateWidth;

// Set font sizes using define for convenience
#define tinyFont u8g_font_5x7r
#define smallFont u8g_font_6x12r
#define mediumFont u8g_font_9x15Br
#define largeFont u8g_font_osb26n
//#define largeFont Ubuntu_31

void setup(void) {
  #ifdef DEBUGS
  Serial.begin(9600);
  #endif
  
  #ifdef DS
  sensors.begin();                            // Start the OneWire (temperature) sensor
  #endif
  
  #ifdef DH
  dht.begin();
  #endif
  
  setSyncProvider(RTC.get);                   // The function to get the time from the RTC
  setSyncInterval(15);
  
  // Since the 'In' and 'Out' labels are static, we just calculate their width and position once at startup
  u8g.setFont(mediumFont);
  labelInWidth = u8g.getStrWidth("In");
  labelOutWidth = u8g.getStrWidth("Out");
  labelInPos = (114-labelInWidth)/2+4-21;
  labelOutPos = (114-labelOutWidth)/2+122-21;
}

/* So far these are not being used
// Single fist character
void firstDigit(int val){
  if(val < 10) u8g.print(' ');
  u8g.print(val);
}

// Zero padding
void digits(int val){
  if (val < 10) u8g.print('0');
  u8g.print(val);
}
*/

// Temperature
void getTemp(void) {                        // Reads the temperature and logs highs and lows
  #ifdef DS
  sensors.requestTemperatures();            // Trigger a temperature conversion on the sensor
  tempIn = sensors.getTempFByIndex(0);      // Read the temperature from the sensors
//  tempOut = sensors.getTempFByIndex(1);     // Read the temperature from the sensor
  #endif

  #ifdef DH
  tempOut = int(dht.readTemperature(1)+0.5);
  #endif
  
  itoa(tempIn,tempInStr,10);                // Convert temp to a string for U8Glib
  itoa(tempOut,tempOutStr,10);              // Convert temp to a string for U8Glib
  if ( tempIn > tempInHi ) {        // Check for new high temp
    tempInHi = tempIn;              // Save if higher than previous
    tempLimitTimes[0] = now();              // Save the time it happened
  }
  if ( tempIn < tempInLo ) {        // Check for new high temp
    tempInLo = tempIn;              // Save if higher than previous
    tempLimitTimes[1] = now();              // Save the time it happened
  }
  if ( tempOut > tempOutHi ) {       // Check for new high temp
    tempOutHi = tempOut;             // Save if higher than previous
    tempLimitTimes[2] = now();              // Save the time it happened
  }
  if ( tempOut < tempOutLo ) {       // Check for new high temp
    tempOutLo = tempOut;             // Save if higher than previous
    tempLimitTimes[3] = now();              // Save the time it happened
  }
}


void tempW(int t){
  itoa(t,tempStr,10);
  tempWidth = u8g.getStrWidth(tempStr);
}

// Time of day in HH:MM AM
void timeHM(time_t t){
  if (format12Hour){
    if (isPM(t)) AMPM = " PM";
    else AMPM = " AM";
    hours = hourFormat12(t);
  }
  else {
    AMPM = "";
    hours = hour(t);
  }
  minutes = minute(t);
  if (minutes < 10) {
    mZero = "0";
  }
  else {
    mZero = "";
  }
  itoa(hours,hourStr,10);
  itoa(minutes,minuteStr,10);
  timeWidth = u8g.getStrWidth(hourStr)+u8g.getStrWidth(":")+u8g.getStrWidth(mZero)+u8g.getStrWidth(minuteStr)+u8g.getStrWidth(AMPM);
  }

// Prints the time in the form: HH:MM PM
void printHM(){
  u8g.print(hours);
  u8g.print(":");
  u8g.print(mZero);
  u8g.print(minutes);
  u8g.print(AMPM);
}

// Graphical elements
void drawGraphics(void){
  u8g.setColorIndex(1);                               // Set pixels to black
  u8g.drawFrame(4,4,114,60);                          // Draw the frames
  u8g.drawFrame(122,4,114,60);
  u8g.setColorIndex(0);                               // Set pixels to clear
  u8g.drawBox(4,4,10,10);                             // Remove 10px from the corners of the frames
  u8g.drawBox(108,4,10,10);
  u8g.drawBox(4,54,10,10);
  u8g.drawBox(108,54,10,10);
  u8g.drawBox(122,4,10,10);
  u8g.drawBox(226,4,10,10);
  u8g.drawBox(122,54,10,10);
  u8g.drawBox(226,54,10,10);
  u8g.setColorIndex(1);                               // Set pixels back to black 
  u8g.drawCircle(13,13,9,U8G_DRAW_UPPER_LEFT);        // Draw round corners of the frames
  u8g.drawCircle(108,13,9,U8G_DRAW_UPPER_RIGHT);
  u8g.drawCircle(13,54,9,U8G_DRAW_LOWER_LEFT);
  u8g.drawCircle(108,54,9,U8G_DRAW_LOWER_RIGHT);
  u8g.drawCircle(131,13,9,U8G_DRAW_UPPER_LEFT);
  u8g.drawCircle(226,13,9,U8G_DRAW_UPPER_RIGHT);
  u8g.drawCircle(131,54,9,U8G_DRAW_LOWER_LEFT);
  u8g.drawCircle(226,54,9,U8G_DRAW_LOWER_RIGHT);
  u8g.drawCircle(tempInPos+(tempInWidth)+3,33,2);     // Draw the "degrees" characters
  u8g.drawCircle(tempOutPos+(tempOutWidth)+3,33,2);
}

// Tiny font
void drawTinyFont(void){
  u8g.setFont(tinyFont);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();

  timeHM(tempLimitTimes[0]);       // Grab and format the current time
  tPos = (44-timeWidth)/2+72;
  u8g.setPrintPos(tPos,23);             // Inside high temp time
  printHM();

  timeHM(tempLimitTimes[1]);       // Grab and format the current time
  tPos = (44-timeWidth)/2+72;
  u8g.setPrintPos(tPos,48);             // Inside low temp time
  printHM();
  
  timeHM(tempLimitTimes[2]);       // Grab and format the current time
  tPos = (44-timeWidth)/2+190;
  u8g.setPrintPos(tPos,23);            // Outside high temp time
  printHM();
  
  timeHM(tempLimitTimes[3]);       // Grab and format the current time
  tPos = (44-timeWidth)/2+190;
  u8g.setPrintPos(tPos,48);            // Outside low temp time
  printHM();
}

// Small font
void drawSmallFont(void){
  u8g.setFont(smallFont);
  u8g.setFontRefHeightExtendedText();
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();
  
  tempW(tempInHi);
  tempWidth = u8g.getStrWidth("H ")+tempWidth;
  tPos = (44-tempWidth)/2+73;
  u8g.setPrintPos(tPos,10);
  u8g.print("H ");
  u8g.print(tempInHi);

  tempW(tempInLo);
  tempWidth = u8g.getStrWidth("L ")+tempWidth;
  tPos = (44-tempWidth)/2+73;
  u8g.setPrintPos(tPos,35);
  u8g.print("L ");
  u8g.print(tempInLo);
  
  tempW(tempOutHi);
  tempWidth = u8g.getStrWidth("H ")+tempWidth;
  tPos = (44-tempWidth)/2+191;
  u8g.setPrintPos(tPos,10);
  u8g.print("H ");
  u8g.print(tempOutHi);
  
  tempW(tempOutLo);
  tempWidth = u8g.getStrWidth("L ")+tempWidth;
  tPos = (44-tempWidth)/2+191;
  u8g.setPrintPos(tPos,35);
  u8g.print("L ");
  u8g.print(tempOutLo);
  
  u8g.setPrintPos(tempInPos+(tempInWidth)+1,45);
  u8g.print("F");
  u8g.setPrintPos(tempOutPos+(tempOutWidth)+1,45);
  u8g.print("F");
}  

// Medium font
void drawMediumFont(void){
  u8g.setFont(mediumFont);                     // Set the font size
  u8g.setFontRefHeightExtendedText();          // Set overall height of the text 
  u8g.setDefaultForegroundColor();
  u8g.setFontPosTop();                         // Sets v-pos of the text baseline
  // Temperature labels
  u8g.setPrintPos(labelInPos,10);
  u8g.print("In");
  u8g.setPrintPos(labelOutPos,10);
  u8g.print("Out");
  // Date and time
  itoa(day(),dyStr,10);
  itoa(year(),yearStr,10);
  dateWidth = u8g.getStrWidth(", ")+u8g.getStrWidth(dayStr(weekday()))+u8g.getStrWidth(monthStr(month()))+u8g.getStrWidth(" ")+u8g.getStrWidth(dyStr)+u8g.getStrWidth(", ")+u8g.getStrWidth(yearStr);
  tPos = (240-dateWidth)/2;
  u8g.setPrintPos(tPos,75);
  u8g.print(dayStr(weekday()));
  u8g.print(", ");
  u8g.print(monthStr(month()));
  u8g.print(" ");
  u8g.print(day());
  u8g.print(", ");
  u8g.print(year());
  timeHM(now());                                // Grab and format the current time
  tPos = (240-timeWidth)/2;
  u8g.setPrintPos(tPos,95);
  printHM();
}

// Large font
void drawLargeFont(void){
  u8g.setFont(largeFont);                       // Set the font size
  u8g.setFontRefHeightExtendedText();           // Set overall height of the text 
  u8g.setDefaultForegroundColor();              
  u8g.setFontPosTop();                          // Sets v-pos of the text baseline
  // Main temperatures
  tempInWidth = u8g.getStrWidth(tempInStr);     // Measure the pixel width of the indoor temp
  tempOutWidth = u8g.getStrWidth(tempOutStr);   // Measure the pixel width of the outdoor temp
  tempInPos = (114-tempInWidth)/2+4-27;         // Calculate the h-pos of the text
  tempOutPos = (114-tempOutWidth)/2+122-27;     // Calculate the h-pos of the text
  u8g.setPrintPos(tempInPos,30);                // Place text here
  u8g.print(tempIn);                            // Print the temperature
  u8g.setPrintPos(tempOutPos,30);               // Place text here
  u8g.print(tempOut);                           // Print the temperature
}

void resetTempHi(void){
  if (hour() == 0 && minute() == 0 && second() <= 2){
    tempInHi=-999;
    tempOutHi=-999;
  }
}

void resetTempLo(void){
  if (hour() == 12 && minute() == 0 && second() <= 2){
    tempInLo=999;
    tempOutLo=999;
  }
}

void loop(void) {
  resetTempLo();
  resetTempHi();
  getTemp();                                    // Read the temperatures and set hi/lo
  Serial.print(now());
  Serial.print("\t");
  Serial.println(timeStatus());
  if ((millis()-lastScreenDraw) > 1000){
    lastScreenDraw = millis();
    u8g.firstPage();                              // LCD picture Loop
    do {
      drawGraphics();
      drawTinyFont();
      drawSmallFont();
      drawMediumFont();
      drawLargeFont();
    } while( u8g.nextPage() );                    // Part of the u8g picture loop
  }
}
