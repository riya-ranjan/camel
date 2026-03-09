# Riya and Aanya's Camera

Bare-metal Raspberry Pi Zero W + HQ Camera (Sony IMX477R). No OS — drives the sensor over I2C, receives CSI-2 data through the BCM2835 Unicam receiver, and DMA's RAW12 frames into SDRAM.

## SD Card Setup (Do This First!)

Copy everything from `firmware/` onto a FAT32 micro SD card. Key files:

- **`config.txt`** — must have `core_freq=400`, `force_turbo=1`, `enable_uart=1`
- **`dt-blob.bin`** — camera GPIO/power config (compiled from `firmware/dt-blob.dts`)
- **`kernel.img`** — serial bootloader (built with UART divisor=433 for 400MHz)
- **`start.elf`**, **`bootcode.bin`** — GPU firmware

Without the correct firmware, the camera won't power on and the bootloader won't communicate.

## Build & Run

```bash
make -C libpi              # build bare-metal library first
make -C src/camera/code    # build + run via bootloader
```

Requires `CS140E_2026_PATH` env var and `arm-none-eabi` toolchain.

## Project Structure

```
camel/
├── src/camera/code/   # camera driver (mbox, i2c, imx477, unicam, capture phases 0-4)
├── bootloader/        # custom bootloader (UART divisor=433 for 400MHz core clock)
├── libpi/             # bare-metal Pi library
├── firmware/          # SD card boot files
└── docs/              # documentation
```

## Phases

| Phase | Description | Status |
|-------|-------------|--------|
| 0 | Boot test | PASS |
| 1 | Camera power + I2C + sensor ID | PASS |
| 2 | Unicam power domains + CAM1 clock | PASS |
| 3 | Sensor register init (412 regs) | PASS |
| 4 | CSI-2 + DMA frame capture (2028x1520 RAW12) | PASS |
