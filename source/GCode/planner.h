/*
 * planner.h
 *
 *  Created on: 11 sep. 2020
 *      Author: perra
 */

#ifndef GCODE_PLANNER_H_
#define GCODE_PLANNER_H_

// The number of linear motions that can be in the plan at any give time
#ifndef BLOCK_BUFFER_SIZE
  #define BLOCK_BUFFER_SIZE 36
#endif

typedef union {
    uint32_t value;
    struct {
        uint16_t rapid_motion         :1,
                 system_motion        :1,
                 jog_motion           :1,
                 backlash_motion      :1,
                 no_feed_override     :1,
                 inverse_time         :1,
                 is_rpm_rate_adjusted :1,
                 is_rpm_pos_adjusted  :1,
                 is_laser_ppi_mode    :1,
                 unassigned           :7;
    };
} planner_cond_t;

// Planner data prototype. Must be used when passing new motions to the planner.
typedef struct {
    float feed_rate;                // Desired feed rate for line motion. Value is ignored, if rapid motion.
    spindle_t spindle;              // Desired spindle speed through line motion.
    planner_cond_t condition;       // Bitfield variable to indicate planner conditions. See defines above.
    gc_override_flags_t overrides;  // Block bitfield variable for overrides
    int32_t line_number;            // Desired line number to report when executing.
//    void *parameters;               // TODO: pointer to extra parameters, for canned cycles and threading?
    char *message;                  // Message to be displayed when block is executed.
    output_command_t *output_commands;
} plan_line_data_t;


void timer_init();

#endif /* GCODE_PLANNER_H_ */
