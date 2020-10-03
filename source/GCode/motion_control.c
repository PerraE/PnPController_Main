/*
 * motion_control.c
 *
 *  Created on: 13 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"
#include "FreeRTOS.h"

settings_t settings;

// Sets up valid jog motion received from g-code parser, checks for soft-limits, and executes the jog.
status_code_t mc_jog_execute (plan_line_data_t *pl_data, parser_block_t *gc_block)
{
    // Initialize planner data struct for jogging motions.
    // NOTE: Spindle and coolant are allowed to fully function with overrides during a jog.
    pl_data->feed_rate = gc_block->values.f;
    pl_data->condition.no_feed_override = On;
    pl_data->condition.jog_motion = On;
    pl_data->line_number = gc_block->values.n;

//    if(settings.limits.flags.jog_soft_limited)
//        system_apply_jog_limits(gc_block->values.xyz);
//    else
//    if (settings.limits.flags.soft_enabled && !system_check_travel_limits(gc_block->values.xyz))
//        return Status_TravelExceeded;

    // Valid jog command. Plan, set state, and execute.
//    mc_line(gc_block->values.xyz, pl_data);
//    if ((sys.state == STATE_IDLE || sys.state == STATE_TOOL_CHANGE) && plan_get_current_block() != NULL) { // Check if there is a block to execute.
//        set_state(STATE_JOG);
//        st_prep_buffer();
//        st_wake_up();  // NOTE: Manual start. No state machine required.
//    }

    return Status_OK;
}

// Distribute commands to respective controller

status_code_t mc_line(parser_block_t * gc_block)
{
	extern QueueHandle_t xPlannerQueue;

	xQueueSendToBack(xPlannerQueue, gc_block, 0 );
}
