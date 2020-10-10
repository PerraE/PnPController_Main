/*
 * planner.c
 *
 *  Created on: 16 sep. 2020
 *      Author: perra
 */

#include "PnPContoller_Main.h"
#include "circular_buffer.h"

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


#define AXIS_BUFFER_SIZE 10000

/*******************************************************************************
 * Variables
 ******************************************************************************/


pit_config_t pitConfig;

uint32_t * Axis_X_buffer;
uint32_t * Axis_Y_buffer;
axis_t Axis_X;
axis_t Axis_Y;

/*******************************************************************************
 * SDRAM
 ******************************************************************************/

cbuf_handle_t cbufX;
cbuf_handle_t cbufY;



//
// Timer clock ticks at 66 MHz
//

void HandlePIT_IRQ(pit_chnl_t c, cbuf_handle_t cbuf, axis_t * axis)
{

	uint32_t data;	// Data in buffer
	bool x;

	// Check if channel has caused the interrupt
	if(PIT_GetStatusFlags(PIT, c) == 1)
	{
		// Disable timer and set move as ready if target reached
		if(axis->ActualPos == axis->TargetPos)
		{
			PIT_StopTimer(PIT, c);
			printf("StopTimer\r\n");
			circular_buf_reset(cbuf);
			axis->moveReady = true;
		}
		else
		{
			GPIO_PinWrite(axis->GPIO, axis->StepPin, 1U);
			axis->ActualPos ++;

			// Set new value for step time
			// We are using same as previous if nothing in queue
			if(circular_buf_get(cbuf, &data)==0)
			{
				PIT_SetTimerPeriod(PIT, c, data);
			}

			GPIO_PinWrite(axis->GPIO, axis->StepPin, 0U);
		}

		// Clear interrupt
		PIT_ClearStatusFlags(PIT, c, kPIT_TimerFlag);

	}
}

void PIT_IRQ_HANDLER(void)
{
	HandlePIT_IRQ(kPIT_Chnl_0, cbufX, &Axis_X);
	HandlePIT_IRQ(kPIT_Chnl_1, cbufY, &Axis_Y);
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

	uint32_t ticksForStep, i;
	BaseController.MoveReady = false;
	Axis_X.moveReady = false;
	Axis_Y.moveReady = true;

	//Loop throu all segments in move and add to ring buffer
	// Min=3us = 198, 2us = 132
	// Min time for routine is 350 ->
	ticksForStep = USEC_TO_COUNT(2, PIT_SOURCE_CLOCK);
	//circular_buf_put(cbufX, 0U);

	Axis_X.TargetPos += 250000;
	for(i=0; i<250000; i++)
	{
		while(circular_buf_full(cbufX))
		{
			vTaskDelay(1);
//			PIT_StartTimer(PIT, kPIT_Chnl_0);
		}
//		ticksForStep = (i*10) + (1000 * block->values.xyz[0]);
		ticksForStep = block->values.xyz[0];
		if(i==0)
		{
			/* Set timer period and start timer for first segment */
			PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, ticksForStep);
			PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, ticksForStep);

			PIT_StartTimer(PIT, kPIT_Chnl_0);
			//PIT_StartTimer(PIT, kPIT_Chnl_1);
		}
		else
		{
			circular_buf_put(cbufX, ticksForStep);
		}
	}

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
//	char xinbuff[50];


	// Init buffer för steppers
#ifdef useSDRAM
	Axis_X_buffer = (uint32_t *)EXAMPLE_SEMC_START_ADDRESS; /* SDRAM start address. */
	Axis_Y_buffer = (uint32_t *)EXAMPLE_SEMC_START_ADDRESS + (AXIS_BUFFER_SIZE * sizeof(uint32_t));
	cbufX = SDRAM_buf_init(AXIS_BUFFER_SIZE);
	cbufY = SDRAM_buf_init(AXIS_BUFFER_SIZE);
#else
//	Axis_X_buffer  = malloc(AXIS_BUFFER_SIZE * sizeof(uint32_t));
//	Axis_Y_buffer  = malloc(AXIS_BUFFER_SIZE * sizeof(uint32_t));

	AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t Axis_X_buffer[AXIS_BUFFER_SIZE], 64U);
	AT_NONCACHEABLE_SECTION_ALIGN(static uint32_t Axis_Y_buffer[AXIS_BUFFER_SIZE], 64U);

	cbufX = circular_buf_init(Axis_X_buffer, AXIS_BUFFER_SIZE);
	cbufY = circular_buf_init(Axis_Y_buffer, AXIS_BUFFER_SIZE);
#endif

	// Init Axis
	Axis_X.GPIO = GPIO1;
	Axis_X.StepPin  = 18;
	Axis_X.AxisNum = 1;
	Axis_X.ActualPos = 0;
	Axis_X.TargetPos = 0;
	Axis_X.DirectionForward = true;

	// Set up timer
	PIT_GetDefaultConfig(&pitConfig);

	/* Init pit module */
	PIT_Init(PIT, &pitConfig);

	PIT_EnableInterrupts(PIT, kPIT_Chnl_0,  kPIT_TimerInterruptEnable);
	PIT_EnableInterrupts(PIT, kPIT_Chnl_1,  kPIT_TimerInterruptEnable);

	/* Enable at the NVIC */
	EnableIRQ(PIT_IRQ_ID);

	PRINTF("\r\n PIT_SOURCE_CLOCK is: %d \r\n", PIT_SOURCE_CLOCK);

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
					printf("start\r\n");
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

				printf("klar\r\n");
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
