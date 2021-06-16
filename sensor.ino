#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "lcdgfx.h"
#include <Wire.h>
#include <ESP32Time.h>
#include "Adafruit_EEPROM_I2C.h"

#define BPMS_SIZE 128

Adafruit_EEPROM_I2C fram;
int bpms[BPMS_SIZE];
int bpmsIndex = 0;
float downloadedBPMSAVG = 0.0;

const char* ssid = "s1";
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

int pressCount = 0;


void setupFram(){
  fram.begin(0x50);
  display.clear();
  display.setColor(0xffff);
  display.drawWindow(0,0,0,0,"Downloading data",true);
  int progressBarValue;
  int beatsSum = 0;
  int readVal;
  int avgCount = 0;
  for(uint16_t i = 0; i < BPMS_SIZE; i++){
    readVal = fram.read8(i);
    progressBarValue = (i * 100) / 128;
    display.drawProgressBar(progressBarValue);
    
    if(readVal != 0){
      Serial.println(fram.read8(i));
      beatsSum += fram.read8(i);
      avgCount++;
    }
  }
  downloadedBPMSAVG = beatsSum / avgCount;
  Serial.print("average bpms: ");
  Serial.println(downloadedBPMSAVG);
}

void setupDisplay(){
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

void buttonHandler(){
  pressCount++;
  if(pressCount == 1){
    whichDisplay = 1;
    progressFirst = 1;
  }
  if(pressCount == 2){
    whichDisplay = 2;
    pressCount = 0;
    sensor = 0;
    digitalWrite(0, LOW);
  }
}

void addInterrupts(){
  attachInterrupt(0, buttonHandler, HIGH);
}

int second = 0;
int mainFirst = 1;
void handleMainDisplay(){
  if(mainFirst == 1){
    display.clear();
    mainFirst = 0;
  }
  if(rtc.getSecond() != second){
    second = rtc.getSecond();
    displayDate = rtc.getTime("%d/%m/%Y %H:%M:%S");
    display.printFixed(0,  0, displayDate.c_str(), STYLE_NORMAL);
    if(downloadedBPMSAVG > 0.0){
      display.printFixed(0,  32, "Previous: ", STYLE_NORMAL);
      display.printFixed(64,  32, String(downloadedBPMSAVG).c_str(), STYLE_NORMAL); 
    }
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
    progressFirst = 1;
    display.clear();
  } else{
    display.drawProgressBar(progress); 
  }
}

int savingFirst = 1;
void savingDisplay(){
//  if(savingFirst == 1){
    display.clear();
    display.setColor(0xffff);
    display.drawWindow(0, 0, 0, 0, "Saving", true);
    savingFirst = 0;
    for(uint16_t i = 0; i < BPMS_SIZE; i++){
      display.drawProgressBar(i + 1);
      fram.write8(i, bpms[i]);
    }
//  }
  whichDisplay = 0;
  display.clear();
}

long peaks[2];
int indx = 0;

int maxVal = 0;
long maxTime;
int minVal = 10000;
void handleSensor(){
  display.printFixed(0, 14, "Pulse: ", STYLE_BOLD);
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
      indx++;
      if(indx > 1){
        int diff = peaks[1] - peaks[0];
        int tmpSensor = (60.0 * 1000.0) / diff;
        display.setColor(0);
        display.fillRect(55, 14, 128, 20);
        display.printFixed(55, 14, String(tmpSensor).c_str(), STYLE_BOLD);
        bpms[bpmsIndex] = tmpSensor;
        bpmsIndex++;
        if(bpmsIndex > BPMS_SIZE){
          whichDisplay = 2;
          pressCount = 0;
          digitalWrite(0, LOW);
        }
        indx = 0;
      }
      maxVal = 0;
      minVal = 10000;
    }
  }
}

void setup() {
  Serial.begin(115200);
  Wire.begin(1, 2);
  setupDisplay();
  setupFram();
  setupNTP();
  setupPins();
  addInterrupts();

}

void loop() {
  if(whichDisplay == 0){
    handleMainDisplay();
  } else if(whichDisplay == 1){
    displayProgressBar();
  } else{
    savingDisplay();
  }

  if(sensor){
    handleSensor();
  }
}
