/*
 * config.h
 *
 *  Created on: 11 sep. 2020
 *      Author: perra
 */

#ifndef CONFIG_H_
#define CONFIG_H_

#include "nuts_bolts.h"
// Number of decimal places (scale) output by Grbl for certain value types. These settings
// are determined by realistic and commonly observed values in CNC machines. For example, position
// values cannot be less than 0.001mm or 0.0001in, because machines can not be physically more
// precise this. So, there is likely no need to change these, but you can if you need to here.
// NOTE: Must be an integer value from 0 to ~4. More than 4 may exhibit round-off errors.
#define N_DECIMAL_COORDVALUE_INCH 4 // Coordinate or position value in inches
#define N_DECIMAL_COORDVALUE_MM   3 // Coordinate or position value in mm
#define N_DECIMAL_RATEVALUE_INCH  1 // Rate or velocity value in in/min
#define N_DECIMAL_RATEVALUE_MM    0 // Rate or velocity value in mm/min
#define N_DECIMAL_SETTINGVALUE    3 // Floating point setting values
#define N_DECIMAL_RPMVALUE        0 // RPM value in rotations per min
#define N_DECIMAL_PIDVALUE        3 // PID value

// Time delay increments performed during a dwell. The default value is set at 50ms, which provides
// a maximum time delay of roughly 55 minutes, more than enough for most any application. Increasing
// this delay will increase the maximum dwell time linearly, but also reduces the responsiveness of
// run-time command executions, like status reports, since these are performed between each dwell
// time step.
#define DWELL_TIME_STEP 50 // Integer (1-255) (milliseconds)

// Define realtime command special characters. These characters are 'picked-off' directly from the
// serial read data stream and are not passed to the grbl line execution parser. Select characters
// that do not and must not exist in the streamed g-code program. ASCII control characters may be
// used, if they are available per user setup. Also, extended ASCII codes (>127), which are never in
// g-code programs, maybe selected for interface programs.
// NOTE: If changed, manually update help message in report.c.

#define CMD_EXIT 0x03 // ctrl-C
#define CMD_RESET 0x18 // ctrl-X
#define CMD_STOP 0x19 // ctrl-Y
#define CMD_STATUS_REPORT_LEGACY '?'
#define CMD_CYCLE_START_LEGACY '~'
#define CMD_FEED_HOLD_LEGACY '!'
#define CMD_PROGRAM_DEMARCATION '%'


// System motion line numbers must be zero.
#define JOG_LINE_NUMBER 0


#endif /* CONFIG_H_ */
