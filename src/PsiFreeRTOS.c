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
static volatile TaskHandle_t* allTasks;
static volatile uint16_t taskCount;
static unsigned long cpuMeasStart;
static unsigned long remainingHeap;
static uint16_t maxCpuBusyTicks;
static volatile TickType_t lastIdleTime;
static XTtcPs xTimerInstance;
static PsiFreeRTOS_FatalHandler fatalErrorHandler_p;

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
		unsigned long runSum = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE() - cpuMeasStart;
		printfSel(isIrqContext, "PsiFreeRTOS CPU-Usage:\r\n");
		printfSel(isIrqContext, "%-20s %10s %4s %3s\r\n", "Name", "Time[clk]", "CPU%", "Prio");
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
			printfSel(isIrqContext, "%-20s %10d %3d%% %d\r\n",
									status.pcTaskName,
									status.ulRunTimeCounter,
									(int)((unsigned long long)status.ulRunTimeCounter*100/runSum),
									status.uxBasePriority);
		}
		XTtcPs_Start( &xTimerInstance );
	}
#endif

/*******************************************************************************************
 * FreeRTOS Hook Functions
 *******************************************************************************************/
void PsiFreeRTOS_TASK_CREATE(const TaskHandle_t task) {
	taskENTER_CRITICAL();

	allTasks[taskCount] = task;
	taskCount++;

	taskEXIT_CRITICAL();
}

void PsiFreeRTOS_TASK_DELETE(const TaskHandle_t task) {
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
	//Automatically restart CPU load measurement if the timer counted half way of its range
	#if (configUSE_TRACE_FACILITY && configGENERATE_RUN_TIME_STATS)
		const uint32_t now = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE();
		if ((now - cpuMeasStart) & 0x80000000) {
			PsiFreeRTOS_StartCpuUsageMeas();
		}
	#endif
}

void vApplicationTickHook() {
	const TickType_t currentTime = xTaskGetTickCountFromISR();
	if (currentTime - lastIdleTime > maxCpuBusyTicks) {
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



/*******************************************************************************************
 * Public Functions
 *******************************************************************************************/

void PsiFreeRTOS_Init(	const uint16_t maxTasks,
						const uint16_t maxTicksWithoutIdle,
						PsiFreeRTOS_FatalHandler fatalHandler_p) {
	PsiFreeRTOS_printMutex = xSemaphoreCreateMutex();
	allTasks = pvPortMalloc(maxTasks*sizeof(TaskHandle_t));
	maxCpuBusyTicks = maxTicksWithoutIdle;
	taskCount = 0;
	remainingHeap = configTOTAL_HEAP_SIZE;
	lastIdleTime = 0;
	cpuMeasStart = 0;
	fatalErrorHandler_p = fatalHandler_p;
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

	void PsiFreeRTOS_StartCpuUsageMeas() {
		taskENTER_CRITICAL();
		XTtcPs_Stop( &xTimerInstance );
		cpuMeasStart = PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE();
		for (uint16_t i = 0; i < taskCount; i++) {
			TaskHandle_t hndl = allTasks[i];
			vTaskClearRunTimeCounter(hndl);
		}
		XTtcPs_Start( &xTimerInstance );
		taskEXIT_CRITICAL();

	}
#endif

void PsiFreeRTOS_PrintHeap() {
	printfInt("PsiFreeRTOS FreeHeap [bytes]:%d\r\n", remainingHeap);
}
