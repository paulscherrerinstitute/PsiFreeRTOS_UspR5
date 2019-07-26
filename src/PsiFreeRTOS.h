#pragma once
/*******************************************************************************************
 * Includes
 *******************************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>

/*******************************************************************************************
 * Types
 *******************************************************************************************/
typedef enum {
	PsiFreeRTOS_FatalReason_StackOvervflow = 1,
	PsiFreeRTOS_FatalReason_MallocFailed = 2,
	PsiFreeRTOS_FatalReason_InfiniteLoop = 3,
} PsiFreeRTOS_FatalReason;
typedef void (*PsiFreeRTOS_FatalHandler)(const PsiFreeRTOS_FatalReason reason);

/*******************************************************************************************
 * Macros for thread-safe printing
 *******************************************************************************************/

#ifdef USE_STDIO_PRINTF
	#include <stdio.h>
	#define printfInt(...) printf(__VA_ARGS__)
#else
	#include <xil_printf.h>
	#define printfInt(...) xil_printf(__VA_ARGS__)
#endif

SemaphoreHandle_t PsiFreeRTOS_printMutex;

#define PsiFreeRTOS_printf(...) { \
	xSemaphoreTake(PsiFreeRTOS_printMutex, portMAX_DELAY); \
	printfInt(__VA_ARGS__); \
	xSemaphoreGive(PsiFreeRTOS_printMutex);}


/*******************************************************************************************
 * Public Functions
 *******************************************************************************************/
/**
 * @brief	This function must be called before the FreeRTOS Scheduler is started to initialize
 * 			the PSI specific additions to FreeRTOS.
 *
 * @param maxTasks				Maximum number of tasks supported by the PSI additions
 * @param maxTicksWithoutIdle	If the idle task is not executed for this time, an inifinite loop is detected
 * 							    and an error is produced.
 * @param fatalHandler_p		User handler function for fatal errors. When non-recoverable fatal errors (e.g. stack overflow) occur,
 *                              an error is printed an all tasks are stopped. Additionally the user can register a handler function to
 *                              do project specific fatal-error-handler (e.g. switch on a red LED).
 *                              It is important that the handler function does only contain bare-metal code and does neither block nor
 *                              rely on any tasks or the scheduler itself.
 */
void PsiFreeRTOS_Init(	const uint16_t maxTasks,
						const uint16_t maxTicksWithoutIdle,
						PsiFreeRTOS_FatalHandler fatalHandler_p);

#if INCLUDE_uxTaskGetStackHighWaterMark
	/**
	 * @brief Print the watermark of the stacks of all tasks to the console. This function requires quite some time
	 *        due to finding the watermark, so it shall only be used for debugging purposes. However, higher priority tasks
	 *        can interrupt this function at any time, so it does not affect realtime behavior.
	 */
	void PsiFreeRTOS_PrintStackWatermark();
#endif

#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
	/**
	 * @brief Print the CPU usage of all tasks and their priorities. The usage is measured from the last call to
	 *        PsiFreeRTOS_StartCpuUsageMeas() to the call of PsiFreeRTOS_PrintCpuUsage(). So before calling this function,
	 *        call PsiFreeRTOS_StartCpuUsageMeas() and wait for some time (e.g. 1 sec).
	 *        This function does not affect the realtime behavior (in contrast to vTaskList()).
	 */
	void PsiFreeRTOS_PrintCpuUsage();
	/**
	 * @brief Starting a CPU usage measurement.
	 */
	void PsiFreeRTOS_StartCpuUsageMeas();
#endif

/**
* @brief Print remaining heap memory to the console. Note that this does not say anything about fragmentation
*        but only about the number of bytes available.
*/
void PsiFreeRTOS_PrintHeap();


