# Arduino Nicla Vision – STM32CubeIDE templates (USB upload via the Arduino bootloader)

Dual-core STM32CubeIDE (HAL) starting points for the **Arduino Nicla Vision**
(STM32H747AII6). They are flashed through the **stock Arduino bootloader over USB
(DFU)**, so you keep the bootloader and don't need an ST-Link.

## The projects

| Project | What it does | Start from it when… |
|---------|--------------|---------------------|
| **[`NICLA_VISION_TEMPLATE`](NICLA_VISION_TEMPLATE)** | Bare board bring-up: bootloader hand-off, 480 MHz clock, MPU and caches, PMIC power rails over I2C2, CM4 boot. The green LED blinks so you can see it runs. | you want a clean base and add your own peripherals |
| **[`NICLA_VISION_USB_CDC_HS`](NICLA_VISION_USB_CDC_HS)** | The same bring-up (at 120 MHz) **plus USB CDC (virtual COM port) on the USB HS peripheral with the ULPI PHY**. USB starts only *after* the PMIC has powered the PHY, and the 1200-baud auto-reset lets uploads run without pressing reset. Prints `Device Found!` and toggles the green LED every second. | you want `printf`-style output / a serial link to the PC |

Both are complete CubeMX projects (`.ioc` included). All custom code is inside
`USER CODE` blocks, so you can open the `.ioc`, add peripherals and regenerate.

> **New to this, or converting your own project?** Read
> **[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md)**. It explains every step
> from an empty workspace: pins, clocks, PMIC, USB, the bootloader changes, the
> 1200-baud auto-reset, `.bin` output, the upload script and the CubeIDE button,
> plus troubleshooting.

## Quick start

1. Install **STM32CubeIDE**, and the **Arduino IDE** with the *Arduino Mbed OS Nicla
   Boards* core. That core provides `dfu-util` and the USB driver.
2. `git clone https://github.com/bastian123321/Arduino-Nicla-Vision-STM32CUBEIDE-Template.git`
3. In STM32CubeIDE: *File → Import → General → Existing Projects into Workspace*,
   select the project folder (e.g. `NICLA_VISION_USB_CDC_HS`), and tick its `_CM7`
   and `_CM4` projects.
4. Build both (**Ctrl+B**).
5. **Double-tap reset** on the board (the green LED pulses: bootloader mode).
6. *Run → External Tools → Upload via USB (Arduino bootloader)*. If it isn't in the
   menu yet, look under *External Tools Configurations… → Program*.

   Or from a terminal in the project folder:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\tools\upload.ps1
   ```

With `NICLA_VISION_USB_CDC_HS` running, later uploads need no double-tap. The
script reboots the board into the bootloader through its COM port. Open that COM
port at any baud rate except 1200 (1200 is the reboot signal) to see the output.

## Flash layout

| Region             | Address                   | Written by                         |
|--------------------|---------------------------|------------------------------------|
| Arduino bootloader | `0x08000000–0x0803FFFF`   | factory (read-only over DFU)       |
| CM7 application    | `0x08040000–0x080FFFFF`   | `<project>_CM7` (768 KB)           |
| CM4 application    | `0x08100000–0x081FFFFF`   | `<project>_CM4` (1 MB)             |

## What makes these projects bootloader-compatible

Compared with a plain CubeMX project flashed by SWD at `0x08000000`:

* **CM7 linker script:** `FLASH` starts at `0x08040000` (768 KB).
* **`Bootloader_Handoff()`**: the first call in CM7 `main()`. The bootloader jumps
  in *without a reset*, so this function restores VTOR, NVIC, caches, MPU, and the
  RCC peripheral resets and clock enables. Otherwise the D2 domain can't enter
  STOP and the CM4 boot handshake ends in `Error_Handler()`. The caches are turned
  off from an `-O2` helper because CMSIS 5.1.1 `SCB_DisableDCache()` hangs at `-O0`.
* **CM4 boot address** is set to `0x08100000` before CM4 is released.
* **Supply = LDO.** The bootloader already configured it, and it can only be set
  once per power-up.
* **`.bin` output** is enabled for both cores, in Debug and Release.
* **`tools/upload.ps1`** plus a CubeIDE launcher (`CM7/Upload via USB (Arduino bootloader).launch`)
  that writes CM4 → `0x08100000` and CM7 → `0x08040000` with `dfu-util`.
* *USB CDC project only:* **1200-baud touch.** Opening the port at 1200 baud and
  dropping DTR writes `0xDF59` to `RTC->BKP0R` and resets, and the bootloader then
  stays in DFU mode. This is the same trick the Arduino IDE uses.

## Using a template for your own project

* **Simplest:** copy the project folder and work in it. The upload script reads the
  project name from the `.ioc` file, so it needs no edits.
* **New name:** create a new CubeMX project and follow the guide, copying
  `tools/` and the code blocks from a template. Renaming an existing dual-core
  CubeIDE project means renaming the `.ioc`, both `.project` names and the
  `.launch` files, which is easy to get wrong.

**Upload both cores at least once.** The CM7 waits for the CM4 during start-up. If
the firmware ever hangs before USB comes up, **double-tap reset** to get back to
the bootloader. Uploads over USB can't overwrite the bootloader.

## Debugging with an ST-Link (optional)

The normal CubeIDE debug configurations still work with the bootloader in place.
They only program the application sectors. On reset the bootloader runs first,
then jumps into your code, and breakpoints in `main()` are still hit.
