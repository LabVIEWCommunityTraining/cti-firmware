# Raspberry Pi Pico & Fallback

This is the basic USB only implementation for most RP2040 based devices and may be suitable for any device with constraints on I/O usage. The only board feature present is the onboard LED but that can be overridden after startup in case any derivative board uses a different pin or source/sink direction.