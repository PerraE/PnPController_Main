/*
 * PnPContoller_Main.h
 *
 *  Created on: 8 sep. 2020
 *      Author: perra
 */

#ifndef PNPCONTOLLER_MAIN_H_
#define PNPCONTOLLER_MAIN_H_

// Define standard libraries used by Grbl.
#include <math.h>
#include <inttypes.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include <assert.h>

// Define the Grbl system include files. NOTE: Do not alter organization.
#include "config.h"
#include "nuts_bolts.h"
#include "settings.h"
#include "hal.h"
#include "system.h"
#include "defaults.h"

//#include "coolant_control.h"
//#include "eeprom.h"
//#include "eeprom_emulate.h"
#include "gcode.h"
#include "limits.h"
#include "planner.h"
#include "motion_control.h"
//#include "protocol.h"
//#include "state_machine.h"
//#include "report.h"
//#include "spindle_control.h"
//#include "stepper.h"
//#include "system.h"
//#include "override.h"
//#include "sleep.h"
//#include "stream.h"

// Axis structure
typedef struct {
	GPIO_Type* GPIO;				// GPIO for axis
	uint32_t StepPin;
	uint32_t DirPin;
	uint32_t EnablePin;
    uint16_t SegmenStepsLeft;  		// Number of steps for segment
    uint8_t  moveReady;				// Signal that axis move is ready
    uint8_t  AxisNum;
} axis_t;


// Circular buffer for steppers
typedef struct {
    uint32_t ticks;         		// Number of ticks delay
    uint16_t SegmentSteps;  		// Number of steps for segment
} stepper_buffer_t;

// Controller signals
typedef struct {
    bool MoveReady;         		// Signal that move is ready to sync controllers
} controllerBoard_t;



extern controllerBoard_t BaseController;
extern controllerBoard_t HeadController;
extern controllerBoard_t Feeder1Controller;
extern controllerBoard_t Feeder2Controller;

#endif /* PNPCONTOLLER_MAIN_H_ */
