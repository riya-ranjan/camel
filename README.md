# Riya and Aanya's Camera

Bare-metal Raspberry Pi A+ project to capture images using the HQ Camera (Sony IMX477R) and display them on a screen. Communicates with the VideoCore GPU via the mailbox property interface — no OS, no Linux.

## Project Structure

```
camel/
├── src/              # project source code
│   └── camera/code/  # camera driver (mailbox interface, capture logic)
├── libpi/            # bare-metal Pi library (GPIO, UART, SPI, interrupts, etc.)
├── firmware/         # SD card boot files (bootcode.bin, start.elf, kernel.img)
├── libunix/          # Unix-side host utilities
└── bin/              # built binaries
```
