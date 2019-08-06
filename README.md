# General Information

## Maintainer
Oliver Bründler [oliver.bruendler@psi.ch]

## Changelog
See [Changelog](Changelog.md)

## Tagging Policy
Stable releases are tagged in the form *major*.*minor*.*bugfix*. 

* Whenever a change is not fully backward compatible, the *major* version number is incremented
* Whenever new features are added, the *minor* version number is incremented
* If only bugs are fixed (i.e. no functional changes are applied), the *bugfix* version is incremented
 
# Dependencies

* None

# Description

## Overview
This library contains the PSI-DSV specific FreeRTOS port for usage with the Cortex-R5 RPUs of the UltraScale+ device. 

We do not just use the Xilinx BSP because there are several flaws with the Xilinx tools:

* The tools do not allow to set all FreeRTOS configuration settings in the GUI
* Changing the header files manually is not an option since the tools overwrite the files everytime the BSP is generated (i.e. whenever a new FPGA bitstream is imported)
* The pure FreeRTOS is missing some features (e.g. diagnosis functionality is not realtime capable)

To overcome this issues, the Xilinx BSP was ported into this library and very slightly modified. All PSI specific modifications are clearly marked as such, so future versions of the Xilnx BSP can easily be merged. 

To use the BSP, a bare-metal project must be crated and the code of this library must be added to it.

This library also contains some additional functionality not available from FreeRTOS off-the-shelf. For example it ensures that CPU usage can be measured also outside of a development context (i.e. when the system runs for weeks) and it contains real-time friendly mechanisms for printing statistics (CPU usage, stack watermarks, available heap memory).

## Functionality

To access PSI specific functionality, include the header file *PsiFreeRTOS.h*

### Save Printing

The library contains a safe printing mechanism *PsiFreeRTOS_printf* that wraps the normal *printf()* function with a mutex to avoid hazzards. Only this printing mechanism shall be used throughout the whole application. 

By default the *xil_printf()* function is used to keep application size small. The the *printf()* function from *stdio.h*, can be chosen by definig a macro *USE_STDIO_PRINTF* in the compiler flags.

### Heap Tracking

The available heap space is calculated by regsitering FreeRTOS trace macros *traceMALLOC()* and *traceFREE()*. So these macros are no more available to the user application. The remaining heap-space can be printed to the console by calling *PsiFreeRTOS_PrintHeap()*.

### Statistics
FreeRTOS supports the creation of various statistics such as CPU usage per task and stack-watermarks. Unfortunately all functions to print these values to the console are not real-time friendly (they suspend the scheduler for extended time peeriods).

To overcome this issue, real-time friendly printing functions are added. They require the *PsiFreeRTOS* code to keep track of all tasks that exist. This is done by registering the FreeRTOS trace macros *traceTASK_CREATE()* and *traceTASK_DELETE()*. So these macros are no more available to the user.

Additionally the *PsiFreeRTOS* code sets-up a timer for the run-time measurement (this is missing in the Xilinx BSP). 

CPU load is measured over regular time intervals defined by the *FreeRTOSConfig.h* constant *configPSI_CPU_LOAD_UPDATE_RATE_TICKS*. The last CPU load measurement can be printed using *PsiFreeRTOS_PrintCpuUsage()*. Additionally CPU load per task can be read at runtime using the function *PsiFreeRTOS_GetCpuLoad()*. For printing the available heap memory, call *PsiFreeRTOS_PrintHeap()*. For printing the stack watermarks for each task, call *PsiFreeRTOS_PrintStackWatermark()*.


### Stack Overflow Detection

Stack overflows are detected by FreeRTOS. the *PsiFreeRTOS* registers the hook *vApplicationStackOverflowHook()* and stops operation after printing an error message if stack overflow occured. Operation must be stopped since stack overflow usually leads to corrupted task-control blocks which will crash the system earlier or later.

### Out-of-Memory Detection

Failing mallocs are detected by FreeRTOS. the *PsiFreeRTOS* registers the hook *vApplicationMallocFailedHook()* and stops operation after printing an error message if malloc failed. 

### Endless loop Detection

If the idle-task does not get any processing time for a specified number of ticks, an endless loop is detected and an error as well as the most recent CPU load is printed and the operation is stopped.


To achieve this, *PsiFreeRTOS* registers the hooks *vApplicationIdleHook()* and *vApplicationTickHook()*.

## Usage

1. Add this repository to your project as submodule.
2. In your application project, include the *src* folder of this repository as include and source paths
3. Create a *FreeRTOSConfig.h* in your application (or copy it from the refdesign of this repo). This file is application specific since each application may configure FreeRTOS differently.
4. Add the path of the *FreeRTOSConfig.h* file to the include paths of your project
5. Before starting the scheduler, call *PsiFreeRTOS\_Init()* to initialize the PSI specific code

## FreeRTOSConfig.h 

To use the library, the entires below must be present in *FreeRTOSConfig.h*.

```
#include "PsiFreeRTOS_Hooks.h"

//Native Free RTOS Configuration
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() PsiFreerRTOS_CONFIGURE_TIMER_FOR_RUN_TIME_STATS()
#define portGET_RUN_TIME_COUNTER_VALUE() PsiFreeRTOS_GET_RUN_TIME_COUNTER_VALUE()
#define traceTASK_CREATE(xTask) PsiFreeRTOS_TASK_CREATE(xTask)
#define traceTASK_DELETE(xTask) PsiFreeRTOS_TASK_DELETE(xTask)
#define traceMALLOC( pvAddress, uiSize) PsiFreeRTOS_MALLOC(uiSize)
#define traceFREE( pvAddress, uiSize) PsiFreeRTOS_FREE(uiSize)

//PSI Port Configuration
#define configPSI_TIMER_RUNTIME_STATS_ID XPAR_XTTCPS_1_DEVICE_ID //Choose any free TTCPS device you want
#define configPSI_MAX_TASKS 32 //Choose the number of tasks to be supported (keep number low for small memory footprint)
#define configPSI_MAX_TICKS_WITHOUT_IDLE 50 //Number of ticks without time for idle task to detect inifinte loops
#define configPSI_CPU_LOAD_UPDATE_RATE_TICKS 100 //CPU load statistics update rate (do not overflow 32-bit performance counters)
```

The CPU load is measured using a 32-bit counter. The counting frequency depends on configuration of the PS but by default it is 100 MHz. Always set the constants *configPSI_MAX_TICKS_WITHOUT_IDLE* and *configPSI_CPU_LOAD_UPDATE_RATE_TICKS* in such a way that:

*configPSI_MAX_TICKS_WITHOUT_IDLE + configPSI_CPU_LOAD_UPDATE_RATE_TICKS < Timer Overflow Time*

Otherwise CPU load measurement may be wrong. If no special requirements are present, using one second for both values is a good starting point.

## Common Pitfalls

* When creating a bare-metal project, set stack- and heap-size in the linker script appropriately (Xilinx defaults are way too small)
* Never do getchar() or scanf() in a task with a priority other than zero. These operations do busy-waiting so if they are executed at a priority higher than zero, they pevent the idle task from getting any processing time.
* Do not rely on stack overflow detection. FreeRTOS does this on a best-effort basis but stack-overflows may not be detected in some cases (especially when large arrays are allocated on the stack), which leads to random behavior.







