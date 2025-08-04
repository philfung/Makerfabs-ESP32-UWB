#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include "DW1000Ranging.h"

// Display support
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Connection pins
#define PIN_RST 27
#define PIN_IRQ 34
#define PIN_SS 4

// Display pins (standard for ESP32 UWB Pro with Display)
#define I2C_SDA 21
#define I2C_SCL 22
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// Global display enable flag
bool DISPLAY_ENABLED = true;

// Create display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Tag configuration
#define TAG_ADDR "7D:00:22:EA:82:60:3B:9C"

// Multi-anchor tracking
struct AnchorInfo {
    uint16_t shortAddress;
    float lastRange;
    float lastRXPower;
    uint32_t lastUpdate;
    bool isActive;
};

#define MAX_ANCHORS 8
AnchorInfo knownAnchors[MAX_ANCHORS];
int anchorCount = 0;

// Statistics
uint32_t totalRanges = 0;
uint32_t lastStatsTime = 0;
uint32_t rangesPerSecond = 0;
uint32_t lastDisplayUpdate = 0;

void newRange();
void rangeComplete(DW1000Device* device);
void protocolError(DW1000Device* device, int errorCode);
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);
void addAnchor(DW1000Device* device);
void updateAnchorInfo(DW1000Device* device);
int getActiveAnchorCount();
void checkInactiveAnchors();
void printStatistics();
void calculatePosition();
void displayInit();
void displayUpdate();
void displayInitStatus(const char* message);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Multi-Anchor UWB Tag Example");
    Serial.println("============================");
    
    // Initialize display if enabled
    if (DISPLAY_ENABLED) {
        displayInit();
        displayInitStatus("Initializing...");
    }
    
    // Initialize anchor tracking
    for (int i = 0; i < MAX_ANCHORS; i++) {
        knownAnchors[i].shortAddress = 0;
        knownAnchors[i].lastRange = 0.0f;
        knownAnchors[i].lastRXPower = 0.0f;
        knownAnchors[i].lastUpdate = 0;
        knownAnchors[i].isActive = false;
    }
    
    // Initialize DW1000 ranging
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    
    // Start as tag with multi-anchor support
    DW1000Ranging.startAsTag(TAG_ADDR, DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    
    // Attach callback handlers for multi-anchor functionality
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);
    
    // NEW: Multi-anchor specific callbacks
    DW1000Ranging.attachRangeComplete(rangeComplete);
    DW1000Ranging.attachProtocolError(protocolError);
    
    Serial.println("Tag initialized. Waiting for anchors...");
    Serial.println();
    
    if (DISPLAY_ENABLED) {
        displayInitStatus("Tag Ready");
        delay(1000);
    }
}

void loop() {
    DW1000Ranging.loop();
    
    // Print statistics every 5 seconds
    if (millis() - lastStatsTime > 5000) {
        printStatistics();
        lastStatsTime = millis();
    }
    
    // Update display every 500ms if enabled
    if (DISPLAY_ENABLED && (millis() - lastDisplayUpdate > 500)) {
        displayUpdate();
        lastDisplayUpdate = millis();
    }
    
    // Check for inactive anchors
    checkInactiveAnchors();
}

// Display initialization
void displayInit() {
    Wire.begin(I2C_SDA, I2C_SCL);
    delay(100);
    
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println(F("SSD1306 allocation failed"));
        DISPLAY_ENABLED = false;
        return;
    }
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.display();
}

// Display initialization status
void displayInitStatus(const char* message) {
    if (!DISPLAY_ENABLED) return;
    
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    
    display.setCursor(0, 0);
    display.println("UWB Tag");
    
    display.setCursor(0, 20);
    display.println(message);
    
    display.display();
}

// Display update function
void displayUpdate() {
    if (!DISPLAY_ENABLED) return;
    
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    
    int cursorY = 0;
    
    // Title
    display.setCursor(0, cursorY);
    display.println("UWB Tag");
    cursorY += 10;
    
    // Active anchors count
    display.setCursor(0, cursorY);
    display.print("Anchors: ");
    display.print(getActiveAnchorCount());
    display.print("/");
    display.print(anchorCount);
    cursorY += 10;
    
    // Ranges per second
    display.setCursor(0, cursorY);
    display.print("Ranges/s: ");
    display.print(rangesPerSecond);
    cursorY += 10;
    
    // Display first few active anchors with range
    int anchorsDisplayed = 0;
    for (int i = 0; i < anchorCount && anchorsDisplayed < 3 && cursorY < SCREEN_HEIGHT - 10; i++) {
        if (knownAnchors[i].isActive) {
            display.setCursor(0, cursorY);
            display.print("0x");
            display.print(knownAnchors[i].shortAddress, HEX);
            display.print(": ");
            display.print(knownAnchors[i].lastRange, 1);
            display.print("m");
            cursorY += 10;
            anchorsDisplayed++;
        }
    }
    
    // Show "No active anchors" if none found
    if (anchorsDisplayed == 0 && anchorCount > 0) {
        display.setCursor(0, cursorY);
        display.println("No active anchors");
    } else if (anchorCount == 0) {
        display.setCursor(0, cursorY);
        display.println("No anchors found");
    }
    
    display.display();
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

// Multi-anchor range complete callback
void rangeComplete(DW1000Device* device) {
    totalRanges++;
    
    // Update anchor info
    updateAnchorInfo(device);
    
    // Print range information
    Serial.print("Range Complete - Anchor: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Range: ");
    Serial.print(device->getRange(), 2);
    Serial.print("m RX Power: ");
    Serial.print(device->getRXPower(), 1);
    Serial.print("dBm FP Power: ");
    Serial.print(device->getFPPower(), 1);
    Serial.print("dBm Quality: ");
    Serial.println(device->getQuality(), 1);
    
    // Calculate position if we have enough anchors
    if (getActiveAnchorCount() >= 3) {
        calculatePosition();
    }
    
    // Update display immediately when new range is received
    if (DISPLAY_ENABLED) {
        displayUpdate();
    }
}

// Protocol error callback
void protocolError(DW1000Device* device, int errorCode) {
    Serial.print("Protocol Error - Anchor: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Error Code: ");
    Serial.println(errorCode);
}

// Blink callback - specific to tag mode
void newBlink(DW1000Device* device) {
    Serial.print("Blink received from Anchor: 0x");
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
    Serial.print("New Anchor Discovered: 0x");
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
    
    // Add to known anchors list
    addAnchor(device);
    
    // Update display immediately when new anchor is discovered
    if (DISPLAY_ENABLED) {
        displayUpdate();
    }
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Anchor Inactive: 0x");
    Serial.println(device->getShortAddress(), HEX);
    
    // Mark anchor as inactive
    for (int i = 0; i < anchorCount; i++) {
        if (knownAnchors[i].shortAddress == device->getShortAddress()) {
            knownAnchors[i].isActive = false;
            break;
        }
    }
}

void addAnchor(DW1000Device* device) {
    if (anchorCount < MAX_ANCHORS) {
        knownAnchors[anchorCount].shortAddress = device->getShortAddress();
        knownAnchors[anchorCount].lastRange = 0.0f;
        knownAnchors[anchorCount].lastRXPower = 0.0f;
        knownAnchors[anchorCount].lastUpdate = millis();
        knownAnchors[anchorCount].isActive = true;
        anchorCount++;
        
        Serial.print("Added anchor to tracking list. Total anchors: ");
        Serial.println(anchorCount);
    } else {
        Serial.println("Warning: Maximum anchor limit reached!");
    }
}

void updateAnchorInfo(DW1000Device* device) {
    for (int i = 0; i < anchorCount; i++) {
        if (knownAnchors[i].shortAddress == device->getShortAddress()) {
            knownAnchors[i].lastRange = device->getRange();
            knownAnchors[i].lastRXPower = device->getRXPower();
            knownAnchors[i].lastUpdate = millis();
            knownAnchors[i].isActive = true;
            break;
        }
    }
}

int getActiveAnchorCount() {
    int count = 0;
    uint32_t currentTime = millis();
    
    for (int i = 0; i < anchorCount; i++) {
        // Consider anchor active if updated within last 10 seconds
        if (knownAnchors[i].isActive && (currentTime - knownAnchors[i].lastUpdate) < 10000) {
            count++;
        }
    }
    
    return count;
}

void checkInactiveAnchors() {
    uint32_t currentTime = millis();
    
    for (int i = 0; i < anchorCount; i++) {
        if (knownAnchors[i].isActive && (currentTime - knownAnchors[i].lastUpdate) > 15000) {
            Serial.print("Marking anchor 0x");
            Serial.print(knownAnchors[i].shortAddress, HEX);
            Serial.println(" as inactive (no updates for 15s)");
            knownAnchors[i].isActive = false;
        }
    }
}

void printStatistics() {
    Serial.println();
    Serial.println("=== Multi-Anchor Statistics ===");
    
    // Calculate ranges per second
    static uint32_t lastTotalRanges = 0;
    rangesPerSecond = (totalRanges - lastTotalRanges) / 5;
    lastTotalRanges = totalRanges;
    
    Serial.print("Total Ranges: ");
    Serial.println(totalRanges);
    Serial.print("Ranges/Second: ");
    Serial.println(rangesPerSecond);
    Serial.print("Active Anchors: ");
    Serial.println(getActiveAnchorCount());
    
    // Print anchor details
    Serial.println("\nAnchor Details:");
    Serial.println("Address  | Range(m) | RX Power | Last Update | Status");
    Serial.println("---------|----------|----------|-------------|--------");
    
    uint32_t currentTime = millis();
    for (int i = 0; i < anchorCount; i++) {
        Serial.print("0x");
        if (knownAnchors[i].shortAddress < 0x1000) Serial.print("0");
        Serial.print(knownAnchors[i].shortAddress, HEX);
        Serial.print("   | ");
        Serial.print(knownAnchors[i].lastRange, 2);
        Serial.print("     | ");
        Serial.print(knownAnchors[i].lastRXPower, 1);
        Serial.print("      | ");
        Serial.print((currentTime - knownAnchors[i].lastUpdate) / 1000);
        Serial.print("s ago     | ");
        Serial.println(knownAnchors[i].isActive ? "Active" : "Inactive");
    }
    
    // Memory usage
    Serial.print("\nFree Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.println("===============================");
    Serial.println();
}

void calculatePosition() {
    // Simple trilateration example (requires at least 3 anchors)
    // This is a basic implementation - real positioning would use more sophisticated algorithms
    
    int activeCount = 0;
    float ranges[MAX_ANCHORS];
    uint16_t addresses[MAX_ANCHORS];
    
    // Collect active anchor data
    for (int i = 0; i < anchorCount && activeCount < MAX_ANCHORS; i++) {
        if (knownAnchors[i].isActive && knownAnchors[i].lastRange > 0) {
            ranges[activeCount] = knownAnchors[i].lastRange;
            addresses[activeCount] = knownAnchors[i].shortAddress;
            activeCount++;
        }
    }
    
    if (activeCount >= 3) {
        Serial.println("\n--- Position Calculation ---");
        Serial.print("Using ");
        Serial.print(activeCount);
        Serial.println(" anchors for positioning:");
        
        for (int i = 0; i < activeCount; i++) {
            Serial.print("  Anchor 0x");
            Serial.print(addresses[i], HEX);
            Serial.print(": ");
            Serial.print(ranges[i], 2);
            Serial.println("m");
        }
        
        // Here you would implement actual trilateration/multilateration
        // For now, just show that we have the data needed
        Serial.println("  [Position calculation would be implemented here]");
        Serial.println("  [Requires known anchor positions and trilateration algorithm]");
        Serial.println("----------------------------\n");
    }
}

// Additional utility functions for advanced usage
void printDeviceInfo() {
    Serial.println("\n=== Device Information ===");
    Serial.print("Tag Address: ");
    Serial.println(TAG_ADDR);
    Serial.print("Network Devices: ");
    Serial.println(DW1000Ranging.getNetworkDevicesNumber());
    // Serial.print("Range Filter: ");
    // Serial.println(DW1000Ranging.isRangeFilterEnabled() ? "Enabled" : "Disabled");
    Serial.println("==========================\n");
}

// Function to manually trigger ranging (if needed)
void triggerRanging() {
    // The refactored library handles ranging automatically
    // This function could be used to force a ranging cycle if needed
    Serial.println("Manual ranging trigger (automatic in refactored library)");
}
