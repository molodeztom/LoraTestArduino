# LoraSender Configuration Message Usage Guide

## Overview

LoraSender now supports sending configuration messages to the Rainsensor device via LoRa. This guide explains how to use the configuration menu to set parameters and send messages.

## Quick Start

### Entering Configuration Menu

1. Open the Serial Monitor (115200 baud)
2. Press **'c'** or **'C'** to enter the configuration menu
3. The menu will display with options 1-8

### Configuration Menu Options

```
=== LoraSender Configuration Menu ===
1. Set ULP Pulses (1-255)
2. Set Wakeup Interval (1-3600 sec)
3. Set Shutdown Delay (100-10000 ms)
4. Set LoRa Delay (100-5000 ms)
5. Show Current Configuration
6. Send SET_CONFIG
7. Send RESET_CONFIG
8. Exit Menu
=====================================
```

## Configuration Parameters

### 1. ULP Pulses (Option 1)
- **Range**: 1-255
- **Default**: 4
- **Description**: Number of pulses required to wake up the CPU from ULP (Ultra Low Power) mode
- **Impact**: Lower values = more sensitive, higher values = less sensitive

### 2. Wakeup Interval (Option 2)
- **Range**: 1-3600 seconds
- **Default**: 60 seconds
- **Description**: How often the device wakes up to check for LoRa messages
- **Impact**: Lower values = more frequent checks (higher power consumption), higher values = less frequent (lower power consumption)

### 3. Shutdown Delay (Option 3)
- **Range**: 100-10000 milliseconds
- **Default**: 1000 ms
- **Description**: Time to wait before entering deep sleep after processing
- **Impact**: Allows time for LoRa module to settle before sleep

### 4. LoRa Delay (Option 4)
- **Range**: 100-5000 milliseconds
- **Default**: 500 ms
- **Description**: Time to wait for LoRa messages after waking up
- **Impact**: Longer delays = more time to receive messages, shorter delays = faster sleep

## Usage Examples

### Example 1: Set Custom Configuration

```
Press 'c' to enter menu

=== LoraSender Configuration Menu ===
...
Enter choice (1-8): 1
Enter ULP Pulses (1-255): 5
ULP Pulses updated.

Enter choice (1-8): 2
Enter Wakeup Interval (1-3600 sec): 120
Wakeup Interval updated.

Enter choice (1-8): 3
Enter Shutdown Delay (100-10000 ms): 2000
Shutdown Delay updated.

Enter choice (1-8): 4
Enter LoRa Delay (100-5000 ms): 1000
LoRa Delay updated.

Enter choice (1-8): 5
--- Current Configuration ---
ULP Pulses: 5
Wakeup Interval: 120 sec
Shutdown Delay: 2000 ms
LoRa Delay: 1000 ms
-----------------------------

Enter choice (1-8): 6
Sending SET_CONFIG message...
Configuration payload:
  messageID: 1
  eventID: 0x0005
  ulp_pulses: 5
  wakeup_sec: 120
  shutdown_ms: 2000
  lora_delay_ms: 1000
  checksum: 0x1234
SET_CONFIG message sent successfully.
Payload hex: [16 bytes]: 01 00 05 00 05 00 78 00 D0 07 E8 03 00 00 34 12

Enter choice (1-8): 8
Exiting configuration menu.
```

### Example 2: Reset to Defaults

```
Press 'c' to enter menu

Enter choice (1-8): 7
Sending RESET_CONFIG message...
Reset configuration payload:
  messageID: 2
  eventID: 0x0006
  checksum: 0x5678
RESET_CONFIG message sent successfully.
Payload hex: [16 bytes]: 02 00 06 00 00 00 00 00 00 00 00 00 00 00 78 56

Enter choice (1-8): 8
Exiting configuration menu.
```

## Message Format

### SET_CONFIG Message (Event ID: 0x0005)

```
Byte Offset | Field Name              | Size | Value
------------|-------------------------|------|--------
0-1         | messageID               | 2    | Auto-incremented
2-3         | lora_eventID            | 2    | 0x0005
4           | ulp_pulses_to_wake_up   | 1    | 1-255
5           | reserved1               | 1    | 0x00
6-7         | wakeup_interval_sec     | 2    | 1-3600
8-9         | shutdown_delay_ms       | 2    | 100-10000
10-11       | lora_receive_delay_ms   | 2    | 100-5000
12-13       | reserved2               | 2    | 0x0000
14-15       | checksum                | 2    | Calculated
```

**Total Size**: 16 bytes

### RESET_CONFIG Message (Event ID: 0x0006)

```
Byte Offset | Field Name              | Size | Value
------------|-------------------------|------|--------
0-1         | messageID               | 2    | Auto-incremented
2-3         | lora_eventID            | 2    | 0x0006
4-13        | (not used)              | 10   | 0x00
14-15       | checksum                | 2    | Calculated
```

**Total Size**: 16 bytes

## Checksum Calculation

The checksum is calculated as the sum of all bytes except the checksum field itself:

```c
checksum = sum(bytes[0:14])
```

This is automatically calculated by the firmware.

## Message Transmission

- **Delimiter**: Each message is followed by two delimiter bytes (0x0C 0x0C)
- **Total Transmission Size**: 16 bytes payload + 2 bytes delimiter = 18 bytes
- **LoRa Limit**: Well within 58-byte LoRa message limit
- **Baud Rate**: 9600 bps (LoRa module)

## Response Messages

After sending a configuration message, the Rainsensor will respond with:

### SET_CONFIG Response (Event ID: 0x1005)
- Contains the actual applied configuration values
- Allows verification that settings were accepted
- Sent immediately after processing

### RESET_CONFIG Response (Event ID: 0x1006)
- Confirms reset to default values
- Contains default configuration values
- Sent immediately after processing

## Validation Rules

The firmware validates all parameters before sending:

| Parameter | Min | Max | Default |
|-----------|-----|-----|---------|
| ULP Pulses | 1 | 255 | 4 |
| Wakeup Interval | 1 | 3600 | 60 |
| Shutdown Delay | 100 | 10000 | 1000 |
| LoRa Delay | 100 | 5000 | 500 |

**Invalid values** will be rejected with an error message.

## Troubleshooting

### "ERROR: Value must be between X and Y"
- You entered a value outside the valid range
- Check the parameter limits above
- Re-enter the correct value

### "ERROR sending SET_CONFIG: ..."
- LoRa module communication failed
- Check LoRa module connections
- Verify LoRa module is powered and initialized
- Check serial output for detailed error

### Message not received by Rainsensor
- Verify both devices are on the same LoRa channel (Channel 6)
- Check LoRa module antenna connections
- Verify Rainsensor is powered and listening
- Check message ID is incrementing (indicates successful send)

### Checksum mismatch on receiver
- This should not happen with the firmware
- If it does, check for memory corruption
- Try resetting both devices

## Advanced Usage

### Monitoring Message IDs

Each message sent increments the messageID counter:
- First SET_CONFIG: messageID = 1
- Second SET_CONFIG: messageID = 2
- etc.

This helps verify that messages are being sent correctly.

### Hex Payload Analysis

The firmware prints the hex payload after each send:
```
Payload hex: [18 bytes]: 01 00 05 00 05 00 78 00 D0 07 E8 03 00 00 34 12 0C 0C
```

Breaking this down:
- `01 00` = messageID (0x0001)
- `05 00` = eventID (0x0005 = SET_CONFIG)
- `05` = ulp_pulses (5)
- `00` = reserved1
- `78 00` = wakeup_sec (120)
- `D0 07` = shutdown_ms (2000)
- `E8 03` = lora_delay_ms (1000)
- `00 00` = reserved2
- `34 12` = checksum (0x1234)
- `0C 0C` = delimiters

## Version Information

- **LoraSender Version**: V0.11+
- **Communication Protocol**: Version 1.0
- **Last Updated**: 2026-02-22

## Related Files

- [`../../HomeAutomation/communication.h`](../../HomeAutomation/communication.h) - Shared message definitions
- [`src/LoraSender.cpp`](src/LoraSender.cpp) - Implementation
- [`plans/LoraSender_impl_plan.md`](plans/LoraSender_impl_plan.md) - Implementation details
