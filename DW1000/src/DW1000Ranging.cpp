/*
 * Copyright (c) 2015 by Thomas Trojer <thomas@trojer.net> and Leopold Sayous <leosayous@gmail.com>
 * Decawave DW1000 library for arduino.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @file DW1000Ranging.h
 * Arduino global library (source file) working with the DW1000 library 
 * for the Decawave DW1000 UWB transceiver IC.
 *
 * @TODO
 * - remove or debugmode for Serial.print
 * - move strings to flash to reduce ram usage
 * - do not safe duplicate of pin settings
 * - maybe other object structure
 * - use enums instead of preprocessor constants
 */


#include "DW1000Ranging.h"
#include "DW1000Device.h"

DW1000RangingClass DW1000Ranging;


//other devices we are going to communicate with which are on our network:
DW1000Device DW1000RangingClass::_networkDevices[MAX_DEVICES];
byte         DW1000RangingClass::_currentAddress[8];
byte         DW1000RangingClass::_currentShortAddress[2];
byte         DW1000RangingClass::_lastSentToShortAddress[2];
volatile uint8_t DW1000RangingClass::_networkDevicesNumber = 0; // TODO short, 8bit?
int16_t      DW1000RangingClass::_lastDistantDevice    = 0; // TODO short, 8bit?
DW1000Mac    DW1000RangingClass::_globalMac;

//module type (anchor or tag)
int16_t      DW1000RangingClass::_type; // TODO enum??

// range filter
volatile boolean DW1000RangingClass::_useRangeFilter = false;
uint16_t DW1000RangingClass::_rangeFilterValue = 15;

// REMOVED: Global protocol state variables (now per-device)
// volatile byte    DW1000RangingClass::_expectedMsgId;
// volatile boolean DW1000RangingClass::_sentAck     = false;
// volatile boolean DW1000RangingClass::_receivedAck = false;
// boolean          DW1000RangingClass::_protocolFailed = false;

// NEW: Message queue for concurrent processing
MessageQueueItem DW1000RangingClass::_messageQueue[MESSAGE_QUEUE_SIZE];
volatile uint8_t DW1000RangingClass::_queueHead = 0;
volatile uint8_t DW1000RangingClass::_queueTail = 0;
volatile uint8_t DW1000RangingClass::_queueCount = 0;

// NEW: Current processing device index for round-robin
uint8_t DW1000RangingClass::_currentProcessingDevice = 0;

// timestamps to remember
int32_t            DW1000RangingClass::timer           = 0;
int16_t            DW1000RangingClass::counterForBlink = 0; // TODO 8 bit?


// data buffer
byte          DW1000RangingClass::data[LEN_DATA];
// reset line to the chip
uint8_t   DW1000RangingClass::_RST;
uint8_t   DW1000RangingClass::_SS;
// watchdog and reset period
uint32_t  DW1000RangingClass::_lastActivity;
uint32_t  DW1000RangingClass::_resetPeriod;
// reply times (same on both sides for symm. ranging)
uint16_t  DW1000RangingClass::_replyDelayTimeUS;
//timer delay
uint16_t  DW1000RangingClass::_timerDelay;
// ranging counter (per second)
uint16_t  DW1000RangingClass::_successRangingCount = 0;
uint32_t  DW1000RangingClass::_rangingCountPeriod  = 0;
//Here our handlers
void (* DW1000RangingClass::_handleNewRange)(void) = 0;
void (* DW1000RangingClass::_handleBlinkDevice)(DW1000Device*) = 0;
void (* DW1000RangingClass::_handleNewDevice)(DW1000Device*) = 0;
void (* DW1000RangingClass::_handleInactiveDevice)(DW1000Device*) = 0;

// NEW: Multi-anchor specific handlers
void (* DW1000RangingClass::_handleRangeComplete)(DW1000Device*) = 0;
void (* DW1000RangingClass::_handleProtocolError)(DW1000Device*, int) = 0;

/* ###########################################################################
 * #### Init and end #######################################################
 * ######################################################################### */

void DW1000RangingClass::initCommunication(uint8_t myRST, uint8_t mySS, uint8_t myIRQ) {
	// reset line to the chip
	_RST              = myRST;
	_SS               = mySS;
	_resetPeriod      = DEFAULT_RESET_PERIOD;
	// reply times (same on both sides for symm. ranging)
	_replyDelayTimeUS = DEFAULT_REPLY_DELAY_TIME;
	//we set our timer delay
	_timerDelay       = DEFAULT_TIMER_DELAY;
	
	// Initialize message queue
	clearMessageQueue();
	
	DW1000.begin(myIRQ, myRST);
	DW1000.select(mySS);
}


void DW1000RangingClass::configureNetwork(uint16_t deviceAddress, uint16_t networkId, const byte mode[]) {
	// general configuration
	DW1000.newConfiguration();
	DW1000.setDefaults();
	DW1000.setDeviceAddress(deviceAddress);
	DW1000.setNetworkId(networkId);
	DW1000.enableMode(mode);
	DW1000.commitConfiguration();
	
}

void DW1000RangingClass::generalStart() {
	// attach callback for (successfully) sent and received messages
	DW1000.attachSentHandler(handleSent);
	DW1000.attachReceivedHandler(handleReceived);
	// anchor starts in receiving mode, awaiting a ranging poll message
	
	
	if(DEBUG) {
		// DEBUG monitoring
		Serial.println("DW1000-arduino");
		// initialize the driver
		
		
		Serial.println("configuration..");
		// DEBUG chip info and registers pretty printed
		char msg[90];
		DW1000.getPrintableDeviceIdentifier(msg);
		Serial.print("Device ID: ");
		Serial.println(msg);
		DW1000.getPrintableExtendedUniqueIdentifier(msg);
		Serial.print("Unique ID: ");
		Serial.print(msg);
		char string[6];
		sprintf(string, "%02X:%02X", _currentShortAddress[0], _currentShortAddress[1]);
		Serial.print(" short: ");
		Serial.println(string);
		
		DW1000.getPrintableNetworkIdAndShortAddress(msg);
		Serial.print("Network ID & Device Address: ");
		Serial.println(msg);
		DW1000.getPrintableDeviceMode(msg);
		Serial.print("Device mode: ");
		Serial.println(msg);
	}
	
	// Vincent changes
	DW1000.large_power_init();
	
	// anchor starts in receiving mode, awaiting a ranging poll message
	receiver();
	// for first time ranging frequency computation
	_rangingCountPeriod = millis();
}


void DW1000RangingClass::startAsAnchor(char address[], const byte mode[], const bool randomShortAddress) {
	//save the address
	DW1000.convertToByte(address, _currentAddress);
	//write the address on the DW1000 chip
	DW1000.setEUI(address);
	Serial.print("device address: ");
	Serial.println(address);
	if (randomShortAddress) {
		//we need to define a random short address:
		randomSeed(analogRead(0));
		_currentShortAddress[0] = random(0, 256);
		_currentShortAddress[1] = random(0, 256);
	}
	else {
		// we use first two bytes in addess for short address
		_currentShortAddress[0] = _currentAddress[0];
		_currentShortAddress[1] = _currentAddress[1];
	}
	
	//we configur the network for mac filtering
	//(device Address, network ID, frequency)
	DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
	
	//general start:
	generalStart();
	
	//defined type as anchor
	_type = ANCHOR;
	
	Serial.println("### ANCHOR ###");
	
}

void DW1000RangingClass::startAsTag(char address[], const byte mode[], const bool randomShortAddress) {
	//save the address
	DW1000.convertToByte(address, _currentAddress);
	//write the address on the DW1000 chip
	DW1000.setEUI(address);
	Serial.print("device address: ");
	Serial.println(address);
	if (randomShortAddress) {
		//we need to define a random short address:
		randomSeed(analogRead(0));
		_currentShortAddress[0] = random(0, 256);
		_currentShortAddress[1] = random(0, 256);
	}
	else {
		// we use first two bytes in addess for short address
		_currentShortAddress[0] = _currentAddress[0];
		_currentShortAddress[1] = _currentAddress[1];
	}
	
	//we configur the network for mac filtering
	//(device Address, network ID, frequency)
	DW1000Ranging.configureNetwork(_currentShortAddress[0]*256+_currentShortAddress[1], 0xDECA, mode);
	
	generalStart();
	//defined type as tag
	_type = TAG;
	
	Serial.println("### TAG ###");
}

boolean DW1000RangingClass::addNetworkDevices(DW1000Device* device, boolean shortAddress) {
	boolean   addDevice = true;
	//we test our network devices array to check
	//we don't already have it
	for(uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if(_networkDevices[i].isAddressEqual(device) && !shortAddress) {
			//the device already exists
			addDevice = false;
			return false;
		}
		else if(_networkDevices[i].isShortAddressEqual(device) && shortAddress) {
			//the device already exists
			addDevice = false;
			return false;
		}
		
	}
	
	if(addDevice) {
		device->setRange(0);
		memcpy(&_networkDevices[_networkDevicesNumber], device, sizeof(DW1000Device));
		_networkDevices[_networkDevicesNumber].setIndex(_networkDevicesNumber);
		// NEW: Initialize per-device protocol state
		_networkDevices[_networkDevicesNumber].resetProtocolState();
		_networkDevicesNumber++;
		return true;
	}
	
	return false;
}

boolean DW1000RangingClass::addNetworkDevices(DW1000Device* device) {
	boolean addDevice = true;
	//we test our network devices array to check
	//we don't already have it
	for(uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if(_networkDevices[i].isAddressEqual(device) && _networkDevices[i].isShortAddressEqual(device)) {
			//the device already exists
			addDevice = false;
			return false;
		}
		
	}
	
	if(addDevice) {
		if(_type == ANCHOR) //for now let's start with 1 TAG
		{
			_networkDevicesNumber = 0;
		}
		memcpy(&_networkDevices[_networkDevicesNumber], device, sizeof(DW1000Device));
		_networkDevices[_networkDevicesNumber].setIndex(_networkDevicesNumber);
		// NEW: Initialize per-device protocol state
		_networkDevices[_networkDevicesNumber].resetProtocolState();
		_networkDevicesNumber++;
		return true;
	}
	
	return false;
}

void DW1000RangingClass::removeNetworkDevices(int16_t index) {
	//if we have just 1 element
	if(_networkDevicesNumber == 1) {
		_networkDevicesNumber = 0;
	}
	else if(index == _networkDevicesNumber-1) //if we delete the last element
	{
		_networkDevicesNumber--;
	}
	else {
		//we translate all the element wich are after the one we want to delete.
		for(int16_t i = index; i < _networkDevicesNumber-1; i++) { // TODO 8bit?
			memcpy(&_networkDevices[i], &_networkDevices[i+1], sizeof(DW1000Device));
			_networkDevices[i].setIndex(i);
		}
		_networkDevicesNumber--;
	}
}

/* ###########################################################################
 * #### Setters and Getters ##################################################
 * ######################################################################### */

//setters
void DW1000RangingClass::setReplyTime(uint16_t replyDelayTimeUs) { _replyDelayTimeUS = replyDelayTimeUs; }

void DW1000RangingClass::setResetPeriod(uint32_t resetPeriod) { _resetPeriod = resetPeriod; }


DW1000Device* DW1000RangingClass::searchDistantDevice(byte shortAddress[]) {
	//we compare the 2 bytes address with the others
	for(uint16_t i = 0; i < _networkDevicesNumber; i++) { // TODO 8bit?
		if(memcmp(shortAddress, _networkDevices[i].getByteShortAddress(), 2) == 0) {
			//we have found our device !
			return &_networkDevices[i];
		}
	}
	
	return nullptr;
}

DW1000Device* DW1000RangingClass::getDistantDevice() {
	//we get the device which correspond to the message which was sent (need to be filtered by MAC address)
	
	return &_networkDevices[_lastDistantDevice];
	
}

/* ###########################################################################
 * #### NEW: Multi-anchor support methods ###################################
 * ######################################################################### */

void DW1000RangingClass::processDeviceMessages() {
	// Process messages from the queue for each device in round-robin fashion
	MessageQueueItem item;
	
	if (dequeueMessage(&item)) {
		DW1000Device* device = searchDistantDevice(item.sourceAddress);
		if (device != nullptr) {
			processDeviceMessage(device, item.data, item.messageType);
		}
	}
}

void DW1000RangingClass::handleDeviceTimeout() {
	// Check each device for protocol timeouts
	for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if (_networkDevices[i].isProtocolTimedOut(2000)) { // 2 second timeout
			_networkDevices[i].handleProtocolTimeout();
			if (_handleProtocolError != 0) {
				(*_handleProtocolError)(&_networkDevices[i], -1); // -1 = timeout error
			}
		}
	}
}

boolean DW1000RangingClass::isAnyDeviceActive() {
	for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if (_networkDevices[i].isProtocolActive()) {
			return true;
		}
	}
	return false;
}

void DW1000RangingClass::resetAllDeviceStates() {
	for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
		_networkDevices[i].resetProtocolState();
	}
}

int DW1000RangingClass::getActiveDeviceCount() {
	int count = 0;
	for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if (_networkDevices[i].isProtocolActive()) {
			count++;
		}
	}
	return count;
}

/* ###########################################################################
 * #### NEW: Message queue methods ###########################################
 * ######################################################################### */

boolean DW1000RangingClass::enqueueMessage(byte data[], byte sourceAddress[], int messageType) {
	if (_queueCount >= MESSAGE_QUEUE_SIZE) {
		return false; // Queue full
	}
	
	// Copy message data
	memcpy(_messageQueue[_queueTail].data, data, LEN_DATA);
	memcpy(_messageQueue[_queueTail].sourceAddress, sourceAddress, 2);
	_messageQueue[_queueTail].messageType = messageType;
	_messageQueue[_queueTail].timestamp = millis();
	_messageQueue[_queueTail].processed = false;
	
	_queueTail = (_queueTail + 1) % MESSAGE_QUEUE_SIZE;
	_queueCount++;
	
	return true;
}

boolean DW1000RangingClass::dequeueMessage(MessageQueueItem* item) {
	if (_queueCount == 0) {
		return false; // Queue empty
	}
	
	// Copy message data
	memcpy(item, &_messageQueue[_queueHead], sizeof(MessageQueueItem));
	
	_queueHead = (_queueHead + 1) % MESSAGE_QUEUE_SIZE;
	_queueCount--;
	
	return true;
}

void DW1000RangingClass::clearMessageQueue() {
	_queueHead = 0;
	_queueTail = 0;
	_queueCount = 0;
}

/* ###########################################################################
 * #### Public methods #######################################################
 * ######################################################################### */

void DW1000RangingClass::checkForReset() {
	uint32_t curMillis = millis();
	// NEW: Check if any device has active protocol state
	if(!isAnyDeviceActive()) {
		// check if inactive
		if(curMillis-_lastActivity > _resetPeriod) {
			resetInactive();
		}
		return; // TODO cc
	}
}

void DW1000RangingClass::checkForInactiveDevices() {
	for(uint8_t i = 0; i < _networkDevicesNumber; i++) {
		if(_networkDevices[i].isInactive()) {
			if(_handleInactiveDevice != 0) {
				(*_handleInactiveDevice)(&_networkDevices[i]);
			}
			//we need to delete the device from the array:
			removeNetworkDevices(i);
			
		}
	}
}

// TODO check return type
int16_t DW1000RangingClass::detectMessageType(byte datas[]) {
	if(datas[0] == FC_1_BLINK) {
		return BLINK;
	}
	else if(datas[0] == FC_1 && datas[1] == FC_2) {
		//we have a long MAC frame message (ranging init)
		return datas[LONG_MAC_LEN];
	}
	else if(datas[0] == FC_1 && datas[1] == FC_2_SHORT) {
		//we have a short mac frame message (poll, range, range report, etc..)
		return datas[SHORT_MAC_LEN];
	}
}

void DW1000RangingClass::loop() {
	//we check if needed to reset !
	checkForReset();
	uint32_t time = millis(); // TODO other name - too close to "timer"
	if(time-timer > _timerDelay) {
		timer = time;
		timerTick();
	}
	
	// NEW: Process device messages and handle timeouts
	processDeviceMessages();
	handleDeviceTimeout();
	
	// The rest of the loop() method will be implemented in the next part...
	// This is getting quite long, so I'll continue in the next section
}

void DW1000RangingClass::useRangeFilter(boolean enabled) {
	_useRangeFilter = enabled;
}

void DW1000RangingClass::setRangeFilterValue(uint16_t newValue) {
	if (newValue < 2) {
		_rangeFilterValue = 2;
	}else{
		_rangeFilterValue = newValue;
	}
}


/* ###########################################################################
 * #### Private methods and Handlers for transmit & Receive reply ############
 * ######################################################################### */


void DW1000RangingClass::handleSent() {
	// NEW: Handle sent messages for per-device protocol state
	// We need to identify which device this transmission relates to and update timestamps
	
	int messageType = detectMessageType(data);
	
	if(messageType != POLL_ACK && messageType != POLL && messageType != RANGE)
		return;
	
	// A message was sent. We launch the ranging protocol when a message was sent
	if(_type == ANCHOR) {
		if(messageType == POLL_ACK) {
			DW1000Device* myDistantDevice = searchDistantDevice(_lastSentToShortAddress);
			
			if (myDistantDevice) {
				DW1000.getTransmitTimestamp(myDistantDevice->timePollAckSent);
				myDistantDevice->setSentAck(true);
			}
		}
	}
	else if(_type == TAG) {
		if(messageType == POLL) {
			DW1000Time timePollSent;
			DW1000.getTransmitTimestamp(timePollSent);
			// If the last device we sent the POLL to is broadcast:
			if(_lastSentToShortAddress[0] == 0xFF && _lastSentToShortAddress[1] == 0xFF) {
				// We save the value for all the devices!
				for(uint16_t i = 0; i < _networkDevicesNumber; i++) {
					_networkDevices[i].timePollSent = timePollSent;
					_networkDevices[i].setSentAck(true);
				}
			}
			else {
				// We search the device associated with the last sent address
				DW1000Device* myDistantDevice = searchDistantDevice(_lastSentToShortAddress);
				// We save the value just for one device
				if (myDistantDevice) {
					myDistantDevice->timePollSent = timePollSent;
					myDistantDevice->setSentAck(true);
				}
			}
		}
		else if(messageType == RANGE) {
			DW1000Time timeRangeSent;
			DW1000.getTransmitTimestamp(timeRangeSent);
			// If the last device we sent the RANGE to is broadcast:
			if(_lastSentToShortAddress[0] == 0xFF && _lastSentToShortAddress[1] == 0xFF) {
				// We save the value for all the devices!
				for(uint16_t i = 0; i < _networkDevicesNumber; i++) {
					_networkDevices[i].timeRangeSent = timeRangeSent;
					_networkDevices[i].setSentAck(true);
				}
			}
			else {
				// We search the device associated with the last sent address
				DW1000Device* myDistantDevice = searchDistantDevice(_lastSentToShortAddress);
				// We save the value just for one device
				if (myDistantDevice) {
					myDistantDevice->timeRangeSent = timeRangeSent;
					myDistantDevice->setSentAck(true);
				}
			}
		}
	}
}

void DW1000RangingClass::handleReceived() {
	// NEW: Instead of global _receivedAck, enqueue the message for processing
	// get message and parse
	DW1000.getData(data, LEN_DATA);
	
	int messageType = detectMessageType(data);
	byte sourceAddress[2];
	
	// Extract source address based on message type
	if(messageType == BLINK) {
		_globalMac.decodeBlinkFrame(data, nullptr, sourceAddress);
	} else if(messageType == RANGING_INIT) {
		_globalMac.decodeLongMACFrame(data, sourceAddress);
	} else {
		_globalMac.decodeShortMACFrame(data, sourceAddress);
	}
	
	// Enqueue message for processing
	enqueueMessage(data, sourceAddress, messageType);
}


void DW1000RangingClass::noteActivity() {
	// update activity timestamp, so that we do not reach "resetPeriod"
	_lastActivity = millis();
}

void DW1000RangingClass::resetInactive() {
	//if inactive
	if(_type == ANCHOR) {
		// NEW: Reset all device expected messages instead of global
		for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
			_networkDevices[i].setExpectedMessage(MSG_POLL);
		}
		receiver();
	}
	noteActivity();
}

void DW1000RangingClass::timerTick() {
	if(_networkDevicesNumber > 0 && counterForBlink != 0) {
		if(_type == TAG) {
			// NEW: Set expected message for all devices
			for (uint8_t i = 0; i < _networkDevicesNumber; i++) {
				_networkDevices[i].setExpectedMessage(MSG_POLL_ACK);
			}
			//send a prodcast poll
			transmitPoll(nullptr);
		}
	}
	else if(counterForBlink == 0) {
		if(_type == TAG) {
			transmitBlink();
		}
		//check for inactive devices if we are a TAG or ANCHOR
		checkForInactiveDevices();
	}
	counterForBlink++;
	if(counterForBlink > 20) {
		counterForBlink = 0;
	}
}


void DW1000RangingClass::copyShortAddress(byte address1[], byte address2[]) {
	*address1     = *address2;
	*(address1+1) = *(address2+1);
}

/* ###########################################################################
 * #### NEW: Per-device message processing ###################################
 * ######################################################################### */

void DW1000RangingClass::processDeviceMessage(DW1000Device* device, byte data[], int messageType) {
	// This method processes messages for a specific device
	// Implementation varies based on device type (anchor/tag) and message type
	
	// Handle special message types that don't require an existing device
	if (messageType == BLINK && _type == ANCHOR) {
		byte address[8];
		byte shortAddress[2];
		_globalMac.decodeBlinkFrame(data, address, shortAddress);
		// We create a new device with the tag
		DW1000Device myTag(address, shortAddress);
		
		if(addNetworkDevices(&myTag)) {
			if(_handleBlinkDevice != 0) {
				(*_handleBlinkDevice)(&myTag);
			}
			// We reply by transmitting ranging init message
			transmitRangingInit(&myTag);
			noteActivity();
		}
		return;
	}
	else if (messageType == RANGING_INIT && _type == TAG) {
		byte address[2];
		_globalMac.decodeLongMACFrame(data, address);
		// We create a new device with the anchor
		DW1000Device myAnchor(address, true);
		
		if(addNetworkDevices(&myAnchor, true)) {
			if(_handleNewDevice != 0) {
				(*_handleNewDevice)(&myAnchor);
			}
		}
		noteActivity();
		return;
	}
	
	// For other message types, we need an existing device
	if (device == nullptr) {
		// We don't have the short address of the device in memory
		if (DEBUG) {
			Serial.println("Device not found for message processing");
		}
		return;
	}
	
	if (_type == ANCHOR) {
		// Handle anchor-specific message processing
		handleDeviceProtocolState(device, messageType);
	} else if (_type == TAG) {
		// Handle tag-specific message processing  
		handleDeviceProtocolState(device, messageType);
	}
}

void DW1000RangingClass::handleDeviceProtocolState(DW1000Device* device, int messageType) {
	// Handle protocol state transitions for a specific device
	// This replaces the global protocol state machine with per-device state machines
	
	if (_type == ANCHOR) {
		// ANCHOR protocol state machine
		if (messageType != device->getExpectedMessage()) {
			// Unexpected message, start over again (except if already POLL)
			device->setProtocolFailed(true);
			if (_handleProtocolError != 0) {
				(*_handleProtocolError)(device, messageType);
			}
		}
		
		if (messageType == POLL) {
			// We receive a POLL which is a broadcast message
			// We need to grab info about it
			int16_t numberDevices = 0;
			memcpy(&numberDevices, data+SHORT_MAC_LEN+1, 1);
			
			for(uint16_t i = 0; i < numberDevices; i++) {
				// We need to test if this value is for us:
				// We grab the mac address of each device:
				byte shortAddress[2];
				memcpy(shortAddress, data+SHORT_MAC_LEN+2+i*4, 2);
				
				// We test if the short address is our address
				if(shortAddress[0] == _currentShortAddress[0] && shortAddress[1] == _currentShortAddress[1]) {
					// We grab the reply time which is for us
					uint16_t replyTime;
					memcpy(&replyTime, data+SHORT_MAC_LEN+2+i*4+2, 2);
					// We configure our reply time
					_replyDelayTimeUS = replyTime;
					
					// On POLL we (re-)start, so no protocol failure
					device->setProtocolFailed(false);
					device->setProtocolState(PROTOCOL_POLL_SENT);
					
					DW1000.getReceiveTimestamp(device->timePollReceived);
					// We note activity for our device
					device->noteActivity();
					device->noteProtocolActivity();
					// We indicate our next receive message for our ranging protocol
					device->setExpectedMessage(MSG_RANGE);
					transmitPollAck(device);
					noteActivity();
					
					return;
				}
			}
		}
		else if (messageType == RANGE) {
			// We receive a RANGE which is a broadcast message
			// We need to grab info about it
			uint8_t numberDevices = 0;
			memcpy(&numberDevices, data+SHORT_MAC_LEN+1, 1);
			
			for(uint8_t i = 0; i < numberDevices; i++) {
				// We need to test if this value is for us:
				// We grab the mac address of each device:
				byte shortAddress[2];
				memcpy(shortAddress, data+SHORT_MAC_LEN+2+i*17, 2);
				
				// We test if the short address is our address
				if(shortAddress[0] == _currentShortAddress[0] && shortAddress[1] == _currentShortAddress[1]) {
					// We grab the range data which is for us
					DW1000.getReceiveTimestamp(device->timeRangeReceived);
					noteActivity();
					device->noteActivity();
					device->noteProtocolActivity();
					device->setExpectedMessage(MSG_POLL);
					device->setProtocolState(PROTOCOL_RANGE_SENT);
					
					if(!device->getProtocolFailed()) {
						device->timePollSent.setTimestamp(data+SHORT_MAC_LEN+4+17*i);
						device->timePollAckReceived.setTimestamp(data+SHORT_MAC_LEN+9+17*i);
						device->timeRangeSent.setTimestamp(data+SHORT_MAC_LEN+14+17*i);
						
						// (re-)compute range as two-way ranging is done
						DW1000Time myTOF;
						computeRangeAsymmetric(device, &myTOF); // CHOSEN RANGING ALGORITHM
						
						float distance = myTOF.getAsMeters();
						
						if (_useRangeFilter) {
							// Skip first range
							if (device->getRange() != 0.0f) {
								distance = filterValue(distance, device->getRange(), _rangeFilterValue);
							}
						}
						
						device->setRXPower(DW1000.getReceivePower());
						device->setRange(distance);
						device->setFPPower(DW1000.getFirstPathPower());
						device->setQuality(DW1000.getReceiveQuality());
						
						// We send the range to TAG
						transmitRangeReport(device);
						device->setProtocolState(PROTOCOL_RANGE_REPORT_SENT);
						
						// We have finished our range computation. We send the corresponding handler
						_lastDistantDevice = device->getIndex();
						if(_handleNewRange != 0) {
							(*_handleNewRange)();
						}
						
						// Call range complete handler for multi-anchor support
						if(_handleRangeComplete != 0) {
							(*_handleRangeComplete)(device);
						}
					}
					else {
						transmitRangeFailed(device);
						device->setProtocolState(PROTOCOL_FAILED);
					}
					
					return;
				}
			}
		}
	}
	else if (_type == TAG) {
		// TAG protocol state machine
		if (messageType != device->getExpectedMessage()) {
			// Unexpected message, start over again
			device->setProtocolFailed(true);
			device->setExpectedMessage(MSG_POLL_ACK);
			if (_handleProtocolError != 0) {
				(*_handleProtocolError)(device, messageType);
			}
			return;
		}
		
		if (messageType == POLL_ACK) {
			DW1000.getReceiveTimestamp(device->timePollAckReceived);
			// We note activity for our device
			device->noteActivity();
			device->noteProtocolActivity();
			device->setProtocolState(PROTOCOL_POLL_ACK_SENT);
			
			// In the case the message comes from our last device:
			if(device->getIndex() == _networkDevicesNumber-1) {
				device->setExpectedMessage(MSG_RANGE_REPORT);
				// And transmit the next message (range) of the ranging protocol (in broadcast)
				transmitRange(nullptr);
			}
		}
		else if (messageType == RANGE_REPORT) {
			float curRange;
			memcpy(&curRange, data+1+SHORT_MAC_LEN, 4);
			float curRXPower;
			memcpy(&curRXPower, data+5+SHORT_MAC_LEN, 4);
			
			if (_useRangeFilter) {
				// Skip first range
				if (device->getRange() != 0.0f) {
					curRange = filterValue(curRange, device->getRange(), _rangeFilterValue);
				}
			}

			// We have a new range to save!
			device->setRange(curRange);
			device->setRXPower(curRXPower);
			device->noteActivity();
			device->noteProtocolActivity();
			device->setProtocolState(PROTOCOL_IDLE);
			
			// We can call our handler!
			// We have finished our range computation. We send the corresponding handler
			_lastDistantDevice = device->getIndex();
			if(_handleNewRange != 0) {
				(*_handleNewRange)();
			}
			
			// Call range complete handler for multi-anchor support
			if(_handleRangeComplete != 0) {
				(*_handleRangeComplete)(device);
			}
		}
		else if (messageType == RANGE_FAILED) {
			// Protocol failed for this device
			device->setProtocolFailed(true);
			device->setProtocolState(PROTOCOL_FAILED);
			device->setExpectedMessage(MSG_POLL_ACK);
			if (_handleProtocolError != 0) {
				(*_handleProtocolError)(device, messageType);
			}
		}
	}
}

/* ###########################################################################
 * #### Methods for ranging protocole   ######################################
 * ######################################################################### */

void DW1000RangingClass::transmitInit() {
	DW1000.newTransmit();
	DW1000.setDefaults();
}


void DW1000RangingClass::transmit(byte datas[]) {
	DW1000.setData(datas, LEN_DATA);
	DW1000.startTransmit();
}


void DW1000RangingClass::transmit(byte datas[], DW1000Time time) {
	DW1000.setDelay(time);
	DW1000.setData(data, LEN_DATA);
	DW1000.startTransmit();
}

void DW1000RangingClass::transmitBlink() {
	transmitInit();
	_globalMac.generateBlinkFrame(data, _currentAddress, _currentShortAddress);
	transmit(data);
}

void DW1000RangingClass::transmitRangingInit(DW1000Device* myDistantDevice) {
	transmitInit();
	//we generate the mac frame for a ranging init message
	_globalMac.generateLongMACFrame(data, _currentShortAddress, myDistantDevice->getByteAddress());
	//we define the function code
	data[LONG_MAC_LEN] = RANGING_INIT;
	
	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	
	transmit(data);
}

void DW1000RangingClass::transmitPoll(DW1000Device* myDistantDevice) {
	
	transmitInit();
	
	if(myDistantDevice == nullptr) {
		//we need to set our timerDelay:
		_timerDelay = DEFAULT_TIMER_DELAY+(uint16_t)(_networkDevicesNumber*3*DEFAULT_REPLY_DELAY_TIME/1000);
		
		byte shortBroadcast[2] = {0xFF, 0xFF};
		_globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
		data[SHORT_MAC_LEN]   = POLL;
		//we enter the number of devices
		data[SHORT_MAC_LEN+1] = _networkDevicesNumber;
		
		for(uint8_t i = 0; i < _networkDevicesNumber; i++) {
			//each devices have a different reply delay time.
			_networkDevices[i].setReplyTime((2*i+1)*DEFAULT_REPLY_DELAY_TIME);
			//we write the short address of our device:
			memcpy(data+SHORT_MAC_LEN+2+4*i, _networkDevices[i].getByteShortAddress(), 2);
			
			//we add the replyTime
			uint16_t replyTime = _networkDevices[i].getReplyTime();
			memcpy(data+SHORT_MAC_LEN+2+2+4*i, &replyTime, 2);
			
		}
		
		copyShortAddress(_lastSentToShortAddress, shortBroadcast);
		
	}
	else {
		//we redefine our default_timer_delay for just 1 device;
		_timerDelay = DEFAULT_TIMER_DELAY;
		
		_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
		
		data[SHORT_MAC_LEN]   = POLL;
		data[SHORT_MAC_LEN+1] = 1;
		uint16_t replyTime = myDistantDevice->getReplyTime();
		memcpy(data+SHORT_MAC_LEN+2, &replyTime, sizeof(uint16_t)); // todo is code correct?
		
		copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	}
	
	transmit(data);
}


void DW1000RangingClass::transmitPollAck(DW1000Device* myDistantDevice) {
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = POLL_ACK;
	// delay the same amount as ranging tag
	DW1000Time deltaTime = DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS);
	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	transmit(data, deltaTime);
}

void DW1000RangingClass::transmitRange(DW1000Device* myDistantDevice) {
	//transmit range need to accept broadcast for multiple anchor
	transmitInit();
	
	
	if(myDistantDevice == nullptr) {
		//we need to set our timerDelay:
		_timerDelay = DEFAULT_TIMER_DELAY+(uint16_t)(_networkDevicesNumber*3*DEFAULT_REPLY_DELAY_TIME/1000);
		
		byte shortBroadcast[2] = {0xFF, 0xFF};
		_globalMac.generateShortMACFrame(data, _currentShortAddress, shortBroadcast);
		data[SHORT_MAC_LEN]   = RANGE;
		//we enter the number of devices
		data[SHORT_MAC_LEN+1] = _networkDevicesNumber;
		
		// delay sending the message and remember expected future sent timestamp
		DW1000Time deltaTime     = DW1000Time(DEFAULT_REPLY_DELAY_TIME, DW1000Time::MICROSECONDS);
		DW1000Time timeRangeSent = DW1000.setDelay(deltaTime);
		
		for(uint8_t i = 0; i < _networkDevicesNumber; i++) {
			//we write the short address of our device:
			memcpy(data+SHORT_MAC_LEN+2+17*i, _networkDevices[i].getByteShortAddress(), 2);
			
			//we get the device which correspond to the message which was sent (need to be filtered by MAC address)
			_networkDevices[i].timeRangeSent = timeRangeSent;
			_networkDevices[i].timePollSent.getTimestamp(data+SHORT_MAC_LEN+4+17*i);
			_networkDevices[i].timePollAckReceived.getTimestamp(data+SHORT_MAC_LEN+9+17*i);
			_networkDevices[i].timeRangeSent.getTimestamp(data+SHORT_MAC_LEN+14+17*i);
		}
		
		copyShortAddress(_lastSentToShortAddress, shortBroadcast);
	}
	else {
		_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
		data[SHORT_MAC_LEN] = RANGE;
		// delay sending the message and remember expected future sent timestamp
		DW1000Time deltaTime = DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS);
		//we get the device which correspond to the message which was sent (need to be filtered by MAC address)
		myDistantDevice->timeRangeSent = DW1000.setDelay(deltaTime);
		myDistantDevice->timePollSent.getTimestamp(data+1+SHORT_MAC_LEN);
		myDistantDevice->timePollAckReceived.getTimestamp(data+6+SHORT_MAC_LEN);
		myDistantDevice->timeRangeSent.getTimestamp(data+11+SHORT_MAC_LEN);
		copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	}
	
	transmit(data);
}

void DW1000RangingClass::transmitRangeReport(DW1000Device* myDistantDevice) {
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = RANGE_REPORT;
	// write final ranging result
	float curRange   = myDistantDevice->getRange();
	float curRXPower = myDistantDevice->getRXPower();
	//We add the Range and then the RXPower
	memcpy(data+1+SHORT_MAC_LEN, &curRange, 4);
	memcpy(data+5+SHORT_MAC_LEN, &curRXPower, 4);
	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	transmit(data, DW1000Time(_replyDelayTimeUS, DW1000Time::MICROSECONDS));
}

void DW1000RangingClass::transmitRangeFailed(DW1000Device* myDistantDevice) {
	transmitInit();
	_globalMac.generateShortMACFrame(data, _currentShortAddress, myDistantDevice->getByteShortAddress());
	data[SHORT_MAC_LEN] = RANGE_FAILED;
	
	copyShortAddress(_lastSentToShortAddress, myDistantDevice->getByteShortAddress());
	transmit(data);
}

void DW1000RangingClass::receiver() {
	DW1000.newReceive();
	DW1000.setDefaults();
	// so we don't need to restart the receiver manually
	DW1000.receivePermanently(true);
	DW1000.startReceive();
}

/* ###########################################################################
 * #### Methods for range computation and corrections  #######################
 * ######################################################################### */

void DW1000RangingClass::computeRangeAsymmetric(DW1000Device* myDistantDevice, DW1000Time* myTOF) {
	// asymmetric two-way ranging (more computation intense, less error prone)
	DW1000Time round1 = (myDistantDevice->timePollAckReceived-myDistantDevice->timePollSent).wrap();
	DW1000Time reply1 = (myDistantDevice->timePollAckSent-myDistantDevice->timePollReceived).wrap();
	DW1000Time round2 = (myDistantDevice->timeRangeReceived-myDistantDevice->timePollAckSent).wrap();
	DW1000Time reply2 = (myDistantDevice->timeRangeSent-myDistantDevice->timePollAckReceived).wrap();
	
	myTOF->setTimestamp((round1*round2-reply1*reply2)/(round1+round2+reply1+reply2));
}

/* FOR DEBUGGING*/
void DW1000RangingClass::visualizeDatas(byte datas[]) {
	char string[60];
	sprintf(string, "%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X:%02X",
					datas[0], datas[1], datas[2], datas[3], datas[4], datas[5], datas[6], datas[7], datas[8], datas[9], datas[10], datas[11], datas[12], datas[13], datas[14], datas[15]);
	Serial.println(string);
}

/* ###########################################################################
 * #### Utils  ###############################################################
 * ######################################################################### */

float DW1000RangingClass::filterValue(float value, float previousValue, uint16_t numberOfElements) {
	float k = 2.0f / ((float)numberOfElements + 1.0f);
	return (value * k) + previousValue * (1.0f - k);
}
