#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "lcdgfx.h"
#include <Wire.h>
#include <ESP32Time.h>
#include "Adafruit_EEPROM_I2C.h"

#define BPMS_SIZE 128

Adafruit_EEPROM_I2C fram;
//int bpms[BPMS_SIZE];
//int bpmsIndex = 0;

const char* ssid     = "s1";
const char* password = "siemanko";
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);
String displayDate;

DisplaySH1106_128x64_I2C display(-1);

ESP32Time rtc;

int whichDisplay = 0;
int progressFirst = 1;
int progressStart;
int sensor = 0;
int sig;
int threshold = 6000;

int history = 0;
uint8_t value;

// Should be working good
int second = 0;
int mainFirst = 1;
void handleMainDisplay(){
  if(mainFirst == 1){
    display.clear();
    mainFirst = 0;
    if(history){
      display.printFixed(0,  32, "Previous: ", STYLE_NORMAL);
      display.printFixed(64,  32, String(value).c_str(), STYLE_NORMAL); 
    }
  }
  if(rtc.getSecond() != second){
    second = rtc.getSecond();
    displayDate = rtc.getTime("%d/%m/%Y %H:%M:%S");
    display.printFixed(0,  0, displayDate.c_str(), STYLE_NORMAL);
  }
}

void setupDisplay(){
  Wire.begin(1, 2);
  display.begin();
  display.clear();
  display.setFixedFont(ssd1306xled_font6x8);
}
void setupNTP(){
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("WiFi connected");

  timeClient.begin();
//  display.clear();
  timeClient.setTimeOffset(7200);

  timeClient.update();
  unsigned long x = timeClient.getEpochTime();
  rtc.setTime(x);
}
void setupPins(){
  pinMode(0, INPUT);
  pinMode(13, OUTPUT);
}
void addInterrupts(){
  attachInterrupt(0, buttonHandler, HIGH);
}
void setupFram(){
  fram.begin(0x50);
  value = fram.read8(0);
  if(value != 0){
    history = 1;
  }
}
void displayProgressBar(){
  if(progressFirst == 1){
    display.clear();
    display.setColor(0xffff);
    display.drawWindow(0,0,0,0,"Setting up",true);
    progressStart = rtc.getSecond();
    progressFirst = 0;
  }
  
  int progress = ((rtc.getSecond() - progressStart) * 100) / 4;
  if(progress == 100){
    whichDisplay = 0;
    sensor = 1;
//    progressFirst = 1;
    display.clear();
  } else{
    display.drawProgressBar(progress); 
  }
}
int savingFirst = 1;
void savingDisplay(){
  if(savingFirst == 1){
    display.clear();
    display.setColor(0xffff);
    display.drawWindow(0,0,0,0,"Saving",true);
    savingFirst = 0;
  }
  // save here
//  for(uint16_t i = 0; i < BPMS_SIZE; i++){
//    display.drawProgressBar(i + 1);
//    fram.write8(i, bpms[i]);
//  }
  whichDisplay = 0;
  display.clear();
}
int pressCount = 0;
void buttonHandler(){
  pressCount++;
  if(pressCount == 1){
    whichDisplay = 1;
    progressFirst = 1;
  }
  if(pressCount == 2){
    whichDisplay = 2;
    pressCount = 0;
    digitalWrite(0, LOW);
    sensor = 0;
  }
}

long peaks[2];
int indx = 0;

int maxVal = 0;
long maxTime;
int minVal = 10000;
void handleSensor(){
  display.printFixed(0, 14, "Pulse: ", STYLE_NORMAL);
  sig = analogRead(03);
  if(sig > threshold){
    digitalWrite(13, HIGH);
    if(sig > maxVal){
      maxVal = sig;
      peaks[indx] = millis();
    }
  } else{
    digitalWrite(13, LOW);
    if(maxVal != 0 || minVal != 10000){
      Serial.print("millis: ");
      Serial.println(millis());
      Serial.print("maxVal: ");
      Serial.println(maxVal);
      indx++;
      if(indx > 1){
        Serial.print("peaks[0]: ");
        Serial.println(peaks[0]);
        Serial.print("peaks[1]: ");
        Serial.println(peaks[1]);
        Serial.print("diff: ");
        Serial.println(abs(peaks[1] - peaks[0]));
        int diff = peaks[1] - peaks[0];
        int tmp = (60.0 * 1000.0) / diff;
        Serial.print("\nbpm: ");
        Serial.println(tmp);
        display.setColor(0);
//        display.drawRect(55, 14, 128, 64);
        display.fillRect(55, 14, 128, 20);
        display.printFixed(55, 14, String(tmp).c_str(), STYLE_NORMAL);
//        bpms[bpmsIndex] = tmp;
//        bpmsIndex++;
//        if(bpmsIndex > BPMS_SIZE){
//          whichDisplay = 2;
//          pressCount = 0;
//          digitalWrite(0, LOW);
//        }
        indx = 0;
      }
      maxVal = 0;
      minVal = 10000; 
    }
  }
}



void setup() {
  Serial.begin(115200);
  setupDisplay();
  setupNTP();
  setupPins();
  addInterrupts();
  setupFram();
}

void loop() {
  if(whichDisplay == 0){
    handleMainDisplay();
  } else if(whichDisplay == 2){
    savingDisplay();
  } else{
    displayProgressBar();
  }
  if(sensor){
    handleSensor();
  }
}
