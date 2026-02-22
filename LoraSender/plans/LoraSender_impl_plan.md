# LoraSender Configuration Message Implementation Plan

## Overview

Implement configuration message sending capability in the LoraSender project to communicate with the Rainsensor device. The LoraSender will provide a serial menu interface allowing users to:
- Set individual configuration parameters (pulses, wakeup interval, shutdown delay, LoRa delay)
- Send SET_CONFIG messages to apply new configuration
- Send RESET_CONFIG messages to restore defaults
- View current configuration values

## Project Context

- **LoraSender**: Arduino-based ESP32-S3 LoRa transmitter (this project)
- **Rainsensor**: ESP32 LoRa receiver with configuration management
- **Communication**: Binary LoRa messages with checksums
- **Message Types**: SET_CONFIG (0x0005) and RESET_CONFIG (0x0006)

## Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│                    LoraSender (ESP32-S3)                    │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         Serial Menu Interface (User Input)           │  │
│  │  - Display menu options                              │  │
│  │  - Parse user commands                               │  │
│  │  - Manage configuration state                        │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │    Configuration Message Builder                     │  │
│  │  - Build lora_config_payload_t structures            │  │
│  │  - Calculate checksums                               │  │
│  │  - Validate parameter ranges                         │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌──────────────────────────────────────────────────────┐  │
│  │         LoRa Message Sender                          │  │
│  │  - Send binary payload with delimiters               │  │
│  │  - Handle LoRa E32 module                            │  │
│  │  - Log transmission status                           │  │
│  └──────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│                    LoRa E32 Module                          │
│                          ↓                                   │
│                    Over-the-air to Rainsensor              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

## Implementation Phases

### Phase 1: Header File Setup
**File**: `include/communication.h`

Create a unified communication header that includes:
- Configuration parameter limits (min/max values)
- Default configuration values
- Event ID definitions
- Message structure definitions (`lora_config_payload_t`)
- Checksum calculation macro
- Debug logging helper

**Status**: Already exists in `src/communicationold.h` - will be moved/updated to `include/communication.h`

### Phase 2: Configuration State Management
**File**: `src/LoraSender.cpp`

Add global configuration state:
- Current configuration values (pulses, wakeup_sec, shutdown_ms, lora_delay_ms)
- Message ID counter
- Helper functions to validate parameter ranges
- Helper functions to display current configuration

### Phase 3: Serial Menu System
**File**: `src/LoraSender.cpp`

Implement interactive serial menu:
- Display main menu with options
- Parse user input (commands)
- Handle parameter setting commands
- Handle send commands
- Display current configuration
- Validate user input against limits

**Menu Structure**:
```
=== LoraSender Configuration Menu ===
1. Set ULP Pulses (1-100)
2. Set Wakeup Interval (10-3600 sec)
3. Set Shutdown Delay (1000-30000 ms)
4. Set LoRa Delay (1000-30000 ms)
5. Show Current Configuration
6. Send SET_CONFIG
7. Send RESET_CONFIG
8. Exit Menu
```

### Phase 4: Message Building Functions
**File**: `src/LoraSender.cpp`

Implement functions to build and send messages:
- `build_config_message()` - Create lora_config_payload_t with current values
- `send_config_message()` - Send SET_CONFIG message
- `send_reset_config_message()` - Send RESET_CONFIG message
- `send_lora_binary()` - Generic binary sender with delimiters

### Phase 5: Main Loop Integration
**File**: `src/LoraSender.cpp`

Modify main loop to:
- Check for serial input
- Route to menu system when appropriate
- Continue existing LoRa receive/send functionality
- Maintain backward compatibility with current event cycling

### Phase 6: Testing & Documentation
- Verify message format matches Rainsensor expectations
- Test checksum calculation
- Test parameter validation
- Document serial menu commands
- Create usage guide

## File Structure

```
LoraSender/
├── include/
│   └── communication.h          (NEW: unified header)
├── src/
│   ├── LoraSender.cpp           (MODIFIED: add menu system)
│   └── communicationold.h        (EXISTING: will be replaced)
└── plans/
    └── implementation_plan.md    (THIS FILE)
```

## Key Design Decisions

### 1. Message Structure
- Use `lora_config_payload_t` from communication.h (16 bytes)
- Includes messageID, eventID, 4 config parameters, reserved fields, checksum
- Well within 58-byte LoRa limit

### 2. Serial Menu Approach
- Text-based menu in serial monitor
- Simple command parsing (numeric input)
- Real-time validation with feedback
- Non-blocking (doesn't interfere with LoRa operations)

### 3. Configuration State
- Store current values in global struct
- Validate against CONFIG_MIN_* and CONFIG_MAX_* constants
- Display current values before sending

### 4. Message Sending
- Reuse existing `e32ttl.sendMessage()` infrastructure
- Add delimiters (0x0C 0x0C) like current implementation
- Increment messageID for each send
- Log all transmissions

## Integration Points

### Existing Code to Preserve
- LoRa E32 module initialization (setup())
- Event cycling system (can coexist with menu)
- Receive functionality (receiveValuesLoRa())
- Debug macros and logging

### New Code to Add
- Serial menu handler in loop()
- Configuration state variables
- Message building functions
- Parameter validation functions

## Success Criteria

1. ✅ Serial menu displays correctly
2. ✅ User can set all 4 configuration parameters
3. ✅ Parameter validation prevents out-of-range values
4. ✅ SET_CONFIG message sends with correct structure
5. ✅ RESET_CONFIG message sends with correct structure
6. ✅ Checksums calculated correctly
7. ✅ Messages received by Rainsensor without errors
8. ✅ Rainsensor applies configuration correctly
9. ✅ Existing functionality (event cycling, receive) still works
10. ✅ No memory leaks or crashes

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Serial menu blocks LoRa operations | Use non-blocking input handling |
| Invalid parameter values sent | Validate against limits before sending |
| Checksum calculation errors | Use macro from communication.h |
| Message format mismatch | Verify structure size and field order |
| Memory issues | Keep state minimal, reuse buffers |

## Timeline Estimate

- Phase 1: Header setup - Quick (already exists)
- Phase 2: State management - Quick
- Phase 3: Serial menu - Medium
- Phase 4: Message functions - Quick
- Phase 5: Integration - Medium
- Phase 6: Testing - Ongoing

## Next Steps

1. Review and approve this plan
2. Switch to Code mode
3. Implement Phase 1-2 (header and state)
4. Implement Phase 3 (serial menu)
5. Implement Phase 4 (message functions)
6. Integrate into main loop (Phase 5)
7. Test end-to-end (Phase 6)
