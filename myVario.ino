#include <toneAC.h>
#include "GyverFilters.h"
#include <Wire.h>
#include "MS5611.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

//===================Vario settings===================
#define RIZE_THREHOLD 1
#define DECLINE_THREHOLD -20

const float ADJUSTED_SEA_LEVEL_PRESURE=102340;
#define MEASUREMENTS_PER_SECOND 5
#define ALT_ANDVARIO_SHOW_PER_SECOND 4
//=================END Vario settings=================

#define i2c_Address 0x3c //initialize with the I2C addr 0x3C Typically eBay OLED's

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1   //   QT-PY / XIAO
Adafruit_SH1106G display = Adafruit_SH1106G(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);



float alt = 0, temp_alt = 0, currentPressure;
unsigned int batVoltage=0, pikPeriod = 100;
unsigned long altAndVarioTmr = 0, pikTmr = 0, BatteryAndTemperatureTmr = 0, printAltitudeTmr = 0;
volatile int vario = 0, int_alt = 0, init_alt = 0, temperature = 0;
char varioString[5] = "+0.0";
char barreryVoltageString[5] = "3.7V";
char altString[19] = "ALT: 1250m | 1000m";
char temperatureStr[3];
//усреднитель
int buff[4] = {0, 0, 0, 0};
unsigned char buffPos = 0;
unsigned long bench = 0;
MS5611 ms5611;
// Фильтр Калмана. 
// параметры: разброс измерения, разброс оценки, скорость изменения значений
// разброс измерения: шум измерений
// разброс оценки: подстраивается сам, можно поставить таким же как разброс измерения
// скорость изменения значений: 0.001-1, варьировать самому
GKalman Kalman(0.01, 0.01, 0.005); 


void setup() {
 // initialize the serial communication:
  //Serial.begin(115200);
   while(!ms5611.begin(MS5611_ULTRA_HIGH_RES))
  {
    delay(1000);
  }
  currentPressure = ms5611.readPressure();
  init_alt  = getAltitude(currentPressure, ADJUSTED_SEA_LEVEL_PRESURE);
  alt = init_alt;
  int_alt = alt;
  display.begin(i2c_Address, true);
  display.setTextColor(SH110X_WHITE, SH110X_BLACK);
  display.clearDisplay();
  printStatic();
  printBatteryAndTemperature();
  printAltitude();
}

void loop() {
  //110ms
  if (millis() - altAndVarioTmr >= 200) { 
//    bench = millis();
    altAndVarioTmr = millis();
    updateAltitudeAndVario();
    printVario();
//    bench = millis() - bench;
//    Serial.println(bench);
  }
  //0ms
  if (millis() - pikTmr >= pikPeriod) {
//    bench = millis();
    
    pikTmr = millis();
    pik();

//    bench = millis() - bench;
//    Serial.println(bench); 
  }
  //16ms
  if (millis() - BatteryAndTemperatureTmr >= 5000) {
//    bench = millis();
    
    BatteryAndTemperatureTmr = millis();
    printBatteryAndTemperature();
    
//    bench = millis() - bench;
//    Serial.println(bench); 
  }
  //16ms
  if (millis() - printAltitudeTmr >= 1000) {
//    bench = millis();
    
    printAltitudeTmr = millis();
    printAltitude();

//    bench = millis() - bench;
//    Serial.println(bench); 
  }
}

void pik() {
  int varioTmp = 0;
  if(vario >= RIZE_THREHOLD) {
    varioTmp = vario > 50 ? 50 : vario;
    toneAC(750+5*varioTmp, 10, 200-2*varioTmp, true); 
    pikPeriod = 450-5*varioTmp;
  } 
  else if(vario <= DECLINE_THREHOLD)
  {
    varioTmp = vario < -50 ? -50 : vario;
    toneAC(250-varioTmp*2, 10, 500, true); 
    pikPeriod = 400;
  }else {
    pikPeriod = 100; 
  }  
}

void updateAltitudeAndVario(){
  currentPressure = ms5611.readPressure();
  //currentPressure = Kalman.filtered(currentPressure);
  temp_alt  = Kalman.filtered(getAltitude(currentPressure, ADJUSTED_SEA_LEVEL_PRESURE));
  //temp_alt  = getAltitude(currentPressure, ADJUSTED_SEA_LEVEL_PRESURE);
  //Serial.println(temp_alt);
  vario = (temp_alt - alt)*MEASUREMENTS_PER_SECOND*10;
  buff[buffPos] = vario;
  buffPos++;
  if(buffPos == 4) {
    buffPos = 0;
  }
  alt = temp_alt;
  int_alt = alt;
}

void printAltitude(){
  display.setTextSize(1);
  display.setCursor(10,54);
  sprintf(altString, "ALT:%5dm |%5dm", int_alt, int_alt-init_alt);
  display.print(altString);
}

void printVario() {
  formatVarioString();
  display.setTextSize(4);
  display.setCursor(7,15);
  display.print(varioString);
  display.display();
}

void printBatteryAndTemperature() {
   batVoltage = analogRead(A0);
   batVoltage = (batVoltage*48)/1000;
   barreryVoltageString[0]=batVoltage/10 + '0';
   barreryVoltageString[2]=batVoltage%10 + '0';
   
   temperature = ms5611.readTemperature();
   sprintf(temperatureStr, "%2d", temperature);
   display.setTextSize(1);
   display.setCursor(16,0);
   display.print(barreryVoltageString);
   display.setCursor(100,0);
   display.print(temperatureStr);
}

void formatVarioString() {
  int vario_tmp = buff[0] + buff[1] + buff[2] + buff[3];
  vario_tmp = vario_tmp/4;
  if(vario_tmp >= 0) {
    varioString[0] = '+';
  } else {
    varioString[0] = '-';
    vario_tmp = vario_tmp*(-1);
  } 
  vario_tmp = vario_tmp > 99 ? 99 : vario_tmp;
  varioString[1] = '0' + vario_tmp/10;
  varioString[3] = '0' + vario_tmp%10;
}

void printStatic(){
  display.setTextSize(1);
  //battery icon
  display.drawFastVLine(0, 3, 2, SH110X_WHITE);
  display.fillRect(1, 1, 10, 6, SH110X_WHITE);
  //degrees sign
  display.setCursor(100,0);
  display.print("   C");
  display.fillRect(115, 0, 2, 2, SH110X_WHITE);

  //M/S - icon
  display.setTextSize(2);
  display.setCursor(108,12);
  display.print("M");
  display.setCursor(108,32);
  display.print("S");
  display.drawFastHLine(108, 28, 10, SH110X_WHITE);
  display.drawFastHLine(108, 29, 10, SH110X_WHITE);
  display.display();
}

float getAltitude(float pressure, float seaLevelPressure)
{
    return (44330.0f * (1.0f - pow(pressure / seaLevelPressure, 0.1902949f)));
}
