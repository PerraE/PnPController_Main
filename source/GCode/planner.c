/*
 * planner.c
 *
 *  Created on: 16 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"

#include "fsl_pit.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* PIT clock = 66MHz */
#define PIT_IRQ_HANDLER 			PIT_IRQHandler
#define PIT_IRQ_ID 					PIT_IRQn


/*******************************************************************************
 * Variables
 ******************************************************************************/

pit_config_t pitConfig, pitConfig_Y;


void HandlePIT_IRQ(pit_chnl_t c, cbuf_handle_t cbuf, axis_t * axis)
{
							// Ticks at 66 MHz
	stepper_buffer_t data;	// Data in buffer

	// Check if channel has caused the interrupt
	if(PIT_GetCurrentTimerCount(PIT, c) == 0)
	{
		//
		PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

		// Check if steps left in segment
		if(axis->SegmenStepsLeft-- == 0)
		{
			// Load next timer value from ringbuffer if it is not empty
			if(circular_buf_get(cbuf, &data) == 0)
			{

				PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, data.ticks);
				axis->SegmenStepsLeft = data.SegmentSteps;
			}
			// Disable timer and set move as ready
			else
			{
				axis->moveReady = 1;
				PIT_DisableInterrupts(PIT, c,  kPIT_TimerInterruptEnable);
			}

		}

	}
}

void PIT_IRQ_HANDLER(void)
{
	HandlePIT_IRQ(kPIT_Chnl_0, Axis_X_buffer, &Axis_X);
	HandlePIT_IRQ(kPIT_Chnl_1, Axis_Y_buffer, &Axis_Y);
}

void submitMove(char *block, char *message)
{

}


static void
planner_thread(void *arg)
{
	extern QueueHandle_t xPlannerQueue;
	char inbuff[50];
	char message[50];

	// Set up timer
	PIT_GetDefaultConfig(&pitConfig);

    /* Init pit module */
    PIT_Init(PIT, &pitConfig);
    /* Enable at the NVIC */
    EnableIRQ(PIT_IRQ_ID);

    /* For command loop */
    /* Set timer period for channel 0 */
   // PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(1000000U, PIT_SOURCE_CLOCK));
    /* Enable timer interrupts for channel 0 */
    PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);


    xPlannerQueue = xQueueCreate(10, 50);
	if( xPlannerQueue == NULL )
	{
	PRINTF("Could not create xPlannerQueue");
	}
	vTaskDelay(1000);
	if( xPlannerQueue != NULL )
	{
	 for( ;; )
	 {
		// Get new GCode line from queue
		if (xQueueReceive(xPlannerQueue, &inbuff, portMAX_DELAY ) == pdPASS)
		{
	//			printf("%s", inbuff);
	//			printf("\r\n");
			submitMove(&inbuff, &message);
		}

		vTaskDelay(1);
	 }
	}
}


/*-----------------------------------------------------------------------------------*/
void
planner_init(void)
{
  sys_thread_new("planner_thread", planner_thread, NULL, 1000, 9);
}
/*-----------------------------------------------------------------------------------*/
