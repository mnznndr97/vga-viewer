/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * <h2><center>&copy; Copyright (c) 2021 STMicroelectronics.
 * All rights reserved.</center></h2>
 *
 * This software component is licensed by ST under BSD 3-Clause license,
 * the "License"; You may not use this file except in compliance with the
 * License. You may obtain a copy of the License at:
 *                        opensource.org/licenses/BSD-3-Clause
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <typedefs.h>
#include <assertion.h>

#include <hal_extensions.h>
#include <cmsis_extensions.h>
#include <crc7.h>
#include <vga/edid.h>
#include <vga/vgascreenbuffer.h>
#include <screen/screen.h>
#include <ram.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/// Event flag for EDID success read from I2C channel
#define I2CVGA_EDID_RECEIVED 0x00000001
#define I2CVGA_EDID_ERROR 0x00000002

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
DMA_HandleTypeDef hdma_tim1_trig;

UART_HandleTypeDef huart4;

/* Definitions for vgaConnectionTa */
osThreadId_t vgaConnectionTaHandle;
const osThreadAttr_t vgaConnectionTa_attributes = { .name = "vgaConnectionTa", .stack_size = 160 * 4, .priority = (osPriority_t) osPriorityNormal, };
/* Definitions for _vgaEDIDRcvEvnt */
osEventFlagsId_t _vgaEDIDRcvEvntHandle;
const osEventFlagsAttr_t _vgaEDIDRcvEvnt_attributes = { .name = "_vgaEDIDRcvEvnt" };
/* USER CODE BEGIN PV */

static EDID _vgaEDID = { 0 };

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2C2_Init(void);
static void MX_UART4_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM1_Init(void);
static void MX_DAC_Init(void);
static void MX_TIM4_Init(void);
void ConnecToVGATask(void *argument);

/* USER CODE BEGIN PFP */
static void HandleI2CError(I2C_HandleTypeDef *hi2c);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

int __io_putchar(int ch) {
	// The __io_putchar migth in a "deep" position in the stack due to libs function calls
	// (at least in Debug mode where inlining is not aggressive)
	// To be absolutely secure, let's check that at least in this level we have enough space in the stack
	// This can be done only when the kernel in running
	osKernelState_t state = osKernelGetState();
	if (state == osKernelRunning) {
		osExEnforeStackProtection(NULL);
	}

	// This call is blocking with infinite timeout so in general we should not have errors
	HAL_StatusTypeDef result = HAL_UART_Transmit(&huart4, (unsigned char*) &ch, 1, HAL_MAX_DELAY);
	if (result != HAL_OK) {
		// Let's just signal with a LED that something has gone wrong
		Error_Handler();
	}

	// No documentation on the return value (it is also not used by the default _write function in syscalls.c
	return 0;
}

void FormatSec(float time) {
	static const char *suffix[] = { "sec", "ms", "us", "ns" };
	const int suffixLength = 3; // We should never get to ns

	int i;
	for (i = 0; i < suffixLength - 1 && time > 0.0f && time < 1.0f; i++) {
		time *= 1000.0f;
	}

	printf("%.2f %s", time, suffix[i]);
}

void OnDMAError(DMA_HandleTypeDef *dma) {
	printf("Error\r\n");
}

void OnDMACplt(DMA_HandleTypeDef *dma) {
	printf("Cplt\r\n");
}

void HandleI2CError(I2C_HandleTypeDef *hi2c) {
	// NB: HAL driver (in theory handles correctly the abort of the current transfer when an error occurs
	// We have to do nothing

	printf("\033[1;33m");
	UInt32 errorCode = hi2c->ErrorCode;
	if (errorCode == HAL_I2C_ERROR_BERR) {
		printf("I2C bus error occurred.");
	} else if (errorCode == HAL_I2C_ERROR_AF) {
		printf("I2C nonacknowledge bit detected.");
		// Nothing to do. We will restart the transmission
	} else if (errorCode == HAL_I2C_ERROR_ARLO) {
		printf("I2C arbitration lost.");
		// Nothing to do. We will restart the transmission
		// STM32F407 Reference manual tells nothing on what to do when this error occurs
		// Let's just retry
	} else if (errorCode == HAL_I2C_ERROR_OVR) {
		printf("I2C overrun/underrun error detected");
		// We are not a slave device, no overrun/underrun errors should be detected
		Error_Handler();
	} else {
		printf("I2C error not handled: %" PRIu32);
		Error_Handler();
	}

	printf("\033[0m\r\n");
}

void TIM1_CC_IRQHandler(void) {
	return;

	//UInt32 timStatus = TIM1->SR;
	//UInt32 isLineStartIRQ = READ_BIT(timStatus, TIM_FLAG_CC3);
	//UInt32 isLineEndIRQ = READ_BIT(timStatus, TIM_FLAG_CC4);

	//CLEAR_BIT(TIM1->SR, isLineStartIRQ | isLineEndIRQ);

	//if (isLineStartIRQ != 0) {
	//	if (_videoActive) {
	//		//ScreenCheckVideoLineEnded();
	//		ScreenEnableDMA();
	//	}
	//} else if (isLineEndIRQ != 0) {
	//	if (_videoActive) {
	//		ScreenDisableDMA();
	//	} else {
	//		_currentLineAddr = &VideoBuffer[0];
	//		_currentLinePrescaler = 0;
	//	}
	//} else {
	//	// We are handling an interrupt that should not be enabled
	//	Error_Handler();
	//}

}

void TIM3_IRQHandler() {
	return;
	//uint32_t timStatus = TIM3->SR;
	//uint32_t isVideoStartIRQ = READ_BIT(timStatus, TIM_FLAG_CC2);
	//uint32_t isVideoEndIRQ = READ_BIT(timStatus, TIM_FLAG_CC3);

	//if (isVideoStartIRQ == 0 && isVideoEndIRQ == 0) {
	//	// We are handling an interrupt that should not be enabled
	//	Error_Handler();
	//}

	//if (isVideoStartIRQ) {
	//	CLEAR_BIT(TIM3->SR, TIM_FLAG_CC2);
	//	_videoActive = 1;
	//} else {
	//	CLEAR_BIT(TIM3->SR, TIM_FLAG_CC3);
	//	/*__HAL_TIM_DISABLE_DMA(&htim1, TIM_DMA_TRIGGER);
	//	 CLEAR_BIT(DMA2_Stream0->CR, DMA_SxCR_EN);*/
	//	_videoActive = 0;
	//}

	//if (HAL_NVIC_GetPendingIRQ(TIM3_IRQn)) {
	//	Error_Handler();
	//}
}

void HAL_I2C_MasterRxCpltCallback(I2C_HandleTypeDef *hi2c) {
	// The vgaConnectionTaHandle is the only task that should enable this interrupt
	// If the task stack overflows, there is the risk that some OS structures (_vgaEDIDRcvEvntHandle)
	// can be corruptes so better add a check to lower the possibility that this event occurs
	// NB: StackOverflow can happen during another IRQ_Handler. Here we are just "asserting" that everything ok
	// otherwhise osEventFlagsSet may fail
	osExEnforeStackProtection(vgaConnectionTaHandle);

	// Not too much to do here. I2C transfer is now completed and we can set our event
	UInt32 result = osEventFlagsSet(_vgaEDIDRcvEvntHandle, I2CVGA_EDID_RECEIVED);
	if (osExResultIsFlagsErrorCode(result)) {
		// Something wrong happened, better stop everything
		Error_Handler();
	}

	// No others flags should be set
	DebugAssert(result == I2CVGA_EDID_RECEIVED);
}

void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c) {
	// The vgaConnectionTaHandle is the only task that should enable this interrupt
	// If the task stack overflows, there is the risk that some OS structures (_vgaEDIDRcvEvntHandle)
	// can be corruptes so better add a check to lower the possibility that this event occurs
	// NB: StackOverflow can happen during another IRQ_Handler. Here we are just "asserting" that everything ok
	// otherwhise osEventFlagsSet may fail
	osExEnforeStackProtection(vgaConnectionTaHandle);

	// Not too much to do here. I2C transfer resulted in an error
	// We simply signal the event. The VGA connection task will handle the error
	// that should have been writtien in hi2c->ErrorCode
	UInt32 result = osEventFlagsSet(_vgaEDIDRcvEvntHandle, I2CVGA_EDID_ERROR);
	if (osExResultIsFlagsErrorCode(result)) {
		// Something wrong happened, better stop everything
		Error_Handler();
	}

	// No others flags should be set
	DebugAssert(result == I2CVGA_EDID_ERROR);
}

void HAL_I2C_AbortCpltCallback(I2C_HandleTypeDef *hi2c) {
	// Never occurred, so for the moment let's keep a printf
	printf("I2C Transfer aborted\r\n");
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
	/* USER CODE BEGIN 1 */
	// We need to disable the io buffering to have the data directly sended
	setvbuf(stdout, NULL, _IONBF, 0);

	SET_BIT(DBGMCU->APB1FZ, DBGMCU_APB1_FZ_DBG_TIM4_STOP);

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_I2C2_Init();
	MX_UART4_Init();
	MX_TIM3_Init();
	MX_TIM1_Init();
	MX_DAC_Init();
	MX_TIM4_Init();
	/* USER CODE BEGIN 2 */

	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_13, GPIO_PIN_SET);

	printf("\033[0;0H\033[2J\033[0mStarting VGA Viewer 0.21.1104.1\r\n");

	/*uint8_t buffer[128];
	 HAL_StatusTypeDef halStatus = HAL_I2C_Master_Receive(&hi2c2, 0x50 << 1, buffer, 128, HAL_MAX_DELAY);

	 if (halStatus == HAL_ERROR) {
	 printf("I2C error: ");
	 switch (hi2c2.ErrorCode) {
	 case HAL_I2C_ERROR_AF:
	 printf("Acknowledge failure\r\n");
	 break;
	 default:
	 printf("Unknown\r\n");
	 break;
	 }
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
	 } else if (halStatus == HAL_BUSY) {
	 printf("HAL Busy\r\n");
	 } else {
	 PEDID pEdid = (PEDID) &buffer[0];
	 EDIDDumpStructure(pEdid);
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12, GPIO_PIN_SET);

	 SET_BIT(GPIOC->BSRR, GPIO_PIN_11);
	 HAL_Delay(1000);

	 _currentLineAddr = &VideoBuffer[0];

	 ScreenInitialize(htim1.Instance, htim3.Instance);

	 HAL_TIM_Base_Start(&htim3);
	 HAL_TIM_Base_Start(&htim1);

	 HAL_StatusTypeDef vSyncTimStatus = HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

	 HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_2);
	 HAL_TIM_PWM_Start_IT(&htim3, TIM_CHANNEL_3);

	 HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	 HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_3);
	 HAL_TIM_PWM_Start_IT(&htim1, TIM_CHANNEL_4);
	 HAL_StatusTypeDef hSyncTimStatus = HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);

	 uint32_t value = htim1.Instance->CNT;
	 uint32_t vSyncValue = htim3.Instance->CNT;

	 if (hSyncTimStatus == HAL_ERROR) {
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
	 } else {
	 printf("HSync timer started %d ...\r\n", value);
	 }

	 if (vSyncTimStatus == HAL_ERROR) {
	 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_14, GPIO_PIN_SET);
	 } else {

	 printf("VSync timer started %d ...\r\n", vSyncValue);
	 }

	 /*int32_t HSyncTimFreq = 40000000;
	 float hSyncFreq = HSyncTimFreq / (htim1.Init.Period + 1.0f);
	 printf("Hsync frequency %f Hz\r\n", hSyncFreq);

	 float hNonPulseTime = HSyncTimFreq / ((float) htim1.Instance->CCR1);
	 hNonPulseTime = 1.0f / hNonPulseTime;
	 float hLineTime = 1.0f / hSyncFreq;
	 printf("Hsync pulse width: ");
	 FormatSec(hLineTime - hNonPulseTime);
	 printf("; line time: ");
	 FormatSec(hLineTime);
	 printf("\r\n");

	 int32_t VSyncTimFreq = hSyncFreq;
	 double vSyncFreq = VSyncTimFreq / (htim3.Init.Period + 1.0);
	 printf("Vsync frequency %f Hz\r\n", vSyncFreq);

	 float vNonPulseTime = VSyncTimFreq / ((float) htim3.Instance->CCR1);
	 vNonPulseTime = 1.0f / vNonPulseTime;
	 float vLineTime = 1.0f / vSyncFreq;
	 printf("Vsync pulse width: ");
	 FormatSec(vLineTime - vNonPulseTime);
	 printf("; line time: ");
	 FormatSec(vLineTime);
	 printf("\r\n");*/
	/*HAL_TIM_Base_Start(&htim4);

	 while (true) {
	 }

	 HAL_DAC_Init(&hdac);
	 HAL_DAC_Start(&hdac, DAC_CHANNEL_1);

	 HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_8B_R, 0xFF);
	 printf("DAC value set \r\n");
	 }*/

	/* USER CODE END 2 */

	/* Init scheduler */
	osKernelInitialize();

	/* USER CODE BEGIN RTOS_MUTEX */
	/* add mutexes, ... */
	/* USER CODE END RTOS_MUTEX */

	/* USER CODE BEGIN RTOS_SEMAPHORES */
	/* add semaphores, ... */
	/* USER CODE END RTOS_SEMAPHORES */

	/* USER CODE BEGIN RTOS_TIMERS */
	/* start timers, add new ones, ... */
	/* USER CODE END RTOS_TIMERS */

	/* USER CODE BEGIN RTOS_QUEUES */
	/* add queues, ... */
	/* USER CODE END RTOS_QUEUES */

	/* Create the thread(s) */
	/* creation of vgaConnectionTa */
	vgaConnectionTaHandle = osThreadNew(ConnecToVGATask, NULL, &vgaConnectionTa_attributes);

	/* USER CODE BEGIN RTOS_THREADS */
	/* add threads, ... */
	/* USER CODE END RTOS_THREADS */

	/* Create the event(s) */
	/* creation of _vgaEDIDRcvEvnt */
	_vgaEDIDRcvEvntHandle = osEventFlagsNew(&_vgaEDIDRcvEvnt_attributes);

	/* USER CODE BEGIN RTOS_EVENTS */
	/* add events, ... */
	/* USER CODE END RTOS_EVENTS */

	/* Start scheduler */
	osKernelStart();

	/* We should never get here as control is now taken by the scheduler */
	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1) {
		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Configure the main internal regulator output voltage
	 */
	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLM = 4;
	RCC_OscInitStruct.PLL.PLLN = 120;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = 7;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}
	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK) {
		Error_Handler();
	}
}

/**
 * @brief DAC Initialization Function
 * @param None
 * @retval None
 */
static void MX_DAC_Init(void) {

	/* USER CODE BEGIN DAC_Init 0 */

	/* USER CODE END DAC_Init 0 */

	DAC_ChannelConfTypeDef sConfig = { 0 };

	/* USER CODE BEGIN DAC_Init 1 */

	/* USER CODE END DAC_Init 1 */
	/** DAC Initialization
	 */
	hdac.Instance = DAC;
	if (HAL_DAC_Init(&hdac) != HAL_OK) {
		Error_Handler();
	}
	/** DAC channel OUT1 config
	 */
	sConfig.DAC_Trigger = DAC_TRIGGER_NONE;
	sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
	if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_1) != HAL_OK) {
		Error_Handler();
	}
	/** DAC channel OUT2 config
	 */
	if (HAL_DAC_ConfigChannel(&hdac, &sConfig, DAC_CHANNEL_2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN DAC_Init 2 */

	/* USER CODE END DAC_Init 2 */

}

/**
 * @brief I2C2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_I2C2_Init(void) {

	/* USER CODE BEGIN I2C2_Init 0 */

	/* USER CODE END I2C2_Init 0 */

	/* USER CODE BEGIN I2C2_Init 1 */

	/* USER CODE END I2C2_Init 1 */
	hi2c2.Instance = I2C2;
	hi2c2.Init.ClockSpeed = 100000;
	hi2c2.Init.DutyCycle = I2C_DUTYCYCLE_2;
	hi2c2.Init.OwnAddress1 = 0;
	hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
	hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
	hi2c2.Init.OwnAddress2 = 0;
	hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
	hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
	if (HAL_I2C_Init(&hi2c2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN I2C2_Init 2 */

	/* USER CODE END I2C2_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void) {

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_SlaveConfigTypeDef sSlaveConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = { 0 };

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim1.Init.Period = 1055;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
		Error_Handler();
	}
	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
	sSlaveConfig.InputTrigger = TIM_TS_ITR3;
	if (HAL_TIM_SlaveConfigSynchro(&htim1, &sSlaveConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_OC2REF;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 928;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_1);
	sConfigOC.Pulse = 88;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_SET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_2);
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_3);
	sConfigOC.Pulse = 888;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim1, TIM_CHANNEL_4);
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 0;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */
	HAL_TIM_MspPostInit(&htim1);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void) {

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_SlaveConfigTypeDef sSlaveConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 0;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 627;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim3) != HAL_OK) {
		Error_Handler();
	}
	sSlaveConfig.SlaveMode = TIM_SLAVEMODE_EXTERNAL1;
	sSlaveConfig.InputTrigger = TIM_TS_ITR0;
	if (HAL_TIM_SlaveConfigSynchro(&htim3, &sSlaveConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 624;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_1) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_1);
	sConfigOC.Pulse = 23;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_2) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_2);
	sConfigOC.Pulse = 623;
	if (HAL_TIM_PWM_ConfigChannel(&htim3, &sConfigOC, TIM_CHANNEL_3) != HAL_OK) {
		Error_Handler();
	}
	__HAL_TIM_DISABLE_OCxPRELOAD(&htim3, TIM_CHANNEL_3);
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */
	HAL_TIM_MspPostInit(&htim3);

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void) {

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 0;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 2;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */

}

/**
 * @brief UART4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_UART4_Init(void) {

	/* USER CODE BEGIN UART4_Init 0 */

	/* USER CODE END UART4_Init 0 */

	/* USER CODE BEGIN UART4_Init 1 */

	/* USER CODE END UART4_Init 1 */
	huart4.Instance = UART4;
	huart4.Init.BaudRate = 9600;
	huart4.Init.WordLength = UART_WORDLENGTH_8B;
	huart4.Init.StopBits = UART_STOPBITS_1;
	huart4.Init.Parity = UART_PARITY_NONE;
	huart4.Init.Mode = UART_MODE_TX_RX;
	huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart4.Init.OverSampling = UART_OVERSAMPLING_16;
	if (HAL_UART_Init(&huart4) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN UART4_Init 2 */

	/* USER CODE END UART4_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void) {

	/* DMA controller clock enable */
	__HAL_RCC_DMA2_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA2_Stream0_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA2_Stream0_IRQn, 5, 0);
	HAL_NVIC_EnableIRQ(DMA2_Stream0_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOE_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();
	__HAL_RCC_GPIOD_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_11 | GPIO_PIN_12, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOD, GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15, GPIO_PIN_RESET);

	/*Configure GPIO pins : PC3 PC4 PC11 PC12 */
	GPIO_InitStruct.Pin = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_11 | GPIO_PIN_12;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/*Configure GPIO pins : PD12 PD13 PD14 PD15 */
	GPIO_InitStruct.Pin = GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/* USER CODE BEGIN Header_ConnecToVGATask */
/**
 * @brief  Function implementing the vgaConnectionTa thread.
 * @param  argument: Not used
 * @retval None
 */
/* USER CODE END Header_ConnecToVGATask */
void ConnecToVGATask(void *argument) {
	/* USER CODE BEGIN 5 */
	bool waitbeforeNextConnection = false;
	/* Infinite loop */
	while (true) {

		if (waitbeforeNextConnection) {
			waitbeforeNextConnection = true;
			osExDelayMs(10 * 1000); // 10 sec delay
		}

		// We have to clear the Error code since in the I2C_Master_Receive the field is cleared AFTER the BUSY_FLAG wait loop,
		// otherwhise we will OR a possible previous error with the HAL_I2C_ERROR_TIMEOUT if the bus is still busy
		hi2c2.ErrorCode = HAL_I2C_ERROR_NONE;
		// Assert: I2C handle should be ready with no error set
		DebugAssert(hi2c2.ErrorCode == 0 && hi2c2.State == HAL_I2C_STATE_READY);
		// First stem: launch the Rcv command with the intterupts enabled
		HAL_StatusTypeDef halStatus = HAL_I2C_Master_Receive_IT(&hi2c2, EDID_DDC2_I2C_DEVICE_ADDRESS << 1, (uint8_t*) &_vgaEDID, sizeof(EDID));
		if (halStatus == HAL_ERROR && hi2c2.ErrorCode & HAL_I2C_ERROR_TIMEOUT) {
			// If the HAL_ERROR is flagged with the timeout, the bus is busy. We can't do anything.
			printf("Unable to initialize VGA I2C transmission. Nothing connected (bus busy)\r\n");
			// We simply repeat with some waiting
			waitbeforeNextConnection = true;
			continue;

		} else if (halStatus != HAL_OK) {
			// Something went wrong that was not addressed in the code
			Error_Handler();
		}

		UInt32 result = osEventFlagsWait(_vgaEDIDRcvEvntHandle, I2CVGA_EDID_ERROR | I2CVGA_EDID_RECEIVED, osFlagsWaitAny, osWaitForever);
		if (osExResultIsFlagsErrorCode(result)) {
			Error_Handler();
		} else if ((result & I2CVGA_EDID_ERROR) == (result & I2CVGA_EDID_RECEIVED)) {
			// We can't have both error + rcv or none of the two
			Error_Handler();
		}

		if (result & I2CVGA_EDID_ERROR) {
			HandleI2CError(&hi2c2);
			waitbeforeNextConnection = true;
			continue;
		}

		if (!EDIDIsChecksumValid(&_vgaEDID)) {
			printf("\033[1;33mVGA EDID checksum is not valid. Cannot connect\033[0m\r\n");
			continue;
		}

		printf("\033[1;92mVGA connected\033[0m\r\n");
		EDIDDumpStructure(&_vgaEDID);

		while (1) {

		}

	}
	/* USER CODE END 5 */
}

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM7 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM7) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */

	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();

	CLEAR_BIT(GPIOD->ODR, (GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_15));
	GPIOD->ODR |= GPIO_PIN_14;
	while (1) {
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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
