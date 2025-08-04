#include <Arduino.h>

#include <SPI.h>
#include "DW1000Ranging.h"

// Connection pins
#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS 4

// Anchor configuration
#define ANCHOR_ADDR "86:17:5B:D5:A9:9A:E2:9C"

// Multi-tag tracking
struct TagInfo {
    uint16_t shortAddress;
    float lastRange;
    float lastRXPower;
    uint32_t lastUpdate;
    bool isActive;
};

#define MAX_TAGS 8
TagInfo knownTags[MAX_TAGS];
int tagCount = 0;

// Statistics
uint32_t totalRanges = 0;
uint32_t lastStatsTime = 0;
uint32_t rangesPerSecond = 0;

void newRange();
void newBlink(DW1000Device* device);
void rangeComplete(DW1000Device* device);
void protocolError(DW1000Device* device, int errorCode);
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);
void addTag(DW1000Device* device);
void updateTagInfo(DW1000Device* device);
int getActiveTagCount();
void checkInactiveTags();
void printStatistics();
void printDeviceInfo();
void triggerRanging();

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Multi-Tag UWB Anchor Example");
    Serial.println("=============================");
    
    // Initialize tag tracking
    for (int i = 0; i < MAX_TAGS; i++) {
        knownTags[i].shortAddress = 0;
        knownTags[i].lastRange = 0.0f;
        knownTags[i].lastRXPower = 0.0f;
        knownTags[i].lastUpdate = 0;
        knownTags[i].isActive = false;
    }
    
    // Initialize DW1000 ranging
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    
    // Start as anchor with multi-tag support
    DW1000Ranging.startAsAnchor(ANCHOR_ADDR, DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    
    // Attach callback handlers for multi-tag functionality
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);
    
    // NEW: Multi-tag specific callbacks
    DW1000Ranging.attachRangeComplete(rangeComplete);
    DW1000Ranging.attachProtocolError(protocolError);
    
    // // Enable range filtering for more stable readings
    // DW1000Ranging.useRangeFilter(true);
    // DW1000Ranging.setRangeFilterValue(10);
    
    Serial.println("Anchor initialized. Waiting for tags...");
    Serial.println();
}

void loop() {
    DW1000Ranging.loop();
    
    // Print statistics every 5 seconds
    if (millis() - lastStatsTime > 5000) {
        printStatistics();
        lastStatsTime = millis();
    }
    
    // Check for inactive tags
    checkInactiveTags();
}

// Legacy callback for backward compatibility
void newRange() {
    DW1000Device* device = DW1000Ranging.getDistantDevice();
    if (device != nullptr) {
        Serial.print("Legacy Range - Device: ");
        Serial.print(device->getShortAddress(), HEX);
        Serial.print(" Range: ");
        Serial.print(device->getRange());
        Serial.println("m");
    }
}

// NEW: Multi-tag range complete callback
void rangeComplete(DW1000Device* device) {
    totalRanges++;
    
    // Update tag info
    updateTagInfo(device);
    
    // Print range information
    Serial.print("Range Complete - Tag: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Range: ");
    Serial.print(device->getRange(), 2);
    Serial.print("m RX Power: ");
    Serial.print(device->getRXPower(), 1);
    Serial.print("dBm FP Power: ");
    Serial.print(device->getFPPower(), 1);
    Serial.print("dBm Quality: ");
    Serial.println(device->getQuality(), 1);
}

// NEW: Protocol error callback
void protocolError(DW1000Device* device, int errorCode) {
    Serial.print("Protocol Error - Tag: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Error Code: ");
    Serial.println(errorCode);
    
    // Mark tag as potentially problematic
    for (int i = 0; i < tagCount; i++) {
        if (knownTags[i].shortAddress == device->getShortAddress()) {
            // Could implement error counting and temporary blacklisting here
            break;
        }
    }
}

// NEW: Blink callback - specific to anchor mode
void newBlink(DW1000Device* device) {
    Serial.print("Blink received from Tag: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Address: ");
    
    // Print full address
    byte* addr = device->getByteAddress();
    for (int i = 0; i < 8; i++) {
        if (addr[i] < 16) Serial.print("0");
        Serial.print(addr[i], HEX);
        if (i < 7) Serial.print(":");
    }
    Serial.println();
}

void newDevice(DW1000Device* device) {
    Serial.print("New Tag Discovered: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Address: ");
    
    // Print full address
    byte* addr = device->getByteAddress();
    for (int i = 0; i < 8; i++) {
        if (addr[i] < 16) Serial.print("0");
        Serial.print(addr[i], HEX);
        if (i < 7) Serial.print(":");
    }
    Serial.println();
    
    // Add to known tags list
    addTag(device);
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Tag Inactive: 0x");
    Serial.println(device->getShortAddress(), HEX);
    
    // Mark tag as inactive
    for (int i = 0; i < tagCount; i++) {
        if (knownTags[i].shortAddress == device->getShortAddress()) {
            knownTags[i].isActive = false;
            break;
        }
    }
}

void addTag(DW1000Device* device) {
    if (tagCount < MAX_TAGS) {
        knownTags[tagCount].shortAddress = device->getShortAddress();
        knownTags[tagCount].lastRange = 0.0f;
        knownTags[tagCount].lastRXPower = 0.0f;
        knownTags[tagCount].lastUpdate = millis();
        knownTags[tagCount].isActive = true;
        tagCount++;
        
        Serial.print("Added tag to tracking list. Total tags: ");
        Serial.println(tagCount);
    } else {
        Serial.println("Warning: Maximum tag limit reached!");
    }
}

void updateTagInfo(DW1000Device* device) {
    for (int i = 0; i < tagCount; i++) {
        if (knownTags[i].shortAddress == device->getShortAddress()) {
            knownTags[i].lastRange = device->getRange();
            knownTags[i].lastRXPower = device->getRXPower();
            knownTags[i].lastUpdate = millis();
            knownTags[i].isActive = true;
            break;
        }
    }
}

int getActiveTagCount() {
    int count = 0;
    uint32_t currentTime = millis();
    
    for (int i = 0; i < tagCount; i++) {
        // Consider tag active if updated within last 10 seconds
        if (knownTags[i].isActive && (currentTime - knownTags[i].lastUpdate) < 10000) {
            count++;
        }
    }
    
    return count;
}

void checkInactiveTags() {
    uint32_t currentTime = millis();
    
    for (int i = 0; i < tagCount; i++) {
        if (knownTags[i].isActive && (currentTime - knownTags[i].lastUpdate) > 15000) {
            Serial.print("Marking tag 0x");
            Serial.print(knownTags[i].shortAddress, HEX);
            Serial.println(" as inactive (no updates for 15s)");
            knownTags[i].isActive = false;
        }
    }
}

void printStatistics() {
    Serial.println();
    Serial.println("=== Multi-Tag Statistics ===");
    
    // Calculate ranges per second
    static uint32_t lastTotalRanges = 0;
    rangesPerSecond = (totalRanges - lastTotalRanges) / 5;
    lastTotalRanges = totalRanges;
    
    Serial.print("Total Ranges: ");
    Serial.println(totalRanges);
    Serial.print("Ranges/Second: ");
    Serial.println(rangesPerSecond);
    Serial.print("Active Tags: ");
    Serial.println(getActiveTagCount());
    
    // Print tag details
    Serial.println("\nTag Details:");
    Serial.println("Address  | Range(m) | RX Power | Last Update | Status");
    Serial.println("---------|----------|----------|-------------|--------");
    
    uint32_t currentTime = millis();
    for (int i = 0; i < tagCount; i++) {
        Serial.print("0x");
        if (knownTags[i].shortAddress < 0x1000) Serial.print("0");
        Serial.print(knownTags[i].shortAddress, HEX);
        Serial.print("   | ");
        Serial.print(knownTags[i].lastRange, 2);
        Serial.print("     | ");
        Serial.print(knownTags[i].lastRXPower, 1);
        Serial.print("      | ");
        Serial.print((currentTime - knownTags[i].lastUpdate) / 1000);
        Serial.print("s ago     | ");
        Serial.println(knownTags[i].isActive ? "Active" : "Inactive");
    }
    
    // Memory usage
    Serial.print("\nFree Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.println("=============================");
    Serial.println();
}

// Additional utility functions for advanced usage
void printDeviceInfo() {
    Serial.println("\n=== Device Information ===");
    Serial.print("Anchor Address: ");
    Serial.println(ANCHOR_ADDR);
    Serial.print("Network Devices: ");
    Serial.println(DW1000Ranging.getNetworkDevicesNumber());
    // Serial.print("Range Filter: ");
    // Serial.println(DW1000Ranging.isRangeFilterEnabled() ? "Enabled" : "Disabled");
    Serial.println("==========================\n");
}

// Function to manually trigger ranging (if needed)
void triggerRanging() {
    // Anchors respond to tag ranging requests automatically
    // This function is not typically needed for anchor implementations
    Serial.println("Anchor responds to tag ranging requests automatically");
}
