/*
 * RTOSHelper.c
 *
 *  Created on: 6 sep. 2020
 *      Author: perra
 */

#include "FreeRTOS.h"

void RTOS_HeapLeft()
{
	 char dbg_msg[20];

	 UBaseType_t uxHighWaterMark;
	 uxHighWaterMark = uxTaskGetStackHighWaterMark( NULL );
	 sprintf(dbg_msg, "Stack left:%d\r\n", uxHighWaterMark);
	 printf(dbg_msg);
}


