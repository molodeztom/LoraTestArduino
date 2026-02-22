#pragma once
#include <stdint.h>
#include <stddef.h>

#define LORA_EVENT_RESUME_SLEEP_MODE    0x0001  // Received message: resume normal sleep mode (allow deep sleep)
#define LORA_EVENT_DISABLE_SLEEP_MODE   0x0002  // Received message: disable sleep mode (stay awake)
#define LORA_EVENT_SEND_LORA_PARAMS     0x0003  // Received message: send LORA parameters
#define LORA_EVENT_SEND_PROG_PARAMS     0x0004  // Received message: send program parameters
#define LORA_EVENT_SET_CONFIG           0x0005  // Received message: set configuration parameters
#define LORA_EVENT_SET_CONFIG_RESPONSE  0x1005  // Response: configuration applied (0x0005 + 0x1000)
#define LORA_EVENT_RESET_CONFIG         0x0006  // Received message: reset configuration to defaults

// Configuration parameter limits (shared between sender and receiver)
#define CONFIG_MIN_ULP_PULSES 1
#define CONFIG_MAX_ULP_PULSES 100
#define CONFIG_MIN_WAKEUP_SEC 10
#define CONFIG_MAX_WAKEUP_SEC 3600
#define CONFIG_MIN_SHUTDOWN_MS 1000
#define CONFIG_MAX_SHUTDOWN_MS 30000
#define CONFIG_MIN_LORA_DELAY_MS 1000
#define CONFIG_MAX_LORA_DELAY_MS 30000

// Default configuration values
#define CONFIG_DEFAULT_ULP_PULSES 4
#define CONFIG_DEFAULT_WAKEUP_SEC 60
#define CONFIG_DEFAULT_SHUTDOWN_MS 4000
#define CONFIG_DEFAULT_LORA_DELAY_MS 6000

// LoRa communication payload struct
// All fields packed, no padding
// elapsed_time_str: formatted as "hh:mm:ss" (8 chars + null terminator)
// checksum: sum of all bytes except checksum field

typedef struct __attribute__((packed)) {
    char elapsed_time_str[9];      // "hh:mm:ss" + '\0'
    uint32_t elapsed_time_ms;      // Elapsed time in ms
    uint32_t pulse_count;          // Number of pulses
    uint32_t send_counter;         // Message ID
    uint16_t checksum;             // Checksum (sum of all bytes except checksum field)
} lora_payload_t;

// Calculate checksum (simple sum of bytes, excluding checksum field)
static inline uint16_t lora_payload_checksum(const lora_payload_t *payload) {
    const uint8_t *data = (const uint8_t *)payload;
    size_t len = sizeof(lora_payload_t) - sizeof(uint16_t); // exclude checksum
    uint16_t sum = 0;
    for (size_t i = 0; i < len; ++i) {
        sum += data[i];
    }
    return sum;
}

// Configuration message structure
// Used for LORA_EVENT_SET_CONFIG and LORA_EVENT_SET_CONFIG_RESPONSE
// Total size: 16 bytes (well within 58 byte LoRa limit)
typedef struct __attribute__((packed)) {
    uint16_t messageID;                 // Message ID
    uint16_t lora_eventID;             // LORA_EVENT_SET_CONFIG or LORA_EVENT_SET_CONFIG_RESPONSE
    uint8_t ulp_pulses_to_wake_up;     // Number of pulses to wake CPU (1-100)
    uint8_t reserved1;                  // Reserved for future use
    uint16_t wakeup_interval_sec;      // Wakeup interval in seconds (10-3600)
    uint16_t shutdown_delay_ms;        // Shutdown delay in milliseconds (1000-30000)
    uint16_t lora_receive_delay_ms;    // LoRa receive delay in milliseconds (1000-30000)
    uint16_t reserved2;                 // Reserved for future use
    uint16_t checksum;                  // Checksum (sum of all bytes except checksum field)
} lora_config_payload_t;

// Generic checksum calculation macro
// Reuses the same checksum algorithm as lora_payload_checksum().
// Works for any structure with a uint16_t checksum field at the end.
#define LORA_CALCULATE_CHECKSUM(struct_ptr, struct_type) ({ \
    const uint8_t *data = (const uint8_t *)(struct_ptr); \
    size_t len = sizeof(struct_type) - sizeof(uint16_t); \
    uint16_t sum = 0; \
    for (size_t i = 0; i < len; ++i) { \
        sum += data[i]; \
    } \
    sum; \
})

// Usage:
// lora_payload_t payload;
// ... fill fields ...
// payload.checksum = lora_payload_checksum(&payload);
// send as raw bytes: e32_send_data((uint8_t *)&payload, sizeof(payload));
// On receiver: validate checksum before using data.
//
// lora_config_payload_t config;
// config.checksum = LORA_CALCULATE_CHECKSUM(&config, lora_config_payload_t);
// send as raw bytes: e32_send_data((uint8_t *)&config, sizeof(config));
