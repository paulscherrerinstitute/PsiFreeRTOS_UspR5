# Functionality

[<< Back to Index](./README.md)

To access PSI specific functionality, include the header file *PsiFreeRTOS.h*

## Save Printing

The library contains a safe printing mechanism *PsiFreeRTOS_printf* that wraps the normal *printf()* function with a mutex to avoid hazzards. Only this printing mechanism shall be used throughout the whole application. 

By default the *xil_printf()* function is used to keep application size small. The the *printf()* function from *stdio.h*, can be chosen by definig a macro *USE_STDIO_PRINTF* in the compiler flags.

## Heap Tracking

The available heap space is calculated by regsitering FreeRTOS trace macros *traceMALLOC()* and *traceFREE()*. So these macros are no more available to the user application. The remaining heap-space can be printed to the console by calling *PsiFreeRTOS_PrintHeap()*.

## Statistics
FreeRTOS supports the creation of various statistics such as CPU usage per task and stack-watermarks. Unfortunately all functions to print these values to the console are not real-time friendly (they suspend the scheduler for extended time peeriods).

To overcome this issue, real-time friendly printing functions are added. They require the *PsiFreeRTOS* code to keep track of all tasks that exist. This is done by registering the FreeRTOS trace macros *traceTASK_CREATE()* and *traceTASK_DELETE()*. So these macros are no more available to the user.

Additionally the *PsiFreeRTOS* code sets-up a timer for the run-time measurement (this is missing in the Xilinx BSP). 

CPU load is measured over regular time intervals defined by the *FreeRTOSConfig.h* constant *configPSI_CPU_LOAD_UPDATE_RATE_TICKS*. The last CPU load measurement can be printed using *PsiFreeRTOS_PrintCpuUsage()*. Additionally CPU load per task can be read at runtime using the function *PsiFreeRTOS_GetCpuLoad()*. For printing the available heap memory, call *PsiFreeRTOS_PrintHeap()*. For printing the stack watermarks for each task, call *PsiFreeRTOS_PrintStackWatermark()*.


## Stack Overflow Detection

Stack overflows are detected by FreeRTOS. the *PsiFreeRTOS* registers the hook *vApplicationStackOverflowHook()* and stops operation after printing an error message if stack overflow occured. Operation must be stopped since stack overflow usually leads to corrupted task-control blocks which will crash the system earlier or later.

## Out-of-Memory Detection

Failing mallocs are detected by FreeRTOS. the *PsiFreeRTOS* registers the hook *vApplicationMallocFailedHook()* and stops operation after printing an error message if malloc failed. 

## Endless loop Detection

If the idle-task does not get any processing time for a specified number of ticks, an endless loop is detected and an error as well as the most recent CPU load is printed and the operation is stopped.


To achieve this, *PsiFreeRTOS* registers the hooks *vApplicationIdleHook()* and *vApplicationTickHook()*.

[<< Back to Index](./README.md)
