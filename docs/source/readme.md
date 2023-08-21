# Software Source

This document provides an overview of the organization of the software source and links to more detailed documentation.

## Platform abstraction

Since this firmware is aimed at supporting multiple embedded targets, the platform abstractions provided in the software are done in a more light-weight way than someone familiar with Hardware Abstraction Layers (HAL) in desktop programming may be used to. The abstraction does not provide run-time selection of the underlying hardware but instead relies on the build system (CMake) and build configurations to include the necessary implementations during firmware building. [Building](../platform) is detailed in other documentation so refer to that section of the docs for information on how the implementation selection works. In this document, we will be looking at how the platform dependent code is organized in the source code and how to use it.

### gPlatform
All of the platform specific functionality is collected in a single global named gPlatform and is available by include `cti/platform.h` in code needing to use functionality that will be implemented differently on different platforms. The global platform functionality is further broken up under the gPlatform object into Info, IO, Timer, and Mem.