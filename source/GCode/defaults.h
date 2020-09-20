/*
 * defaults.h
 *
 *  Created on: 13 sep. 2020
 *      Author: perra
 */

#ifndef GCODE_DEFAULTS_H_
#define GCODE_DEFAULTS_H_

// At power-up or a reset, Grbl will check the limit switch states to ensure they are not active
// before initialization. If it detects a problem and the hard limits setting is enabled, Grbl will
// simply message the user to check the limits and enter an alarm state, rather than idle. Grbl will
// not throw an alarm message.
#define DEFAULT_CHECK_LIMITS_AT_INIT 0 // Default disabled. Set to 1 to enable.

#define DEFAULT_INVERT_LIMIT_PINS 0 // false
#define DEFAULT_SOFT_LIMIT_ENABLE 0 // false
#define DEFAULT_HARD_LIMIT_ENABLE 0  // false

#ifndef DISABLE_LIMIT_PINS_PULL_UP_MASK
#define DISABLE_LIMIT_PINS_PULL_UP_MASK 0
#endif
#ifndef INVERT_LIMIT_PIN_MASK
#define INVERT_LIMIT_PIN_MASK 0
#endif

#endif /* GCODE_DEFAULTS_H_ */
