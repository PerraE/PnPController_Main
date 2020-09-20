/*
 * motion_control.h
 *
 *  Created on: 13 sep. 2020
 *      Author: perra
 */

#ifndef GCODE_MOTION_CONTROL_H_
#define GCODE_MOTION_CONTROL_H_

// Sets up valid jog motion received from g-code parser, checks for soft-limits, and executes the jog.
status_code_t mc_jog_execute(plan_line_data_t *pl_data, parser_block_t *gc_block);

#endif /* GCODE_MOTION_CONTROL_H_ */
