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

## Documentation
1. [Functionality](Functionality.md)
2. [User Guide](UserGuide.md)








