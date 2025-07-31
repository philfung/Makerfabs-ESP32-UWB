/**
 * Example demonstrating payload functionality in DW1000 ranging
 * This TAG example shows how to send and receive payload data
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

// Example sensor data
uint32_t sensorValue = 0;
uint32_t messageCounter = 0;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  //init the configuration
  DW1000Ranging.initCommunication(PIN_RST, PIN_SS, PIN_IRQ); //Reset, CS, IRQ pin
  
  //define the sketch as tag
  DW1000Ranging.attachNewRange(newRange);
  DW1000Ranging.attachNewDevice(newDevice);
  DW1000Ranging.attachInactiveDevice(inactiveDevice);
  
  //Enable the filter to smooth the distance
  //DW1000Ranging.useRangeFilter(true);
  
  //we start the module as a tag
  DW1000Ranging.startAsTag("7D:00:22:EA:82:60:3B:9C", DW1000.MODE_LONGDATA_RANGE_ACCURACY);
  
  Serial.println("### TAG with Payload Support ###");
  Serial.println("Payload Types:");
  Serial.println("  0x01 - Sensor Data");
  Serial.println("  0x02 - Status");
  Serial.println("  0x03 - Command");
  Serial.println("  0x04 - Heartbeat");
}

void loop() {
  DW1000Ranging.loop();
  
  // Update payload data before each ranging cycle
  updatePayloadData();
}

void updatePayloadData() {
  // Simulate different types of data to send
  messageCounter++;
  
  if (messageCounter % 4 == 0) {
    // Send heartbeat every 4th message
    DW1000Ranging.setPayloadFromTag(PAYLOAD_TYPE_HEARTBEAT, messageCounter);
  } else if (messageCounter % 3 == 0) {
    // Send status every 3rd message
    uint32_t status = (millis() / 1000) % 256; // Simple status based on uptime
    DW1000Ranging.setPayloadFromTag(PAYLOAD_TYPE_STATUS, status);
  } else {
    // Send sensor data (simulated)
    sensorValue = analogRead(A0); // Read from analog pin
    DW1000Ranging.setPayloadFromTag(PAYLOAD_TYPE_SENSOR_DATA, sensorValue);
  }
}

void newRange() {
  Serial.print("from: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getShortAddress(), HEX);
  Serial.print("\t Range: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getRange()); 
  Serial.print(" m");
  Serial.print("\t RX power: "); 
  Serial.print(DW1000Ranging.getDistantDevice()->getRXPower()); 
  Serial.print(" dBm");
  
  // Check for received payload from RANGE_REPORT
  uint32_t dataType, dataValue;
  if (DW1000Ranging.getPayloadFromAnchor(&dataType, &dataValue)) {
    Serial.print("\t RX Payload: Type=0x");
    Serial.print(dataType, HEX);
    Serial.print(" Value=");
    Serial.print(dataValue);
    
    // Interpret payload based on type
    switch(dataType) {
      case PAYLOAD_TYPE_SENSOR_DATA:
        Serial.print(" (Anchor Sensor)");
        break;
      case PAYLOAD_TYPE_STATUS:
        Serial.print(" (Anchor Status)");
        break;
      case PAYLOAD_TYPE_COMMAND:
        Serial.print(" (Command from Anchor)");
        break;
      case PAYLOAD_TYPE_HEARTBEAT:
        Serial.print(" (Anchor Heartbeat)");
        break;
      default:
        Serial.print(" (Unknown)");
        break;
    }
  }
  
  Serial.println();
}

void newDevice(DW1000Device* device) {
  Serial.print("ranging init; 1 device added ! -> ");
  Serial.print(" short:");
  Serial.println(device->getShortAddress(), HEX);
}

void inactiveDevice(DW1000Device* device) {
  Serial.print("delete inactive device: ");
  Serial.println(device->getShortAddress(), HEX);
}
