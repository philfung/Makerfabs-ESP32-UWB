/**
 * Example demonstrating payload functionality in DW1000 ranging
 * This ANCHOR example shows how to send and receive payload data
 * during the ranging process.
 * 
 * Payload Format:
 * - RANGE message: TAG can send 8 bytes (4 bytes dataType + 4 bytes dataValue) to ANCHOR
 * - RANGE_REPORT message: ANCHOR can send 8 bytes back to TAG
 */
#include <SPI.h>
#include "DW1000Ranging.h"

// connection pins
const uint8_t PIN_RST = 9; // reset pin
const uint8_t PIN_IRQ = 2; // irq pin
const uint8_t PIN_SS = SS; // spi select pin

// Payload data types (application-specific)
#define PAYLOAD_TYPE_SENSOR_DATA    0x01
#define PAYLOAD_TYPE_STATUS         0x02
#define PAYLOAD_TYPE_COMMAND        0x03
#define PAYLOAD_TYPE_HEARTBEAT      0x04

// Example anchor data
uint32_t anchorStatus = 0;
uint32_t responseCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  //init the configuration
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); //Reset, CS, IRQ pin
  
  //define the sketch as anchor
  DW1000Ranging.attachNewRange(UWBnewRange);
  DW1000Ranging.attachBlinkDevice(newBlink);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  
  //Enable the filter to smooth the distance
  //DW1000Ranging.useRangeFilter(true);
  
  //we start the module as an anchor
  DW1000Ranging.startAsAnchor("82:17:5B:D5:A9:9A:E2:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
  
  Serial.println("### ANCHOR with Payload Support ###");
  Serial.println("Payload Types:");
  Serial.println("  0x01 - Sensor Data");
  Serial.println("  0x02 - Status");
  Serial.println("  0x03 - Command");
  Serial.println("  0x04 - Heartbeat");
}

void loop() {
  DW1000Ranging.loop();
  
  // Update payload data for responses
  updateResponsePayload();
}

void updateResponsePayload() {
  // Prepare response payload based on anchor status
  responseCounter++;
  
  if (responseCounter % 5 == 0) {
    // Send command every 5th response
    uint32_t command = 0x100; // Example command code
    DW1000Ranging.setPayloadFromAnchor(PAYLOAD_TYPE_COMMAND, command);
  } else if (responseCounter % 3 == 0) {
    // Send heartbeat every 3rd response
    DW1000Ranging.setPayloadFromAnchor(PAYLOAD_TYPE_HEARTBEAT, responseCounter);
  } else {
    // Send anchor status (uptime in seconds)
    anchorStatus = millis() / 1000;
    DW1000Ranging.setPayloadFromAnchor(PAYLOAD_TYPE_STATUS, anchorStatus);
  }
}

void UWBnewRange() {
  Serial.print("from: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getRange()); 
  Serial.print(" m");
  Serial.print("\t RX power: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getRXPower()); 
  Serial.print(" dBm");
  
  // Check for received payload from RANGE message
  uint32_t dataType, dataValue;
  if (DW1000Ranging.getPayloadFromTag(&dataType, &dataValue)) {
    Serial.print("\t RX Payload: Type=0x");
    Serial.print(dataType, HEX);
    Serial.print(" Value=");
    Serial.print(dataValue);
    
    // Interpret payload based on type
    switch(dataType) {
      case PAYLOAD_TYPE_SENSOR_DATA:
        Serial.print(" (TAG Sensor: ");
        Serial.print(dataValue);
        Serial.print(")");
        // Could process sensor data here
        break;
      case PAYLOAD_TYPE_STATUS:
        Serial.print(" (TAG Status: ");
        Serial.print(dataValue);
        Serial.print(")");
        break;
      case PAYLOAD_TYPE_COMMAND:
        Serial.print(" (Command from TAG: 0x");
        Serial.print(dataValue, HEX);
        Serial.print(")");
        // Could execute command here
        break;
      case PAYLOAD_TYPE_HEARTBEAT:
        Serial.print(" (TAG Heartbeat #");
        Serial.print(dataValue);
        Serial.print(")");
        break;
      default:
        Serial.print(" (Unknown Type)");
        break;
    }
  }
  
  Serial.println();
}

void newBlink(DW1000Device* device) {
  Serial.print("blink; 1 device added ! -> ");
  Serial.print(" short:");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
}
