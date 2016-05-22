/**
 * @file
 * @author Rostislav Špinar <rostislav.spinar@microrisc.com>
 * @version 1.0
 *
 * @section LICENSE
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

#include "iqrf_library.h"

/*
 * Locally used function prototypes
 */
uint8_t calculateCRC(void);
uint8_t checkCRC(void);
void TR_Module_OFF(void);
void TR_Module_ON(void);
void TR_Info_Task(void);
void TR_Control_Task(void);
void TR_process_id_packet_com(uint8_t pktId, uint8_t pktResult);
void TR_process_id_packet_pgm(void);
void TR_dummy_func_com(void);
void TR_dummy_func_pgm(uint8_t pktId, uint8_t pktResult);

/*
 * Public variable declarations
 */
/// SPI Tx buffer
uint8_t IQ_SPI_TxBuf[IQ_PKT_SIZE];
/// SPI Rx buffer
uint8_t IQ_SPI_RxBuf[IQ_PKT_SIZE];
uint8_t PTYPE;
/// SPI status
uint8_t spiStatus;
/// Number of attempts to send data
uint8_t repCnt;
/// Counts number of send/receive bytes
uint8_t tmpCnt;
/// Packet length
uint8_t packetLength;
uint8_t trInfoReading;
/// Data length
uint8_t dataLength;
uint8_t spiIqBusy;
/// SPI Master status
uint8_t spiMaster;
/// Fast SPI status
uint8_t fastSpi;
/// Actual Tx packet ID
uint8_t txPacketId;
/// Tx packet ID counter
uint8_t txPacketIdCounter;
/// TR control state
uint8_t TR_Control_TaskSM = trControlStatuses::READY;
/// TR programming flag
uint8_t TR_Control_ProgFlag;
unsigned long iqrfCheckMicros;
unsigned long iqrfMicros;
unsigned long iqrfSpiByteBytePause;
/// TR info structure
TR_INFO_STRUCT trInfoStruct;
/// IQRF packet buffer
IQRF_PACKET_BUFFER iqrfPacketBuffer[PACKET_BUFFER_SIZE];
/// Packet buffer
uint16_t iqrfPacketBufferInPtr, iqrfPacketBufferOutPtr;
/// IQRF Rx callback
IQRF_RX_CALL_BACK iqrf_rx_call_back_fn;
/// IQRF Tx callback
IQRF_TX_CALL_BACK iqrf_tx_call_back_fn;
/// Packet to end program mode
const uint8_t endPgmMode[] = {0xDE, 0x01, 0xFF};

/**
 * Function perform a TR-module driver initialization
 * Function performes initialization of trInfoStruct identification data structure
 * @param rx_call_back_fn Pointer to callback function. Function is called when the driver receives data from the TR module
 * @param tx_call_back_fn Pointer to callback function. unction is called when the driver sent data to the TR module
 */
void IQRF_Init(IQRF_RX_CALL_BACK rx_call_back_fn, IQRF_TX_CALL_BACK tx_call_back_fn) {
	spiIqBusy = 0;
	spiStatus = spiStatuses::DISABLED;
	iqrfCheckMicros = 0;
	// normal SPI communication
	fastSpi = 0;
	// set byte to byte pause to 1000us
	TR_SetByteToByteTime(1000);
	TR_Module_ON();
	pinMode(TR_SS_IO, OUTPUT);
	digitalWrite(TR_SS_IO, HIGH);
	SPI.begin();
	// enable SPI master function in driver
	IQRF_SPIMasterEnable();
	// read TR module info
	trInfoReading = 2;
	// wait for TR module ID reading
	while (trInfoReading) {
		// IQRF SPI communication driver
		IQRF_Driver();
		// TR module info reading task
		TR_Info_Task();
	}
	// if TR72D or TR76D is conected
	if (IQRF_GetModuleType() == trTypes::TR_72D || IQRF_GetModuleType() == trTypes::TR_76D) {
		// set fast SPI mode
		fastSpi = 1;
		// set byte to byte pause to 150us
		TR_SetByteToByteTime(150);
		Serial.println("IQRF_Init - set fast spi");
	}
	iqrf_rx_call_back_fn = rx_call_back_fn;
	iqrf_tx_call_back_fn = tx_call_back_fn;
}

/**
 * Periodically called IQRF_Driver
 */
void IQRF_Driver(void) {
	// SPI Master enabled
	if (spiMaster) {
		iqrfMicros = micros();
		// is anything to send in IQ_SPI_TxBuf?
		if (spiIqBusy != spiMasterStatuses::FREE) {
			// send 1 byte every defined time interval via SPI
			if ((iqrfMicros - iqrfCheckMicros) > iqrfSpiByteBytePause) {
				// reset counter
				iqrfCheckMicros = iqrfMicros;
				// send/receive 1 byte via SPI
				IQ_SPI_RxBuf[tmpCnt] = IQRF_SPI_Byte(IQ_SPI_TxBuf[tmpCnt]);
				// counts number of send/receive bytes, it must be zeroing on packet preparing
				tmpCnt++;
				// pacLen contains length of whole packet it must be set on packet preparing sent everything? + buffer overflow protection
				if (tmpCnt == packetLength || tmpCnt == IQ_PKT_SIZE) {
					// CS - deactive
					//digitalWrite(TR_SS_IO, HIGH);
					// CRC ok
					if ((IQ_SPI_RxBuf[dataLength + 3] == spiStatuses::CRCM_OK) && checkCRC()) {
						if (spiIqBusy == spiMasterStatuses::WRITE) {
							iqrf_tx_call_back_fn(txPacketId, txPacketStatuses::OK);
						}
						if (spiIqBusy == spiMasterStatuses::READ) {
							iqrf_rx_call_back_fn();
						}
						spiIqBusy = spiMasterStatuses::FREE;
					} else { // CRC error
						// rep_cnt - must be set on packet preparing
						if (--repCnt) {
							// another attempt to send data
							tmpCnt = 0;
						} else {
							if (spiIqBusy == spiMasterStatuses::WRITE) {
								iqrf_tx_call_back_fn(txPacketId, txPacketStatuses::ERROR);
							}
							spiIqBusy = spiMasterStatuses::FREE;
						}
					}
				}
			}
		} else { // no data to send => SPI status will be updated every 10ms
			if ((iqrfMicros - iqrfCheckMicros) > (MICRO_SECOND / 100)) {
				// reset counter
				iqrfCheckMicros = iqrfMicros;
				// get SPI status of TR module
				spiStatus = IQRF_SPI_Byte(spiCommands::CHECK);
				// CS - deactive
				//digitalWrite(TR_SS_IO, HIGH);      
				// if the status is dataready prepare packet to read it
				if ((spiStatus & 0xC0) == 0x40) {
					memset(IQ_SPI_TxBuf, 0, sizeof (IQ_SPI_TxBuf));
					// state 0x40 means 64B
					if (spiStatus == 0x40) {
						dataLength = 64;
					} else {
						// clear bit 7,6 - rest is length (1 az 63B)
						dataLength = spiStatus & 0x3F;
					}
					PTYPE = dataLength;
					IQ_SPI_TxBuf[0] = spiCommands::WR_RD;
					IQ_SPI_TxBuf[1] = PTYPE;
					// CRC
					IQ_SPI_TxBuf[dataLength + 2] = calculateCRC();
					// length of whole packet + (CMD, PTYPE, CRCM, 0)
					packetLength = dataLength + 4;
					// counter of sent bytes
					tmpCnt = 0;
					// number of attempts to send data
					repCnt = 1;
					// reading from buffer COM of TR module
					spiIqBusy = spiMasterStatuses::READ;
					// current SPI status must be updated
					spiStatus = spiStatuses::DATA_TRANSFER;
				}
				// if TR module ready and no data in module pending
				if (!spiIqBusy) {
					// check if packet to send ready
					if (iqrfPacketBufferInPtr != iqrfPacketBufferOutPtr) {
						memset(IQ_SPI_TxBuf, 0, sizeof (IQ_SPI_TxBuf));
						dataLength = iqrfPacketBuffer[iqrfPacketBufferOutPtr].dataLength;
						// PBYTE set bit7 - write to buffer COM of TR module
						PTYPE = (dataLength | 0x80);
						IQ_SPI_TxBuf[0] = iqrfPacketBuffer[iqrfPacketBufferOutPtr].spiCmd;
						if (IQ_SPI_TxBuf[0] == spiCommands::MODULE_INFO && dataLength == 16) {
							PTYPE = 0x10;
						}
						IQ_SPI_TxBuf[1] = PTYPE;
						memcpy(&IQ_SPI_TxBuf[2], iqrfPacketBuffer[iqrfPacketBufferOutPtr].pDataBuffer, dataLength);
						// CRCM
						IQ_SPI_TxBuf[dataLength + 2] = calculateCRC();
						// length of whole packet + (CMD, PTYPE, CRCM, 0)
						packetLength = dataLength + 4;
						// set actual TX packet ID
						txPacketId = iqrfPacketBuffer[iqrfPacketBufferOutPtr].pktId;
						// counter of sent bytes
						tmpCnt = 0;
						// number of attempts to send data
						repCnt = 3;
						// writing to buffer COM of TR module
						spiIqBusy = spiMasterStatuses::WRITE;
						if (iqrfPacketBuffer[iqrfPacketBufferOutPtr].unallocationFlag) {
							// unallocate temporary TX data buffer
							free(iqrfPacketBuffer[iqrfPacketBufferOutPtr].pDataBuffer);
						}
						if (++iqrfPacketBufferOutPtr >= PACKET_BUFFER_SIZE) {
							iqrfPacketBufferOutPtr = 0;
						}
						// current SPI status must be updated
						spiStatus = spiStatuses::DATA_TRANSFER;
					}
				}
			}
		}
	} else {
		// SPI master is disabled
		TR_Control_Task();
	}
}

/**
 * Function perform a TR-module reset
 */
void IQRF_TR_Reset(void) {
	// SPI Master enabled
	if (spiMaster) {
		// TR module OFF
		TR_Module_OFF();
		// RESET pause
		delay(100);
		// TR module ON
		TR_Module_ON();
		delay(1);
	} else {
		// TR module RESET process in SPI Master disable mode
		TR_Control_TaskSM = trControlStatuses::RESET;
		spiStatus = spiStatuses::BUSY;
	}
}

/**
 * Function switch TR-module to programming mode
 */
void IQRF_TR_EnterProgMode(void) {
	unsigned long enterP_millis;
	// SPI Master enabled
	if (spiMaster) {
		// SPI EE-TR OFF
		SPI.end();
		IQRF_TR_Reset();
		pinMode(TR_SS_IO, OUTPUT);
		// TR CS - must be low
		digitalWrite(TR_SS_IO, LOW);
		// TR SDO - output
		pinMode(TR_SDO_IO, OUTPUT);
		// TR SDI - input
		pinMode(TR_SDI_IO, INPUT);
		enterP_millis = millis();
		do {
			// copy SDI to SDO for approx. 500ms => TR into prog. mode
			digitalWrite(TR_SDO_IO, digitalRead(TR_SDI_IO));
		} while ((millis() - enterP_millis) < (MILLI_SECOND / 2));
		digitalWrite(TR_SS_IO, HIGH);
		SPI.begin();
	} else {
		TR_Control_TaskSM = trControlStatuses::RESET;
		TR_Control_ProgFlag = 1;
		spiStatus = spiStatuses::BUSY;
	}
}

/**
 * Function sends data from buffer to TR module
 * @param pDataBuffer Pointer to a buffer that contains data that I want to send to TR module
 * @param dataLength Number of bytes to send
 * @param unallocationFlag If the pDataBuffer is dynamically allocated using malloc function.
   If you wish to unallocate buffer after data is sent, set the unallocationFlag to 1, otherwise to 0.
 * @return TX packet ID (number 1-255)
 */
uint8_t IQRF_SendData(uint8_t *pDataBuffer, uint8_t dataLength, uint8_t unallocationFlag) {
	return TR_SendSpiPacket(spiCommands::WR_RD, pDataBuffer, dataLength, unallocationFlag);
}

/**
 * Function is usually called inside the callback function, whitch is called when the driver receives data from TR module
 * @param userDataBuffer Pointer to my buffer, to which I want to load data received from the TR module
 * @param rxDataSize Number of bytes I want to read
 */
void IQRF_GetRxData(uint8_t *userDataBuffer, uint8_t rxDataSize) {
	memcpy(userDataBuffer, &IQ_SPI_RxBuf[2], rxDataSize);
}

/**
 * Send and receive single byte over SPI
 * @param Tx_Byte Character to be send via SPI
 * @return Byte received via SPI
 */
uint8_t IQRF_SPI_Byte(uint8_t Tx_Byte) {
	uint8_t Rx_Byte = 0;
	digitalWrite(TR_SS_IO, LOW);
	delayMicroseconds(10);
	SPI.beginTransaction(SPISettings(IQRF_SPI_CLK, MSBFIRST, SPI_MODE0));
	Rx_Byte = SPI.transfer(Tx_Byte);
	SPI.endTransaction();
	delayMicroseconds(10);
	digitalWrite(TR_SS_IO, HIGH);
	return Rx_Byte;
}

/**
 * Read Module Info from TR module, uses SPI master implementation
 */
void TR_Info_Task(void) {
	static uint8_t dataToModule[16];
	static uint8_t attempts;
	static unsigned long timeoutMilli;
	static uint8_t idfMode;

	static enum {
		TR_INFO_INIT_TASK = 0,
		TR_INFO_ENTER_PROG_MODE,
		TR_INFO_SEND_REQUEST,
		TR_INFO_WAIT_INFO,
		TR_INFO_DONE
	} TR_Info_TaskSM = TR_INFO_INIT_TASK;
	switch (TR_Info_TaskSM) {
		case TR_INFO_INIT_TASK:
			// try enter to programming mode
			attempts = 1;
			// try to read idf in com mode
			idfMode = 0;
			// set call back function to process id data
			iqrf_rx_call_back_fn = TR_dummy_func_com;
			// set call back function after data were sent
			iqrf_tx_call_back_fn = TR_process_id_packet_com;
			trInfoStruct.mcuType = trMcuTypes::UNKNOWN;
			memset(&dataToModule[0], 0, 16);
			timeoutMilli = millis();
			// next state - will read info in PGM mode or /* in COM mode */
			TR_Info_TaskSM = TR_INFO_ENTER_PROG_MODE /* TR_INFO_SEND_REQUEST */;
			break;
		case TR_INFO_ENTER_PROG_MODE:
			// enter prog. mode
			IQRF_TR_EnterProgMode();
			// try to read idf in pgm mode
			idfMode = 1;
			// set call back function to process id data
			iqrf_rx_call_back_fn = TR_process_id_packet_pgm;
			// set call back function after data were sent
			iqrf_tx_call_back_fn = TR_dummy_func_pgm;
			timeoutMilli = millis();
			TR_Info_TaskSM = TR_INFO_SEND_REQUEST;
			break;
		case TR_INFO_SEND_REQUEST:
			// Only if the IQRF_Driver() is not busy and TR mudule is in communication mode packet preparing
			if (spiStatus == spiStatuses::COMMUNICATION_MODE && spiIqBusy == 0) {
				TR_SendSpiPacket(spiCommands::MODULE_INFO, &dataToModule[0], 16, 0);
				// initialize timeout timer
				timeoutMilli = millis();
				// next state
				TR_Info_TaskSM = TR_INFO_WAIT_INFO;
			} else {
				// only if the IQRF_Driver() is not busy and TR mudule is in programming mode packet preparing
				if (spiStatus == spiStatuses::PROGRAMMING_MODE && spiIqBusy == 0) {
					TR_SendSpiPacket(spiCommands::MODULE_INFO, &dataToModule[0], 1, 0);
					// initialize timeout timer
					timeoutMilli = millis();
					// next state
					TR_Info_TaskSM = TR_INFO_WAIT_INFO;
				} else {
					if (millis() - timeoutMilli >= MILLI_SECOND / 2) {
						// in a case, try it twice to enter programming mode
						if (attempts) {
							attempts--;
							TR_Info_TaskSM = TR_INFO_ENTER_PROG_MODE;
						} else {
							// TR module probably does not work
							TR_Info_TaskSM = TR_INFO_DONE;
						}
					}
				}
			}
			break;
			// wait for info data from TR module
		case TR_INFO_WAIT_INFO:
			if ((trInfoReading == 1) || (millis() - timeoutMilli >= MILLI_SECOND / 2)) {
				if (idfMode == 1) {
					// send end of PGM mode packet
					TR_SendSpiPacket(spiCommands::EEPROM_PGM, (uint8_t *) & endPgmMode[0], 3, 0);
				}
				// next state
				TR_Info_TaskSM = TR_INFO_DONE;
			}
			break;
			// the task is finished
		case TR_INFO_DONE:
			// if no packet is pending to send to TR module
			if (iqrfPacketBufferInPtr == iqrfPacketBufferOutPtr && spiIqBusy == 0) {
				trInfoReading = 0;
			}
			break;
	}
}

/**
 * Make TR module reset or switch to prog mode when SPI master is disabled
 */
void TR_Control_Task(void) {
	static unsigned long timeoutMilli;
	// TR control state machine
	switch (TR_Control_TaskSM) {
		case trControlStatuses::READY:
			// set SPI state DISABLED
			spiStatus = spiStatuses::DISABLED;
			TR_Control_ProgFlag = 0;
			break;
		case trControlStatuses::RESET:
			// set SPI state BUSY
			spiStatus = spiStatuses::BUSY;
			// SPI EE-TR OFF
			SPI.end();
			// SPI controller OFF
			// TR module OFF
			TR_Module_OFF();
			// read actual tick
			timeoutMilli = millis();
			// goto next state
			TR_Control_TaskSM = trControlStatuses::WAIT;
			break;
		case trControlStatuses::WAIT:
			// set SPI state BUSY
			spiStatus = spiStatuses::BUSY;
			// wait 300 ms
			if (millis() - timeoutMilli >= MILLI_SECOND / 3) {
				// TR module ON
				TR_Module_ON();
				if (TR_Control_ProgFlag) {
					// goto enter programming mode
					TR_Control_TaskSM = trControlStatuses::PROG_MODE;
				} else {
					digitalWrite(TR_SS_IO, HIGH);
					SPI.begin();
					// goto ready state
					TR_Control_TaskSM = trControlStatuses::READY;
				}
			}
			break;
		case trControlStatuses::PROG_MODE:
			// SPI EE-TR OFF
			SPI.end();
			IQRF_TR_Reset();
			pinMode(TR_SS_IO, OUTPUT);
			// TR CS - must be low
			digitalWrite(TR_SS_IO, LOW);
			// TR SDO - output
			pinMode(TR_SDO_IO, OUTPUT);
			// TR SDI - input
			pinMode(TR_SDI_IO, INPUT);
			timeoutMilli = millis();
			do {
				// copy SDI to SDO for approx. 500ms => TR into prog. mode
				digitalWrite(TR_SDO_IO, digitalRead(TR_SDI_IO));
			} while ((millis() - timeoutMilli) < (MILLI_SECOND / 2));
			digitalWrite(TR_SS_IO, HIGH);
			SPI.begin();
			// goto ready state
			TR_Control_TaskSM = trControlStatuses::READY;
			break;
	}
}

/**
 * Process identification data packet from TR module
 * @param pktId Packet ID
 * @param pktResult Operation result
 */
void TR_process_id_packet_com(uint8_t pktId, uint8_t pktResult) {
	TR_process_id_packet_pgm();
}

/**
 * Process identification data packet from TR module
 */
void TR_process_id_packet_pgm(void) {
	memcpy((uint8_t *) & trInfoStruct.moduleInfoRawData, (uint8_t *) & IQ_SPI_RxBuf[2], 8);
	trInfoStruct.moduleId = (uint32_t) IQ_SPI_RxBuf[2] << 24 | (uint32_t) IQ_SPI_RxBuf[3] << 16 | (uint32_t) IQ_SPI_RxBuf[4] << 8 | IQ_SPI_RxBuf[5];
	trInfoStruct.osVersion = (uint16_t) (IQ_SPI_RxBuf[6] / 16) << 8 | (IQ_SPI_RxBuf[6] % 16);
	trInfoStruct.mcuType = IQ_SPI_RxBuf[7] & 0x07;
	trInfoStruct.fcc = (IQ_SPI_RxBuf[7] & 0x08) >> 3;
	trInfoStruct.moduleType = IQ_SPI_RxBuf[7] >> 4;
	trInfoStruct.osBuild = (uint16_t) IQ_SPI_RxBuf[9] << 8 | IQ_SPI_RxBuf[8];
	// TR info data processed
	trInfoReading--;
}

/**
 * Function called after TR module identification request were sent
 * @param pktId Packet ID
 * @param pktResult Operation result
 */
void TR_dummy_func_pgm(uint8_t pktId, uint8_t pktResult) {
	__asm__("nop\n\t");
}

/**
 * Function called after TR module identification request were sent
 */
void TR_dummy_func_com(void) {
	__asm__("nop\n\t");
}

/**
 * Prepare SPI packet to packet buffer
 * @param spiCmd Command that I want to send to TR module
 * @param pDataBuffer Pointer to a buffer that contains data that I want to send to TR module
 * @param dataLength Number of bytes to send
 * @param unallocationFlag If the pDataBuffer is dynamically allocated using malloc function.
   If you wish to unallocate buffer after data is sent, set the unallocationFlag to 1, otherwise to 0.
 * @return Packet ID
 */
uint8_t TR_SendSpiPacket(uint8_t spiCmd, uint8_t *pDataBuffer, uint8_t dataLength, uint8_t unallocationFlag) {
	if (dataLength == 0) {
		return 0;
	}
	if (dataLength > IQ_PKT_SIZE - 4) {
		dataLength = IQ_PKT_SIZE - 4;
	}
	if ((++txPacketIdCounter) == 0) {
		txPacketIdCounter++;
	}
	iqrfPacketBuffer[iqrfPacketBufferInPtr].pktId = txPacketIdCounter;
	iqrfPacketBuffer[iqrfPacketBufferInPtr].spiCmd = spiCmd;
	iqrfPacketBuffer[iqrfPacketBufferInPtr].pDataBuffer = pDataBuffer;
	iqrfPacketBuffer[iqrfPacketBufferInPtr].dataLength = dataLength;
	iqrfPacketBuffer[iqrfPacketBufferInPtr].unallocationFlag = unallocationFlag;
	if (++iqrfPacketBufferInPtr >= PACKET_BUFFER_SIZE) {
		iqrfPacketBufferInPtr = 0;
	}
	return txPacketIdCounter;
}

/**
 * Calculate CRC before master's send
 * @return crc_val
 */
uint8_t calculateCRC(void) {
	unsigned char i, crc_val;
	crc_val = 0x5F;
	for (i = 0; i < (dataLength + 2); i++) {
		crc_val ^= IQ_SPI_TxBuf[i];
	}
	return crc_val;
}

/**
 * Confirm CRC from SPI slave upon received data
 * @return error code
 */
uint8_t checkCRC(void) {
	unsigned char i, crc_val;
	crc_val = 0x5F ^ PTYPE;
	for (i = 2; i < (dataLength + 2); i++) {
		crc_val ^= IQ_SPI_RxBuf[i];
	}
	if (IQ_SPI_RxBuf[dataLength + 2] == crc_val) {
		// CRCS ok
		return 1;
	} else {
		// CRCS error
		return 0;
	}
}

/**
 * Enter TR module into OFF state
 */
void TR_Module_OFF(void) {
	pinMode(TR_RESET_IO, OUTPUT);
	digitalWrite(TR_RESET_IO, HIGH);
}

/**
 * Enter TR module into ON state
 */
void TR_Module_ON(void) {
	pinMode(TR_RESET_IO, OUTPUT);
	digitalWrite(TR_RESET_IO, LOW);
}

/**
 * Set byte to byte pause is SPI driver
 * @param byteToByteTime byte to byte time in us
 */
void TR_SetByteToByteTime(uint16_t byteToByteTime) {
	iqrfSpiByteBytePause = byteToByteTime;
}