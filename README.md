# Nicla Vision – STM32CubeIDE firmware, uploaded over USB

STM32CubeIDE (HAL, dual core) firmware for the **Arduino Nicla Vision** (STM32H747AII6)
that is flashed through the **stock Arduino bootloader over USB (DFU)**. You don't need
to erase the bootloader, and you don't need an ST-Link.

The demo: CM7 powers up the PMIC rails, enumerates a USB CDC port ("NICLA Vision
VCPort") and prints LSM6DSOX accelerometer, gyro and temperature data. CM4 runs a TIM7 tick.

## Flash layout

| Region                    | Address                  | Who writes it            |
|---------------------------|--------------------------|--------------------------|
| Arduino bootloader        | `0x08000000–0x0803FFFF`  | factory (read-only over DFU) |
| **CM7 application**       | `0x08040000–0x080FFFFF`  | `CM7` project            |
| **CM4 application**       | `0x08100000–0x081FFFFF`  | `CM4` project            |

This is the same layout Arduino uses with the "1MB M7 + 1MB M4" flash split.

## Differences from a bare-metal (SWD at 0x08000000) CubeMX project

* `CM7/STM32H747AIIX_FLASH.ld`: `FLASH` starts at `0x08040000` (768K).
* `CM7/Core/Src/main.c`, `Bootloader_Handoff()`: the bootloader jumps to the app
  *without* a reset. This function puts VTOR, NVIC, caches, MPU and the RCC
  peripheral resets/clock enables back to their reset state. Without it, the clock
  enables the bootloader left on D2 peripherals (USB, I2C2) stop the D2 domain from
  entering STOP. The CM4 boot handshake then times out and the CM7 ends up in
  `Error_Handler()`.
* The CM4 boot address is set to `0x08100000` before CM4 is released.
* `CM7/USB_DEVICE/App/usbd_cdc_if.c`: the **1200-baud touch**. If you open the COM port at
  1200 baud and drop DTR, the board writes `0xDF59` to `RTC->BKP0R` and resets, and the
  bootloader then stays in DFU mode. This is the same mechanism the Arduino IDE uses.
  `GET/SET_LINE_CODING` are implemented properly as well.
* Both projects have *Convert to binary file* enabled, so every build produces a `.bin`.

Everything is inside `USER CODE` blocks, so regenerating code from the `.ioc` keeps it.
The linker-script change and the `.cproject` option are outside CubeMX's control
and survive regeneration as well.

## Build

Open STM32CubeIDE → *File → Import → General → Existing Projects into Workspace* →
select this folder (tick both `NICLA_VISION_USB_SPI_CM7` and `_CM4`) → build both
(`Ctrl+B`).

## Upload over USB

Requirements: the Arduino IDE with the **Arduino Mbed OS Nicla Boards** core installed.
It provides `dfu-util` and the Windows DFU driver.

From CubeIDE: *Run → External Tools → Upload via USB (Arduino bootloader)*. The first
time, open *External Tools Configurations…* and you will find it under *Program*.

From a terminal:

```powershell
.\tools\upload.ps1                 # Debug build, both cores
.\tools\upload.ps1 -Config Release
.\tools\upload.ps1 -Core CM7       # only the M7 image
```

The script does the 1200-baud touch on the running board, or waits for you to
**double-tap reset** (the green LED pulses). It then writes CM4 → `0x08100000` and
CM7 → `0x08040000` with `dfu-util` and starts the firmware.

> **Upload both cores at least once.** CM7 waits for CM4 to boot. If `0x08100000`
> is empty, CM7 stops in `Error_Handler()`.

If the firmware ever hangs before USB comes up, **double-tap reset** to get back to
the bootloader. Nothing you upload this way can overwrite it.

## Debugging with an ST-Link (optional)

SWD debugging still works without removing the bootloader. The launch configuration
programs only the application sectors. On reset the bootloader runs first, then jumps
into your code, and breakpoints in `main()` are still hit.

## Restoring the bootloader

If a board has had its bootloader erased, flash
`%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\mbed_nicla\<ver>\bootloaders\NICLA_VISION\bootloader.bin`
at `0x08000000` with STM32CubeProgrammer, or use *Burn Bootloader* in the Arduino IDE
with a debugger attached.
