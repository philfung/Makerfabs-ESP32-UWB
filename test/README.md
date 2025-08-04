# Multi-Anchor UWB Test Suite

This directory contains comprehensive test scenarios to validate the multi-anchor functionality of the DW1000Ranging library using simulated data.

## Overview

The test suite validates the refactored DW1000Ranging library's ability to handle multiple anchors communicating with a single tag simultaneously. It uses mock data and simulated message exchanges to test the functionality without requiring actual hardware.

## Test Coverage

### Core Functionality Tests
- **Device State Management**: Tests per-device protocol state variables and transitions
- **Message Queue**: Validates message queuing, enqueueing, and dequeueing operations
- **Single Anchor Operation**: Backward compatibility test for single anchor ranging
- **Dual Anchor Operation**: Tests concurrent ranging with two anchors
- **Multi-Anchor Operation**: Tests concurrent ranging with 3-4 anchors

### Protocol Tests
- **Broadcast Message Handling**: Tests BLINK, POLL, and RANGE broadcast messages
- **Protocol State Transitions**: Validates state machine transitions for both anchors and tags
- **Error Handling**: Tests protocol error detection, timeout handling, and recovery

### Message Types Tested
- `BLINK` - Tag discovery messages
- `RANGING_INIT` - Anchor-to-tag initialization
- `POLL` - Broadcast ranging poll messages
- `POLL_ACK` - Anchor acknowledgment messages
- `RANGE` - Broadcast range messages with timestamps
- `RANGE_REPORT` - Range result messages
- `RANGE_FAILED` - Error condition messages

## Test Configuration

### Mock Devices
The test suite uses predefined mock devices:

**Test Anchors:**
- Anchor 1: Address `01:02:03:04:05:06:07:08`, Short `01:01`, Expected Range: 2.5m
- Anchor 2: Address `02:02:03:04:05:06:07:08`, Short `02:02`, Expected Range: 3.2m
- Anchor 3: Address `03:02:03:04:05:06:07:08`, Short `03:03`, Expected Range: 4.1m
- Anchor 4: Address `04:02:03:04:05:06:07:08`, Short `04:04`, Expected Range: 1.8m

**Test Tag:**
- Address: `7D:00:22:EA:82:60:3B:9C`, Short: `7D:00`

### Test Parameters
- `TEST_DEBUG`: Enable/disable debug output (1 = enabled)
- `MAX_TEST_DEVICES`: Maximum number of test devices (4)
- `TEST_TIMEOUT_MS`: Test timeout in milliseconds (5000)

## Running the Tests

### Hardware Requirements
- ESP32 development board
- Arduino IDE or PlatformIO
- Serial monitor for viewing results

### Setup Instructions

1. **Copy the test file** to your Arduino sketch directory or PlatformIO project
2. **Include the DW1000Ranging library** in your project
3. **Upload the test sketch** to your ESP32
4. **Open the serial monitor** at 115200 baud to view test results

### Expected Output

```
=== Multi-Anchor UWB Test Suite ===

✓ PASS: Device State Management
✓ PASS: Message Queue
✓ PASS: Single Anchor Operation
✓ PASS: Dual Anchor Operation
✓ PASS: Multi-Anchor Operation
✓ PASS: Broadcast Message Handling
✓ PASS: Error Handling
✓ PASS: Protocol State Transitions

=== Test Results ===
Tests Run: 8
Tests Passed: 8
Tests Failed: 0
Success Rate: 100.00%
Total Execution Time: 1234ms

=== Test Suite Complete ===
```

## Test Details

### Device State Management Test
- Creates a test device and validates initial state (PROTOCOL_IDLE)
- Tests state transitions (IDLE → POLL_SENT)
- Validates protocol activity tracking
- Tests timeout detection and handling

### Message Queue Test
- Tests message enqueueing and dequeueing
- Validates message type and source address preservation
- Tests queue overflow protection

### Single Anchor Operation Test
- Tests backward compatibility with original single-anchor code
- Simulates complete ranging sequence: RANGING_INIT → POLL_ACK → RANGE_REPORT
- Validates range value accuracy

### Dual Anchor Operation Test
- Tests concurrent ranging with two anchors
- Validates independent protocol state management
- Ensures both anchors complete ranging successfully

### Multi-Anchor Operation Test
- Tests concurrent ranging with 3-4 anchors
- Validates scalability of the refactored architecture
- Tests device management with maximum anchor count

### Broadcast Message Handling Test
- Tests BLINK message processing for device discovery
- Validates broadcast POLL message handling
- Tests anchor reply time extraction from broadcast messages

### Error Handling Test
- Tests protocol error detection for unexpected messages
- Validates timeout handling and recovery
- Tests error callback functionality

### Protocol State Transitions Test
- Tests anchor protocol state machine (IDLE → POLL_SENT → RANGE_SENT)
- Tests tag protocol state machine (IDLE → POLL_ACK_SENT → IDLE)
- Validates state transitions for different message types

## Interpreting Results

### Success Indicators
- All tests pass (100% success rate)
- No protocol errors detected during normal operation
- Range values match expected values within tolerance (±0.1m)
- All callback functions triggered correctly

### Failure Indicators
- Test failures indicate specific functionality issues
- Protocol errors suggest state machine problems
- Incorrect range values indicate timestamp or calculation issues
- Missing callbacks suggest handler registration problems

## Troubleshooting

### Common Issues

**Include Path Errors:**
- These are expected in the IDE and don't affect functionality
- The code is designed for Arduino/ESP32 environment

**Test Failures:**
- Check that the refactored library files are properly included
- Verify that all new methods are implemented
- Ensure callback handlers are properly registered

**Memory Issues:**
- Monitor available heap memory during tests
- Reduce `MAX_TEST_DEVICES` if memory is limited
- Check for memory leaks in device management

### Debug Mode
Enable `TEST_DEBUG` to see detailed test execution:
- Device creation and state changes
- Message processing steps
- Callback function triggers
- Error conditions and recovery

## Integration with Hardware Testing

This test suite provides a foundation for hardware testing:

1. **Validate software logic** with simulated data first
2. **Identify issues** before hardware deployment
3. **Establish baseline** for expected behavior
4. **Compare results** between simulated and hardware tests

## Extending the Tests

### Adding New Tests
1. Create a new test function following the pattern: `bool testNewFeature()`
2. Add the test to `runAllTests()` function
3. Use `logTestResult()` to record pass/fail status
4. Reset counters with `resetTestCounters()` at the start

### Custom Mock Data
- Modify `testAnchors[]` array for different device configurations
- Adjust expected range values for your test scenarios
- Add new mock devices as needed

### Performance Testing
- Add timing measurements to individual tests
- Test with larger numbers of devices
- Measure memory usage during operations

## Validation Checklist

Before deploying to hardware, ensure:
- [ ] All tests pass consistently
- [ ] No memory leaks detected
- [ ] Protocol state transitions work correctly
- [ ] Error handling functions properly
- [ ] Broadcast messages processed correctly
- [ ] Multiple anchor ranging completes successfully
- [ ] Callback functions trigger as expected
- [ ] Range values are within expected tolerance

## Next Steps

After successful test validation:
1. **Hardware Testing**: Deploy to actual UWB hardware
2. **Performance Optimization**: Optimize based on test results
3. **Integration Testing**: Test with robocaddy positioning system
4. **Field Testing**: Validate in real-world conditions
