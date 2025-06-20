/**************************************************************************
LoRa WLAN Bridge


  Hardware:
  ESP32-S3-DevKitC-1 mit Wroom N16R8
  LoRa E32-900T30D connected M0 M1 and Rx Tx

  Function:
  Receive data from RainSensor
  Debug out data

  History: master if not shown otherwise
  20250405  V0.1: Copy from LoRABridge
  20250407  V0.2: Successfully tested with LoraESPIDF Sender Version 0.5




*/

#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1

#include "LoRa_E32.h"
#include <PubSubClient.h> //MQTT
#include <ArduinoJson.h>

// Data structure for message
#include <HomeAutomationCommon.h>

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

const byte M0 = GPIO_NUM_10;  // LoRa M0
const byte M1 = GPIO_NUM_11;  // LoRa M1
const byte TxD = GPIO_NUM_12; // TX to LoRa Rx
const byte RxD = GPIO_NUM_13; // RX to LoRa Tx
const byte AUX = GPIO_NUM_14; // Auxiliary
const int ChannelNumber = 6;

// set LoRa to working mode 0  Transmitting
// LoRa_E32 e32ttl(RxD, TxD,AUX, M0, M1, UART_BPS_9600);
// use hardware serial #1
LoRa_E32 e32ttl(&Serial1, AUX, M0, M1); // RX, TX

const String sSoftware = "LoraSendReceiver V0.2";

// put function declarations here:

void printParameters(struct Configuration configuration);
void receiveValuesLoRa();
void printReceivedData();

void setup()
{
  Serial.begin(56000);
  delay(2000);
  Serial.println();
  Serial.println(sSoftware);
  Serial1.begin(9600, SERIAL_8N1, RxD, TxD);
  // Explizite Konfiguration setzen
  Configuration config;
  config.ADDH = 0x00;
  config.ADDL = 0x00;
  config.CHAN = 0x06;                             // Kanal 7
  config.SPED.airDataRate = AIR_DATA_RATE_010_24; // 2.4kbps
  config.SPED.uartBaudRate = UART_BPS_9600;
  config.SPED.uartParity = MODE_00_8N1;
  config.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  config.OPTION.fec = FEC_1_ON; // Turn off Forward Error Correction Switch

  delay(500);
  Serial.println("in setup routine");

  //  Startup all pins and UART
  e32ttl.begin();
  e32ttl.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);

  delay(500);
  ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  // It's important get configuration pointer before all other operation
  Configuration configuration = *(Configuration *)c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);

  printParameters(configuration);
  c.close();
}

void loop()
{
    Serial.println("Loop Start");
  while (1)
  {
    /* code */
  
  delay(1000);
 
//   Serial.println("Wait for receiving a message");
  

  receiveValuesLoRa();
  }
}

void receiveValuesLoRa()
{
  // LORA_DATA_STRUCTURE sLoRaReceiveData;
  //  If something available
  if (e32ttl.available() > 1)
  {
    // read the String message
    ResponseContainer rc = e32ttl.receiveMessage();
   
    // Is something goes wrong print error
    if (rc.status.code != 1)
    {
      rc.status.getResponseDescription();
      Serial.println("Error receiving data");
    }
    else
    {
      // Print the data received
      Serial.println(rc.data);
      neopixelWrite(RGB_BUILTIN, 50, 0, 0);
      delay(500);
      neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off

      
    }

   
      
  }
}
void printReceivedData()
{
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