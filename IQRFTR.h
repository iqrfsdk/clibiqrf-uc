/**
 * @file
 * @author Rostislav Špinar <rostislav.spinar@microrisc.com>
 * @author Roman Ondráček <ondracek.roman@centrum.cz>
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

#ifndef IQRFTR_H
#define IQRFTR_H

#include <Arduino.h>
#include <SPI.h>

#include "IQRFSettings.h"
#include "IQRFSPI.h"

/**
 * IQRF TR
 */
class IQRFTR {
public:
	void reset();
	void enterProgramMode();
	void turnOn();
	void turnOff();
	uint8_t getControlStatus();
	void setControlStatus(uint8_t status);
	void enableProgramFlag();
	void disableProgramFlag();
	bool getProgramFlag();
	void controlTask();
	/**
	 * TR control statuses
	 */
	enum controlStatuses {
		READY = 0, //!< TR ready state
		RESET = 1, //!< TR reset process
		WAIT = 2, //!< TR wait state
		PROG_MODE = 3 //!< TR programming mode
	};
private:
	uint8_t controlStatus;
	bool programFlag;
	IQRFSPI* spi = new IQRFSPI;
};

#endif

