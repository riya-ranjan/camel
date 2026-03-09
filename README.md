# Riya and Aanya's Camera

This is our driver for the [raspberry pi HQ camera](https://www.raspberrypi.com/products/raspberry-pi-high-quality-camera/).

We use I2C to set up the camera configuration, CSI to communicate with the camera, and then convert the raw bytes into a png. 

## Project Structure

```
camel/
├── src/camera/code/   # camera driver (mbox, i2c, imx477, unicam, capture phases 0-4)
├── libpi/             # bare-metal Pi library
├── firmware/          # SD card boot files, include dt-blob 
└── docs/              # documentation
```

## Usage/Image Capture
Run `make` in `src/camera/code/` to capture a test image on your raspberry pi. This test captures a single frame, and saves the file in .RAW format to your SD card. 
