## User Guide

[<< Back to Index](./README.md)

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

## Usage for C

1. Add this repository to your project as submodule.
2. In your application project, include the *src* folder of this repository as include and source paths
3. Create a *FreeRTOSConfig.h* in your application (or copy it from the refdesign of this repo). This file is application specific since each application may configure FreeRTOS differently.
4. Add the path of the *FreeRTOSConfig.h* file to the include paths of your project
5. Before starting the scheduler, call *PsiFreeRTOS\_Init()* to initialize the PSI specific code

## Usage for C++

FreeRTOS must always be compiled as C-code. C\++ compilers are more restrictive with casting and will produce errors when attempting to compile FreeRTOS. For C++ projects, it is easiest to compile the PSI-FreeRTOS port as a separate library. This library is project specific because it contains exactly the FreeRTOS configuration required for this specific project.

1. Add this repository to your project as submodule.
2. In SDK, create a new library project (New > Xilinx > Library Project) and name it *psi_freertos*
3. In this library project, include the *src* folder of this repository as include and source paths
4. Create a *FreeRTOSConfig.h* in your library project (or copy it from the refdesign of this repo). This file is application specific since each application may configure FreeRTOS differently.
5. Add the path to the *FreeRTOSConfig.h* file to the include paths of your application
6. Add the path to the *src* folder of this repository to the include paths of your application
7. Link your application project agains teh library project
  * Add path to the directory containing the *libpsi_freertos.a* file to the Library search path (-L) of the linker
  * Add the library to the linker (-l psi\_freertos)
8. Before starting the scheduler, call *PsiFreeRTOS\_Init()* to initialize the PSI specific code

[<< Back to Index](./README.md)






