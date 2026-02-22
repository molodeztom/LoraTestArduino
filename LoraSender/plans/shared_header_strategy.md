# Shared Header File Strategy

## Problem Statement

Both LoraSender (Arduino/PlatformIO) and Rainsensor (ESP-IDF) projects need to use the same `communication.h` header file to ensure message format consistency. However, they are:
- Located in different directories
- Using different build systems (PlatformIO vs ESP-IDF)
- Using different LoRa libraries (Arduino E32 lib vs ESP-IDF E32 lib)

## Current Situation

### Directory Structure
```
d:/Projects/
├── LoraTestArduino/
│   └── LoraSender/
│       ├── include/
│       ├── src/
│       │   ├── LoraSender.cpp
│       │   └── communicationold.h (local copy)
│       └── platformio.ini
│
├── Rainsensor/
│   ├── include/
│   │   └── communication.h (master copy)
│   └── src/
│
└── HomeAutomation/
    └── (shared libraries)
```

### Current Build Configuration
**LoraSender** (`platformio.ini`):
```ini
build_flags = -I "..\..\HomeAutomation" -I../../Rainsensor/include
```

This means LoraSender can already include files from `../../Rainsensor/include/`

## Recommended Solution: Shared Header in HomeAutomation

### Rationale
1. **Centralized Location**: `HomeAutomation/` is already used as a shared library directory
2. **Neutral Ground**: Not tied to either project
3. **Build System Agnostic**: Both PlatformIO and ESP-IDF can include from it
4. **Existing Pattern**: LoraSender already includes from `../../HomeAutomation`
5. **Version Control**: Single source of truth for message format

### Proposed Structure
```
HomeAutomation/
├── communication.h          (NEW: shared message definitions)
├── HomeAutomationCommon.h   (existing)
└── ... (other shared files)
```

### Implementation Steps

#### Step 1: Create Shared Header
**File**: `../../HomeAutomation/communication.h`

Copy the content from `../../Rainsensor/include/communication.h` to create the shared version.

**Advantages**:
- Single source of truth
- Both projects include from same location
- Easy to update both projects simultaneously
- No duplication

#### Step 2: Update LoraSender Build Configuration
**File**: `platformio.ini`

```ini
build_flags = -I "..\..\HomeAutomation" -I../../Rainsensor/include
```

Already includes `../../HomeAutomation`, so no changes needed!

#### Step 3: Update Rainsensor Build Configuration
**File**: `Rainsensor/CMakeLists.txt` (or equivalent)

Add include path:
```cmake
target_include_directories(${COMPONENT_NAME} PUBLIC "../../HomeAutomation")
```

#### Step 4: Update Include Statements

**LoraSender** (`src/LoraSender.cpp`):
```cpp
// OLD:
#include "../../Rainsensor/include/communication.h"

// NEW:
#include "communication.h"  // Found via -I../../HomeAutomation
```

**Rainsensor** (`main/rainsensor_main.c`):
```cpp
// OLD:
#include "communication.h"  // From local include/

// NEW:
#include "communication.h"  // From ../../HomeAutomation (if added to CMakeLists)
// OR keep local copy that includes shared version
```

## Alternative Solutions Considered

### Option A: Symlink (Not Recommended)
- **Pros**: Single file, automatic sync
- **Cons**: Windows symlinks are problematic, not portable
- **Decision**: ❌ Rejected

### Option B: Git Submodule (Not Recommended)
- **Pros**: Version control integration
- **Cons**: Adds complexity, requires git setup
- **Decision**: ❌ Rejected for now

### Option C: Copy to Both Projects (Current State)
- **Pros**: Simple, independent
- **Cons**: Duplication, sync issues, error-prone
- **Decision**: ❌ Not ideal

### Option D: Shared HomeAutomation Directory (Recommended) ✅
- **Pros**: Centralized, already used for shared code, simple
- **Cons**: Requires coordination between projects
- **Decision**: ✅ Recommended

## Implementation Plan

### Phase 1: Create Shared Header
1. Copy `../../Rainsensor/include/communication.h` to `../../HomeAutomation/communication.h`
2. Verify both projects can include it
3. Test compilation

### Phase 2: Update LoraSender
1. Update `src/LoraSender.cpp` include path
2. Remove `src/communicationold.h`
3. Verify compilation and functionality

### Phase 3: Update Rainsensor (Optional)
1. Update build configuration to use shared header
2. Keep local copy as fallback if needed
3. Verify compilation and functionality

### Phase 4: Documentation
1. Document the shared header location
2. Add comments to both projects explaining the dependency
3. Create maintenance guide

## Maintenance Guidelines

### When to Update communication.h
1. **New Message Types**: Add to shared header
2. **New Event IDs**: Add to shared header
3. **Structure Changes**: Update in shared header, test both projects
4. **Constant Changes**: Update in shared header, verify ranges

### Update Process
1. Edit `../../HomeAutomation/communication.h`
2. Recompile both LoraSender and Rainsensor
3. Test both projects
4. Commit changes with clear message

### Versioning
Add version comment to header:
```c
// Communication Protocol Version: 1.0
// Last Updated: 2026-02-22
// Used by: LoraSender, Rainsensor
```

## Risk Mitigation

| Risk | Mitigation |
|------|-----------|
| Projects get out of sync | Single shared file eliminates duplication |
| Build path issues | Both projects already use relative paths |
| Accidental modifications | Clear documentation of shared nature |
| Version conflicts | Version comment in header |

## Conclusion

**Recommended Approach**: Place `communication.h` in `../../HomeAutomation/` as the single source of truth for message format definitions. This leverages existing infrastructure, maintains consistency, and simplifies maintenance.

**Next Steps**:
1. Create `../../HomeAutomation/communication.h` from Rainsensor version
2. Update LoraSender to use shared header
3. Test both projects
4. Document the shared dependency
