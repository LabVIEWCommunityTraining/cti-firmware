# Standard Modes

The goal will be to provide firmware that covers different operating modes such as VISA instruments, buffered streaming via a DAQmx style API, and maybe others. When possible for a target, based on flash and memory availability, these modes should be combined into a single firmware with the ability to switch modes in software to reduce the need to deploy different firmware.

## VISA Instrument Mode

VISA is an ascii text based mode where the device parses and responds to commands and queries, typically over serial or TCP. The goal is for the device to present as a standard instrument class such as Scope or FGen and response to expected SCPI messages.

## uDAQ Mode

uDAQ is an API similar in style to DAQmx with the idea of providing hardware-timed buffered I/O streaming. The LabVIEW API for interacting in this mode should follow DAQmx as close as possible. The idea with this mode is to provide training such that transitioning to other hardware that does use the DAQmx API should be as simple as possible for similar capabilities.

## Native Mode

This is less of a standard mode as capabilities may vary drastically across devices. This mode should expose as much capability of a device as possible. This includes things such as being able to perform SPI and I2C communications, interact with available UARTS, and use analog / gpio.

Native mode will be up to the developer of the firmware for a particular device as far as what features it will provide but the native mode will provide a common protocol for communicating with a target and will include mechanisms for determining which device capabilities are available to support meaningful error messages on the LabVIEW API side.

# Device Connection

Different devices will support different connection methods but any connection should provide identical capabilities for a given mode and device. There may be some limitations based on available data bandwidth for a given connection which can lead to different capabilities but no other factors should impact usage. For instance, with VISA, changing between wired USB connections and WiFi should only require changing the VISA resource configuration and the rest of the code should operate identically. 

### USB

All planned targets will support a USB connection that works as a serial port directly connected to a PC. This will be the simplest way of interacting with hardware and will be the way that different versions of firmware can be deployed and the device can be configured.

### WiFi

When a board has the necessary components, WiFi mode should be supported which allows wirelessly interacting with a device. An initial connection via USB is necessary for configuring the device's WiFi connection before it will be available on a wireless network.