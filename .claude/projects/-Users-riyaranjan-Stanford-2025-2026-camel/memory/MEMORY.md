# Project Memory

## Workflow
- **Always run a test on hardware after every phase.** Do not move to the next phase until the current one passes on real hardware.
- Build: `make -C src/camera/code`
- Deploy: `my-install <test>.bin`

## Current Status
- Phase 0 in progress: config.txt updated, 0-test-boot.c created. Need to download `start_x.elf` and test on Pi.
