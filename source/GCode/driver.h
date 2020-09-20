/*
 * driver.h
 *
 *  Created on: 12 sep. 2020
 *      Author: perra
 */

#ifndef GCODE_DRIVER_H_
#define GCODE_DRIVER_H_

// Define step pulse output pins.
#define X_STEP_PIN      (2u)
#define Y_STEP_PIN      (4u)
#define Z_STEP_PIN      (6u)
#define A_STEP_PIN      (6u)
#define B_STEP_PIN      (6u)
#define C_STEP_PIN      (6u)
#define D_STEP_PIN      (6u)
#define U_STEP_PIN      (6u)

// Define step direction output pins.
#define X_DIRECTION_PIN (3u)
#define Y_DIRECTION_PIN (5u)
#define Z_DIRECTION_PIN (7u)
#define A_DIRECTION_PIN (7u)
#define B_DIRECTION_PIN (7u)
#define C_DIRECTION_PIN (7u)
#define D_DIRECTION_PIN (7u)
#define U_DIRECTION_PIN (7u)


// Define stepper driver enable/disable output pin(s).
#define STEPPERS_ENABLE_PIN (10u)

// Define homing/hard limit switch input pins.
#define X_LIMIT_PIN     (20u)
#define Y_LIMIT_PIN     (21u)
#define Z_LIMIT_PIN     (22u)
#define U_LIMIT_PIN     (22u)



// Define spindle enable and spindle direction output pins.
#define SPINDLE_ENABLE_PIN      (12u)
#define SPINDLE_DIRECTION_PIN   (11u)
#define SPINDLEPWMPIN           (13u) // NOTE: only pin 12 or pin 13 can be assigned!

// Define flood and mist coolant enable output pins.
#define COOLANT_FLOOD_PIN   (19u)
#define COOLANT_MIST_PIN    (18u)

// Define user-control CONTROLs (cycle start, reset, feed hold, door) input pins.
#define RESET_PIN           (14u)
#define FEED_HOLD_PIN       (16u)
#define CYCLE_START_PIN     (17u)
#define SAFETY_DOOR_PIN     (29u)

// Define probe switch input pin.
#define PROBE_PIN           (15U)

#if EEPROM_ENABLE
#define I2C_PORT    4
#define I2C_SCL4    (24u) // Not used, for info only
#define I2C_SDA4    (25u) // Not used, for info only
#endif


#ifndef IOPORTS_ENABLE
#define IOPORTS_ENABLE 1
#endif

// The following struct is pulled from the Teensy Library core, Copyright (c) 2019 PJRC.COM, LLC.

typedef struct {
    const uint8_t pin;              // The pin number
    const uint32_t mux_val;         // Value to set for mux;
    volatile uint32_t *select_reg;  // Which register controls the selection
    const uint32_t select_val;      // Value for that selection
} pin_info_t;

#endif /* GCODE_DRIVER_H_ */
