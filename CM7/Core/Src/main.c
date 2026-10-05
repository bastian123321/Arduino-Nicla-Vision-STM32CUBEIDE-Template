/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include <string.h>
#include <stdio.h>
#include "lsm6dsox_reg.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define SENSOR_BUS hspi5

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif

/* Start of the CM4 image in flash (must match CM4/STM32H747AIIX_FLASH.ld) */
#define CM4_IMAGE_ADDRESS    0x08100000U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define    BOOT_TIME            10 //ms
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c2;

SPI_HandleTypeDef hspi5;

/* USER CODE BEGIN PV */
static int16_t data_raw_acceleration[3];
static int16_t data_raw_angular_rate[3];
static int16_t data_raw_temperature;
static float acceleration_mg[3];
static float angular_rate_mdps[3];
static float temperature_degC;
static uint8_t whoamI, rst;
static uint8_t tx_buffer[1000];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
static void MX_SPI5_Init(void);
/* USER CODE BEGIN PFP */
static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len);
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len);
static void tx_com( uint8_t *tx_buffer, uint16_t len );
static void platform_delay(uint32_t ms);
static void Bootloader_Handoff(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  // We are started by the Arduino bootloader (or by SWD): take over the
  // vector table and clean up whatever state the bootloader left behind
  Bootloader_Handoff();

  // Boot up the M4 core as is set to have it off by default using fuses.
  // Point it at its image first, the same way the Arduino core does.
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_SYSCFG_CM4BootAddConfig(SYSCFG_BOOT_ADDR0, CM4_IMAGE_ADDRESS);
  HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* Enable the NICLA Vision oscillator pin */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  GPIO_InitTypeDef  gpio_osc_init_structure;
  gpio_osc_init_structure.Pin = GPIO_PIN_1;
  gpio_osc_init_structure.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_osc_init_structure.Pull = GPIO_PULLUP;
  gpio_osc_init_structure.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOH, &gpio_osc_init_structure);
  HAL_Delay(10);
  HAL_GPIO_WritePin(GPIOH, GPIO_PIN_1, 1);

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_I2C2_Init();
  MX_SPI5_Init();
  //MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */

  /* Setup power supply rails
   * This part enables all required rails on the power management module via I2C*/
  uint8_t data[2];

  // Charger LED Driver Enable controlled via software
  data[0]=0x9c;
  data[1]=0x80;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // Charger LED Driver Enable controlled via software
  data[0]=0x9e;
  data[1]=0x20;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // SW3 current limit at 1.5A
  data[0]=0x42;
  data[1]=0x02;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // VBUS current limit at 1.5A
  data[0]=0x94;
  data[1]=0xa0;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // LDO1
  data[0] = 0x4d;
  data[1] = 0x01;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // LDO2
  data[0] = 0x50;
  data[1] = 0x01;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // LDO3 to 1.2V
  data[0] = 0x53;
  data[1] = 0x01;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data), 100);

  // SW2
  data[0] = 0x3b;
  data[1] = 0x81;
  HAL_I2C_Master_Transmit(&hi2c2, 0x08 << 1, data, sizeof(data),100);

  // wait for the power lines to settle
  HAL_Delay(250);

  /* Enable USB PHY pin */
  HAL_GPIO_WritePin(USB_PHY_RST_GPIO_Port, USB_PHY_RST_Pin, SET);

  HAL_Delay(20);

  // Must be called only after 3.1V power supply has settled and the PA_2 is pulled high
  MX_USB_DEVICE_Init();

  stmdev_ctx_t dev_ctx;
  /* Initialize mems driver interface */
  dev_ctx.write_reg = platform_write;
  dev_ctx.read_reg = platform_read;
  dev_ctx.mdelay = platform_delay;
  dev_ctx.handle = &SENSOR_BUS;
  /* Wait sensor boot time */
  platform_delay(BOOT_TIME);
  /* Check device ID */
  lsm6dsox_device_id_get(&dev_ctx, &whoamI);

  if (whoamI != LSM6DSOX_ID)
  {
    while (1);
  }

  /* Restore default configuration */
  lsm6dsox_reset_set(&dev_ctx, PROPERTY_ENABLE);

  do {
    lsm6dsox_reset_get(&dev_ctx, &rst);
  } while (rst);

  /* Disable I3C interface */
  lsm6dsox_i3c_disable_set(&dev_ctx, LSM6DSOX_I3C_DISABLE);
  /* Enable Block Data Update */
  lsm6dsox_block_data_update_set(&dev_ctx, PROPERTY_ENABLE);
  /* Set Output Data Rate */
  lsm6dsox_xl_data_rate_set(&dev_ctx, LSM6DSOX_XL_ODR_12Hz5);
  lsm6dsox_gy_data_rate_set(&dev_ctx, LSM6DSOX_GY_ODR_12Hz5);
  /* Set full scale */
  lsm6dsox_xl_full_scale_set(&dev_ctx, LSM6DSOX_2g);
  lsm6dsox_gy_full_scale_set(&dev_ctx, LSM6DSOX_2000dps);
  /* Configure filtering chain(No aux interface)
   * Accelerometer - LPF1 + LPF2 path
   */
  lsm6dsox_xl_hp_path_on_out_set(&dev_ctx, LSM6DSOX_LP_ODR_DIV_100);
  lsm6dsox_xl_filter_lp2_set(&dev_ctx, PROPERTY_ENABLE);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	uint8_t reg;
	/* Read output only if new xl value is available */
	lsm6dsox_xl_flag_data_ready_get(&dev_ctx, &reg);

	if (reg) {
	  /* Read acceleration field data */
	  memset(data_raw_acceleration, 0x00, 3 * sizeof(int16_t));
	  lsm6dsox_acceleration_raw_get(&dev_ctx, data_raw_acceleration);
	  acceleration_mg[0] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[0]);
	  acceleration_mg[1] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[1]);
	  acceleration_mg[2] = lsm6dsox_from_fs2_to_mg(data_raw_acceleration[2]);
	  sprintf((char *)tx_buffer, "Acceleration [mg]:%4.2f\t%4.2f\t%4.2f\r\n", acceleration_mg[0], acceleration_mg[1], acceleration_mg[2]);
	  tx_com(tx_buffer, strlen((char const *)tx_buffer));
	}

	lsm6dsox_gy_flag_data_ready_get(&dev_ctx, &reg);

	if (reg) {
	  /* Read angular rate field data */
	  memset(data_raw_angular_rate, 0x00, 3 * sizeof(int16_t));
	  lsm6dsox_angular_rate_raw_get(&dev_ctx, data_raw_angular_rate);
	  angular_rate_mdps[0] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[0]);
	  angular_rate_mdps[1] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[1]);
	  angular_rate_mdps[2] = lsm6dsox_from_fs2000_to_mdps(data_raw_angular_rate[2]);
	  sprintf((char *)tx_buffer, "Angular rate [mdps]:%4.2f\t%4.2f\t%4.2f\r\n", angular_rate_mdps[0], angular_rate_mdps[1], angular_rate_mdps[2]);
	  tx_com(tx_buffer, strlen((char const *)tx_buffer));
	}

	lsm6dsox_temp_flag_data_ready_get(&dev_ctx, &reg);

	if (reg) {
	  /* Read temperature data */
	  memset(&data_raw_temperature, 0x00, sizeof(int16_t));
	  lsm6dsox_temperature_raw_get(&dev_ctx, &data_raw_temperature);
	  temperature_degC = lsm6dsox_from_lsb_to_celsius(data_raw_temperature);
	  sprintf((char *)tx_buffer, "Temperature [degC]:%6.2f\r\n", temperature_degC);
	  tx_com(tx_buffer, strlen((char const *)tx_buffer));
	}

	HAL_Delay(2000);
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 48;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x107075B0;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief SPI5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI5_Init(void)
{

  /* USER CODE BEGIN SPI5_Init 0 */

  /* USER CODE END SPI5_Init 0 */

  /* USER CODE BEGIN SPI5_Init 1 */

  /* USER CODE END SPI5_Init 1 */
  /* SPI5 parameter configuration*/
  hspi5.Instance = SPI5;
  hspi5.Init.Mode = SPI_MODE_MASTER;
  hspi5.Init.Direction = SPI_DIRECTION_2LINES;
  hspi5.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi5.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi5.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi5.Init.NSS = SPI_NSS_SOFT;
  hspi5.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_4;
  hspi5.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi5.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi5.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi5.Init.CRCPolynomial = 0x0;
  hspi5.Init.NSSPMode = SPI_NSS_PULSE_ENABLE;
  hspi5.Init.NSSPolarity = SPI_NSS_POLARITY_LOW;
  hspi5.Init.FifoThreshold = SPI_FIFO_THRESHOLD_01DATA;
  hspi5.Init.TxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.RxCRCInitializationPattern = SPI_CRC_INITIALIZATION_ALL_ZERO_PATTERN;
  hspi5.Init.MasterSSIdleness = SPI_MASTER_SS_IDLENESS_00CYCLE;
  hspi5.Init.MasterInterDataIdleness = SPI_MASTER_INTERDATA_IDLENESS_00CYCLE;
  hspi5.Init.MasterReceiverAutoSusp = SPI_MASTER_RX_AUTOSUSP_DISABLE;
  hspi5.Init.MasterKeepIOState = SPI_MASTER_KEEP_IO_STATE_DISABLE;
  hspi5.Init.IOSwap = SPI_IO_SWAP_DISABLE;
  if (HAL_SPI_Init(&hspi5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI5_Init 2 */

  /* USER CODE END SPI5_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(USB_PHY_RST_GPIO_Port, USB_PHY_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : LED_R_Pin */
  GPIO_InitStruct.Pin = LED_R_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_R_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : LED_G_Pin */
  GPIO_InitStruct.Pin = LED_G_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_G_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CS_up_Pin */
  GPIO_InitStruct.Pin = CS_up_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(CS_up_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : USB_PHY_RST_Pin */
  GPIO_InitStruct.Pin = USB_PHY_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(USB_PHY_RST_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

static int32_t platform_write(void *handle, uint8_t reg, const uint8_t *bufp, uint16_t len)
{
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Transmit(handle, (uint8_t*) bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
  return 0;
}

/*
 * @brief  Read generic device register (platform dependent)
 *
 * @param  handle    customizable argument. In this examples is used in
 *                   order to select the correct sensor bus handler.
 * @param  reg       register to read
 * @param  bufp      pointer to buffer that store the data read
 * @param  len       number of consecutive register to read
 *
 */
static int32_t platform_read(void *handle, uint8_t reg, uint8_t *bufp, uint16_t len)
{
  reg |= 0x80;
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(handle, &reg, 1, 1000);
  HAL_SPI_Receive(handle, bufp, len, 1000);
  HAL_GPIO_WritePin(CS_up_GPIO_Port, CS_up_Pin, GPIO_PIN_SET);
  return 0;
}

/*
 * @brief  platform specific outputs on terminal (platform dependent)
 *
 * @param  tx_buffer     buffer to transmit
 * @param  len           number of byte to send
 *
 */
static void tx_com(uint8_t *tx_buffer, uint16_t len)
{
  CDC_Transmit_HS(tx_buffer, len);
}

/*
 * @brief  platform specific delay (platform dependent)
 *
 * @param  ms        delay in ms
 *
 */
static void platform_delay(uint32_t ms)
{
  HAL_Delay(ms);
}

/*
 * @brief  Clean hand-off from the Arduino bootloader
 *
 * The bootloader (mbed based) jumps to 0x08040000 without a reset, so the
 * chip is not in the state CubeMX code expects after power-on:
 *  - caches are enabled and peripherals (USB, I2C2, GPIO...) are configured
 *  - CM7 still has clocks enabled on D2 peripherals: while CM7 owns anything
 *    in D2, the D2 domain can never enter STOP and the CM4 boot handshake in
 *    main() times out into Error_Handler()
 * SystemInit() already reset the clock tree, here we undo the rest so the
 * application starts exactly as it does after a reset with SWD.
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
  SCB_DisableDCache();
  SCB_DisableICache();
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

  /* ...and give up CM7's clock enables (reset values) so D2 can go to STOP.
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

/*
 * @brief  Reboot into the Arduino bootloader (DFU mode)
 *
 * Called on the "1200 baud touch". The bootloader stays in DFU mode instead
 * of starting the application when it finds 0xDF59 in RTC backup register 0.
 */
void Enter_Arduino_Bootloader(void)
{
  __disable_irq();

  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_RTC_CLK_ENABLE();
  RTC->BKP0R = ARDUINO_BOOTLOADER_MAGIC;
  __DSB();

  NVIC_SystemReset();
}
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
