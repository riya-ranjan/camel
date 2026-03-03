# Bare-Metal Camera Capture: Incremental Implementation Plan

## Context

We want to capture images from the Raspberry Pi HQ Camera (Sony IMX477R) on a Pi A+ (BCM2835, 256MB RAM) with no OS. The camera data flows over CSI-2 into the VideoCore GPU, so we must talk to the GPU firmware (`start.elf`) via the **VCHIQ protocol** and **MMAL message layer** to request captures. We already have a working mailbox interface.

This plan follows the cs140e lab style: small numbered test programs, each verifying ONE thing before moving on.

## Reference Implementations

- **Ultibo** (Pascal, fully working): [ultibohub/Core](https://github.com/ultibohub/Core) — `source/rtl/ultibo/drivers/vc4vchiq.pas`
- **Ultibo Userland** (MMAL port): [ultibohub/Userland](https://github.com/ultibohub/Userland)
- **Circle** (C++ VCHIQ): [rsta2/circle addon/vc4/vchiq/](https://github.com/rsta2/circle/tree/master/addon/vc4/vchiq)
- **Linux VCHIQ driver**: [raspberrypi/linux vchiq_core.c](https://github.com/raspberrypi/linux/blob/rpi-5.10.y/drivers/staging/vc04_services/interface/vchiq_arm/vchiq_core.c)
- **Linux MMAL driver**: [raspberrypi/linux mmal-vchiq.c](https://github.com/raspberrypi/linux/blob/rpi-5.10.y/drivers/staging/vc04_services/vchiq-mmal/mmal-vchiq.c)
- **Linux MMAL message defs**: [mmal-msg.h](https://github.com/raspberrypi/linux/blob/rpi-5.10.y/drivers/staging/vc04_services/vchiq-mmal/mmal-msg.h)
- **Raspberry Pi firmware wiki**: [Mailbox property interface](https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface)
- **Key forum thread** (bare-metal VCHIQ camera attempt): [forums.raspberrypi.com/viewtopic.php?t=213322](https://forums.raspberrypi.com/viewtopic.php?t=213322)
- **VCHIQ-MMAL architecture thread**: [forums.raspberrypi.com/viewtopic.php?t=306547](https://forums.raspberrypi.com/viewtopic.php?t=306547)

---

## Phase 0: Firmware & Hardware Setup

**Goal:** Get the right firmware on the SD card and verify the camera is physically connected.

### Tasks
1. **Replace `start.elf` with `start_x.elf`** in `firmware/` — the `_x` variant includes camera/codec firmware. Without it, MMAL camera components don't exist on the GPU.
2. **Update `firmware/config.txt`:**
   - `start_file=start_x.elf`
   - `gpu_mem=128` (camera needs GPU memory; 64MB is not enough)
   - Keep `enable_uart=1`
3. **Physically connect** the HQ Camera to the Pi A+ CSI port, verify the ribbon cable orientation.

### Test: `0-test-boot.c`
- Just runs existing `test-mbox.c` logic (serial number, memory, temp) to verify the Pi still boots with the new firmware.
- **Pass:** prints board info over UART. **Fail:** no output or panic.

### Files to modify
- `firmware/config.txt`
- `firmware/start_x.elf` (download from [raspberrypi/firmware](https://github.com/raspberrypi/firmware/tree/master/boot))

---

## Phase 1: GPU Memory Allocation via Mailbox

**Goal:** Learn to allocate, lock, and free GPU-side memory using mailbox property tags. This memory will later be used for the VCHIQ shared memory region and camera frame buffers.

### New mailbox tags to implement
| Tag | ID | Purpose |
|-----|----|---------|
| Allocate Memory | `0x0003000C` | Allocate contiguous GPU memory |
| Lock Memory | `0x0003000D` | Lock allocation, get bus address |
| Unlock Memory | `0x0003000E` | Unlock allocation |
| Release Memory | `0x0003000F` | Free allocation |

### New functions in `mbox.c`
```c
uint32_t gpu_mem_alloc(uint32_t size, uint32_t alignment, uint32_t flags);
uint32_t gpu_mem_lock(uint32_t handle);
void gpu_mem_unlock(uint32_t handle);
void gpu_mem_free(uint32_t handle);
```

### Test: `1-gpu-alloc.c`
- Allocate 4KB of GPU memory (aligned to 4096, flags=0xC for direct uncached)
- Lock it to get a bus address
- Print the handle and bus address
- Unlock and free
- **Pass:** valid non-zero handle and bus address. **Fail:** handle=0 or panic.

### Test: `1-gpu-alloc-large.c`
- Allocate 512KB (what we'll need for VCHIQ slots)
- Verify the bus address is valid
- **Pass:** successful allocation of large block.

### Files to modify
- `src/camera/code/mbox.h` — add tag definitions and function prototypes
- `src/camera/code/mbox.c` — implement GPU memory functions

---

## Phase 2: VCHIQ Shared Memory Setup

**Goal:** Set up the VCHIQ shared memory structure and tell the GPU about it via the `VCHIQ_INIT` mailbox tag.

### Key protocol details
- Allocate ~270KB via GPU memory alloc (from Phase 1)
- Fill in `vchiq_slot_zero` structure at the base:
  - `magic = 'VCHI'` (0x56434849)
  - `version = 8`, `version_min = 3`
  - `slot_size = 4096`, `max_slots = 128`, `max_slots_per_side = 64`
  - Initialize `master` (GPU) and `slave` (ARM) shared state structs
  - Divide data slots: GPU gets slots 0-31, ARM gets slots 32-63
- Send bus address to GPU via mailbox tag `0x00048010`

### New files
- `src/camera/code/vchiq.h` — slot_zero struct, shared_state struct, remote_event struct, slot_info struct, constants
- `src/camera/code/vchiq.c` — `vchiq_init_slots()`, `vchiq_platform_init()`

### Test: `2-vchiq-init.c`
- Allocate GPU memory for slots
- Fill in slot_zero structure
- Send VCHIQ_INIT mailbox message
- Check return status (0 = success)
- **Pass:** mailbox returns success (0). **Fail:** returns error or panic.

### Critical pitfalls
- Structs must NOT be `__attribute__((packed))` — GPU firmware rejects them
- Bus address passed to GPU should be OR'd with `0xC0000000` (uncached alias for L2)
- `VCHIQ_ENABLE_DEBUG` changes struct sizes — do NOT enable it

### Files to create/modify
- `src/camera/code/vchiq.h` (new)
- `src/camera/code/vchiq.c` (new)
- `src/camera/code/mbox.h` — add `VCHIQ_INIT` tag (`0x00048010`)

---

## Phase 3: VCHIQ Doorbell & Connect Handshake

**Goal:** Set up interrupt-driven doorbell signaling and complete the VCHIQ connection handshake with the GPU.

### Doorbell registers (BCM2835 physical)
- `BELL0` = `0x2000B840` — ARM reads this; GPU writes to signal ARM
- `BELL2` = `0x2000B848` — ARM writes this to signal GPU

### Steps
1. Set `slave.initialised = 1` in shared memory
2. Ring doorbell (write 0 to BELL2) to notify GPU
3. Poll for `master.initialised = 1` (GPU is ready)
4. Write a `CONNECT` message (type=1) into an ARM slot
5. Advance `slave.tx_pos`, update slot queue, ring doorbell
6. Poll for GPU's `CONNECT` reply in master slots
7. Connection established!

### New code
- `vchiq_ring_doorbell()` — write to BELL2
- `vchiq_poll_doorbell()` — read BELL0 (polling, no IRQ yet)
- `vchiq_send_msg()` — write a message header + payload into current slot
- `vchiq_recv_msg()` — read a message from GPU's slot queue
- `vchiq_connect()` — full connect handshake

### Test: `3-vchiq-connect.c`
- Initialize VCHIQ (from Phase 2)
- Set slave.initialised, ring doorbell
- Wait for master.initialised
- Send CONNECT, receive CONNECT reply
- **Pass:** prints "VCHIQ connected!" **Fail:** timeout or wrong message type.

### Note on polling vs interrupts
Start with **polling** (simpler). We can add IRQ-driven doorbell later if needed for performance. Bare-metal single-threaded polling is fine for a single camera capture.

### Files to modify
- `src/camera/code/vchiq.h` — message types, header struct
- `src/camera/code/vchiq.c` — doorbell, send/recv, connect

---

## Phase 4: VCHIQ Service — Open 'mmal'

**Goal:** Open the MMAL service over the VCHIQ connection.

### Steps
1. Send `OPEN` message (type=2) with fourcc `'mmal'` (0x6D6D616C)
2. Wait for `OPENACK` message (type=3) from GPU
3. Record the service handle (localport + remoteport)

### Test: `4-mmal-open.c`
- Full VCHIQ init + connect (from Phase 3)
- Send OPEN with fourcc='mmal'
- Receive OPENACK
- **Pass:** prints "MMAL service opened, remoteport=X". **Fail:** timeout or no OPENACK.

### Files to modify
- `src/camera/code/vchiq.c` — `vchiq_open_service()`
- `src/camera/code/mmal.h` (new) — MMAL message header, message types
- `src/camera/code/mmal.c` (new) — MMAL message send/receive helpers

---

## Phase 5: MMAL Camera Component

**Goal:** Create the camera component on the GPU and query its ports.

### MMAL messages to implement
| Message | Type ID | Purpose |
|---------|---------|---------|
| `COMPONENT_CREATE` | 4 | Create "ril.camera" |
| `COMPONENT_CREATE_REPLY` | 4 (response) | Returns component handle + port counts |
| `PORT_INFO_GET` | 8 | Query port details (format, buffer requirements) |
| `PORT_INFO_SET` | 9 | Set port format (resolution, encoding) |
| `COMPONENT_ENABLE` | 6 | Enable the component |

### Steps
1. Send `COMPONENT_CREATE` with name="ril.camera"
2. Receive reply with component handle and port counts
3. Send `PORT_INFO_GET` for the still capture port (port type=output, index=2)
4. Print port capabilities (supported resolutions, encodings)

### Test: `5-mmal-camera-create.c`
- Full init + connect + open mmal (from Phase 4)
- Create "ril.camera" component
- Print component handle, number of ports
- Query capture port info
- **Pass:** valid component handle, port info shows resolution/format. **Fail:** component creation fails.

### Test: `5-mmal-camera-config.c`
- Same as above, plus:
- Set capture port to a specific resolution (e.g., 640x480) and encoding (e.g., RGB24 or JPEG)
- Set `MMAL_PARAMETER_CAMERA_NUM = 0`
- Enable the component
- **Pass:** port_info_set and component_enable succeed.

### Files to create/modify
- `src/camera/code/mmal.h` — all MMAL struct definitions (msg header, component create, port info, etc.)
- `src/camera/code/mmal.c` — `mmal_create_component()`, `mmal_port_info_get()`, `mmal_port_info_set()`, `mmal_component_enable()`

---

## Phase 6: Camera Capture — Single Frame

**Goal:** Actually capture one image from the camera.

### Steps
1. Allocate a frame buffer via GPU memory (large enough for one frame)
2. Enable the capture port via `PORT_ACTION` (type=10, action=ENABLE)
3. Submit empty buffer to GPU via `BUFFER_FROM_HOST` (type=11)
4. Set `MMAL_PARAMETER_CAPTURE = 1` via `PORT_PARAMETER_SET` (type=14)
5. Wait for `BUFFER_TO_HOST` (type=12) — GPU returns the filled buffer
6. For large frames, handle `BULK_RX` transfer to get actual pixel data
7. Read image data from the buffer

### Test: `6-capture-frame.c`
- Full pipeline from Phase 0-5
- Capture a single 640x480 RGB frame (small to start)
- Dump first 64 bytes of frame data over UART as hex
- **Pass:** receives non-zero buffer data, first bytes look like valid pixel values. **Fail:** buffer timeout or all zeros.

### Test: `6-capture-jpeg.c`
- Same pipeline but request JPEG encoding
- Dump JPEG header bytes (should start with `0xFF 0xD8`)
- **Pass:** JPEG magic bytes present. **Fail:** wrong data or timeout.

### Files to modify
- `src/camera/code/mmal.c` — `mmal_port_enable()`, `mmal_buffer_submit()`, `mmal_capture()`, `mmal_buffer_receive()`
- `src/camera/code/vchiq.c` — bulk transfer support if needed

---

## Phase 7: Image Output (Future — Display or Serial Dump)

**Goal:** Do something useful with the captured image.

### Options (to decide later)
- **UART hex dump** — dump raw bytes for offline reconstruction
- **Framebuffer display** — allocate GPU framebuffer via mailbox, blit captured image
- **SPI display** — drive an SPI LCD panel
- **SD card write** — write JPEG to SD card

This phase is deferred — we'll decide the output method after we have a working capture.

---

## File Structure (Final)

```
src/camera/code/
├── Makefile
├── mbox.h / mbox.c          # Mailbox interface (existing + GPU mem alloc)
├── vchiq.h / vchiq.c        # VCHIQ transport layer (slots, doorbell, messages)
├── mmal.h / mmal.c          # MMAL message layer (components, ports, buffers)
├── 0-test-boot.c            # Phase 0: verify firmware
├── 1-gpu-alloc.c            # Phase 1: GPU memory allocation
├── 1-gpu-alloc-large.c      # Phase 1: large allocation test
├── 2-vchiq-init.c           # Phase 2: VCHIQ shared memory setup
├── 3-vchiq-connect.c        # Phase 3: VCHIQ connect handshake
├── 4-mmal-open.c            # Phase 4: open MMAL service
├── 5-mmal-camera-create.c   # Phase 5: create camera component
├── 5-mmal-camera-config.c   # Phase 5: configure camera
├── 6-capture-frame.c        # Phase 6: capture a frame
└── 6-capture-jpeg.c         # Phase 6: capture JPEG
```

### Makefile update
```makefile
# Uncomment one at a time, cs140e style:
PROGS := 0-test-boot.c
# PROGS := 1-gpu-alloc.c
# PROGS := 2-vchiq-init.c
# PROGS := 3-vchiq-connect.c
# PROGS := 4-mmal-open.c
# PROGS := 5-mmal-camera-create.c
# PROGS := 6-capture-frame.c

COMMON_SRC := mbox.c vchiq.c mmal.c
```

---

## Implementation Order

We implement **one phase at a time**, verifying on real hardware before moving to the next:

1. **Phase 0** — Update firmware, test boot *(~30 min, hardware only)*
2. **Phase 1** — GPU memory alloc/lock/free *(~1-2 sessions)*
3. **Phase 2** — VCHIQ slot memory setup + VCHIQ_INIT *(~1-2 sessions)*
4. **Phase 3** — Doorbell + CONNECT handshake *(~2-3 sessions — hardest debugging)*
5. **Phase 4** — Open MMAL service *(~1 session)*
6. **Phase 5** — Create & configure camera component *(~2-3 sessions)*
7. **Phase 6** — Capture a frame *(~2-3 sessions)*

Each phase produces a test binary that prints clear pass/fail output over UART.

---

## Verification Plan

At each phase:
1. Build: `make -C src/camera/code`
2. Deploy: `my-install /dev/ttyUSB0 <test>.bin`
3. Read UART output for pass/fail
4. Only move to next phase when current phase passes consistently

## Starting Point

We begin with **Phase 0** (firmware update) and **Phase 1** (GPU memory allocation), since these build directly on the existing mailbox code.
