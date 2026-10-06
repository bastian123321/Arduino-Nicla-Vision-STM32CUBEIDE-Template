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

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* Start of the CM4 image in flash (must match CM4/STM32H747AIIX_FLASH.ld) */
#define CM4_IMAGE_ADDRESS    0x08100000U

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C2_Init(void);
/* USER CODE BEGIN PFP */
static void Bootloader_Handoff(void);
static void Disable_Caches(void);
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

  Bootloader_Handoff();

  // Release the M4 (it is held off by the option bytes), pointing it at its image
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  HAL_SYSCFG_CM4BootAddConfig(SYSCFG_BOOT_ADDR0, CM4_IMAGE_ADDRESS);

  // Boot up the M4 core as portenta is set to have it off by default using fuses
  HAL_RCCEx_EnableBootCore(RCC_BOOT_C2);

  /* USER CODE END 1 */
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* Enable the CPU Cache */

  /* Enable I-Cache---------------------------------------------------------*/
  SCB_EnableICache();

  /* Enable D-Cache---------------------------------------------------------*/
  SCB_EnableDCache();

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

  /* Enable the portenta oscillator pin */
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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	HAL_GPIO_TogglePin(LED_G_GPIO_Port, LED_G_Pin);
	HAL_Delay(3000);
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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 192;
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
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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
  hi2c2.Init.Timing = 0x307075B1;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_R_GPIO_Port, LED_R_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_G_GPIO_Port, LED_G_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LED_B_GPIO_Port, LED_B_Pin, GPIO_PIN_SET);

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

  /*Configure GPIO pin : LED_B_Pin */
  GPIO_InitStruct.Pin = LED_B_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_B_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
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

#ifdef  USE_FULL_ASSERT
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
