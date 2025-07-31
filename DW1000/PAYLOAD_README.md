# DW1000 Ranging Library - Payload Support

This document describes the payload functionality added to the DW1000 ranging library, which allows bidirectional data transmission during the ranging process.

## Overview

The payload feature adds 8-byte data transmission capability to both RANGE and RANGE_REPORT messages:

- **RANGE Message**: TAG can send 8 bytes (4 bytes dataType + 4 bytes dataValue) to ANCHOR
- **RANGE_REPORT Message**: ANCHOR can send 8 bytes (4 bytes dataType + 4 bytes dataValue) back to TAG

## Message Format Changes

### RANGE Message (TAG → ANCHOR)
**Before:**
```
[MAC Header][Message Type][Device Count][Per Device: ShortAddr(2) + Timing(15)]
```

**After:**
```
[MAC Header][Message Type][Device Count][Per Device: ShortAddr(2) + Timing(15) + Payload(8)]
```
- Per-device data increased from 17 bytes to 25 bytes

### RANGE_REPORT Message (ANCHOR → TAG)
**Before:**
```
[MAC Header][Message Type][Range(4)][RXPower(4)]
```

**After:**
```
[MAC Header][Message Type][Range(4)][RXPower(4)][Payload(8)]
```
- Message size increased from 8 bytes to 16 bytes

## API Reference

### Setting Payload Data

```cpp
// Set payload for outgoing RANGE messages (TAG side)
void DW1000Ranging.setRangePayload(uint32_t dataType, uint32_t dataValue);

// Set payload for outgoing RANGE_REPORT messages (ANCHOR side)
void DW1000Ranging.setRangeReportPayload(uint32_t dataType, uint32_t dataValue);
```

### Getting Received Payload Data

```cpp
// Get payload from received RANGE messages (ANCHOR side)
boolean DW1000Ranging.getRangePayload(uint32_t* dataType, uint32_t* dataValue);

// Get payload from received RANGE_REPORT messages (TAG side)
boolean DW1000Ranging.getRangeReportPayload(uint32_t* dataType, uint32_t* dataValue);
```

### Per-Device Payload Access

```cpp
// Set payload data for specific device
void device->setRangePayload(uint32_t dataType, uint32_t dataValue);
void device->setRangeReportPayload(uint32_t dataType, uint32_t dataValue);

// Get payload data from specific device
boolean device->getRangePayload(uint32_t* dataType, uint32_t* dataValue);
boolean device->getRangeReportPayload(uint32_t* dataType, uint32_t* dataValue);
```

## Usage Examples

### TAG Example (Sending Data to ANCHOR)

```cpp
#include "DW1000Ranging.h"

#define PAYLOAD_TYPE_SENSOR_DATA    0x01
#define PAYLOAD_TYPE_STATUS         0x02

void setup() {
  // Initialize ranging...
  DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void loop() {
  // Set payload before ranging
  uint32_t sensorValue = analogRead(A0);
  DW1000Ranging.setRangePayload(PAYLOAD_TYPE_SENSOR_DATA, sensorValue);
  
  DW1000Ranging.loop();
}

void newRange() {
  // Check for received payload from ANCHOR
  uint32_t dataType, dataValue;
  if (DW1000Ranging.getRangeReportPayload(&dataType, &dataValue)) {
    Serial.print("Received from ANCHOR: Type=0x");
    Serial.print(dataType, HEX);
    Serial.print(" Value=");
    Serial.println(dataValue);
  }
}
```

### ANCHOR Example (Receiving and Responding with Data)

```cpp
#include "DW1000Ranging.h"

#define PAYLOAD_TYPE_COMMAND        0x03
#define PAYLOAD_TYPE_STATUS         0x02

void setup() {
  // Initialize ranging...
  DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
}

void loop() {
  // Set response payload
  uint32_t anchorStatus = millis() / 1000; // Uptime in seconds
  DW1000Ranging.setRangeReportPayload(PAYLOAD_TYPE_STATUS, anchorStatus);
  
  DW1000Ranging.loop();
}

void newRange() {
  // Check for received payload from TAG
  uint32_t dataType, dataValue;
  if (DW1000Ranging.getRangePayload(&dataType, &dataValue)) {
    Serial.print("Received from TAG: Type=0x");
    Serial.print(dataType, HEX);
    Serial.print(" Value=");
    Serial.println(dataValue);
    
    // Process received data based on type
    switch(dataType) {
      case PAYLOAD_TYPE_SENSOR_DATA:
        processSensorData(dataValue);
