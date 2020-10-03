/*
 * planner.c
 *
 *  Created on: 16 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"

#include "fsl_pit.h"
#include "fsl_clock.h"
#include "fsl_gpio.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* PIT clock = 66MHz */
#define PIT_IRQ_HANDLER 			PIT_IRQHandler
#define PIT_IRQ_ID 					PIT_IRQn
#define PIT_SOURCE_CLOCK 			CLOCK_GetFreq(kCLOCK_PerClk)

/*******************************************************************************
 * Variables
 ******************************************************************************/

pit_config_t pitConfig, pitConfig_Y;


//
// Timer clock ticks at 66 MHz
// Segments are calculated at a frequency of 10KHz
// A segment can have many steps
// Number of segment steps are 2 * steps since we are toggling signals
//
void HandlePIT_IRQ(pit_chnl_t c, cbuf_handle_t cbuf, axis_t * axis)
{

	stepper_buffer_t data;	// Data in buffer

	// Check if channel has caused the interrupt
	if(PIT_GetStatusFlags(PIT, c) == 1)
	{
		// Clear interrupt
		PIT_ClearStatusFlags(PIT, c, kPIT_TimerFlag);

		//  Toggle step pin
		GPIO_PortToggle	(axis->GPIO, (1 << axis->StepPin));

		// Check if steps left in segment
		if(axis->SegmenStepsLeft-- == 0)
		{
			// Load next timer value from ringbuffer if it is not empty
			if(circular_buf_get(cbuf, &data) == 0)
			{

				PIT_SetTimerPeriod(PIT, c, data.ticks);
				axis->SegmenStepsLeft = data.SegmentSteps;
			}
			// Disable timer and set move as ready
			else
			{
				axis->moveReady = 1;
				PIT_StopTimer(PIT, c);
			}

		}

	}
}

void PIT_IRQ_HANDLER(void)
{
	HandlePIT_IRQ(kPIT_Chnl_0, Axis_X_buffer, &Axis_X);
	HandlePIT_IRQ(kPIT_Chnl_1, Axis_Y_buffer, &Axis_Y);
}

//
// Send move to Head controler via CAN
void submitMoveHead(parser_block_t *block, char *message)
{

}

//
// Submit move on Base controller
void submitMoveBase(parser_block_t *block, char *message)
{
	BaseController.MoveReady = false;


	//Loop throu all segments in move and add to ring buffer

	/* Set timer period for channel 0 */
	PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, USEC_TO_COUNT(10000U, PIT_SOURCE_CLOCK));
	PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, USEC_TO_COUNT(10000U, PIT_SOURCE_CLOCK));


	PIT_StartTimer(PIT, kPIT_Chnl_0);
	PIT_StartTimer(PIT, kPIT_Chnl_1);
}

void AxisReady(void)
{
	if(Axis_X.moveReady && Axis_Y.moveReady) BaseController.MoveReady = true;
}
static void planner_thread(void *arg)
{
	extern QueueHandle_t xPlannerQueue;
	parser_block_t inbuff;
	char message[50];
	char xinbuff[50];
	// Set up timer
	PIT_GetDefaultConfig(&pitConfig);

	/* Init pit module */
	PIT_Init(PIT, &pitConfig);

	PIT_EnableInterrupts(PIT, kPIT_Chnl_0,  kPIT_TimerInterruptEnable);
	PIT_EnableInterrupts(PIT, kPIT_Chnl_1,  kPIT_TimerInterruptEnable);

	/* Enable at the NVIC */
	EnableIRQ(PIT_IRQ_ID);

	PRINTF("\r\n PIT_SOURCE_CLOCK is: %d \r\n", PIT_SOURCE_CLOCK);

	/* Enable timer interrupts for channel 0 */
	//PIT_EnableInterrupts(PIT, kPIT_Chnl_0, kPIT_TimerInterruptEnable);

	xPlannerQueue = xQueueCreate(10, sizeof(parser_block_t));
	if (xPlannerQueue == NULL)
	{
		PRINTF("Could not create xPlannerQueue");
	}
	vTaskDelay(1000);
	if (xPlannerQueue != NULL)
	{
		for (;;)
		{
			// Get new GCode line from GCode queue
			if (xQueueReceive(xPlannerQueue, &inbuff, portMAX_DELAY) == pdPASS)
			{
				//			printf("%s", inbuff);
				//			printf("\r\n");

				// Send command to involved controllers
				if (inbuff.controlers.Ctrl_Feeder2)
				{

				}
				if (inbuff.controlers.Ctrl_Feeder1)
				{

				}
				if (inbuff.controlers.Ctrl_Head)
				{
					submitMoveHead(&inbuff, &message);
				}
				if (inbuff.controlers.Ctrl_Base)
				{
					submitMoveBase(&inbuff, &message);
				}

				// Wait for all controllers to report ready
				while (!BaseController.MoveReady
						|| !HeadController.MoveReady
						|| !Feeder1Controller.MoveReady
						|| !Feeder2Controller.MoveReady)
				{
					vTaskDelay(1);
					AxisReady();
				}
			}
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
