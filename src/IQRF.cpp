/**
 * @file
 * @author Rostislav Špinar <rostislav.spinar@microrisc.com>
 * @author Roman Ondráček <ondracek.roman@centrum.cz>
 * @version 1.1
 *
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

#include "IQRF.h"

/**
 * Set PTYPE
 * @param PTYPE PTYPE
 */
void IQRF::setPTYPE(uint8_t PTYPE) {
	this->PTYPE = PTYPE;
}

/**
 * Get PTYPE
 * @return PTYPE
 */
uint8_t IQRF::getPTYPE() {
	return this->PTYPE;
}

/**
 * Set count of attepmts to send data
 * @param attepmts Count of attepmts to send data
 */
void IQRF::setAttepmtsCount(uint8_t attepmts) {
	this->attepmtsCounter = attepmts;
}

/**
 * Get count of attepmts to send data
 * @return Count of attepmts to send data 
 */
uint8_t IQRF::getAttepmtsCount() {
	return this->attepmtsCounter;
}


/**
 * Set byte count
 * @param count Byte count
 */
void IQRF::setByteCount(uint8_t count) {
	this->byteCounter = count;
}

/**
 * Get byte count
 * @return Byte count
 */
uint8_t IQRF::getByteCount() {
	return this->byteCounter;
}


/**
 * Set count of microseconds from counter
 * @param us Count of microseconds
 */
void IQRF::setUsCount0(unsigned long us) {
	this->usCounter0 = us;
}

/**
 * Get count of microseconds form counter
 * @return Count of microseconds
 */
unsigned long IQRF::getUsCount0() {
	return this->usCounter0;
}

/**
 * Set count of microseconds from counter
 * @param us Count of microseconds
 */
void IQRF::setUsCount1(unsigned long us) {
	this->usCounter1 = us;
}

/**
 * Get count of microseconds form counter
 * @return Count of microseconds
 */
unsigned long IQRF::getUsCount1() {
	return this->usCounter1;
}
