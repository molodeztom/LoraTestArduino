/**************************************************************************
RainSensor


  Hardware:
  ESP32-S3-DevKitC-1 mit Wroom N16R8
  LoRa E32-900T30D connected M0 M1 and Rx Tx
  Hall Sensor with Adapter

  Try sending a message to remote LoRa
  Sleep for a certain time
  Put LoRa to sleep mode
  Setup LoRa to defined parameters
  Measure Temperature
  Send Temperature and Time Stamp

  History: master if not shown otherwise
  20250405  V0.1: Copy from RainSensor Project




*/

#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include "LoRa_E32.h"
// Data structure for message
#include <HomeAutomationCommon.h>
const String sSoftware = "RainSensor V0.7";

// debug macro
#if DEBUG == 1
#define debug(x) Serial.print(x)
#define debugln(x) Serial.println(x)
#define debugv(s, v)    \
  {                     \
    Serial.print(F(s)); \
    Serial.println(v);  \
  }

#else
#define debug(x)
#define debugln(x)
#define debugv(format, ...)
#define debugarg(...)
#endif

#define BUTTON_PIN_BITMASK 0x8000

/***************************
 * Pin Settings
 **************************/
//Hall sensor
const byte HSENSD = GPIO_NUM_8;
// LORA module
const byte M0 = GPIO_NUM_10;  // LoRa M0
const byte M1 = GPIO_NUM_11;  // LoRa M1
const byte TxD = GPIO_NUM_12; // TX to LoRa Rx
const byte RxD = GPIO_NUM_13; // RX to LoRa Tx
const byte AUX = GPIO_NUM_14; // Auxiliary
const int ChannelNumber = 6;


// global data

float fTemp, fRelHum, fRainMM;
volatile int interruptCounter  = 0; //indicator an interrupt has occured
int numberOfInterrupts = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// set LoRa to working mode 0  Transmitting
// LoRa_E32 e32ttl(RxD, TxD,AUX, M0, M1, UART_BPS_9600);

RTC_DATA_ATTR int bootCount = 0;
LoRa_E32 e32ttl(&Serial1, AUX, M0, M1); // RX, TX

// forward declarations
void printParameters(struct Configuration configuration);
void measureTempHumi();
void sendValuesLoRa();
void sendSingleData(LORA_DATA_STRUCTURE data);
void IRAM_ATTR handleInterrupt();

void setup()
{
  Serial.begin(56000);
#ifdef DEBUG
  delay(4000);
  Serial.println("START");
  Serial.println(sSoftware);
#endif
  // Serial1 connects to LoRa module
  Serial1.begin(9600, SERIAL_8N1, RxD, TxD);
  delay(500);
    Serial.println("in setup routine");
    pinMode(HSENSD, INPUT_PULLUP);
 // attachInterrupt(digitalPinToInterrupt(HSENSD), handleInterrupt, CHANGE);
  //Serial.println("Boot Nr.: " + String(bootCount));
  // esp_sleep_enable_timer_wakeup(90e+6);
  //  Startup all pins and UART

// Explizite Konfiguration setzen
Configuration config;
config.ADDH = 0x00;
config.ADDL = 0x00;
config.CHAN = 0x06; // Kanal 7
config.SPED.airDataRate = AIR_DATA_RATE_010_24; // 2.4kbps
config.SPED.uartBaudRate = UART_BPS_9600;
config.SPED.uartParity = MODE_00_8N1;
config.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;

delay(500);
Serial.println("in setup routine");

//  Startup all pins and UART
e32ttl.begin();
e32ttl.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);

   neopixelWrite(RGB_BUILTIN, 0, 0, 60); // BLUE
  fRainMM = 0;
  delay(500);
  ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  // It's important get configuration pointer before all other operation
  Configuration configuration = *(Configuration*) c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

  printParameters(configuration) 
  ;

  Serial.println("Hi, I'm going to send message!");

  // Send message
   ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
}

void loop()
{
  Serial.println("Loop Start");
  neopixelWrite(RGB_BUILTIN, 60, 0, 0); // dark
  ++bootCount;
  delay(2000);
  Serial.println("Hi, I'm going to send message!");

  // Send message
   ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
  //measureTempHumi();
if(interruptCounter > 0){
  portENTER_CRITICAL(&mux);
  interruptCounter = 0;
    portEXIT_CRITICAL(&mux);
    numberOfInterrupts++;
    Serial.print("An interrupt has occured. Total: ");
    Serial.println(numberOfInterrupts);
    Serial.println(interruptCounter);

}

/*   
  int val;
  
  val = digitalRead(HSENSD);
if (val == 1){
      neopixelWrite(RGB_BUILTIN, 60, 0, 0); // Red
      fRainMM++;
      Serial.print("Rain pulses: ");
      Serial.println(fRainMM);

  }
  else neopixelWrite(RGB_BUILTIN, 0, 60, 0); // Green */

 
  //Serial.println("Hi, I'm going to send message!");
  //sendValuesLoRa();

  //delay(1000);

  /*  // If something available
   if (e32ttl.available() > 1)
   {
     // read the String message
     ResponseContainer rc = e32ttl.receiveMessage();
     Serial.println("Received something");
     // Is something goes wrong print error
     if (rc.status.code != 1)
     {
       Serial.println(rc.status.getResponseDescription());
     }
     else
     {
       // Print the data received
       Serial.println(rc.status.getResponseDescription());
       Serial.println(rc.data);
     }
   }
   if (Serial.available())
   {
     String input = Serial1.readString();
     e32ttl.sendMessage(input);
   }
   ResponseStatus rs = e32ttl.sendMessage("Hello, world? ");
   String result = rs.getResponseDescription();
   Serial.println(result); */
  //Serial.println("Loop End");

  // esp_deep_sleep_start();
}
/* optimized data structure to save bytes to transfer. We send only one measurement
and name the measurement in eDataSource. So far the measurements are not different
so the structure remains the same */
void sendValuesLoRa()
{
  LORA_DATA_STRUCTURE data;
  int nDataSize = 0;


  uint8_t bs[sizeof(data)];
  data.iSensorChannel = 8;
  // send temp
  data.eDataSource = tempSensor;
  data.iData = fTemp;
  sendSingleData(data);

  delay(1000);
  // send humi
  data.eDataSource = humiSensor;
  data.iData = fRelHum;
   sendSingleData(data);

  delay(1000);
  // send rain
  data.eDataSource = rainSensor;
  data.iData = fRainMM;
   sendSingleData(data);

  delay(1000);
}

void sendSingleData(LORA_DATA_STRUCTURE data)
{
  uint8_t bs[sizeof(data)];

  // copy data to send buffer
  memcpy(bs, &data, sizeof(data));
  // Send message
  Serial.println("DataSize: ");
  Serial.println(sizeof(data));
  // ResponseStatus rs = e32ttl.sendMessage("Hello, world? "  + String(bootCount));
  ResponseStatus rs = e32ttl.sendMessage(bs, sizeof(data));
  //    ResponseStatus rs = e32ttl.sendMessage(&data, sizeof(data));
  Serial.println(rs.getResponseDescription());
}

void measureTempHumi()
{

  fTemp = 21.3 + bootCount;
  fRelHum = 90.2 + bootCount;
 
}

void printParameters(struct Configuration configuration)
{
  Serial.println("----------------------------------------");

  Serial.print(F("HEAD BIN: "));
  Serial.print(configuration.HEAD, BIN);
  Serial.print(" ");
  Serial.print(configuration.HEAD, DEC);
  Serial.print(" ");
  Serial.println(configuration.HEAD, HEX);
  Serial.println(F(" "));
  Serial.print(F("AddH BIN: "));
  Serial.println(configuration.ADDH, BIN);
  Serial.print(F("AddL BIN: "));
  Serial.println(configuration.ADDL, BIN);
  Serial.print(F("Chan BIN: "));
  Serial.print(configuration.CHAN, DEC);
  Serial.print(" -> ");
  Serial.println(configuration.getChannelDescription());
  Serial.println(F(" "));
  Serial.print(F("SpeedParityBit BIN    : "));
  Serial.print(configuration.SPED.uartParity, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTParityDescription());
  Serial.print(F("SpeedUARTDataRate BIN : "));
  Serial.print(configuration.SPED.uartBaudRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getUARTBaudRate());
  Serial.print(F("SpeedAirDataRate BIN  : "));
  Serial.print(configuration.SPED.airDataRate, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.SPED.getAirDataRate());

  Serial.print(F("OptionTrans BIN       : "));
  Serial.print(configuration.OPTION.fixedTransmission, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getFixedTransmissionDescription());
  Serial.print(F("OptionPullup BIN      : "));
  Serial.print(configuration.OPTION.ioDriveMode, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getIODroveModeDescription());
  Serial.print(F("OptionWakeup BIN      : "));
  Serial.print(configuration.OPTION.wirelessWakeupTime, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getWirelessWakeUPTimeDescription());
  Serial.print(F("OptionFEC BIN         : "));
  Serial.print(configuration.OPTION.fec, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getFECDescription());
  Serial.print(F("OptionPower BIN       : "));
  Serial.print(configuration.OPTION.transmissionPower, BIN);
  Serial.print(" -> ");
  Serial.println(configuration.OPTION.getTransmissionPowerDescription());

  Serial.println("----------------------------------------");
}
void printModuleInformation(struct ModuleInformation moduleInformation)
{
  Serial.println("----------------------------------------");
  Serial.print(F("HEAD BIN: "));
  Serial.print(moduleInformation.HEAD, BIN);
  Serial.print(" ");
  Serial.print(moduleInformation.HEAD, DEC);
  Serial.print(" ");
  Serial.println(moduleInformation.HEAD, HEX);

  Serial.print(F("Freq.: "));
  Serial.println(moduleInformation.frequency, HEX);
  Serial.print(F("Version  : "));
  Serial.println(moduleInformation.version, HEX);
  Serial.print(F("Features : "));
  Serial.println(moduleInformation.features, HEX);
  Serial.println("----------------------------------------");
}
void IRAM_ATTR handleInterrupt() {
  portENTER_CRITICAL_ISR(&mux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&mux);
}
