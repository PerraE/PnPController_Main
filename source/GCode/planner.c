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

#define EXAMPLE_SEMC_START_ADDRESS (0x80000000U)
#define AXIS_BUFFER_SIZE 300
//#define useSDRAM

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

#ifdef useSDRAM

typedef struct  {
	uint32_t head;
	uint32_t tail;
	uint32_t max; //of the buffer
	bool full;
} sdram_buf_t;

typedef sdram_buf_t * sdram_handle_t;

sdram_handle_t cbufX;
sdram_handle_t cbufY;

static void SDRAM_buf_advance(sdram_handle_t cbuf)
{

	if(cbuf->full)
    {
        cbuf->tail = (cbuf->tail + 1) % cbuf->max;
    }

	cbuf->head = (cbuf->head + 1) % cbuf->max;

	// We mark full because we will advance tail on the next time around
	cbuf->full = (cbuf->head == cbuf->tail);
}

static void SDRAM_buf_retreat(sdram_handle_t cbuf)
{
	cbuf->full = false;
	cbuf->tail = (cbuf->tail + 1) % cbuf->max;
}

void SDRAM_buf_reset(sdram_handle_t cbuf)
{
    cbuf->head = 0;
    cbuf->tail = 0;
    cbuf->full = false;
}
sdram_handle_t SDRAM_buf_init(uint32_t size)
{
	sdram_handle_t cbuf = malloc(sizeof(sdram_buf_t));
	cbuf->max = size;
	SDRAM_buf_reset(cbuf);

	return cbuf;
}

void SDRAM_buf_put(sdram_handle_t cbuf, uint8_t axisNum, uint32_t value)
{
    uint32_t index;
    index = cbuf->head;

    if(axisNum == 1)
	{
		Axis_X_buffer[index] = value;
	}
	else
	{
		Axis_Y_buffer[index] = value;
	}
    SDRAM_buf_advance(cbuf);
}
bool SDRAM_buf_empty(sdram_handle_t cbuf)
{
    return (!cbuf->full && (cbuf->head == cbuf->tail));
}
int SDRAM_buf_get(sdram_handle_t cbuf, uint8_t axisNum, uint32_t *value)
{

    int r = -1;
    uint32_t index;
    index = cbuf->tail;

    if(!SDRAM_buf_empty(cbuf))
    {
    	if(axisNum == 1)
    	{
    		*value = Axis_X_buffer[index];
    	}
    	else
    	{
    		*value = Axis_Y_buffer[index];
    	}
        SDRAM_buf_retreat(cbuf);
        r = 0;
    }
    return r;
}

#else
cbuf_handle_t cbufX;
cbuf_handle_t cbufY;
#endif


//
// Timer clock ticks at 66 MHz
// Segments are calculated at a frequency of 10KHz
// A segment can have many steps
// Number of segment steps are 2 * steps since we are toggling signals
//
#ifdef useSDRAM
void HandlePIT_IRQ(pit_chnl_t c, sdram_handle_t cbuf, axis_t * axis)
#else
void HandlePIT_IRQ(pit_chnl_t c, cbuf_handle_t cbuf, axis_t * axis)
#endif
{

	uint32_t data;	// Data in buffer

	// Check if channel has caused the interrupt
	if(PIT_GetStatusFlags(PIT, c) == 1)
	{

		// Check if steps left in segment
//		if(axis->SegmenStepsLeft-- == 0)
		{
			// Load next timer value from ringbuffer if it is not empty
#ifdef useSDRAM
			if(SDRAM_buf_get(cbuf, axis->AxisNum, &data) == 0)
#else
			if(circular_buf_get(cbuf, &data)==0)
#endif
			{
				PIT_SetTimerPeriod(PIT, c, data);
				//  Toggle step pin if data != 0. First post is a dummy to start timer
				if(data != 0)
				{
					GPIO_PortToggle	(axis->GPIO, (1 << axis->StepPin));
					Axis_X.ActualPos ++;
				}
			}
			// Disable timer and set move as ready
			if(Axis_X.ActualPos == Axis_X.TargetPos)
			{
				PIT_StopTimer(PIT, c);
				printf("StopTimer\r\n");
				axis->moveReady = 1;
			}

		}

		// Clear interrupt
		PIT_ClearStatusFlags(PIT, c, kPIT_TimerFlag);

	}
}

void PIT_IRQ_HANDLER(void)
{
#ifdef useSDRAM
	HandlePIT_IRQ(kPIT_Chnl_0, cbufX, &Axis_X);
	HandlePIT_IRQ(kPIT_Chnl_1, cbufY, &Axis_Y);
#else
	HandlePIT_IRQ(kPIT_Chnl_0, cbufX, &Axis_X);
	HandlePIT_IRQ(kPIT_Chnl_1, cbufY, &Axis_Y);
#endif
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

//    uint32_t index;
//    uint32_t *sdram  = (uint32_t *)EXAMPLE_SEMC_START_ADDRESS; /* SDRAM start address. */
//    bool result      = true;
//    uint32_t sdram_writeBuffer[1000];
//    uint32_t sdram_readBuffer[1000];
//    /* Prepare data and write to SDRAM. */
//    for (index = 0; index < 1000; index++)
//    {
//        sdram_writeBuffer[index] = index;
//        sdram[index]             = sdram_writeBuffer[index];
//    }
//    /* Read data from the SDRAM. */
//    for (index = 0; index < 1000; index++)
//    {
//        sdram_readBuffer[index] = sdram[index];
//    }


	uint32_t ticksForStep, i;
	BaseController.MoveReady = false;
	Axis_X.moveReady = 1;
	Axis_Y.moveReady = 1;

	//Loop throu all segments in move and add to ring buffer
	// Min=3us = 198
	// Min time for routine is 350 ->
	ticksForStep = USEC_TO_COUNT(3, PIT_SOURCE_CLOCK);

#ifdef useSDRAM
	SDRAM_buf_put(cbufX, Axis_X.AxisNum, 0U);
#else
	circular_buf_put(cbufX, 0U);
#endif

	/* Set timer period for channel 0 */
	PIT_SetTimerPeriod(PIT, kPIT_Chnl_0, ticksForStep);
	PIT_SetTimerPeriod(PIT, kPIT_Chnl_1, ticksForStep);

	PIT_StartTimer(PIT, kPIT_Chnl_0);
	//PIT_StartTimer(PIT, kPIT_Chnl_1);
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

#ifdef useSDRAM
		SDRAM_buf_put(cbufX, Axis_X.AxisNum, ticksForStep);
#else
		circular_buf_put(cbufX, ticksForStep);
#endif
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
	Axis_X_buffer  = malloc(AXIS_BUFFER_SIZE * sizeof(uint32_t));
	Axis_Y_buffer  = malloc(AXIS_BUFFER_SIZE * sizeof(uint32_t));
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
					printf("klar\r\n");
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
