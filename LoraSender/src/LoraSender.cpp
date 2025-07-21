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
  20250517  V0.2: Add receive function
  20250721  V0.3: Recieve packed struct from LoRa and print it




*/

#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include "LoRa_E32.h"
#include "../../Rainsensor/include/communication.h"
// Data structure for message
#include <HomeAutomationCommon.h>
const String sSoftware = "LoraBridge V0.3";

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
void receiveValuesLoRa();
void IRAM_ATTR handleInterrupt();

// --- Magic Bytes Config ---
#define USE_MAGIC_BYTES 0 // Set to 0 to disable magic bytes
static const uint8_t MAGIC_BYTES[3] = {0xAA, 0xBB, 0xCC}; // Example magic bytes
const size_t MAGIC_BYTES_LEN = sizeof(MAGIC_BYTES);

String addMagicBytes(const String& payload) {
#if USE_MAGIC_BYTES
  String result;
  for (size_t i = 0; i < MAGIC_BYTES_LEN; ++i) {
    result += (char)MAGIC_BYTES[i];
  }
  result += payload;
  return result;
#else
  return payload;
#endif
}

void setup()
{
  Serial.begin(115200);
#ifdef DEBUG
  delay(4000);
  Serial.println("START");
  Serial.print("Software Version: ");
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
// Initialize LoRa E32 before configuration
e32ttl.begin();

// Explizite Konfiguration setzen
Configuration config;
config.ADDH = 0x00;
config.ADDL = 0x00;
config.CHAN = 0x06; // Kanal 7
config.SPED.airDataRate = AIR_DATA_RATE_010_24; // 2.4kbps
config.SPED.uartBaudRate = UART_BPS_9600;
config.SPED.uartParity = MODE_00_8N1;
// Ensure transparent transmission mode
config.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
config.OPTION.fec = FEC_1_ON; // Turn off Forward Error Correction Switch

// Save configuration and check status
ResponseStatus setCfgStatus = e32ttl.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
if (setCfgStatus.code != 1) {
  Serial.print("Error setting configuration: ");
  Serial.println(setCfgStatus.getResponseDescription());
}

delay(500);
Serial.println("in setup routine");
e32ttl.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);

   neopixelWrite(RGB_BUILTIN, 0, 0, 0); // BLUE
  fRainMM = 0;
  delay(500);
  ResponseStructContainer c;
  c = e32ttl.getConfiguration();
  // It's important get configuration pointer before all other operation
  Configuration configuration = *(Configuration*) c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);
  // Print and check transmission mode
  Serial.print("Fixed Transmission mode (should be 0 for transparent): ");
  Serial.println(configuration.OPTION.fixedTransmission);
  printParameters(configuration);
  // Free the container to prevent memory leaks
  c.close();

  Serial.println("Hi, I'm going to send message!");

  // Send message without non-ASCII header
  String msg = "Hello, world? "  + String(bootCount) + "!";
  ResponseStatus rs = e32ttl.sendMessage(msg);
}

void loop()
{
  
 

  ++bootCount;
  delay(3000);
  Serial.println("Hi, I'm going to send message!");



  // Send message without non-ASCII header
  String msg = "Hello, world? "  + String(bootCount) + "!";

  ResponseStatus rs = e32ttl.sendMessage(msg);
  Serial.println("Wait for receiving a message");
  delay(8000);
  receiveValuesLoRa(); 
  

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


void receiveValuesLoRa()
{
  if (e32ttl.available() > 1)
  {
    ResponseContainer rc = e32ttl.receiveMessage();
    if (rc.status.code != 1)
    {
      Serial.print("LoRa receive error: ");
      Serial.println(rc.status.getResponseDescription());
      return;
    }
    // Expect binary payload matching lora_payload_t
    if (rc.data.length() != sizeof(lora_payload_t)) {
      Serial.print("Error: Received payload size ");
      Serial.print(rc.data.length());
      Serial.print(" does not match expected size ");
      Serial.println(sizeof(lora_payload_t));
      return;
    }
    // Copy buffer into struct
    lora_payload_t payload;
    memcpy(&payload, rc.data.c_str(), sizeof(lora_payload_t));
    // Validate checksum
    uint16_t calc_checksum = lora_payload_checksum(&payload);
    bool checksum_ok = (calc_checksum == payload.checksum);
    // Print all fields
    Serial.print("Elapsed time (string): ");
    Serial.println(payload.elapsed_time_str);
    Serial.print("Elapsed time (ms): ");
    Serial.println(payload.elapsed_time_ms);
    Serial.print("Pulse count: ");
    Serial.println(payload.pulse_count);
    Serial.print("Send counter: ");
    Serial.println(payload.send_counter);
    Serial.print("Checksum: 0x");
    Serial.println((uint16_t)payload.checksum, HEX);
    Serial.print("Checksum valid: ");
    Serial.println(checksum_ok ? "YES" : "NO");
    if (!checksum_ok) {
      Serial.print("Expected checksum: 0x");
      Serial.println(calc_checksum, HEX);
    }
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    delay(500);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off
  }
}
