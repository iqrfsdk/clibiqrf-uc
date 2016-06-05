/**
 * Copyright 2015 MICRORISC s.r.o.
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
 */

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>
#include <IQRF.h>
#include <MsTimer2.h>

// 5000@1ms = 5s
#define USER_TIMER_PERIOD 5000

// LOCAL PROTOTYPES
void setup();
void loop();
void rxHandler();
void txHandler(uint8_t packetId, uint8_t packetResult);
void msTimerCallback();

// GLOBAL VARIABLES

/**
 * App data structure
 */
typedef struct {
	uint8_t rxBuffer[PACKET_SIZE];
	uint8_t *txBuffer;
	uint8_t packetId;
	volatile uint16_t timer;
	volatile bool timerAck;
} appVarsStruct;
appVarsStruct appVars;

/// Instances
IQRF* iqrf = new IQRF(rxHandler, txHandler);

// Const data
const uint8_t testBuffer[64] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64};

/**
 * Init peripherals
 */
void setup() {
	// User LED
	pinMode(13, OUTPUT);
	// Up - PC
	Serial.begin(9600);
	iqrf->begin();
	// Info - TR
	iqrf->identifyTR();
	MsTimer2::set(1, msTimerCallback);
	MsTimer2::start();
	// Clear variables
	memset(&appVars, 0, sizeof(appVarsStruct));
	appVars.timer = USER_TIMER_PERIOD;
	// Done here
	Serial.println("Peripherals and IQRF init done");
}

/**
 * Main loop
 */
void loop() {
	// TR module SPI comunication driver
	iqrf->driver();
	// Test send data every 5s
	if (appVars.timerAck) {
		// Allocate memory for Tx packet
		appVars.txBuffer = (uint8_t *) malloc(sizeof(testBuffer));
		if (appVars.txBuffer != NULL) {
			// Copy data from test to IQRF TX packet
			memcpy(appVars.txBuffer, (uint8_t *) & testBuffer, sizeof(testBuffer));
			// Send data and unallocate data buffer
			appVars.packetId = iqrf->sendData(appVars.txBuffer, sizeof(testBuffer), 1);
		}
		appVars.timerAck = false;
	}
}

/**
 * 1ms timer callback
 */
void msTimerCallback() {
	// app timer, call handler
	if (appVars.timer) {
		if ((--appVars.timer) == 0) {
			appVars.timerAck = true;
			appVars.timer = USER_TIMER_PERIOD;
		}
	}
}

/**
 * IQRF Rx callback
 */
void rxHandler() {
	// Read and print received data
	iqrf->getData(appVars.rxBuffer, iqrf->getDataLength());
	Serial.print("IQRF receive done: ");
	Serial.write(appVars.rxBuffer, iqrf->getDataLength());
	Serial.println();
}

/**
 * IQRF Tx callback
 * @param packetId Packet ID
 * @param packetResult Packet writing result
 */
void txHandler(uint8_t packetId, uint8_t packetResult) {
	Serial.println("IQRF send done");
}