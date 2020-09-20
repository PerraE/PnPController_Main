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
#include "circular_buffer.h"
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
    uint16_t SegmenStepsLeft;  		// Number of steps for segment
    uint8_t  moveReady;				// Signal that axis move is ready
} axis_t;


// Circular buffer for steppers
typedef struct {
    uint32_t ticks;         		// Number of ticks delay
    uint16_t SegmentSteps;  		// Number of steps for segment
} stepper_buffer_t;

#define AXIS_BUFFER_SIZE 50
extern stepper_buffer_t * Axis_X_buffer;
extern stepper_buffer_t * Axis_Y_buffer;
extern cbuf_handle_t cbufX;
extern cbuf_handle_t cbufY;
extern axis_t Axis_X;
extern axis_t Axis_Y;


#endif /* PNPCONTOLLER_MAIN_H_ */
