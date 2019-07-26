#include "FreeRTOS.h"
#include "task.h"
#include <xil_printf.h>
#include <stdbool.h>
#include "PsiFreeRTOS.h"
#include <stdio.h>

void Task_NoLoad(void* arg_p) {
	for (;;) {
		PsiFreeRTOS_printf("%s Loop\r\n", pcTaskGetName(NULL));
		vTaskDelay(10);
	}
}

void Task_Load(void* arg_p) {
	int cycles = (int) arg_p;
	volatile int x = 0;
	for (;;) {
		PsiFreeRTOS_printf("%s Loop\r\n", pcTaskGetName(NULL));
		for (int i = 0; i < cycles; i++) {
			x = i;
		}
		vTaskDelay(10);
		(void)(x);
	}
}

void Task_StackOverflow(void* arg_p) {
	{
		vTaskDelay(20);
		TaskHandle_t currentTask = xGetCurrentTaskHandle();
		uint32_t stackSpace = uxTaskGetStackHighWaterMark(currentTask);
		{
			//Allocate exactly one word more than available on stack
			volatile int x[stackSpace+1];
			for (;;) {
				for (int i = 1; i < stackSpace+1; i++){
					x[i] = x[i-1];
				}
				vTaskDelay(20);
				(void)(x);
			}
		}
	}
}

void Task_MallocFail(void* arg_p){
	pvPortMalloc(10000000);
}

void Task_InfiniteLoop(void* arg_p) {
	vTaskDelay(25);
	for (;;) {}
}


void Task_Menu(void* arg_p) {
	//Variables
	TaskHandle_t task1;

	//Test Selection
	PsiFreeRTOS_printf("Select Test:\r\n");
	PsiFreeRTOS_printf("s Stack overflow \r\n");
	PsiFreeRTOS_printf("w Print stack watermark\r\n");
	PsiFreeRTOS_printf("n Normal operation\r\n");
	PsiFreeRTOS_printf("d Task delete\r\n");
	PsiFreeRTOS_printf("c Print CPU Usage\r\n");
	PsiFreeRTOS_printf("m Failing memory allocation\r\n");
	PsiFreeRTOS_printf("h Print Heap\r\n");
	PsiFreeRTOS_printf("i Infinite loop detection\r\n");
	char c = inbyte();

	//Create tasks always required
	xTaskCreate(Task_NoLoad, "NoLoad1", 400, NULL, 1, NULL);
	xTaskCreate(Task_NoLoad, "NoLoad2", 400, NULL, 1, NULL);
	xTaskCreate(Task_Load, "Load1", 400,  (void*)10, 1, NULL);
	xTaskCreate(Task_Load, "Load2", 400, (void*)10000, 1, NULL);

	//Reset measurement counter to see statistics during actual use (not during creation)
	PsiFreeRTOS_StartCpuUsageMeas();

	//Create tasks required only for given tests
	switch (c) {
	case 's':
		xTaskCreate(Task_StackOverflow, "StackOverflow", 400, NULL, 2, NULL);
		break;
	case 'd':
		xTaskCreate(Task_NoLoad, "*Test Task", 400, NULL, 1, &task1);
		break;
	case 'c':
		PsiFreeRTOS_StartCpuUsageMeas();
		break;
	case 'm':
		xTaskCreate(Task_MallocFail, "MallocTask", 400, NULL, 1, NULL);
		break;
	case 'i':
		xTaskCreate(Task_InfiniteLoop, "Infinite", 400, NULL, 1, NULL);
	default:
		break;
	}

	//Run Test for a while
	vTaskDelay(100);

	//Execute Post Test
	switch (c) {
	case 'w':
		PsiFreeRTOS_PrintStackWatermark();
		break;
	case 'd':
		PsiFreeRTOS_PrintStackWatermark();
		PsiFreeRTOS_printf("DELETE TASK\r\n");
		vTaskDelete(task1);
		vTaskDelay(40);
		PsiFreeRTOS_PrintStackWatermark();
		break;
	case 'c':
		PsiFreeRTOS_PrintCpuUsage();
		break;
	case 'h':
		PsiFreeRTOS_printf("BEFORE NEW TASK CREATED\r\n");
		PsiFreeRTOS_PrintHeap();
		xTaskCreate(Task_NoLoad, "*Test Task", 200, NULL, 1, &task1);
		PsiFreeRTOS_printf("AFTER NEW TASK CREATED\r\n");
		PsiFreeRTOS_PrintHeap();
		break;
	default:
		break;
	}
	PsiFreeRTOS_printf("Test Done!\r\n");

	//Stop everything after test
	vTaskSuspendAll();
	for (;;) {
	}
}

void FatalHandler(const PsiFreeRTOS_FatalReason reason) {
	xil_printf("FATAL: %d\r\n", reason);
}

int main() {
	//Initialize
	PsiFreeRTOS_Init(32, 50, FatalHandler);

	//Create tasks
	xTaskCreate(Task_Menu, "Menu", 1000, NULL, 0, NULL);

	//Start Scheduler
	vTaskStartScheduler();

	//Should never end up here
	xil_printf("Unexpected exit of vStartScheduler()\n");
	for (;;){}

	return 0;
}



