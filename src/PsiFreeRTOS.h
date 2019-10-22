#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************************
 * Includes
 *******************************************************************************************/
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <stdint.h>
#include "xscugic.h"

/*******************************************************************************************
 * Types
 *******************************************************************************************/
typedef enum {
	PsiFreeRTOS_FatalReason_StackOvervflow = 1,
	PsiFreeRTOS_FatalReason_MallocFailed = 2,
	PsiFreeRTOS_FatalReason_InfiniteLoop = 3,
	PsiFreeRTOS_FatalReason_CreatedTooManyTasks = 4
} PsiFreeRTOS_FatalReason;

/**
 * @brief	Handler function for fatal errors. The code must not block and not depend on any
 * 			FreeRTOs functionality (the system may be fully instable)
 *
 * @param 	reason	Reson for the fatal error
 */
typedef void (*PsiFreeRTOS_FatalHandler)(const PsiFreeRTOS_FatalReason reason);

/**
 * @brief	User code to be executed in the Tick-Hook (the FreeRTOS Tick-Hook is occupied
 * 			by PSI specific functionality)
 */
typedef void (*PsiFreeRTOS_TickHandler)(void);

/*******************************************************************************************
 * Macros for thread-safe printing
 *******************************************************************************************/

#ifdef USE_STDIO_PRINTF
	#include <stdio.h>
	#define printfInt(...) printf(__VA_ARGS__)
	#define putcharInt(c) putchar(c)
    #define getcharInt() getchar()
#else
	#include <xil_printf.h>
	#define printfInt(...) xil_printf(__VA_ARGS__)
	#define putcharInt(c) outbyte(c)
	#define getcharInt() inbyte()
#endif

extern SemaphoreHandle_t PsiFreeRTOS_printMutex;

//Execute code with the print-protection mutex locked
//... this is useful if an existing function could potentially print. In this case
//... the existing code does not have to be modified (i.e. printf() does not have to be changed to PsiFreeRTOS_printf()
#define PSI_FREERTOS_LOCKED_PRINT(x) { \
		xSemaphoreTakeRecursive(PsiFreeRTOS_printMutex, portMAX_DELAY); \
		x; \
		xSemaphoreGiveRecursive(PsiFreeRTOS_printMutex);}

#define PsiFreeRTOS_printf(...) PSI_FREERTOS_LOCKED_PRINT(printfInt(__VA_ARGS__))

#define PsiFreeRTOS_putchar(c) PSI_FREERTOS_LOCKED_PRINT(putcharInt(c))

#define PsiFreeRTOS_getchar() ({ \
	xSemaphoreTakeRecursive(PsiFreeRTOS_printMutex, portMAX_DELAY); \
	char c = getcharInt(); \
	xSemaphoreGiveRecursive(PsiFreeRTOS_printMutex); \
	c;})



/*******************************************************************************************
 * Public Functions
 *******************************************************************************************/
/**
 * @brief	This function must be called before the FreeRTOS Scheduler is started to initialize
 * 			the PSI specific additions to FreeRTOS.
 *
 * @param fatalHandler_p		User handler function for fatal errors. When non-recoverable fatal errors (e.g. stack overflow) occur,
 *                              an error is printed an all tasks are stopped. Additionally the user can register a handler function to
 *                              do project specific fatal-error-handler (e.g. switch on a red LED).
 *                              It is important that the handler function does only contain bare-metal code and does neither block nor
 *                              rely on any tasks or the scheduler itself.
 *                              Pass NULL if unused.
 * @param tickHandler_p			Function to be called on every tick (since FreeRTOS tick hook is occupied by PsiFreeRTOS).
 * 								Pass NULL if unused.
 */
void PsiFreeRTOS_Init(	PsiFreeRTOS_FatalHandler fatalHandler_p,
						PsiFreeRTOS_TickHandler tickHandler_p);

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
	 * @brief 	Get the CPU load for a given task handle
	 *
	 * @param 	task_p	Task to get the CPU load for
	 * @return 			CPU load in percent
	 */
	uint8_t PsiFreeRTOS_GetCpuLoad(TaskHandle_t task_p);
#endif

/**
* @brief Print remaining heap memory to the console. Note that this does not say anything about fragmentation
*        but only about the number of bytes available.
*/
void PsiFreeRTOS_PrintHeap();

/**
* @brief Return remaining heap memor. Note that this does not say anything about fragmentation
*        but only about the number of bytes available.
*
* @return	Remaining heap size in bytes
*/
unsigned long PsiFreeRTOS_GetHeap();

/**
 * The Xilinx FreeRTOS Port initializeds the GIC interrupt controller. This function allows getting
 * the instance pointer of the GIC to register additional IRQs.
 *
 * @return Pointer to the XScuGic instance
 */
XScuGic* PsiFreeRTOS_GetXScuGic();

#ifdef __cplusplus
}
#endif


