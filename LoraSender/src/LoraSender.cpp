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
  20250724  V0.4: Wait little bit longer between receive and send
  20250727  V0.5: New payload
  20250804  V0.6: Send lora_payload_t struct, no terminator
  20251004  V0.7: Send payload with 2  delimiters, changed method to generate send string, debugHex Print for message sent. Receive length + 2 for delimiter
  20260219  V0.8: Add event cycling with configurable timer and names
  20260219  V0.9: Fix swapped event codes and debug output names
  20260219  V0.10: Correct event codes to match communicationold.h
  20260221  V0.11: Add send_config function
  20260222  V0.12: Remove menu function, add config variables at top




*/
 
#include <Arduino.h>
// 1 means debug on 0 means off
#define DEBUG 1
#include "LoRa_E32.h"
#include "communication.h"
// Data structure for message
#include <HomeAutomationCommon.h>
const String sSoftware = "LoraBridge V0.12";

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
// Hall sensor
const byte HSENSD = GPIO_NUM_8;
// LORA module
const byte M0 = GPIO_NUM_10;  // LoRa M0
const byte M1 = GPIO_NUM_11;  // LoRa M1
const byte TxD = GPIO_NUM_12; // TX to LoRa Rx
const byte RxD = GPIO_NUM_13; // RX to LoRa Tx
const byte AUX = GPIO_NUM_14; // Auxiliary
const int ChannelNumber = 6;

/***************************
 * CONFIGURATION VARIABLES
 * Edit these values and recompile to change configuration
 **************************/
// Configuration values to send to receiver
uint8_t CONFIG_ULP_PULSES = 10;           // ULP pulses (1-255)
uint16_t CONFIG_WAKEUP_SEC = 60;          // Wakeup interval in seconds (1-3600)
uint16_t CONFIG_SHUTDOWN_MS = 1000;       // Shutdown delay in ms (100-10000)
uint16_t CONFIG_LORA_DELAY_MS = 500;      // LoRa receive delay in ms (100-5000)

// Set to true to send SET_CONFIG message, false to send normal data
bool SEND_CONFIG_MESSAGE = false;

// Set to true to send RESET_CONFIG message
bool SEND_RESET_CONFIG = false;

// global data

float fTemp, fRelHum, fRainMM;
volatile int interruptCounter = 0; // indicator an interrupt has occured
int numberOfInterrupts = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

// --- Event ID Configuration ---
// List of event IDs to cycle through during testing
// Edit this list to change which events are sent
const uint8_t eventIdList[] = {
  0x0001,  // LORA_EVENT_RESUME_SLEEP_MODE (or use actual enum value)
  0x0002   // LORA_EVENT_DISABLE_SLEEP_MODE (or use actual enum value)
};

// Event names for logging
const char* eventNameList[] = {
  "RESUME_SLEEP_MODE",
  "DISABLE_SLEEP_MODE"
};

const size_t eventIdListSize = sizeof(eventIdList) / sizeof(eventIdList[0]);
size_t currentEventIndex = 1;

// Timer configuration for event switching (in seconds)
// Edit this value to change how often events are switched
const unsigned long EVENT_SWITCH_INTERVAL_SECONDS = 30; // Change event every 5 seconds
unsigned long lastEventSwitchTime = 0;

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
static void format_time(uint32_t ms, int *hours, int *minutes, int *seconds);
void printPayloadHex(const uint8_t *data, size_t len);

// Configuration message functions
void sendConfigMessage();
void sendResetConfigMessage();
void printConfigPayloadHex(const uint8_t *data, size_t len);

// --- Magic Bytes Config ---
#define USE_MAGIC_BYTES 0                                 // Set to 0 to disable magic bytes
static const uint8_t MAGIC_BYTES[3] = {0xAA, 0xBB, 0xCC}; // Example magic bytes
const size_t MAGIC_BYTES_LEN = sizeof(MAGIC_BYTES);

// Message delimiter constants
#define E32_MSG_DELIMITER_1 0x0C    // First byte of message delimiter
#define E32_MSG_DELIMITER_2 0x0C    // Second byte of message delimiter

uint16_t messageIdCounter = 1;

String addMagicBytes(const String &payload)
{
#if USE_MAGIC_BYTES
  String result;
  for (size_t i = 0; i < MAGIC_BYTES_LEN; ++i)
  {
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
  // Serial.println("Boot Nr.: " + String(bootCount));
  // esp_sleep_enable_timer_wakeup(90e+6);
  //  Startup all pins and UART
  // Initialize LoRa E32 before configuration
  e32ttl.begin();

  // Explizite Konfiguration setzen
  Configuration config;
  config.ADDH = 0x00;
  config.ADDL = 0x00;
  config.CHAN = 0x06;                             // Kanal 7
  config.SPED.airDataRate = AIR_DATA_RATE_010_24; // 2.4kbps
  config.SPED.uartBaudRate = UART_BPS_9600;
  config.SPED.uartParity = MODE_00_8N1;
  // Ensure transparent transmission mode
  config.OPTION.fixedTransmission = FT_TRANSPARENT_TRANSMISSION;
  config.OPTION.fec = FEC_1_ON; // Turn off Forward Error Correction Switch

  // Save configuration and check status
  ResponseStatus setCfgStatus = e32ttl.setConfiguration(config, WRITE_CFG_PWR_DWN_SAVE);
  if (setCfgStatus.code != 1)
  {
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
  Configuration configuration = *(Configuration *)c.data;
  Serial.println(c.status.getResponseDescription());
  Serial.println(c.status.code);
  // Print and check transmission mode
  Serial.print("Fixed Transmission mode (should be 0 for transparent): ");
  Serial.println(configuration.OPTION.fixedTransmission);
  printParameters(configuration);
  // Free the container to prevent memory leaks
  c.close();
}

void loop()
{
  // Check if we should send a config message
  if (SEND_CONFIG_MESSAGE) {
    sendConfigMessage();
    SEND_CONFIG_MESSAGE = false;  // Reset flag after sending
    delay(1000);
    return;
  }

  // Check if we should send a reset config message
  if (SEND_RESET_CONFIG) {
    sendResetConfigMessage();
    SEND_RESET_CONFIG = false;  // Reset flag after sending
    delay(1000);
    return;
  }

  // Normal operation: Wait for a message from LoRa
  bool messageReceived = false;
  while (!messageReceived)
  {
    // Check if it's time to switch to the next event
    unsigned long currentTime = millis() / 1000; // Convert to seconds
    if (currentTime - lastEventSwitchTime >= EVENT_SWITCH_INTERVAL_SECONDS)
    {
      lastEventSwitchTime = currentTime;
      //currentEventIndex = (currentEventIndex + 1) % eventIdListSize;
      Serial.print("Switching to event index: ");
      Serial.print(currentEventIndex);
      Serial.print(", Event ID: 0x");
      Serial.print(eventIdList[currentEventIndex], HEX);
      Serial.print(", Event Name: ");
      Serial.println(eventNameList[currentEventIndex]);
    }

    // Check for incoming LoRa message
    if (e32ttl.available() > 1)
    {
      receiveValuesLoRa();
      messageReceived = true;
    }

    delay(100); // Small delay to avoid busy loop
  }
  
  delay(10); // Wait a bit before sending the next message
  ++bootCount;
  Serial.println("Hi, I'm going to send message!");
  lora_payload_t payload;
  payload.messageID = bootCount;
  // Use the current event from the list
  payload.lora_eventID = eventIdList[currentEventIndex];
  payload.elapsed_time_ms = millis();
  payload.pulse_count = interruptCounter;
  payload.checksum = lora_payload_checksum(&payload);     // Calculate checksum

    // 2) Binärdaten in String sicher verpacken (Byte-für-Byte)
  String msg;
  msg.reserve(sizeof(payload) + 2); // +2 für die Delimiter
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&payload);
  for (size_t i = 0; i < sizeof(payload); ++i) {
    msg += (char)p[i];  // explizit jedes Byte anhängen (inkl. 0x00)
  }

  // 3) Delimiter *danach* anhängen (Empfänger wartet darauf)
  msg += (char)E32_MSG_DELIMITER_1;
  msg += (char)E32_MSG_DELIMITER_2;

  
  ResponseStatus rs = e32ttl.sendMessage(msg);

  Serial.print("message sent: ");

printPayloadHex((const uint8_t*)msg.c_str(), msg.length());
  Serial.println("Message sent. Waiting for next receive...");
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
void IRAM_ATTR handleInterrupt()
{
  portENTER_CRITICAL_ISR(&mux);
  interruptCounter++;
  portEXIT_CRITICAL_ISR(&mux);
}

void receiveValuesLoRa()
{
  uint32_t ms = 0;
  int hours = 0, minutes = 0, seconds = 0;
  char elapsed_time_str[9];
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
    if (rc.data.length() != sizeof(lora_payload_t) + 2) // Account for 2 byte delimiter
    {
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
    // calculate elapsed time to string
    Serial.print("Calculated time string: ");
    format_time(payload.elapsed_time_ms, &hours, &minutes, &seconds);
    snprintf(elapsed_time_str, sizeof(elapsed_time_str), "%02d:%02d:%02d", hours, minutes, seconds);
    Serial.println(elapsed_time_str);

    // Print all fields
    Serial.print("Message ID: ");
    Serial.println(payload.messageID);
    Serial.print("Event ID: ");
    Serial.println(payload.lora_eventID);
    Serial.print("Elapsed time (ms): ");
    Serial.println(payload.elapsed_time_ms);
    Serial.print("Pulse count: ");
    Serial.println(payload.pulse_count);
    Serial.print("Checksum: 0x");
    Serial.println((uint16_t)payload.checksum, HEX);
    Serial.print("Checksum valid: ");
    Serial.println(checksum_ok ? "YES" : "NO");
    if (!checksum_ok)
    {
      Serial.print("Expected checksum: 0x");
      Serial.println(calc_checksum, HEX);
    }
    neopixelWrite(RGB_BUILTIN, 0, 0, 0);
    delay(500);
    neopixelWrite(RGB_BUILTIN, 0, 0, 0); // Off
  }
}

// Function to convert milliseconds into hours, minutes, and seconds
static void format_time(uint32_t ms, int *hours, int *minutes, int *seconds)
{
  *hours = ms / 3600000;             // Calculate hours (ms / 3600000)
  *minutes = (ms % 3600000) / 60000; // Calculate minutes ((ms % 3600000) / 60000)
  *seconds = (ms % 60000) / 1000;    // Calculate seconds ((ms % 60000) / 1000)
}

void printPayloadHex(const uint8_t *data, size_t len)
{
  Serial.print("Payload [");
  Serial.print(len);
  Serial.println(" bytes]:");

  for (size_t i = 0; i < len; i++)
  {
    if (data[i] < 0x10)
      Serial.print('0'); // führende Null
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}

/* ============================================================================
 * CONFIGURATION MESSAGE FUNCTIONS
 * ============================================================================ */

/**
 * @brief Send configuration message with current values
 */
void sendConfigMessage()
{
  Serial.println("\nSending SET_CONFIG message...");

  lora_config_payload_t config;
  config.messageID = messageIdCounter++;
  config.lora_eventID = LORA_EVENT_SET_CONFIG;
  config.ulp_pulses_to_wake_up = CONFIG_ULP_PULSES;
  config.reserved1 = 0;
  config.wakeup_interval_sec = CONFIG_WAKEUP_SEC;
  config.shutdown_delay_ms = CONFIG_SHUTDOWN_MS;
  config.lora_receive_delay_ms = CONFIG_LORA_DELAY_MS;
  config.reserved2 = 0;
  config.checksum = lora_config_payload_checksum(&config);

  // Log configuration
  Serial.println("Configuration payload:");
  Serial.print("  messageID: ");
  Serial.println(config.messageID);
  Serial.print("  eventID: 0x");
  Serial.println(config.lora_eventID, HEX);
  Serial.print("  ulp_pulses: ");
  Serial.println(config.ulp_pulses_to_wake_up);
  Serial.print("  wakeup_sec: ");
  Serial.println(config.wakeup_interval_sec);
  Serial.print("  shutdown_ms: ");
  Serial.println(config.shutdown_delay_ms);
  Serial.print("  lora_delay_ms: ");
  Serial.println(config.lora_receive_delay_ms);
  Serial.print("  checksum: 0x");
  Serial.println(config.checksum, HEX);

  // Pack into message with delimiters
  String msg;
  msg.reserve(sizeof(config) + 2);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&config);
  for (size_t i = 0; i < sizeof(config); ++i) {
    msg += (char)p[i];
  }
  msg += (char)E32_MSG_DELIMITER_1;
  msg += (char)E32_MSG_DELIMITER_2;

  // Send message
  ResponseStatus rs = e32ttl.sendMessage(msg);
  if (rs.code == 1) {
    Serial.println("SET_CONFIG message sent successfully.");
    Serial.print("Payload hex: ");
    printConfigPayloadHex((const uint8_t*)msg.c_str(), msg.length());
  } else {
    Serial.print("ERROR sending SET_CONFIG: ");
    Serial.println(rs.getResponseDescription());
  }
}

/**
 * @brief Send reset configuration message
 */
void sendResetConfigMessage()
{
  Serial.println("\nSending RESET_CONFIG message...");

  lora_config_payload_t config;
  config.messageID = messageIdCounter++;
  config.lora_eventID = LORA_EVENT_RESET_CONFIG;
  config.ulp_pulses_to_wake_up = 0;  // Not used for reset
  config.reserved1 = 0;
  config.wakeup_interval_sec = 0;    // Not used for reset
  config.shutdown_delay_ms = 0;      // Not used for reset
  config.lora_receive_delay_ms = 0;  // Not used for reset
  config.reserved2 = 0;
  config.checksum = lora_config_payload_checksum(&config);

  // Log configuration
  Serial.println("Reset configuration payload:");
  Serial.print("  messageID: ");
  Serial.println(config.messageID);
  Serial.print("  eventID: 0x");
  Serial.println(config.lora_eventID, HEX);
  Serial.print("  checksum: 0x");
  Serial.println(config.checksum, HEX);

  // Pack into message with delimiters
  String msg;
  msg.reserve(sizeof(config) + 2);
  const uint8_t* p = reinterpret_cast<const uint8_t*>(&config);
  for (size_t i = 0; i < sizeof(config); ++i) {
    msg += (char)p[i];
  }
  msg += (char)E32_MSG_DELIMITER_1;
  msg += (char)E32_MSG_DELIMITER_2;

  // Send message
  ResponseStatus rs = e32ttl.sendMessage(msg);
  if (rs.code == 1) {
    Serial.println("RESET_CONFIG message sent successfully.");
    Serial.print("Payload hex: ");
    printConfigPayloadHex((const uint8_t*)msg.c_str(), msg.length());
  } else {
    Serial.print("ERROR sending RESET_CONFIG: ");
    Serial.println(rs.getResponseDescription());
  }
}

/**
 * @brief Print configuration payload in hex format
 */
void printConfigPayloadHex(const uint8_t *data, size_t len)
{
  Serial.print("Config Payload [");
  Serial.print(len);
  Serial.println(" bytes]:");

  for (size_t i = 0; i < len; i++)
  {
    if (data[i] < 0x10)
      Serial.print('0');
    Serial.print(data[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
}