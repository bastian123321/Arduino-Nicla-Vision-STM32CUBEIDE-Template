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

> **Want to build these projects yourself?** See
> [Setting up the projects from zero](#setting-up-the-projects-from-zero) below.
> [docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) has the background to every
> step, plus troubleshooting and an FAQ.

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

---

## Setting up the projects from zero

These steps rebuild both projects in this repository from an empty STM32CubeIDE
workspace:

* **Part A** produces `NICLA_VISION_TEMPLATE`.
* **Part B** adds USB CDC on top of Part A and produces `NICLA_VISION_USB_CDC_HS`.

Every code block goes inside the `USER CODE` section named above it, so CubeMX
keeps it when you regenerate.
[docs/GETTING_STARTED.md](docs/GETTING_STARTED.md) explains why each step is
needed and has a troubleshooting table.

### Before you start

* **STM32CubeIDE** (tested with 1.19.0).
* **Arduino IDE** with the **Arduino Mbed OS Nicla Boards** core installed
  (*Tools → Board → Boards Manager*). The upload uses its `dfu-util.exe` and its
  Windows USB driver.
* The Nicla Vision must still have its **Arduino bootloader**. Double-tap reset:
  if the green LED pulses, it's there.

### Part A – the template (`NICLA_VISION_TEMPLATE`)

#### A1. Create the project

1. *File → New → STM32 Project*, part number **STM32H747AIIx**.
2. Enter a name, e.g. `MY_NICLA`, and keep the dual-core layout. CubeIDE creates
   `MY_NICLA_CM7` and `MY_NICLA_CM4`.

#### A2. Pins and peripherals (`.ioc`)

| Where in CubeMX | Setting | Context |
|-----------------|---------|---------|
| *System Core → RCC* | HSE: **BYPASS Clock Source** (PH0, 25 MHz oscillator) | CM7 |
| *System Core → SYS* | Debug: **Serial Wire** (PA13/PA14) | CM7 |
| *Connectivity → I2C2* | **I2C** on **PF0 (SDA) / PF1 (SCL)**. This bus goes to the PMIC. | CM7 |
| PE3 / PC13 / PF4 | GPIO_Output, labels **`LED_R` / `LED_G` / `LED_B`**, output level **High** (the LEDs are active-low) | any |
| *System Core → CORTEX_M7* | ICache and DCache **Enabled**, MPU: background region (default mode) | CM7 |

Don't configure PH1 in CubeMX. It enables the oscillator and is driven in code (A4).

#### A3. Clock (*Clock Configuration* tab and RCC)

* *RCC → Parameter Settings → Supply Source*: **PWR_LDO_SUPPLY**. This is
  required: the bootloader already set the supply to LDO, and that setting can only
  be written once per power-up.
* Input frequency **25 MHz**, PLL source **HSE**.
* PLL1 **/M = 5**, **×N = 192**, **/P = 2** → **480 MHz** SYSCLK.
* HPRE **/2** (240 MHz HCLK), and APB1/APB2/APB3/APB4 **/2**.

Then *Project → Generate Code*.

#### A4. CM7 `main.c` – board bring-up

**`USER CODE BEGIN Init`** (after `HAL_Init()`). Turn on the 25 MHz oscillator
before the clock is configured:

```c
  __HAL_RCC_GPIOH_CLK_ENABLE();
  GPIO_InitTypeDef gpio_osc_init_structure;
  gpio_osc_init_structure.Pin   = GPIO_PIN_1;
  gpio_osc_init_structure.Mode  = GPIO_MODE_OUTPUT_PP;
  gpio_osc_init_structure.Pull  = GPIO_PULLUP;
  gpio_osc_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &gpio_osc_init_structure);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, 1);
```

**`USER CODE BEGIN 2`**. Switch on the power rails through the PMIC (I2C address `0x08`):

```c
  uint8_t data[2];
  data[0] = 0x9c; data[1] = 0x80; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // charger LED by SW
  data[0] = 0x9e; data[1] = 0x20; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // charger LED by SW
  data[0] = 0x42; data[1] = 0x02; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // SW3 limit 1.5 A
  data[0] = 0x94; data[1] = 0xa0; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // VBUS limit 1.5 A
  data[0] = 0x4d; data[1] = 0x01; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // LDO1
  data[0] = 0x50; data[1] = 0x01; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // LDO2
  data[0] = 0x53; data[1] = 0x01; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // LDO3 1.2 V
  data[0] = 0x3b; data[1] = 0x81; HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, 2, 100); // SW2
  HAL_Delay(250);  // let the rails settle
```

**`USER CODE BEGIN 3`**. Something to show it runs:

```c
	HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
	HAL_Delay(3000);
```

#### A5. CM7 `main.c` – make it start from the Arduino bootloader

The bootloader jumps into your code **without resetting the chip**. These blocks
undo what it left behind and start the CM4 from the right address.

**`USER CODE BEGIN PD`**

```c
/* Start of the CM4 image in flash (must match CM4/STM32H747AIIX_FLASH.ld) */
#define CM4_IMAGE_ADDRESS    0x08100000U
```

**`USER CODE BEGIN PFP`**

```c
static void Bootloader_Handoff(void);
static void Disable_Caches(void);
```

**`USER CODE BEGIN 1`**: this must be the **first** code in `main()`. Replace the
generated `HAL_RCCEx_EnableBootCore` line if you already added one:

```c
  Bootloader_Handoff();

  // Release the M4 (it is held off by the option bytes), pointing it at its image
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_SYSCFG_CM4BootAddConfig(SYSCFG_BOOT_ADDR0, CM4_IMAGE_ADDRESS);
  HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);
```

**`USER CODE BEGIN 4`**

```c
/* -O2 on purpose: CMSIS 5.1.1 SCB_DisableDCache() hangs at -O0 (its loop
 * counters live on the cached stack and are read back stale) */
__attribute__((optimize("O2"))) static void Disable_Caches(void)
{
  SCB_DisableICache();
  SCB_DisableDCache();
}

/* Put VTOR, NVIC, caches, MPU and the RCC peripheral resets / clock enables
 * back to their power-on state after the jump from the bootloader */
static void Bootloader_Handoff(void)
{
  extern uint32_t g_pfnVectors[];

  __disable_irq();

  SCB->VTOR = (uint32_t)g_pfnVectors;
  __DSB();
  __ISB();

  SysTick->CTRL = 0;
  for (uint32_t i = 0; i < 8; i++)
  {
    NVIC->ICER[i] = 0xFFFFFFFFU;
    NVIC->ICPR[i] = 0xFFFFFFFFU;
  }

  Disable_Caches();
  HAL_MPU_Disable();

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

  /* Give up CM7's clock enables so the D2 domain can enter STOP (CM4 handshake).
   * AHB3ENR also holds the FLASH/TCM/AXI SRAM bits: only clear peripherals. */
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

The generated code turns the MPU and caches back on right after this (A2), so you
keep their speed.

#### A6. CM7 linker script

In `MY_NICLA_CM7/STM32H747AIIX_FLASH.ld`, change the `FLASH` line so the code goes
after the bootloader:

```
  FLASH   (rx)   : ORIGIN = 0x08040000, LENGTH = 768K
```

Leave `_estack` as generated (`0x24080000` is accepted by the bootloader). Leave
the CM4 linker script alone: it already starts at `0x08100000`.

#### A7. Generate `.bin` files

For **both** `MY_NICLA_CM7` and `MY_NICLA_CM4`:

1. Open *Properties → C/C++ Build → Settings* and set Configuration to **[All configurations]**.
2. Under *MCU Post build outputs*, tick **Convert to binary file (-O binary)**.
3. Click **Apply**.

#### A8. Upload tooling

1. Copy the **`tools`** folder from `NICLA_VISION_TEMPLATE` into your project
   root, next to `CM4`, `CM7` and the `.ioc`. No edits are needed: the script takes
   the project name from the `.ioc` file.
2. Create the CubeIDE button:
   1. Open *Run → External Tools → External Tools Configurations…*
   2. Right-click **Program** → **New Configuration**, and fill in (replace `MY_NICLA_CM7`):
      * **Location:** `${env_var:SystemRoot}\System32\WindowsPowerShell\v1.0\powershell.exe`
      * **Working Directory:** `${project_loc:MY_NICLA_CM7}\..`
      * **Arguments:** `-NoProfile -ExecutionPolicy Bypass -File "${project_loc:MY_NICLA_CM7}\..\tools\upload.ps1"`
   3. On the *Common* tab, tick *Display in favorites menu → External Tools*. Then **Apply**.

   Alternatively, copy `CM7/Upload via USB (Arduino bootloader).launch` from a
   template into your CM7 folder, replace `NICLA_VISION_TEMPLATE_CM7` in it with
   your CM7 project name, and refresh the project (**F5**).

#### A9. Build and upload

1. Build both projects (**Ctrl+B**).
2. **Double-tap reset** on the board.
3. Run *Run → External Tools → Upload via USB*.

The console should end with `Done - firmware is starting.`, and then the green LED
toggles every 3 seconds.

### Part B – add USB CDC (`NICLA_VISION_USB_CDC_HS`)

Start from a project made with Part A. USB needs the PMIC rails to be on before the
USB PHY is used, so USB is started by hand **after** the PMIC code.

#### B1. `.ioc` changes

| Where in CubeMX | Setting |
|-----------------|---------|
| *Connectivity → USB_OTG_HS* (CortexM7) | Mode **External Phy → Device_Only** (ULPI pins PA3, PA5, PB0, PB1, PB5, PB10–PB13, PC0, PC2_C, PC3_C); NVIC: **USB On The Go HS global interrupt** enabled |
| *Middleware → USB_DEVICE* (CortexM7) | Class For HS IP: **Communication Device Class (Virtual Port Com)** |
| PA2 | GPIO_Output, label **`USB_PHY_RST`**, pull-down, output level **Low**. This is the USB PHY reset. |
| *Clock Configuration* | Give USB a **48 MHz** clock. `NICLA_VISION_USB_CDC_HS` runs at **120 MHz**: PLL1 /M 5, ×N 48, /P 2, **/Q 5** (= 48 MHz), and the USB clock mux on **PLL1Q**. |
| *Project Manager → Advanced Settings* | For `MX_USB_DEVICE_Init`, tick **Do Not Generate Function Call** |

Generate code.

#### B2. Start USB after the PMIC

**`USER CODE BEGIN Includes`**

```c
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
```

**`USER CODE BEGIN 2`**: append after the PMIC code and `HAL_Delay(250)`:

```c
  HAL_GPIO_WritePin(USB_PHY_RST_GPIO_Port, USB_PHY_RST_Pin, GPIO_PIN_SET); // release PHY reset
  HAL_Delay(20);
  MX_USB_DEVICE_Init();   // only now: the PHY is powered and out of reset
```

**`USER CODE BEGIN PV`** and **`USER CODE BEGIN 3`**: example output:

```c
static uint8_t tx_buffer[64];
```

```c
	sprintf((char *)tx_buffer, "Device Found! \r\n");
	CDC_Transmit_HS(tx_buffer, strlen((char *)tx_buffer));
	HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
	HAL_Delay(1000);
```

#### B3. 1200-baud auto-reset (uploads without pressing reset)

The upload script opens the board's COM port at 1200 baud and closes it. This code
answers that by rebooting into the bootloader, which is the same trick the Arduino
IDE uses.

**`main.h` → `USER CODE BEGIN EC`**

```c
#define ARDUINO_BOOTLOADER_MAGIC   0xDF59U   /* RTC->BKP0R value: bootloader stays in DFU */
```

**`main.h` → `USER CODE BEGIN EFP`**

```c
void Enter_Arduino_Bootloader(void);
```

**`main.c` → `USER CODE BEGIN 4`**

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

**`USB_DEVICE/App/usbd_cdc_if.c`**

```c
/* USER CODE BEGIN INCLUDE */
#include <string.h>
#include "main.h"
```

```c
/* USER CODE BEGIN PRIVATE_VARIABLES */
static uint8_t line_coding[7] = { 0x00, 0xC2, 0x01, 0x00, 0x00, 0x00, 0x08 }; /* 115200 8N1 */
static uint8_t dtr_active = 0;
```

```c
/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
static void CDC_Check_1200bps_Touch(void);
```

In `CDC_Control_HS()`, replace these three empty cases:

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
```

#### B4. Build and upload

1. Build both projects.
2. **Double-tap reset** once, because the board is still running firmware without the auto-reset.
3. Click **Upload via USB**.

A new COM port appears and prints `Device Found!` every second. From now on,
**build → Upload via USB** is all you need: the script resets the board into the
bootloader by itself.

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
* **New name:** create a new CubeMX project and follow
  [Setting up the projects from zero](#setting-up-the-projects-from-zero). Renaming an existing dual-core
  CubeIDE project means renaming the `.ioc`, both `.project` names and the
  `.launch` files, which is easy to get wrong.

**Upload both cores at least once.** The CM7 waits for the CM4 during start-up. If
the firmware ever hangs before USB comes up, **double-tap reset** to get back to
the bootloader. Uploads over USB can't overwrite the bootloader.

## Debugging with an ST-Link (optional)

The normal CubeIDE debug configurations still work with the bootloader in place.
They only program the application sectors. On reset the bootloader runs first,
then jumps into your code, and breakpoints in `main()` are still hit.
