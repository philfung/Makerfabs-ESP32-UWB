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

// Anchor configuration
#define ANCHOR_ADDR "86:17:5B:D5:A9:9A:E2:9C"

// Single tag tracking
struct TagInfo {
    uint16_t shortAddress;
    float lastRange;
    float lastRXPower;
    uint32_t lastUpdate;
    bool isActive;
    bool isConnected;
};

TagInfo currentTag;
uint32_t totalRanges = 0;
uint32_t lastStatsTime = 0;
uint32_t rangesPerSecond = 0;
uint32_t lastDisplayUpdate = 0;

void newRange();
void newBlink(DW1000Device* device);
void rangeComplete(DW1000Device* device);
void protocolError(DW1000Device* device, int errorCode);
void newDevice(DW1000Device* device);
void inactiveDevice(DW1000Device* device);
void updateTagInfo(DW1000Device* device);
void printStatistics();
void displayInit();
void displayUpdate();
void displayInitStatus(const char* message);

void setup() {
    Serial.begin(115200);
    delay(1000);
    
    Serial.println("Single-Tag UWB Anchor Example");
    Serial.println("=============================");
    
    // Initialize display if enabled
    if (DISPLAY_ENABLED) {
        displayInit();
        displayInitStatus("Initializing...");
    }
    
    // Initialize tag tracking
    currentTag.shortAddress = 0;
    currentTag.lastRange = 0.0f;
    currentTag.lastRXPower = 0.0f;
    currentTag.lastUpdate = 0;
    currentTag.isActive = false;
    currentTag.isConnected = false;
    
    // Initialize DW1000 ranging
    DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ);
    
    // Start as anchor
    DW1000Ranging.startAsAnchor(ANCHOR_ADDR, DW1000.MODE_LONGDATA_RANGE_LOWPOWER);
    
    // Attach callback handlers
    DW1000Ranging.attachNewRange(newRange);
    DW1000Ranging.attachBlinkDevice(newBlink);
    DW1000Ranging.attachNewDevice(newDevice);
    DW1000Ranging.attachInactiveDevice(inactiveDevice);
    DW1000Ranging.attachRangeComplete(rangeComplete);
    DW1000Ranging.attachProtocolError(protocolError);
    
    Serial.println("Anchor initialized. Waiting for tag...");
    Serial.println();
    
    if (DISPLAY_ENABLED) {
        displayInitStatus("Anchor Ready");
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
    
    // Check for inactive tag
    if (currentTag.isActive && (millis() - currentTag.lastUpdate) > 15000) {
        Serial.print("Tag 0x");
        Serial.print(currentTag.shortAddress, HEX);
        Serial.println(" marked as inactive (no updates for 15s)");
        currentTag.isActive = false;
    }
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
    display.println("UWB Anchor");
    
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
    display.println("UWB Anchor");
    cursorY += 10;
    
    // Tag connection status
    display.setCursor(0, cursorY);
    if (currentTag.isConnected) {
        display.print("Tag: 0x");
        display.print(currentTag.shortAddress, HEX);
    } else {
        display.print("No Tag");
    }
    cursorY += 10;
    
    // Range display
    display.setCursor(0, cursorY);
    if (currentTag.isActive) {
        display.print("Range: ");
        display.print(currentTag.lastRange, 2);
        display.print("m");
    } else if (currentTag.isConnected) {
        display.print("Range: -- m");
    } else {
        display.print("Range: N/A");
    }
    cursorY += 10;
    
    // RX Power
    display.setCursor(0, cursorY);
    if (currentTag.isActive) {
        display.print("RX: ");
        display.print(currentTag.lastRXPower, 1);
        display.print("dBm");
    } else {
        display.print("RX: -- dBm");
    }
    cursorY += 10;
    
    // Ranges per second
    display.setCursor(0, cursorY);
    display.print("Ranges/s: ");
    display.print(rangesPerSecond);
    
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

// Range complete callback
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

// Protocol error callback
void protocolError(DW1000Device* device, int errorCode) {
    Serial.print("Protocol Error - Tag: 0x");
    Serial.print(device->getShortAddress(), HEX);
    Serial.print(" Error Code: ");
    Serial.println(errorCode);
}

// Blink callback - specific to anchor mode
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
    
    // Update tag info
    currentTag.shortAddress = device->getShortAddress();
    currentTag.isConnected = true;
    updateTagInfo(device);
    
    // Update display immediately when new tag is discovered
    if (DISPLAY_ENABLED) {
        displayUpdate();
    }
}

void inactiveDevice(DW1000Device* device) {
    Serial.print("Tag Inactive: 0x");
    Serial.println(device->getShortAddress(), HEX);
    
    currentTag.isConnected = false;
    currentTag.isActive = false;
}

void updateTagInfo(DW1000Device* device) {
    currentTag.lastRange = device->getRange();
    currentTag.lastRXPower = device->getRXPower();
    currentTag.lastUpdate = millis();
    currentTag.isActive = true;
}

void printStatistics() {
    Serial.println();
    Serial.println("=== Anchor Statistics ===");
    
    // Calculate ranges per second
    static uint32_t lastTotalRanges = 0;
    rangesPerSecond = (totalRanges - lastTotalRanges) / 5;
    lastTotalRanges = totalRanges;
    
    Serial.print("Total Ranges: ");
    Serial.println(totalRanges);
    Serial.print("Ranges/Second: ");
    Serial.println(rangesPerSecond);
    
    if (currentTag.isConnected) {
        Serial.print("Tag Address: 0x");
        Serial.println(currentTag.shortAddress, HEX);
        Serial.print("Last Range: ");
        Serial.print(currentTag.lastRange, 2);
        Serial.println("m");
        Serial.print("Last RX Power: ");
        Serial.print(currentTag.lastRXPower, 1);
        Serial.println("dBm");
        Serial.print("Last Update: ");
        Serial.print((millis() - currentTag.lastUpdate) / 1000);
        Serial.println("s ago");
        Serial.print("Status: ");
        Serial.println(currentTag.isActive ? "Active" : "Inactive");
    } else {
        Serial.println("No tag connected");
    }
    
    // Memory usage
    Serial.print("Free Heap: ");
    Serial.print(ESP.getFreeHeap());
    Serial.println(" bytes");
    
    Serial.println("========================");
    Serial.println();
}
