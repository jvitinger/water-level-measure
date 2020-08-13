/**
 * Blynk virtual pins:
 * V0 - the value - level of water or weight
 * V1 - calibration button
 * V2 - calibration value
 * V3 - LCD for all the info
 */

/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

/* Defines constants WIFI_SSID and WIFI_PASS, do not push to git */
#include "credentials.h"
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <EEPROM.h>

// You should get Auth Token in the Blynk App.
// Go to the Project Settings (nut icon).
char auth[] = BLYNK_AUTH;

// Your WiFi credentials.
// Set password to "" for open networks.
char ssid[] = WIFI_SSID;
char pass[] = WIFI_PASS;

#include "HX711.h" // https://github.com/bogde/HX711

// HX711 circuit wiring
// NodeMCU board must be selected in Arduino IDE to know D5 etc. constants
#define LOADCELL_DOUT_PIN D2
#define LOADCELL_SCK_PIN D3

float weight;
float rawValue;
float valueFromApp;

// Temporary values for the calibration point 1 (point 2 is not needed, it is immediately used for calculataion of multiplier and offset)
float calibrationValueFromApp;
float calibrationValueFromScale;

// Values that needs to be calibrated
float multiplier = 1;
float offset = 0;

bool calibrationInProgress = false;

HX711 scale;
BlynkTimer timer;
WidgetLCD lcd(V3);

void setup() {
  Serial.begin(9600);

  EEPROM.begin(512);

  EEPROM.get(0, multiplier);
  EEPROM.get(sizeof(float), offset);

  Serial.println("calibration data stored:");
  Serial.println("multiplier = " + floatToString(multiplier));
  Serial.println("offset = " + floatToString(offset));

  Blynk.begin(auth, ssid, pass);
  timer.setInterval(1000L, onBlynkTimer);

  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);

  writeCalibrationInfoToLCD();
}

void onBlynkTimer() {
  scale.power_up();

  rawValue = scale.read_average(10);
  weight = getWeight(rawValue);

  Serial.print("raw value:\t");
  Serial.print(rawValue, 2);
  Serial.print("\tweight:\t");
  Serial.println(weight, 2);

  Blynk.virtualWrite(V0, weight);

  scale.power_down(); // put the ADC in sleep mode
}

float getWeight(float value) {
  return value * multiplier + offset;
}

void writeCalibrationInfoToLCD() {
  lcd.clear();
  // use: (position X: 0-15, position Y: 0-1, "Message you want to print")
  if (calibrationInProgress) {
    lcd.print(0, 0, "Calibration"); 
    lcd.print(0, 1, "in progress...");
  }
  else if (!isCalibred()) {
    lcd.print(0, 0, "Start"); 
    lcd.print(0, 1, "calibration");
  }
  else {
    lcd.print(0, 0, "K = " + floatToString(multiplier)); 
    lcd.print(0, 1, "O = " + floatToString(offset));
  }
}

bool isCalibred() {
  return !(abs(multiplier - 1) < 0.00001 && abs(offset) < 0.00001);
}

String floatToString(float value) {
  char buff[10];
  return dtostrf(value, 4, 4, buff);  //4 is mininum width, 6 is precision
}

// Calibration values
BLYNK_WRITE(V2)
{
  valueFromApp = param.asFloat();
  Serial.print("calibration value from the app: \t");
  Serial.println(valueFromApp, 2);
}

// Button pressed
BLYNK_WRITE(V1)
{
  int pinValue = param.asInt();
  if (pinValue == 1) {
      Serial.println("calibration button pressed");
      if (calibrationInProgress) {
        multiplier = (valueFromApp - calibrationValueFromApp) / (rawValue - calibrationValueFromScale);
        offset = calibrationValueFromApp - multiplier * calibrationValueFromScale;

        EEPROM.put(0, multiplier);
        EEPROM.put(sizeof(float), offset);
        EEPROM.commit();

        calibrationInProgress = false;
        writeCalibrationInfoToLCD();
      }
      else {
        calibrationValueFromApp = valueFromApp;
        calibrationValueFromScale = rawValue;
        calibrationInProgress = true;
        writeCalibrationInfoToLCD();
      }
  }
}

void loop() {
  Blynk.run();
  timer.run();
}
