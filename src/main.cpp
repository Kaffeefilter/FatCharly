#include <Arduino.h>
#include <HX711_ADC.h>
#include <EEPROM.h>
#include <ESP8266WiFiMulti.h>

#include "data.h"

void calibrate();
int maintainWifi();

ESP8266WiFiMulti wifiMulti;
boolean wifiConnected = false;

//pins:
const int HX711_dout_1 = D3; //mcu > HX711 no 1 dout pin
const int HX711_sck_1 = D4; //mcu > HX711 no 1 sck pin
const int HX711_dout_2 = D5; //mcu > HX711 no 2 dout pin
const int HX711_sck_2 = D6; //mcu > HX711 no 2 sck pin

//HX711 constructor (dout pin, sck pin)
HX711_ADC LoadCell_1(HX711_dout_1, HX711_sck_1); //HX711 1
HX711_ADC LoadCell_2(HX711_dout_2, HX711_sck_2); //HX711 2

const int calVal_eepromAdress_1 = 0; // eeprom adress for calibration value load cell 1 (4 bytes)
const int calVal_eepromAdress_2 = 4; // eeprom adress for calibration value load cell 2 (4 bytes)
unsigned long t = 0;

void setup() {
  Serial.begin(9600); delay(10);
  Serial.println();
  Serial.println("Starting...");

  float calibrationValue_1; // calibration value load cell 1
  float calibrationValue_2; // calibration value load cell 2

  EEPROM.begin(512); // uncomment this if you use ESP8266 and want to fetch the value from eeprom
  EEPROM.get(calVal_eepromAdress_1, calibrationValue_1); // uncomment this if you want to fetch the value from eeprom
  EEPROM.get(calVal_eepromAdress_2, calibrationValue_2); // uncomment this if you want to fetch the value from eeprom
  Serial.print("calibration value 1");
  Serial.println(calibrationValue_1);
  Serial.print("calibration value 2");
  Serial.println(calibrationValue_2);

  LoadCell_1.begin();
  LoadCell_2.begin();
  unsigned long stabilizingtime = 2000; // tare preciscion can be improved by adding a few seconds of stabilizing time
  boolean _tare = true; //set this to false if you don't want tare to be performed in the next step
  byte loadcell_1_rdy = 0;
  byte loadcell_2_rdy = 0;
  while ((loadcell_1_rdy + loadcell_2_rdy) < 2) { //run startup, stabilization and tare, both modules simultaniously
    if (!loadcell_1_rdy) loadcell_1_rdy = LoadCell_1.startMultiple(stabilizingtime, _tare);
    if (!loadcell_2_rdy) loadcell_2_rdy = LoadCell_2.startMultiple(stabilizingtime, _tare);
  }
  if (LoadCell_1.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 no.1 wiring and pin designations");
  }
  if (LoadCell_2.getTareTimeoutFlag()) {
    Serial.println("Timeout, check MCU>HX711 no.2 wiring and pin designations");
  }
  LoadCell_1.setCalFactor(calibrationValue_1); // user set calibration value (float)
  LoadCell_2.setCalFactor(calibrationValue_2); // user set calibration value (float)

  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WLAN_NAME_1, WLAN_PW_1);
  wifiMulti.addAP(WLAN_NAME_2, WLAN_PW_2);

  while (maintainWifi() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("Startup is complete");
  Serial.println("send w to continue...");
  boolean resume = false;
  while (resume == false)
  {
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 'w') {
        resume = true;
      }
    }
  }
  
}

void loop() {
  static boolean newDataReady = 0;
  const int serialPrintInterval = 500; //increase value to slow down serial print activity

  // check for new data/start next conversion:
  if (LoadCell_1.update()) newDataReady = true;
  LoadCell_2.update();

  //get smoothed value from data set
  if ((newDataReady)) {
    if (millis() > t + serialPrintInterval) {
      float a = LoadCell_1.getData();
      float b = LoadCell_2.getData();
      Serial.print("Load_cell 1 output val: ");
      Serial.print(a);
      Serial.print("    Load_cell 2 output val: ");
      Serial.print(b);
      Serial.print("    Combinded output val: ");
      Serial.println(a+b);
      newDataReady = 0;
      t = millis();
    }
  }

  // receive command from serial terminal, send 't' to initiate tare operation:
  if (Serial.available() > 0) {
    char inByte = Serial.read();
    if (inByte == 't') {
      LoadCell_1.tareNoDelay();
      LoadCell_2.tareNoDelay();
    }
    else if (inByte == 'c') {
      Serial.println("Start calibration? (y/n)");
      boolean resume = false;
      while (resume == false)
      {
        if (Serial.available() > 0)
        {
          inByte = Serial.read();
          if (inByte == 'y')
          {
            calibrate();
            resume = true;
          }
          else if (inByte == 'n')
          {
            Serial.println("calibration aborted");
            resume = true;
          }
        }
      }
    }
  }

  //check if last tare operation is complete
  if (LoadCell_1.getTareStatus() == true) {
    Serial.println("Tare load cell 1 complete");
  }
  if (LoadCell_2.getTareStatus() == true) {
    Serial.println("Tare load cell 2 complete");
  }

}

void calibrate() {

  Serial.println("***");
  Serial.println("Start calibration:");
  Serial.println("Place the load cell an a level stable surface.");
  Serial.println("Remove any load applied to the load cell.");
  Serial.println("Send 't' from serial monitor to set the tare offset.");

  boolean resume = false;
  int tareStatus = 0;
  while (resume == false) {
    LoadCell_1.update();
    LoadCell_2.update();
    if (Serial.available() > 0) {
      char inByte = Serial.read();
      if (inByte == 't') {
        LoadCell_1.tareNoDelay();
        LoadCell_2.tareNoDelay();
      }
    }
    if (LoadCell_1.getTareStatus() == true)
    {
      tareStatus++;
      Serial.println("Tare Loadcell 1 complete");
    }
    if (LoadCell_2.getTareStatus() == true)
    {
        tareStatus++;
        Serial.println("Tare Loadcell 2 complete");
    }
    if (tareStatus >= 2)
    {
      Serial.println("Tare complete");
      resume = true;
    }
  }

  Serial.println("Now, place your known mass on the right loadcell.");
  Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");
  float known_mass = 0;
  resume = false;
  while (resume == false)
  {
    LoadCell_1.update();
    if (Serial.available() > 0)
    {
      known_mass = Serial.parseFloat();
      if (known_mass != 0)
      {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        resume = true;
      }
    }
  }
  LoadCell_1.refreshDataSet();
  float newCalibrationValue_1 = LoadCell_1.getNewCalibration(known_mass);
  Serial.print("New calibration value has been set to: ");
  Serial.println(newCalibrationValue_1);

  Serial.println("\nNow, place your known mass on the left loadcell.");
  Serial.println("Then send the weight of this mass (i.e. 100.0) from serial monitor.");
  known_mass = 0;
  resume = false;
  while (resume == false)
  {
    LoadCell_2.update();
    if (Serial.available() > 0)
    {
      known_mass = Serial.parseFloat();
      if (known_mass != 0)
      {
        Serial.print("Known mass is: ");
        Serial.println(known_mass);
        resume = true;
      }
    }
  }
  LoadCell_2.refreshDataSet();
  float newCalibrationValue_2 = LoadCell_2.getNewCalibration(known_mass);

  Serial.print("New calibration value has been set to: ");
  Serial.println(newCalibrationValue_2);

  Serial.println("Save this values to EEPROM? (y/n)");
  resume = false;
  while (resume == false)
  {
    if (Serial.available() > 0)
    {
      char inByte = Serial.read();
      if (inByte == 'y')
      {
        EEPROM.begin(512);
        EEPROM.put(calVal_eepromAdress_1, newCalibrationValue_1);
        EEPROM.put(newCalibrationValue_2, newCalibrationValue_2);
        EEPROM.commit();

        EEPROM.get(calVal_eepromAdress_1, newCalibrationValue_1);
        Serial.print("Value ");
        Serial.print(newCalibrationValue_1);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress_1);
        EEPROM.get(calVal_eepromAdress_2, newCalibrationValue_2);
        Serial.print("Value ");
        Serial.print(newCalibrationValue_2);
        Serial.print(" saved to EEPROM address: ");
        Serial.println(calVal_eepromAdress_2);
        resume = true;
      }
      else if (inByte == 'n')
      {
        Serial.println("Value not saved to EEPROm");
        resume =  true;
      }
    }
  }

  Serial.println("End calibration");
  Serial.println("Resuming....");
  Serial.println("***");
  delay(3000);
}

int maintainWifi(){
  //wifiConnected to check if the wifi has been connected before
  if (WiFi.status() != WL_CONNECTED) wifiConnected = 0;
  if (wifiMulti.run() == WL_CONNECTED) {
    if(wifiConnected == 0) {
      Serial.print("WiFi connected: ");
      Serial.print(WiFi.SSID());
      Serial.print(" ");
      Serial.println(WiFi.localIP());
      wifiConnected = 1;
    }
    return WL_CONNECTED;
  } else {
    wifiConnected = 0;
    return WL_DISCONNECTED;
  }
}