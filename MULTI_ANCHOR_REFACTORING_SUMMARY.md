# Multi-Anchor Support Refactoring Summary

## Overview

This document summarizes the refactoring performed on the DW1000Ranging library to support multiple anchors communicating with a single tag simultaneously. The original library had global protocol state variables that prevented concurrent ranging operations with multiple devices.

## Problem Analysis

The original `DW1000Ranging.cpp` had several global state variables that created conflicts when multiple anchors tried to communicate with a single tag:

- `_sentAck` - Global flag indicating message sent
- `_receivedAck` - Global flag indicating message received  
- `_protocolFailed` - Global protocol error state
- `_expectedMsgId` - Global expected message type

These variables caused race conditions and protocol conflicts when multiple anchors were active simultaneously.

## Solution Architecture

### Phase 1: Per-Device State Management

**Modified Files:**
- `DW1000Device.h` - Added per-device protocol state variables
- `DW1000Device.cpp` - Implemented per-device state management methods

**Key Changes:**
- Added `ProtocolState` enum for device state tracking
- Added `MessageType` enum for message identification
- Added per-device protocol state variables:
  - `_protocolState` - Current protocol state
  - `_expectedMsgId` - Expected message type for this device
  - `_sentAck` - Message sent flag for this device
  - `_receivedAck` - Message received flag for this device
  - `_protocolFailed` - Protocol error flag for this device
  - `_lastProtocolActivity` - Timestamp for timeout detection

**New Methods:**
- `resetProtocolState()` - Initialize/reset device protocol state
- `isProtocolActive()` - Check if device has active protocol
- `handleProtocolTimeout()` - Handle protocol timeouts
- `isProtocolTimedOut()` - Check for protocol timeout
- `noteProtocolActivity()` - Update activity timestamp

### Phase 2: Message Queue System

**Modified Files:**
- `DW1000Ranging.h` - Added message queue structure and methods
- `DW1000Ranging.cpp` - Implemented message queue processing

**Key Changes:**
- Added `MessageQueueItem` structure for queuing incoming messages
- Implemented circular buffer message queue with configurable size
- Added queue management methods:
  - `enqueueMessage()` - Add message to queue
  - `dequeueMessage()` - Remove message from queue
  - `clearMessageQueue()` - Reset queue

### Phase 3: Concurrent Protocol Processing

**Modified Files:**
- `DW1000Ranging.h` - Removed global protocol state variables
- `DW1000Ranging.cpp` - Refactored main loop and message processing

**Key Changes:**
- Removed global protocol state variables
- Added per-device message processing methods:
  - `processDeviceMessages()` - Process queued messages
  - `handleDeviceTimeout()` - Check for device timeouts
  - `processDeviceMessage()` - Process message for specific device
  - `handleDeviceProtocolState()` - Handle device protocol transitions

**New Handler Support:**
- `attachRangeComplete()` - Handler for completed ranging operations
- `attachProtocolError()` - Handler for protocol errors

## Implementation Status

### ✅ Completed
1. **Per-device state management** - All devices now maintain independent protocol state
2. **Message queue system** - Incoming messages are queued for processing
3. **Basic concurrent processing framework** - Infrastructure for handling multiple devices
4. **Protocol state isolation** - No more global state conflicts
5. **Full protocol logic migration** - Complete implementation of `processDeviceMessage()` and `handleDeviceProtocolState()`
6. **Sent message handling** - `handleSent()` updated with device-specific logic and timestamp management
7. **Received message handling** - `handleReceived()` updated to use message queue system
8. **Broadcast message handling** - POLL and RANGE broadcast messages properly handled
9. **BLINK and RANGING_INIT support** - Device discovery and initialization messages implemented

### 🚧 Partially Implemented
1. **Device synchronization** - Basic coordination exists, may need refinement for complex scenarios
2. **Error recovery** - Basic timeout and protocol error handling implemented, could be enhanced

### ❌ Not Yet Implemented
1. **Performance optimization** - Queue management and processing efficiency could be improved
2. **Comprehensive testing** - Hardware testing with multiple anchors needed
3. **Advanced error recovery** - More sophisticated error handling strategies

## Next Steps for Completion

### 1. Complete Message Processing Logic

The `processDeviceMessage()` and `handleDeviceProtocolState()` methods need to be fully implemented with the original protocol logic, adapted for per-device operation.

### 2. Implement Sent Message Handling

The `handleSent()` method needs to identify which device the sent message relates to and update the appropriate device's `_sentAck` flag.

### 3. Add Broadcast Message Support

Special handling is needed for broadcast messages (POLL and RANGE) that are sent to multiple devices simultaneously.

### 4. Implement Device Coordination

Add logic to coordinate multiple ranging sessions and prevent conflicts between devices.

### 5. Add Comprehensive Error Handling

Implement robust error recovery for protocol failures, timeouts, and device disconnections.

## Testing Strategy

### Unit Testing
- Test per-device state management
- Test message queue operations
- Test protocol state transitions

### Integration Testing
- Test single anchor operation (backward compatibility)
- Test dual anchor operation
- Test multiple anchor operation (3-4 anchors)

### Performance Testing
- Measure ranging frequency with multiple anchors
- Test queue overflow handling
- Measure memory usage impact

## Memory Impact

The refactoring increases memory usage per device:
- Original: ~74 bytes per device
- Refactored: ~90-100 bytes per device (estimated)

Additional global memory:
- Message queue: ~720 bytes (8 items × 90 bytes each)
- Queue management: ~10 bytes

Total additional memory: ~800-900 bytes

## Backward Compatibility

The refactoring maintains backward compatibility for single-anchor operations. Existing code should continue to work without modification.

## Usage Example

```cpp
// Initialize as tag
DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);

// Attach handlers for multi-anchor support
DW1000Ranging.attachRangeComplete(onRangeComplete);
DW1000Ranging.attachProtocolError(onProtocolError);

void onRangeComplete(DW1000Device* device) {
    Serial.print("Range complete for device: ");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Range: ");
    Serial.println(device->getRange());
}

void onProtocolError(DW1000Device* device, int errorCode) {
    Serial.print("Protocol error for device: ");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Error: ");
    Serial.println(errorCode);
}
```

## Conclusion

This refactoring provides the foundation for multi-anchor support by eliminating global state conflicts and implementing per-device protocol management. The core infrastructure is in place, but additional work is needed to complete the full protocol implementation and add robust error handling.

The changes maintain backward compatibility while enabling concurrent ranging operations with multiple anchors, which is essential for accurate positioning systems.
