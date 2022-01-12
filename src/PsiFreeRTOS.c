/*******************************************************************************************
 * Includes
 *******************************************************************************************/
#include "PsiFreeRTOS.h"
#include "FreeRTOSConfig.h"
#include <stdbool.h>
#include "xttcps.h"
#include <xparameters.h>
#include <xparameters_ps.h>
#include "timers.h"
#include "task.h"

/*******************************************************************************************
 * Private Variables
 *******************************************************************************************/
static volatile TaskHandle_t allTasks[configPSI_MAX_TASKS];
static uint8_t taskCpuLoad[configPSI_MAX_TASKS];
static uint32_t taskCpuTicks[configPSI_MAX_TASKS];
static volatile uint16_t taskCount;
static unsigned long cpuMeasStartIncr;
static TickType_t cpuMeasStartTicks;
static unsigned long remainingHeap;
static volatile TickType_t lastIdleTime;
static XTtcPs xTimerInstance;
static PsiFreeRTOS_FatalHandler fatalErrorHandler_p;
static PsiFreeRTOS_TickHandler userTickHandler_p;
static bool infLoopDet;
SemaphoreHandle_t PsiFreeRTOS_printMutex;

/*******************************************************************************************
 * Private Helper Functions
 *******************************************************************************************/
#define printfSel(unsafe, ...) { \
	if (unsafe) { \
		printfInt(__VA_ARGS__); \
	} \
	else { \
		PsiFreeRTOS_printf(__VA_ARGS__); \
	}}

#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
	void PsiFreeRTOS_PrintCpuUsageInternal(bool isIrqContext) {
		//Checks
		if ((!configUSE_TRACE_FACILITY) || (!configGENERATE_RUN_TIME_STATS)) {
			printfSel(isIrqContext, "INFO: Cannot print CPU Usage because of FreeRTOS Settings");
		}

		//Implementation
		TaskStatus_t status;
		XTtcPs_Stop( &xTimerInstance );
		unsigned long runSum = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE() - cpuMeasStartIncr;
		unsigned long runSumPercent = runSum/100;
		printfSel(isIrqContext, "PsiFreeRTOS CPU-Usage:\r\n");
		printfSel(isIrqContext, "%-20s %4s %5s %10s\r\n", "Name", "CPU%", "Prio", "Cycles");
		for (uint16_t i = 0; ; i++) {
			if (!isIrqContext) {
				taskENTER_CRITICAL();
			}
			if (i >= taskCount) {
				break;
			}
			TaskHandle_t hndl = allTasks[i];
			if (!isIrqContext) {
				taskEXIT_CRITICAL();
			}
			vTaskGetInfo(hndl, &status, pdFALSE, eBlocked);
			//Use pre-calculated value if called from normal context, calculate fresh value during crash analysis
			uint8_t cpuLoad;
			uint32_t cpuTicks;
			if (!isIrqContext) {
				cpuLoad = taskCpuLoad[i];
				cpuTicks = taskCpuTicks[i];
			}
			else {
				cpuLoad = (uint8_t)(status.ulRunTimeCounter/runSumPercent);
				cpuTicks = status.ulRunTimeCounter;
			}
			//Print
			printfSel(isIrqContext, "%-20s %3d%% %5d %10d\r\n",
									status.pcTaskName,
									cpuLoad,
									status.uxBasePriority,
									cpuTicks);
		}
		XTtcPs_Start( &xTimerInstance );
	}
#endif

#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
	void PsiFreeRTOS_StartCpuUsageMeas() {
		taskENTER_CRITICAL();
		XTtcPs_Stop( &xTimerInstance );
		cpuMeasStartIncr = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE();
		cpuMeasStartTicks = xTaskGetTickCount();
		for (uint16_t i = 0; i < taskCount; i++) {
			TaskHandle_t hndl = allTasks[i];
			vTaskClearRunTimeCounter(hndl);
		}
		XTtcPs_Start( &xTimerInstance );
		taskEXIT_CRITICAL();

	}
#endif

/*******************************************************************************************
 * FreeRTOS Hook Functions
 *******************************************************************************************/
void PsiFreeRTOS_TASK_CREATE(void* taskvp) {
	const TaskHandle_t task = (TaskHandle_t)taskvp;

	taskENTER_CRITICAL();

	//Do use unsafe print because task creation is likely to happen before scheduler is started
	//.. block because this is a fatal error.
	if (taskCount >= configPSI_MAX_TASKS) {
		printfInt("PsiFreeRTOS: Created more tasks than allowed\r\n");
		(*fatalErrorHandler_p)(PsiFreeRTOS_FatalReason_CreatedTooManyTasks);
		for(;;){}
	}

	allTasks[taskCount] = task;
	taskCpuLoad[taskCount] = 0;
	taskCpuTicks[taskCount] = 0;
	taskCount++;

	taskEXIT_CRITICAL();
}

void PsiFreeRTOS_TASK_DELETE(void* taskvp) {
	const TaskHandle_t task = (TaskHandle_t)taskvp;

	taskENTER_CRITICAL();

	//Implementation
	bool taskFound = false;
	for (uint16_t i = 0; i < taskCount; i++) {
		if (task == allTasks[i]) {
			taskFound = true;
		}
		if (taskFound) {
			allTasks[i] = allTasks[i+1];
		}
	}
	taskCount--;

	taskEXIT_CRITICAL();
}

void PsiFreeRTOS_MALLOC(const unsigned int size) {
	taskENTER_CRITICAL();
	remainingHeap -= size;
	taskEXIT_CRITICAL();
}

void PsiFreeRTOS_FREE(const unsigned int size) {
	taskENTER_CRITICAL();
	remainingHeap += size;
	taskEXIT_CRITICAL();
}

void PsiFreerRTOS_CONFIGURE_TIMER_FOR_RUN_TIME_STATS() {
	int iStatus;
	XTtcPs_Config* pxTimerConfig = XTtcPs_LookupConfig( configPSI_TIMER_RUNTIME_STATS_ID );
	iStatus = XTtcPs_CfgInitialize( &xTimerInstance, pxTimerConfig, pxTimerConfig->BaseAddress );

	if( iStatus != XST_SUCCESS )
	{
		XTtcPs_Stop(&xTimerInstance);
		iStatus = XTtcPs_CfgInitialize( &xTimerInstance, pxTimerConfig, pxTimerConfig->BaseAddress );
		if( iStatus != XST_SUCCESS )
		{
			printfInt( "In %s: Timer Cfg initialization failed...\r\n", __func__ );
			return;
		}
	}
	XTtcPs_SetOptions( &xTimerInstance, XTTCPS_OPTION_WAVE_DISABLE );
	/* Enable the interrupt for timer. */
	XTtcPs_Start( &xTimerInstance );
}

unsigned long PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE() {
	return XTtcPs_GetCounterValue(&xTimerInstance);
}

void vApplicationStackOverflowHook(const xTaskHandle pxTask, const signed char *pcTaskName) {
	//Do not acquire semaphore since no other task may run correctly, hence other
	//... tasks may not return their semaphorese
	printfInt("\r\nERROR: Stack Overflow in '%s' !!!\r\n", pcTaskName);
	vTaskSuspendAll();
	if (NULL != fatalErrorHandler_p) {
		(*fatalErrorHandler_p)(PsiFreeRTOS_FatalReason_StackOvervflow);
	}
	for(;;){}
}

void vApplicationMallocFailedHook() {
	//Do not acquire semaphore since no other task may run correctly, hence other
	//... tasks may not return their semaphorese
	printfInt("\r\nERROR: Memory Allocation Failed in '%s'!!!\r\n", pcTaskGetName(NULL));
	vTaskSuspendAll();
	if (NULL != fatalErrorHandler_p) {
		(*fatalErrorHandler_p)(PsiFreeRTOS_FatalReason_MallocFailed);
	}
	for(;;){}
}

void vApplicationIdleHook() {
	lastIdleTime = xTaskGetTickCount();
	//Automatically restart CPU load measurement at the rate selected by the user
	//.. this is best effort (if IDLE does not get any time, the restart can be delayed.
	//.. Reason: Do not restart during the detection of an idle loop since otherwise the CPU load printed
	//.. may not contain much information.
	#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
		const uint32_t now = xTaskGetTickCount();
		if ((now - cpuMeasStartTicks) >= configPSI_CPU_LOAD_UPDATE_RATE_TICKS) {

			//Safe CPU load results
			TaskStatus_t status;
			XTtcPs_Stop( &xTimerInstance );
			unsigned long runSum = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE() - cpuMeasStartIncr;
			unsigned long runSumDiv = runSum/100; //Used to calculate runtime in percent
			for (uint16_t i = 0; ; i++) {
				taskENTER_CRITICAL();
				if (i >= taskCount) {
					taskEXIT_CRITICAL();
					break;
				}
				TaskHandle_t hndl = allTasks[i];
				taskEXIT_CRITICAL();

				vTaskGetInfo(hndl, &status, pdFALSE, eBlocked);
				taskCpuLoad[i] = (uint8_t)(status.ulRunTimeCounter/runSumDiv);
				taskCpuTicks[i] = status.ulRunTimeCounter;
			}
			XTtcPs_Start( &xTimerInstance );

			//Restart Measurement
			PsiFreeRTOS_StartCpuUsageMeas();
		}
	#endif
}

void vApplicationTickHook() {
	if (infLoopDet) {
		const TickType_t currentTime = xTaskGetTickCountFromISR();

		//Detect an infinite look (in this case, block execution since crash is fatal)
		if (currentTime - lastIdleTime > configPSI_MAX_TICKS_WITHOUT_IDLE) {
			//Do not acquire semaphore since no other task may run correctly, hence other
			//... tasks may not return their semaphorese
			printfInt("\r\nERROR: One task seems to be hanging (consumes all CPU power) !!!\r\n");
			#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
				PsiFreeRTOS_PrintCpuUsageInternal(true);
			#endif
			vTaskSuspendAll();
			if (NULL != fatalErrorHandler_p) {
				(*fatalErrorHandler_p)(PsiFreeRTOS_FatalReason_InfiniteLoop);
			}
			for(;;){}
		}
	}

	//Call user tick hook
	if (NULL != userTickHandler_p) {
		(*userTickHandler_p)();
	}

}



/*******************************************************************************************
 * Public Functions
 *******************************************************************************************/

void PsiFreeRTOS_Init(	PsiFreeRTOS_FatalHandler fatalHandler_p,
						PsiFreeRTOS_TickHandler tickHandler_p,
						bool infLoopDetection) {
	PsiFreeRTOS_printMutex = xSemaphoreCreateRecursiveMutex();
	taskCount = 0;
	remainingHeap = configTOTAL_HEAP_SIZE;
	lastIdleTime = 0;
	cpuMeasStartIncr = 0;
	fatalErrorHandler_p = fatalHandler_p;
	userTickHandler_p = tickHandler_p;
	infLoopDet = infLoopDetection;
}

#if INCLUDE_uxTaskGetStackHighWaterMark
	void PsiFreeRTOS_PrintStackWatermark() {
		//Checks
		if (!INCLUDE_uxTaskGetStackHighWaterMark ) {
			PsiFreeRTOS_printf("INFO: Cannot print Stack Watermark because of FreeRTOS Settings");
		}
		//Implementation
		printfInt("PsiFreeRTOS Stack-Watermarks:\r\n");
		taskENTER_CRITICAL();
		for (uint16_t i = 0; i < taskCount; i++) {
			const TaskHandle_t hndl = allTasks[i];
			taskEXIT_CRITICAL();
			const char* const name = pcTaskGetName(hndl);
			int watermark = uxTaskGetStackHighWaterMark(hndl);

			//Print but release semaphore during slow printing operation
			PsiFreeRTOS_printf("%-20s %d\r\n", name, watermark);
			taskENTER_CRITICAL();
		}
		taskEXIT_CRITICAL();
	}
#endif

#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
	void PsiFreeRTOS_PrintCpuUsage() {
		PsiFreeRTOS_PrintCpuUsageInternal(false);
	}

	uint8_t PsiFreeRTOS_GetCpuLoad(TaskHandle_t task_p) {
		for (int i = 0; i < taskCount; i++) {
			if (allTasks[i] == task_p) {
				return taskCpuLoad[i];
			}
		}
		return 0;
	}

#endif

void PsiFreeRTOS_PrintHeap() {
	printfInt("PsiFreeRTOS FreeHeap [bytes]:%d\r\n", remainingHeap);
}

unsigned long PsiFreeRTOS_GetHeap() {
	return remainingHeap;
}

extern XScuGic xInterruptController; //defined in portZynqUltrascale.c
XScuGic* PsiFreeRTOS_GetXScuGic() {
	return &xInterruptController;
}
