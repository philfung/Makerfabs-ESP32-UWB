/*
 * Multi-Anchor UWB Ranging Test Suite
 * 
 * This test suite validates the multi-anchor functionality of the DW1000Ranging library
 * using simulated data without requiring actual hardware.
 * 
 * Test Coverage:
 * - Single anchor operation (backward compatibility)
 * - Dual anchor operation
 * - Multiple anchor operation (3-4 anchors)
 * - Message queue functionality
 * - Protocol state transitions
 * - Error conditions and timeout handling
 * - Broadcast message handling
 */

#include <Arduino.h>
#include "../common/lib/Makerfabs-ESP32-UWB/DW1000/src/DW1000Ranging.h"
#include "../common/lib/Makerfabs-ESP32-UWB/DW1000/src/DW1000Device.h"

// Test configuration
#define TEST_DEBUG 1
#define MAX_TEST_DEVICES 4
#define TEST_TIMEOUT_MS 5000

// Test result tracking
struct TestResult {
    const char* testName;
    bool passed;
    const char* errorMessage;
    uint32_t executionTime;
};

// Test statistics
static int testsRun = 0;
static int testsPassed = 0;
static int testsFailed = 0;
static TestResult testResults[50]; // Max 50 tests

// Mock device data for testing
struct MockDevice {
    byte address[8];
    byte shortAddress[2];
    float expectedRange;
    bool isActive;
    uint32_t lastActivity;
};

// Test anchor configurations
MockDevice testAnchors[MAX_TEST_DEVICES] = {
    {{0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, {0x01, 0x01}, 2.5f, true, 0},
    {{0x02, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, {0x02, 0x02}, 3.2f, true, 0},
    {{0x03, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, {0x03, 0x03}, 4.1f, true, 0},
    {{0x04, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}, {0x04, 0x04}, 1.8f, true, 0}
};

// Test tag configuration
MockDevice testTag = {{0x7D, 0x00, 0x22, 0xEA, 0x82, 0x60, 0x3B, 0x9C}, {0x7D, 0x00}, 0.0f, true, 0};

// Test callback counters
static int rangeCompleteCount = 0;
static int protocolErrorCount = 0;
static int newDeviceCount = 0;
static int blinkDeviceCount = 0;
static float lastRangeValue = 0.0f;
static DW1000Device* lastRangeDevice = nullptr;

// Test callback functions
void onRangeComplete(DW1000Device* device) {
    rangeCompleteCount++;
    lastRangeValue = device->getRange();
    lastRangeDevice = device;
    if (TEST_DEBUG) {
        Serial.print("Range complete: Device ");
        Serial.print(device->getShortAddress(), HEX);
        Serial.print(" Range: ");
        Serial.println(device->getRange());
    }
}

void onProtocolError(DW1000Device* device, int errorCode) {
    protocolErrorCount++;
    if (TEST_DEBUG) {
        Serial.print("Protocol error: Device ");
        Serial.print(device->getShortAddress(), HEX);
        Serial.print(" Error: ");
        Serial.println(errorCode);
    }
}

void onNewDevice(DW1000Device* device) {
    newDeviceCount++;
    if (TEST_DEBUG) {
        Serial.print("New device: ");
        Serial.println(device->getShortAddress(), HEX);
    }
}

void onBlinkDevice(DW1000Device* device) {
    blinkDeviceCount++;
    if (TEST_DEBUG) {
        Serial.print("Blink device: ");
        Serial.println(device->getShortAddress(), HEX);
    }
}

void onNewRange() {
    // Legacy callback for backward compatibility testing
    if (TEST_DEBUG) {
        Serial.println("Legacy range callback triggered");
    }
}

// Test utility functions
void resetTestCounters() {
    rangeCompleteCount = 0;
    protocolErrorCount = 0;
    newDeviceCount = 0;
    blinkDeviceCount = 0;
    lastRangeValue = 0.0f;
    lastRangeDevice = nullptr;
}

void logTestResult(const char* testName, bool passed, const char* errorMessage = nullptr) {
    testResults[testsRun].testName = testName;
    testResults[testsRun].passed = passed;
    testResults[testsRun].errorMessage = errorMessage;
    testResults[testsRun].executionTime = millis();
    
    testsRun++;
    if (passed) {
        testsPassed++;
        if (TEST_DEBUG) {
            Serial.print("✓ PASS: ");
            Serial.println(testName);
        }
    } else {
        testsFailed++;
        if (TEST_DEBUG) {
            Serial.print("✗ FAIL: ");
            Serial.print(testName);
            if (errorMessage) {
                Serial.print(" - ");
                Serial.println(errorMessage);
            } else {
                Serial.println();
            }
        }
    }
}

// Mock message generation functions
void generateBlinkMessage(byte data[], MockDevice* device) {
    // Simulate BLINK message format
    data[0] = FC_1_BLINK;
    memcpy(data + 1, device->address, 8);
    memcpy(data + 9, device->shortAddress, 2);
}

void generateRangingInitMessage(byte data[], MockDevice* from, MockDevice* to) {
    // Simulate RANGING_INIT message format
    data[0] = FC_1;
    data[1] = FC_2;
    memcpy(data + 2, from->shortAddress, 2);
    memcpy(data + 4, to->address, 8);
    data[LONG_MAC_LEN] = RANGING_INIT;
}

void generatePollMessage(byte data[], MockDevice* from, MockDevice* anchors[], int numAnchors) {
    // Simulate POLL message format (broadcast)
    data[0] = FC_1;
    data[1] = FC_2_SHORT;
    memcpy(data + 2, from->shortAddress, 2);
    byte broadcast[2] = {0xFF, 0xFF};
    memcpy(data + 4, broadcast, 2);
    data[SHORT_MAC_LEN] = POLL;
    data[SHORT_MAC_LEN + 1] = numAnchors;
    
    // Add anchor data
    for (int i = 0; i < numAnchors; i++) {
        memcpy(data + SHORT_MAC_LEN + 2 + 4*i, anchors[i].shortAddress, 2);
        uint16_t replyTime = (2*i + 1) * DEFAULT_REPLY_DELAY_TIME;
        memcpy(data + SHORT_MAC_LEN + 4 + 4*i, &replyTime, 2);
    }
}

void generatePollAckMessage(byte data[], MockDevice* from, MockDevice* to) {
    // Simulate POLL_ACK message format
    data[0] = FC_1;
    data[1] = FC_2_SHORT;
    memcpy(data + 2, from->shortAddress, 2);
    memcpy(data + 4, to->shortAddress, 2);
    data[SHORT_MAC_LEN] = POLL_ACK;
}

void generateRangeMessage(byte data[], MockDevice* from, MockDevice* anchors[], int numAnchors) {
    // Simulate RANGE message format (broadcast)
    data[0] = FC_1;
    data[1] = FC_2_SHORT;
    memcpy(data + 2, from->shortAddress, 2);
    byte broadcast[2] = {0xFF, 0xFF};
    memcpy(data + 4, broadcast, 2);
    data[SHORT_MAC_LEN] = RANGE;
    data[SHORT_MAC_LEN + 1] = numAnchors;
    
    // Add timestamp data for each anchor (17 bytes per anchor)
    for (int i = 0; i < numAnchors; i++) {
        memcpy(data + SHORT_MAC_LEN + 2 + 17*i, anchors[i].shortAddress, 2);
        // Mock timestamps (5 bytes each)
        uint64_t mockTime = millis() * 1000 + i * 1000;
        memcpy(data + SHORT_MAC_LEN + 4 + 17*i, &mockTime, 5);  // timePollSent
        memcpy(data + SHORT_MAC_LEN + 9 + 17*i, &mockTime, 5);  // timePollAckReceived
        memcpy(data + SHORT_MAC_LEN + 14 + 17*i, &mockTime, 5); // timeRangeSent
    }
}

void generateRangeReportMessage(byte data[], MockDevice* from, MockDevice* to, float range) {
    // Simulate RANGE_REPORT message format
    data[0] = FC_1;
    data[1] = FC_2_SHORT;
    memcpy(data + 2, from->shortAddress, 2);
    memcpy(data + 4, to->shortAddress, 2);
    data[SHORT_MAC_LEN] = RANGE_REPORT;
    memcpy(data + 1 + SHORT_MAC_LEN, &range, 4);
    float rxPower = -45.0f; // Mock RX power
    memcpy(data + 5 + SHORT_MAC_LEN, &rxPower, 4);
}

void generateRangeFailedMessage(byte data[], MockDevice* from, MockDevice* to) {
    // Simulate RANGE_FAILED message format
    data[0] = FC_1;
    data[1] = FC_2_SHORT;
    memcpy(data + 2, from->shortAddress, 2);
    memcpy(data + 4, to->shortAddress, 2);
    data[SHORT_MAC_LEN] = RANGE_FAILED;
}

// Test simulation functions
void simulateMessageReceived(byte data[]) {
    // Simulate the handleReceived callback
    DW1000Ranging.handleReceived();
}

void simulateMessageSent() {
    // Simulate the handleSent callback
    DW1000Ranging.handleSent();
}

// Individual test functions
bool testDeviceStateManagement() {
    resetTestCounters();
    
    // Create a test device
    DW1000Device testDevice(testAnchors[0].address, testAnchors[0].shortAddress);
    
    // Test initial state
    if (testDevice.getProtocolState() != PROTOCOL_IDLE) {
        logTestResult("Device State Management", false, "Initial state not IDLE");
        return false;
    }
    
    // Test state transitions
    testDevice.setProtocolState(PROTOCOL_POLL_SENT);
    if (testDevice.getProtocolState() != PROTOCOL_POLL_SENT) {
        logTestResult("Device State Management", false, "State transition failed");
        return false;
    }
    
    // Test protocol activity
    testDevice.noteProtocolActivity();
    if (!testDevice.isProtocolActive()) {
        logTestResult("Device State Management", false, "Protocol activity not detected");
        return false;
    }
    
    // Test timeout detection
    delay(100);
    if (testDevice.isProtocolTimedOut(50)) {
        // Should be timed out
        testDevice.handleProtocolTimeout();
        if (testDevice.getProtocolState() != PROTOCOL_IDLE) {
            logTestResult("Device State Management", false, "Timeout handling failed");
            return false;
        }
    } else {
        logTestResult("Device State Management", false, "Timeout detection failed");
        return false;
    }
    
    logTestResult("Device State Management", true);
    return true;
}

bool testMessageQueue() {
    resetTestCounters();
    
    // Clear the message queue
    DW1000Ranging.clearMessageQueue();
    
    // Test enqueue/dequeue
    byte testData[LEN_DATA] = {0};
    generateBlinkMessage(testData, &testTag);
    
    bool enqueued = DW1000Ranging.enqueueMessage(testData, testTag.shortAddress, BLINK);
    if (!enqueued) {
        logTestResult("Message Queue", false, "Failed to enqueue message");
        return false;
    }
    
    MessageQueueItem item;
    bool dequeued = DW1000Ranging.dequeueMessage(&item);
    if (!dequeued) {
        logTestResult("Message Queue", false, "Failed to dequeue message");
        return false;
    }
    
    if (item.messageType != BLINK) {
        logTestResult("Message Queue", false, "Message type mismatch");
        return false;
    }
    
    if (memcmp(item.sourceAddress, testTag.shortAddress, 2) != 0) {
        logTestResult("Message Queue", false, "Source address mismatch");
        return false;
    }
    
    logTestResult("Message Queue", true);
    return true;
}

bool testSingleAnchorOperation() {
    resetTestCounters();
    
    // Initialize as tag for single anchor test
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000Ranging.attachRangeComplete(onRangeComplete);
    DW1000Ranging.attachNewDevice(onNewDevice);
    
    // Simulate anchor discovery
    byte data[LEN_DATA] = {0};
    generateRangingInitMessage(data, &testAnchors[0], &testTag);
    
    // Process the message
    DW1000Ranging.processDeviceMessage(nullptr, data, RANGING_INIT);
    
    // Check if device was added
    if (newDeviceCount != 1) {
        logTestResult("Single Anchor Operation", false, "Device not added");
        return false;
    }
    
    // Simulate ranging sequence
    generatePollAckMessage(data, &testAnchors[0], &testTag);
    DW1000Device* anchor = DW1000Ranging.searchDistantDevice(testAnchors[0].shortAddress);
    if (anchor == nullptr) {
        logTestResult("Single Anchor Operation", false, "Anchor device not found");
        return false;
    }
    
    DW1000Ranging.processDeviceMessage(anchor, data, POLL_ACK);
    
    // Simulate range report
    generateRangeReportMessage(data, &testAnchors[0], &testTag, testAnchors[0].expectedRange);
    DW1000Ranging.processDeviceMessage(anchor, data, RANGE_REPORT);
    
    // Check if range was completed
    if (rangeCompleteCount != 1) {
        logTestResult("Single Anchor Operation", false, "Range not completed");
        return false;
    }
    
    if (abs(lastRangeValue - testAnchors[0].expectedRange) > 0.1f) {
        logTestResult("Single Anchor Operation", false, "Range value incorrect");
        return false;
    }
    
    logTestResult("Single Anchor Operation", true);
    return true;
}

bool testDualAnchorOperation() {
    resetTestCounters();
    
    // Initialize as tag for dual anchor test
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000Ranging.attachRangeComplete(onRangeComplete);
    DW1000Ranging.attachNewDevice(onNewDevice);
    
    byte data[LEN_DATA] = {0};
    
    // Add first anchor
    generateRangingInitMessage(data, &testAnchors[0], &testTag);
    DW1000Ranging.processDeviceMessage(nullptr, data, RANGING_INIT);
    
    // Add second anchor
    generateRangingInitMessage(data, &testAnchors[1], &testTag);
    DW1000Ranging.processDeviceMessage(nullptr, data, RANGING_INIT);
    
    if (newDeviceCount != 2) {
        logTestResult("Dual Anchor Operation", false, "Both anchors not added");
        return false;
    }
    
    // Simulate ranging with both anchors
    DW1000Device* anchor1 = DW1000Ranging.searchDistantDevice(testAnchors[0].shortAddress);
    DW1000Device* anchor2 = DW1000Ranging.searchDistantDevice(testAnchors[1].shortAddress);
    
    if (anchor1 == nullptr || anchor2 == nullptr) {
        logTestResult("Dual Anchor Operation", false, "Anchor devices not found");
        return false;
    }
    
    // Simulate POLL_ACK from both anchors
    generatePollAckMessage(data, &testAnchors[0], &testTag);
    DW1000Ranging.processDeviceMessage(anchor1, data, POLL_ACK);
    
    generatePollAckMessage(data, &testAnchors[1], &testTag);
    DW1000Ranging.processDeviceMessage(anchor2, data, POLL_ACK);
    
    // Simulate RANGE_REPORT from both anchors
    generateRangeReportMessage(data, &testAnchors[0], &testTag, testAnchors[0].expectedRange);
    DW1000Ranging.processDeviceMessage(anchor1, data, RANGE_REPORT);
    
    generateRangeReportMessage(data, &testAnchors[1], &testTag, testAnchors[1].expectedRange);
    DW1000Ranging.processDeviceMessage(anchor2, data, RANGE_REPORT);
    
    // Check if both ranges were completed
    if (rangeCompleteCount != 2) {
        logTestResult("Dual Anchor Operation", false, "Both ranges not completed");
        return false;
    }
    
    logTestResult("Dual Anchor Operation", true);
    return true;
}

bool testMultiAnchorOperation() {
    resetTestCounters();
    
    // Initialize as tag for multi-anchor test
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000Ranging.attachRangeComplete(onRangeComplete);
    DW1000Ranging.attachNewDevice(onNewDevice);
    
    byte data[LEN_DATA] = {0};
    
    // Add all test anchors
    for (int i = 0; i < MAX_TEST_DEVICES; i++) {
        generateRangingInitMessage(data, &testAnchors[i], &testTag);
        DW1000Ranging.processDeviceMessage(nullptr, data, RANGING_INIT);
    }
    
    if (newDeviceCount != MAX_TEST_DEVICES) {
        logTestResult("Multi-Anchor Operation", false, "All anchors not added");
        return false;
    }
    
    // Simulate concurrent ranging with all anchors
    for (int i = 0; i < MAX_TEST_DEVICES; i++) {
        DW1000Device* anchor = DW1000Ranging.searchDistantDevice(testAnchors[i].shortAddress);
        if (anchor == nullptr) {
            logTestResult("Multi-Anchor Operation", false, "Anchor device not found");
            return false;
        }
        
        // POLL_ACK
        generatePollAckMessage(data, &testAnchors[i], &testTag);
        DW1000Ranging.processDeviceMessage(anchor, data, POLL_ACK);
        
        // RANGE_REPORT
        generateRangeReportMessage(data, &testAnchors[i], &testTag, testAnchors[i].expectedRange);
        DW1000Ranging.processDeviceMessage(anchor, data, RANGE_REPORT);
    }
    
    // Check if all ranges were completed
    if (rangeCompleteCount != MAX_TEST_DEVICES) {
        logTestResult("Multi-Anchor Operation", false, "All ranges not completed");
        return false;
    }
    
    logTestResult("Multi-Anchor Operation", true);
    return true;
}

bool testBroadcastMessageHandling() {
    resetTestCounters();
    
    // Initialize as anchor for broadcast test
    DW1000Ranging.startAsAnchor("01:02:03:04:05:06:07:08", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000Ranging.attachBlinkDevice(onBlinkDevice);
    
    byte data[LEN_DATA] = {0};
    
    // Test BLINK message handling
    generateBlinkMessage(data, &testTag);
    DW1000Ranging.processDeviceMessage(nullptr, data, BLINK);
    
    if (blinkDeviceCount != 1) {
        logTestResult("Broadcast Message Handling", false, "BLINK not handled");
        return false;
    }
    
    // Test broadcast POLL message
    MockDevice* anchors[2] = {&testAnchors[0], &testAnchors[1]};
    generatePollMessage(data, &testTag, anchors, 2);
    
    DW1000Device* tag = DW1000Ranging.searchDistantDevice(testTag.shortAddress);
    if (tag != nullptr) {
        DW1000Ranging.processDeviceMessage(tag, data, POLL);
        // Should trigger protocol state change
        if (tag->getProtocolState() != PROTOCOL_POLL_SENT) {
            logTestResult("Broadcast Message Handling", false, "POLL not processed correctly");
            return false;
        }
    }
    
    logTestResult("Broadcast Message Handling", true);
    return true;
}

bool testErrorHandling() {
    resetTestCounters();
    
    // Initialize as tag
    DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    DW1000Ranging.attachProtocolError(onProtocolError);
    
    // Add a test anchor
    byte data[LEN_DATA] = {0};
    generateRangingInitMessage(data, &testAnchors[0], &testTag);
    DW1000Ranging.processDeviceMessage(nullptr, data, RANGING_INIT);
    
    DW1000Device* anchor = DW1000Ranging.searchDistantDevice(testAnchors[0].shortAddress);
    if (anchor == nullptr) {
        logTestResult("Error Handling", false, "Test anchor not found");
        return false;
    }
    
    // Set expected message to POLL_ACK
    anchor->setExpectedMessage(MSG_POLL_ACK);
    
    // Send wrong message type (should trigger error)
    generateRangeFailedMessage(data, &testAnchors[0], &testTag);
    DW1000Ranging.processDeviceMessage(anchor, data, RANGE_FAILED);
    
    if (protocolErrorCount == 0) {
        logTestResult("Error Handling", false, "Protocol error not detected");
        return false;
    }
    
    // Test timeout handling
    anchor->noteProtocolActivity();
    delay(100);
    DW1000Ranging.handleDeviceTimeout();
    
    logTestResult("Error Handling", true);
    return true;
}

bool testProtocolStateTransitions() {
    resetTestCounters();
    
    // Test anchor state transitions
    DW1000Ranging.startAsAnchor("01:02:03:04:05:06:07:08", DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    
    // Add tag device
    byte data[LEN_DATA] = {0};
    generateBlinkMessage(data, &testTag);
    DW1000Ranging.processDeviceMessage(nullptr, data, BLINK);
    
    DW1000Device* tag = DW1000Ranging.searchDistantDevice(testTag.shortAddress);
    if (tag == nullptr) {
        logTestResult("Protocol State Transitions", false, "Tag device not found");
        return false;
    }
    
    // Test POLL -> POLL_SENT transition
    MockDevice* anchors[1] = {&testAnchors[0]};
    generatePollMessage(data, &testTag, anchors, 1);
    DW1000Ranging.processDeviceMessage(tag, data, POLL);
    
    if (tag->getProtocolState() != PROTOCOL_POLL_SENT) {
        logTestResult("Protocol State Transitions", false, "POLL_SENT state not reached");
        return false;
    }
    
    // Test RANGE -> RANGE_SENT transition
    generateRangeMessage(data, &testTag, anchors, 1);
    DW1000Ranging.processDeviceMessage(tag, data, RANGE);
    
    if (tag->getProtocolState() != PROTOCOL_RANGE_SENT) {
        logTestResult("Protocol State Transitions", false, "RANGE_SENT state not reached");
        return false;
    }
    
    logTestResult("Protocol State Transitions", true);
    return true;
}

// Main test runner
void runAllTests() {
    Serial.println("=== Multi-Anchor UWB Test Suite ===");
    Serial.println();
    
    uint32_t startTime = millis();
    
    // Run all tests
    testDeviceStateManagement();
    testMessageQueue();
    testSingleAnchorOperation();
    testDualAnchorOperation();
    testMultiAnchorOperation();
    testBroadcastMessageHandling();
    testErrorHandling();
    testProtocolStateTransitions();
    
    uint32_t totalTime = millis() - startTime;
    
    // Print results
    Serial.println();
    Serial.println("=== Test Results ===");
    Serial.print("Tests Run: ");
    Serial.println(testsRun);
    Serial.print("Tests Passed: ");
    Serial.println(testsPassed);
    Serial.print("Tests Failed: ");
    Serial.println(testsFailed);
    Serial.print("Success Rate: ");
    Serial.print((float)testsPassed / testsRun * 100.0f);
    Serial.println("%");
    Serial.print("Total Execution Time: ");
    Serial.print(totalTime);
    Serial.println("ms");
    
    if (testsFailed > 0) {
        Serial.println();
        Serial.println("Failed Tests:");
        for (int i = 0; i < testsRun; i++) {
            if (!testResults[i].passed) {
                Serial.print("- ");
                Serial.print(testResults[i].testName);
                if (testResults[i].errorMessage) {
                    Serial.print(": ");
                    Serial.println(testResults[i].errorMessage);
                } else {
                    Serial.println();
                }
            }
        }
    }
    
    Serial.println();
    Serial.println("=== Test Suite Complete ===");
}

// Arduino setup and loop for running tests
void setup() {
    Serial.begin(115200);
    delay(2000);
    
    Serial.println("Starting Multi-Anchor UWB Test Suite...");
    runAllTests();
}

void loop() {
    // Tests run once in setup()
    delay(1000);
}
