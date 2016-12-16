/*
  Simple one-button conroller for the RX5808 receiver module
  based on the rx5808-pro-diversity project
  Adiitionally a low-voltage battery warning is added

  Single press: change channel number
  Hold: Search for the strongest signal and choose channel
  Double press: change band
*/

#include <avr/pgmspace.h>
#include <EEPROM.h>
#include "ClickButton.h"

// Channels to sent to the SPI registers
const uint16_t channelTable[] PROGMEM = {
  // Channel 1 - 8
  0x2A05,    0x299B,    0x2991,    0x2987,    0x291D,    0x2913,    0x2909,    0x289F,    // Band A
  0x2903,    0x290C,    0x2916,    0x291F,    0x2989,    0x2992,    0x299C,    0x2A05,    // Band B
  0x2895,    0x288B,    0x2881,    0x2817,    0x2A0F,    0x2A19,    0x2A83,    0x2A8D,    // Band E
  0x2906,    0x2910,    0x291A,    0x2984,    0x298E,    0x2998,    0x2A02,    0x2A0C,    // Band F / Airwave
  0x281D,    0x288F,    0x2902,    0x2914,    0x2987,    0x2999,    0x2A0C,    0x2A1E     // Band C / Immersion Raceband
};

// Channels with their Mhz Values
const uint16_t channelFreqTable[] PROGMEM = {
  // Channel 1 - 8
  5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725, // Band A
  5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866, // Band B
  5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945, // Band E
  5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880, // Band F / Airwave
  5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917  // Band C / Immersion Raceband
};

// All Channels of the above List ordered by Mhz
const uint8_t channelList[] PROGMEM = {
  19, 18, 32, 17, 33, 16, 7, 34, 8, 24, 6, 9, 25, 5, 35, 10, 26, 4, 11, 27, 3, 36, 12, 28, 2, 13, 29, 37, 1, 14, 30, 0, 15, 31, 38, 20, 21, 39, 22, 23
};

// Define pin numbers
#define spiDataPin 5 //ch1
#define slaveSelectPin 6 //ch2
#define spiClockPin 7 //ch3
#define rssiPinA A1
#define buzzer A4
#define buttonPin 3 //was A3
#define voltageInput A2

//define some constants
#define RSSI_READS 50
#define MIN_TUNE_TIME 25

//init variables
uint8_t channel = 0;
uint8_t bandNum = 0;
uint8_t channelNum = 0;
uint16_t rssi = 0;
unsigned long lastAlarm = 0;

//init button handler
ClickButton button1(buttonPin, LOW, CLICKBTN_PULLUP);

void setup() {
  //initialize pins
  pinMode(buzzer, OUTPUT);
  pinMode(rssiPinA, INPUT);
  pinMode(voltageInput, INPUT);
  pinMode(slaveSelectPin, OUTPUT);
  pinMode(spiDataPin, OUTPUT);
  pinMode(spiClockPin, OUTPUT);

  //init channel
  channel = constrain(EEPROM.read(10),0,39);
  setChannelModule(channel);
  channelNum = channel % 8;
  bandNum = channel / 8;

  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  button1.debounceTime   = 20;   // Debounce timer in ms
  button1.multiclickTime = 250;  // Time limit for multi clicks
  button1.longClickTime  = 1000; // time until "held-down clicks" register
}

void loop() {
  //read button and do action
  button1.Update();
  if (button1.clicks == 1) changeChannel(); //one klick
  if (button1.clicks == 2) changeBand(); //double klick
  if (button1.clicks == -1) search(); //hold


  //run voltage scan for battery every 500ms
  if (millis() - lastAlarm >= 500) {
    lastAlarm = millis();
    voltageAlarm();
  }
}

void beep(uint8_t times, uint16_t freq){
  for(uint8_t i=0; i < times; i++){
    tone(buzzer, freq, 60);
    delay(120);
  }
}

void changeChannel() {
  channelNum += 1;
  if (channelNum > 7) channelNum = 0;
  channel = bandNum*8 + channelNum;
  setChannelModule(channel);
  EEPROM.write(10,channel);
  beep(channelNum+1, 2000);
}

void changeBand() {
  bandNum += 1;
  if (bandNum > 4) bandNum = 0;
  channel = bandNum*8 + channelNum;
  setChannelModule(channel);
  EEPROM.write(10,channel);
  beep(bandNum+1, 1000);
}

void search() { //will take aprox 1 second
  uint16_t bestChan[] = {0, 0}; //channel number, rssi

  for (uint8_t i = 0; i < 40; i++) {
    channel = pgm_read_byte_near(channelList + i);
    setChannelModule(channel);
    delay(MIN_TUNE_TIME);
    rssi = readRSSI();
    if (rssi > bestChan[1]) {
      bestChan[0] = channel;
      bestChan[1] = rssi;
    }
  }

  // select the best channel
  setChannelModule(bestChan[0]);
  EEPROM.write(10,bestChan[0]);
}

uint16_t readRSSI() {
  uint16_t rssiRead = 0;
  for (uint8_t i = 0; i < RSSI_READS; i++)
  {
    rssiRead += analogRead(rssiPinA);
  }
  rssiRead = rssiRead / RSSI_READS; // average of RSSI_READS readings
  return rssiRead;
}

void voltageAlarm() {
  static uint8_t cells = 2;
  uint16_t voltage = 0;

  voltage = analogRead(voltageInput) * 13.7;

  if (voltage > cells * 4700) {
    cells++;
  }
  else if (voltage < cells * 2500 && cells > 1) {
    cells--;
  }
  else {
    if (voltage < cells * 3500 && voltage > cells * 3300) {
      tone(buzzer, 1500, 100);
    }
    else if (voltage < 3300) {
      tone(buzzer, 1500, 250);
    }
  }
}

void setChannelModule(uint8_t channel)
{
  uint8_t i;
  uint16_t channelData;

  channelData = pgm_read_word_near(channelTable + channel);

  // bit bash out 25 bits of data
  // Order: A0-3, !R/W, D0-D19
  // A0=0, A1=0, A2=0, A3=1, RW=0, D0-19=0
  SERIAL_ENABLE_HIGH();
  delayMicroseconds(1);
  //delay(2);
  SERIAL_ENABLE_LOW();

  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT1();

  SERIAL_SENDBIT0();

  // remaining zeros
  for (i = 20; i > 0; i--)
    SERIAL_SENDBIT0();

  // Clock the data in
  SERIAL_ENABLE_HIGH();
  //delay(2);
  delayMicroseconds(1);
  SERIAL_ENABLE_LOW();

  // Second is the channel data from the lookup table
  // 20 uint8_ts of register data are sent, but the MSB 4 bits are zeros
  // register address = 0x1, write, data0-15=channelData data15-19=0x0
  SERIAL_ENABLE_HIGH();
  SERIAL_ENABLE_LOW();

  // Register 0x1
  SERIAL_SENDBIT1();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();
  SERIAL_SENDBIT0();

  // Write to register
  SERIAL_SENDBIT1();

  // D0-D15
  //   note: loop runs backwards as more efficent on AVR
  for (i = 16; i > 0; i--)
  {
    // Is bit high or low?
    if (channelData & 0x1)
    {
      SERIAL_SENDBIT1();
    }
    else
    {
      SERIAL_SENDBIT0();
    }

    // Shift bits along to check the next one
    channelData >>= 1;
  }

  // Remaining D16-D19
  for (i = 4; i > 0; i--)
    SERIAL_SENDBIT0();

  // Finished clocking data in
  SERIAL_ENABLE_HIGH();
  delayMicroseconds(1);
  //delay(2);

  digitalWrite(slaveSelectPin, LOW);
  digitalWrite(spiClockPin, LOW);
  digitalWrite(spiDataPin, LOW);

  channelNum = channel % 8;
  bandNum = channel / 8;
}


void SERIAL_SENDBIT1()
{
  digitalWrite(spiClockPin, LOW);
  delayMicroseconds(1);

  digitalWrite(spiDataPin, HIGH);
  delayMicroseconds(1);
  digitalWrite(spiClockPin, HIGH);
  delayMicroseconds(1);

  digitalWrite(spiClockPin, LOW);
  delayMicroseconds(1);
}

void SERIAL_SENDBIT0()
{
  digitalWrite(spiClockPin, LOW);
  delayMicroseconds(1);

  digitalWrite(spiDataPin, LOW);
  delayMicroseconds(1);
  digitalWrite(spiClockPin, HIGH);
  delayMicroseconds(1);

  digitalWrite(spiClockPin, LOW);
  delayMicroseconds(1);
}

void SERIAL_ENABLE_LOW()
{
  delayMicroseconds(1);
  digitalWrite(slaveSelectPin, LOW);
  delayMicroseconds(1);
}

void SERIAL_ENABLE_HIGH()
{
  delayMicroseconds(1);
  digitalWrite(slaveSelectPin, HIGH);
  delayMicroseconds(1);
}
