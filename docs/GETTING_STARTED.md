# Getting started: a Nicla Vision STM32CubeIDE project uploaded over USB

This guide starts from an empty STM32CubeIDE workspace and ends with a dual-core
project that you build in CubeIDE and upload through the **Arduino bootloader
over USB**. You don't need an ST-Link and you don't erase the bootloader.

If you just want to use this repository as-is, read the
[README](../README.md). Use this guide when you are starting a new project or
converting an existing one.

**Contents**

1. [How it works](#1-how-it-works)
2. [What you need](#2-what-you-need)
3. [Create the CubeMX project](#3-create-the-cubemx-project)
4. [Board bring-up code (power, clock, USB)](#4-board-bring-up-code-power-clock-usb)
5. [Make the firmware bootloader-compatible](#5-make-the-firmware-bootloader-compatible)
6. [Optional: reset into the bootloader from the PC (1200-baud touch)](#6-optional-reset-into-the-bootloader-from-the-pc-1200-baud-touch)
7. [Produce .bin files](#7-produce-bin-files)
8. [Upload script and CubeIDE button](#8-upload-script-and-cubeide-button)
9. [First upload and day-to-day workflow](#9-first-upload-and-day-to-day-workflow)
10. [Debugging with an ST-Link (optional)](#10-debugging-with-an-st-link-optional)
11. [Troubleshooting](#11-troubleshooting)
12. [FAQ](#12-faq)

---

## 1. How it works

The Nicla Vision ships with an Arduino bootloader at the start of flash. On every
reset it runs first and then either:

* **starts your application** at `0x08040000` (normal case), or
* **stays in DFU mode** (USB ID `2341:035f`) so the PC can write new firmware.
  It does this when you double-tap reset, when no valid application is present,
  or when the application asked for it by writing `0xDF59` to `RTC->BKP0R`.

The PC writes firmware with `dfu-util`, which the Arduino IDE installs for you.

### Flash layout (2 MB internal flash)

| Region             | Address range             | Contents                         |
|--------------------|---------------------------|----------------------------------|
| Arduino bootloader | `0x08000000 – 0x0803FFFF` | factory; read-only over DFU      |
| CM7 application    | `0x08040000 – 0x080FFFFF` | your `*_CM7` project (768 KB)    |
| CM4 application    | `0x08100000 – 0x081FFFFF` | your `*_CM4` project (1 MB)      |

This is the same layout the Arduino IDE uses with the "1MB M7 + 1MB M4" split.

### What's different from a normal (SWD) CubeMX project

A CubeMX project normally assumes it is the first code that runs after reset,
at `0x08000000`. Here three things change:

1. The CM7 is **linked at `0x08040000`** instead of `0x08000000`.
2. The bootloader jumps into your code **without resetting the chip**. Its caches,
   clocks and peripherals are still set up, so the application must **clean up
   first** (see [section 5](#5-make-the-firmware-bootloader-compatible)).
3. A **`.bin` file** is produced and written with `dfu-util` instead of an ST-Link.

---

## 2. What you need

* **STM32CubeIDE** (tested with 1.19.0 / CubeMX 6.15).
* **Arduino IDE** with the **"Arduino Mbed OS Nicla Boards"** core installed
  (*Tools → Board → Boards Manager*). It provides `dfu-util.exe` and the Windows
  USB driver for the bootloader. You never have to open the Arduino IDE again.
* A Nicla Vision **with its bootloader**. To check, double-tap reset: the green
  LED should pulse, and Windows should see a new USB device.

> **Bootloader erased?** Flash
> `%LOCALAPPDATA%\Arduino15\packages\arduino\hardware\mbed_nicla\<version>\bootloaders\NICLA_VISION\bootloader.bin`
> to `0x08000000` once with STM32CubeProgrammer and an ST-Link. Don't use full chip
> erase afterwards.

---

## 3. Create the CubeMX project

### 3.1 New project

1. *File → New → STM32 Project*.
2. Part number: **STM32H747AII6** (listed as `STM32H747AIIx`).
3. Name it (e.g. `MY_NICLA`). Targeted project type: **STM32Cube**, and keep the
   default **dual-core** structure. CubeIDE creates `MY_NICLA_CM7` and
   `MY_NICLA_CM4`.

### 3.2 Pinout

The board wiring sets these pins, so configure them as follows:

| Pin(s) | Function | Configure as | Used by |
|--------|----------|--------------|---------|
| PH0 | 25 MHz oscillator input | RCC → HSE: **BYPASS Clock Source** | CM7 |
| PH1 | Oscillator **enable** | (done in code, see 4.1) | CM7 |
| PF0 / PF1 | I2C2 SDA / SCL | I2C2 → I2C, Timing `0x107075B0` | CM7 (PMIC @ `0x08`) |
| PA2 | USB PHY reset | GPIO_Output, label **`USB_PHY_RST`**, pull-down, initial low | CM7 |
| PA3, PA5, PB0, PB1, PB5, PB10–PB13, PC0, PC2_C, PC3_C | USB ULPI | USB_OTG_HS → **External Phy: Device_Only** (ULPI) | CM7 |
| PE3 / PC13 / PF4 | LED red / green / blue (active low) | GPIO_Output, labels `LED_R`, `LED_G`, `LED_B`, initial **high** (= off) | any |
| PF7 / PF8 / PF11 | SPI5 SCK / MISO / MOSI | SPI5 Full-Duplex Master | CM7 (LSM6DSOX IMU) |
| PF6 | IMU chip-select | GPIO_Output, label `CS_up`, initial high | CM7 |
| PA13 / PA14 | SWD | SYS → Debug: **Serial Wire** | – |

For each pin, set **Pin Context Assignment** (right-click in the pinout view) to
the core that uses it.

### 3.3 Middleware

* **USB_DEVICE (CortexM7)** → Class for HS IP: **Communication Device Class
  (Virtual Port Com)**. Change the product string if you like (this project uses
  `NICLA Vision VCPort`).
* USB_OTG_HS speed: **Full Speed** (`PCD_SPEED_FULL`), with the OTG_HS global
  interrupt enabled in NVIC.

### 3.4 Clock configuration

* **Power regulator: LDO** (*RCC → Supply Source → PWR_LDO_SUPPLY*). This is
  **mandatory**: the bootloader already set the supply to LDO, and that register
  can only be written once per power-up.
* HSE = **25 MHz**, bypass.
* PLL1: HSE → `/M = 5`, `×N = 48`, `/P = 2` → **SYSCLK = 120 MHz** (raise it if you
  need to; this project runs at 120 MHz).
* HSI48 on (USB clock source).

### 3.5 Project Manager → Advanced Settings

In the "Generated Function Calls" list, tick **Do Not Generate Function Call**
for **`MX_USB_DEVICE_Init`** (CortexM7). USB must only start *after* the PMIC
has powered the PHY, so you call it yourself (4.3).

Generate code.

---

## 4. Board bring-up code (power, clock, USB)

All of this goes in **CM7 `Core/Src/main.c`**, inside the `USER CODE` blocks
named below, so regenerating from the `.ioc` keeps it.

### 4.1 Enable the 25 MHz oscillator (before `SystemClock_Config`)

The external oscillator has an enable pin (PH1). Turn it on before switching to
HSE:

```c
  /* USER CODE BEGIN Init */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  GPIO_InitTypeDef gpio_osc_init_structure;
  gpio_osc_init_structure.Pin   = GPIO_PIN_1;
  gpio_osc_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_osc_init_structure.Pull  = GPIO_PULLUP;
  gpio_osc_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &gpio_osc_init_structure);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, 1);
  /* USER CODE END Init */
```

### 4.2 Power rails: PMIC over I2C2

The board's power-management IC (I2C address `0x08`) must switch on the rails for
the USB PHY, the sensors and so on:

```c
  /* USER CODE BEGIN 2 */
  uint8_t data[2];

  data[0] = 0x9c; data[1] = 0x80;  // charger LED driver controlled by software
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x9e; data[1] = 0x20;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x42; data[1] = 0x02;  // SW3 current limit 1.5 A
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x94; data[1] = 0xa0;  // VBUS current limit 1.5 A
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x4d; data[1] = 0x01;  // LDO1 on
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x50; data[1] = 0x01;  // LDO2 on
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x53; data[1] = 0x01;  // LDO3 1.2 V
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);
  data[0] = 0x3b; data[1] = 0x81;  // SW2 on
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  HAL_Delay(250);                  // let the rails settle
```

The bootloader programs the PMIC too, so running this again does no harm. Keep
it: the same firmware then also works when flashed by SWD without a bootloader.

### 4.3 Start USB after the power is up

```c
  HAL_GPIO_WritePin(USB_PHY_RST_GPIO_Port, USB_PHY_RST_Pin, GPIO_PIN_SET); // release PHY reset
  HAL_Delay(20);
  MX_USB_DEVICE_Init();
  /* ... your application init ... */
  /* USER CODE END 2 */
```

Send data with `CDC_Transmit_HS(buf, len)` (include `usbd_cdc_if.h`).

### 4.4 CM4 core

The CM4 project needs nothing special. The generated boot sequence (HSEM
notification, then STOP until the CM7 releases it) is correct. In this repo the
CM4 blinks the blue LED from TIM7.

---

## 5. Make the firmware bootloader-compatible

These steps are **required**.

### 5.1 CM7 linker script

In `MY_NICLA_CM7/STM32H747AIIX_FLASH.ld`, change the `FLASH` line in `MEMORY`:

```
  FLASH   (rx)   : ORIGIN = 0x08040000, LENGTH = 768K
```

* Leave `_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1);` alone. That is
  `0x24080000`, and the bootloader accepts any stack starting with `0x20`,
  `0x24`, `0x30` or `0x38`.
* Leave the CM4 linker script alone: it already starts at `0x08100000`.

### 5.2 Clean hand-off from the bootloader

The bootloader starts your app **without a reset**. Without cleaning up, the CM7
hangs before USB comes up:

* The bootloader leaves CM7 clocks enabled on **D2-domain peripherals** (USB,
  I2C2…). While the CM7 owns anything in D2, the D2 domain can't enter STOP. The
  generated "wait for CM4 to enter STOP" loop then times out into
  `Error_Handler()`.
* The **caches** are on, and peripherals are still configured.

**a)** `/* USER CODE BEGIN PD */`

```c
/* Start of the CM4 image in flash (must match CM4/STM32H747AIIX_FLASH.ld) */
#define CM4_IMAGE_ADDRESS    0x08100000U
```

**b)** `/* USER CODE BEGIN PFP */`

```c
static void Bootloader_Handoff(void);
static void Disable_Caches(void);
```

**c)** `/* USER CODE BEGIN 1 */`: the **very first** code in `main()`:

```c
  Bootloader_Handoff();

  // Release the M4 (it is held off by the option bytes), pointing it at its image
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_SYSCFG_CM4BootAddConfig(SYSCFG_BOOT_ADDR0, CM4_IMAGE_ADDRESS);
  HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);
```

**d)** `/* USER CODE BEGIN 4 */`

```c
/*
 * Built with -O2 on purpose: CMSIS 5.1.1 SCB_DisableDCache() disables the
 * cache before cleaning it, so at -O0 its loop counters live on the (cached)
 * stack, get read back stale from RAM and the loop never ends.
 */
__attribute__((optimize("O2"))) static void Disable_Caches(void)
{
  SCB_DisableICache();
  SCB_DisableDCache();
}

/*
 * The bootloader jumps here without a reset. Put VTOR, NVIC, caches, MPU and
 * the RCC peripheral resets / clock enables back to their power-on state.
 */
static void Bootloader_Handoff(void)
{
  extern uint32_t g_pfnVectors[];

  __disable_irq();

  /* Our own vector table, wherever we were linked */
  SCB->VTOR = (uint32_t)g_pfnVectors;
  __DSB();
  __ISB();

  /* No interrupts left over from the bootloader */
  SysTick->CTRL = 0;
  for (uint32_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFU;
    NVIC->ICPR[i] = 0xFFFFFFFFU;
  }

  /* Caches off and MPU off, as after reset */
  Disable_Caches();
  HAL_MPU_Disable();

  /* Reset every peripheral the bootloader may have used */
  __HAL_RCC_AHB1_FORCE_RESET();
  __HAL_RCC_AHB2_FORCE_RESET();
  __HAL_RCC_AHB3_FORCE_RESET();
  __HAL_RCC_AHB4_FORCE_RESET();
  __HAL_RCC_APB1L_FORCE_RESET();
  __HAL_RCC_APB1H_FORCE_RESET();
  __HAL_RCC_APB2_FORCE_RESET();
  __HAL_RCC_APB3_FORCE_RESET();
  __HAL_RCC_APB4_FORCE_RESET();
  __HAL_RCC_AHB1_RELEASE_RESET();
  __HAL_RCC_AHB2_RELEASE_RESET();
  __HAL_RCC_AHB3_RELEASE_RESET();
  __HAL_RCC_AHB4_RELEASE_RESET();
  __HAL_RCC_APB1L_RELEASE_RESET();
  __HAL_RCC_APB1H_RELEASE_RESET();
  __HAL_RCC_APB2_RELEASE_RESET();
  __HAL_RCC_APB3_RELEASE_RESET();
  __HAL_RCC_APB4_RELEASE_RESET();

  /* Give up CM7's clock enables so D2 can go to STOP.
   * AHB3ENR also holds the FLASH/TCM/AXI SRAM bits: only touch peripherals. */
  RCC->AHB1ENR  = 0;
  RCC->AHB2ENR  = 0;
  RCC->AHB3ENR &= ~(RCC_AHB3ENR_MDMAEN | RCC_AHB3ENR_DMA2DEN | RCC_AHB3ENR_JPGDECEN |
                    RCC_AHB3ENR_FMCEN | RCC_AHB3ENR_QSPIEN | RCC_AHB3ENR_SDMMC1EN);
  RCC->AHB4ENR  = 0;
  RCC->APB1LENR = 0;
  RCC->APB1HENR = 0;
  RCC->APB2ENR  = 0;
  RCC->APB3ENR  = 0;
  RCC->APB4ENR  = RCC_APB4ENR_RTCAPBEN;
  __DSB();

  __enable_irq();
}
```

The clock tree itself needs no work: the generated `SystemInit()` already resets
it to HSI before `main()` runs.

`Bootloader_Handoff()` does no harm without a bootloader (e.g. when debugging
over SWD), so leave it in all the time.

### 5.3 Check `SystemClock_Config`

It must call `HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);`. With any SMPS setting the
call fails silently and voltage scaling may hang.

---

## 6. Optional: reset into the bootloader from the PC (1200-baud touch)

**What it's for:** uploads only work while the bootloader is in DFU mode. While your
firmware runs, the board has to be put back into the bootloader first. You can
do that two ways:

| Method | How | Needs |
|--------|-----|-------|
| By hand | **Double-tap reset** | nothing; always works |
| From the PC | Script opens the COM port at **1200 baud** and closes it. The firmware sees this, writes `0xDF59` to `RTC->BKP0R` and resets, so the bootloader stays in DFU | firmware with USB CDC + this section |

The PC can only send that signal through a COM port your firmware creates, so
this only applies to projects that use USB CDC. It is the same mechanism the
Arduino IDE uses ("1200 bps touch").

### 6.1 `Core/Inc/main.h`

```c
/* USER CODE BEGIN EC */
/* RTC->BKP0R value that keeps the Arduino bootloader in DFU mode after reset */
#define ARDUINO_BOOTLOADER_MAGIC   0xDF59U
/* USER CODE END EC */
```

```c
/* USER CODE BEGIN EFP */
void Enter_Arduino_Bootloader(void);
/* USER CODE END EFP */
```

### 6.2 `Core/Src/main.c`, in `USER CODE BEGIN 4`

```c
void Enter_Arduino_Bootloader(void)
{
  __disable_irq();

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_RTC_CLK_ENABLE();
  RTC->BKP0R = ARDUINO_BOOTLOADER_MAGIC;
  __DSB();

  NVIC_SystemReset();
}
```

You can also call this from your own code, e.g. on a command received over USB.

### 6.3 `USB_DEVICE/App/usbd_cdc_if.c`

```c
/* USER CODE BEGIN INCLUDE */
#include <string.h>
#include "main.h"
/* USER CODE END INCLUDE */
```

```c
/* USER CODE BEGIN PRIVATE_VARIABLES */
/* Line coding as last set by the host (baud LE32, stop bits, parity, data bits) */
static uint8_t line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 }; /* 115200 8N1 */
static uint8_t dtr_active = 0;
/* USER CODE END PRIVATE_VARIABLES */
```

```c
/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
static void CDC_Check_1200bps_Touch(void);
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */
```

In `CDC_Control_HS()`, replace the three empty cases:

```c
  case CDC_SET_LINE_CODING:
    memcpy(line_coding, pbuf, sizeof(line_coding));
    CDC_Check_1200bps_Touch();
    break;

  case CDC_GET_LINE_CODING:
    memcpy(pbuf, line_coding, sizeof(line_coding));
    break;

  case CDC_SET_CONTROL_LINE_STATE:
    /* No data stage: pbuf is the setup request, DTR is bit 0 of wValue */
    dtr_active = (((USBD_SetupReqTypedef *)pbuf)->wValue & 0x0001U) != 0U;
    CDC_Check_1200bps_Touch();
    break;
```

```c
/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
static void CDC_Check_1200bps_Touch(void)
{
  uint32_t baud = (uint32_t)line_coding[0]
                | ((uint32_t)line_coding[1] << 8)
                | ((uint32_t)line_coding[2] << 16)
                | ((uint32_t)line_coding[3] << 24);

  if ((baud == 1200U) && !dtr_active)
  {
    Enter_Arduino_Bootloader();
  }
}
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */
```

Implementing `GET_LINE_CODING` properly is also good practice: some terminal
programs misbehave when the device returns garbage there.

---

## 7. Produce .bin files

`dfu-util` writes raw binaries. Do this for **both** the CM7 and CM4 projects:

1. Right-click the project → **Properties**.
2. *C/C++ Build → Settings*, set Configuration to **[All configurations]**.
3. *Tool Settings → MCU Post build outputs* → tick **Convert to binary file (-O binary)**.
4. Apply.

Every build now produces `Debug/<project>_CM7.bin` and `Debug/<project>_CM4.bin`
(and the same under `Release/`).

---

## 8. Upload script and CubeIDE button

### 8.1 The script

Copy [`tools/upload.ps1`](../tools/upload.ps1) into a `tools` folder in the project
root, next to `CM4/`, `CM7/` and the `.ioc`. Then set the project name near the
top of the script:

```powershell
$ProjectName = 'MY_NICLA'      # the part of 'MY_NICLA_CM7' before _CM7
```

The script:

1. finds `dfu-util.exe` (on `PATH` or in the Arduino IDE install);
2. if the bootloader isn't already in DFU mode, does the 1200-baud touch on the
   board's COM port, or waits for you to double-tap reset;
3. writes `CM4.bin` → `0x08100000` and `CM7.bin` → `0x08040000`, then starts the firmware.

Options:

```powershell
.\tools\upload.ps1                      # Debug build, both cores
.\tools\upload.ps1 -Config Release
.\tools\upload.ps1 -Core CM7            # only the M7 image
.\tools\upload.ps1 -Port COM7           # don't auto-detect the COM port
.\tools\upload.ps1 -WaitSeconds 60      # wait longer for a double-tap
```

If your USB VID/PID isn't ST's default (`0483:5740`), add it to `$AppUsbIds`
in the script so the port is found automatically.

### 8.2 A button in CubeIDE: what's a `.launch` file?

CubeIDE (Eclipse) stores every saved *Run* / *External Tools* configuration as a
small XML `.launch` file. This repo has one,
`CM7/Upload via USB (Arduino bootloader).launch`, which runs `powershell.exe`
with `tools\upload.ps1`. When the file is inside a project, CubeIDE lists it
automatically.

**Option A: create it by hand** (simplest for a new project)

1. *Run → External Tools → External Tools Configurations…*
2. Right-click **Program** → **New Configuration**, name it `Upload via USB`.
3. **Location:**
   ```
   ${env_var:SystemRoot}\System32\WindowsPowerShell\v1.0\powershell.exe
   ```
4. **Working Directory** (use your CM7 project name):
   ```
   ${project_loc:MY_NICLA_CM7}\..
   ```
5. **Arguments:**
   ```
   -NoProfile -ExecutionPolicy Bypass -File "${project_loc:MY_NICLA_CM7}\..\tools\upload.ps1"
   ```
6. *Common* tab → tick **Display in favorites menu → External Tools**.
   Optionally pick **Shared file** and point it at your CM7 project folder. CubeIDE
   then saves the `.launch` file there, so it ends up in git with the project.
7. **Apply**, **Run**.

**Option B: copy the file**

1. Copy `CM7/Upload via USB (Arduino bootloader).launch` into your CM7 folder.
2. In a text editor, replace both `NICLA_VISION_USB_SPI_CM7` with your CM7 project name.
3. In CubeIDE select the project and press **F5** (refresh).

**It doesn't appear under *Run → External Tools*:** that menu only shows
favourites and recently run tools. Open *External Tools Configurations…* and look
under **Program**, run it once, or add it with *Organize Favorites…*.

The output appears in the CubeIDE **Console** view.

---

## 9. First upload and day-to-day workflow

**First upload** (the board is still running other firmware):

1. Build both projects (**Ctrl+B**).
2. **Double-tap reset**: the green LED pulses, and the bootloader is in DFU mode.
3. Click **Upload via USB** (or run `.\tools\upload.ps1`).
4. You should see `Bootloader found`, two `File downloaded successfully` lines and
   `Done - firmware is starting.` Your COM port appears a second later.

**After that** (with section 6 in your firmware), just **build → Upload via USB**.
No button presses.

> **Upload both cores at least once.** The CM7 waits for the CM4 during start-up.
> If `0x08100000` is empty, the CM7 ends up in `Error_Handler()`. After that you
> can upload just the core you changed with `-Core CM7` / `-Core CM4`.

---

## 10. Debugging with an ST-Link (optional)

SWD debugging still works with the bootloader in place, using CubeIDE's normal
debug configuration:

* It only programs your application's flash pages, so the bootloader survives.
  Never use "full chip erase".
* On reset the bootloader runs first, then jumps to your code. Breakpoints in
  `main()` are still hit.

---

## 11. Troubleshooting

| Symptom | Cause / fix |
|---------|-------------|
| Upload OK, but no COM port and the board seems dead | Firmware hangs early. Double-tap reset to recover. Check that `Bootloader_Handoff()` is the **first** call in `main()`, that `Disable_Caches()` has the `optimize("O2")` attribute, that the CM4 image was uploaded, and that the supply is LDO. |
| `Bootloader (2341:035f) did not show up` | The firmware lacks the 1200-baud code, or it is hung. Double-tap reset and run again. |
| `Missing ...\CM7\Debug\..._CM7.bin` | "Convert to binary file" isn't enabled for that project/configuration (section 7), or `$ProjectName` is wrong. |
| `dfu-util not found` | Install the Arduino Mbed OS Nicla Boards core, or pass `-DfuUtil C:\path\dfu-util.exe`. |
| `dfu-util: Cannot open DFU device` / `LIBUSB_ERROR` | The Windows DFU driver is missing. Installing the Nicla core in the Arduino IDE installs it (`post_install.bat`). |
| Board stays in the bootloader after every reset | `RTC->BKP0R` still holds `0xDF59`: something called `Enter_Arduino_Bootloader()`. Or the app at `0x08040000` is invalid (wrong linker address or stack outside RAM). |
| `HAL_RCC_OscConfig` fails / `Error_Handler` at clock config | Oscillator enable (PH1) not set before `SystemClock_Config` (4.1), or HSE not set to **bypass**. |
| USB enumerates only with SWD flashing, not via the bootloader | Missing `Bootloader_Handoff()`, so the USB core and PHY are still in the bootloader's state. |
| My code uses the RTC with a different clock source | The bootloader already set `RTCSEL`. Changing it needs a backup-domain reset (`__HAL_RCC_BACKUPRESET_FORCE/RELEASE`). |

---

## 12. FAQ

**Do I have to change `_estack`?**
No. `_estack = ORIGIN(RAM_D1) + LENGTH(RAM_D1)` = `0x24080000` is accepted. The
bootloader only checks that the stack address starts with `0x20`, `0x24`, `0x30`
or `0x38`.

**Can I still regenerate code from the `.ioc`?**
Yes. All code changes are inside `USER CODE` blocks. The linker-script change and
the "Convert to binary" option are not touched by code generation.

**Can I overwrite or brick the bootloader by uploading this way?**
No. Its flash sector is read-only over DFU. A double-tap of reset always gets you
back to DFU mode.

**Why `0x08040000` and not right after the bootloader?**
That is the address the bootloader jumps to: it is hard-coded in the bootloader,
and the Arduino IDE uses the same one. The first 256 KB stay reserved for the
bootloader and its data.

**Why disable the caches? I want them.**
The hand-off turns them off so you start from a known state. Enable them again in
your own init (`SCB_EnableICache(); SCB_EnableDCache();`) if you want them. Both
functions invalidate first, so that is safe.

**What does the bootloader need from my firmware, in short?**
A valid vector table at `0x08040000` with a RAM stack pointer. Nothing else. All
the extra code exists because the bootloader doesn't reset the chip before it
jumps.

**Can I use this with a single-core (CM7-only) project?**
Yes. Do everything for the CM7 and skip the CM4 lines (`HAL_SYSCFG_CM4BootAddConfig`,
`HAL_RCCEx_EnableBootCore`) together with the generated CM4 boot handshake, then
upload with `-Core CM7`.
