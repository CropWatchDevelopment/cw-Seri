/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include <stdbool.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

int is_connected = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */





//typedef struct {
//    int port;
//    char data[64];  // Adjust size as needed
//    bool has_data;
//} rx_message_t;
//
//int parse_rx_message(uint8_t *buffer, rx_message_t *result) {
//    char *rx_start = strstr((char*)buffer, "RX: ");
//    if (!rx_start) {
//        return 0; // No RX message found
//    }
//
//    // Initialize result
//    result->port = -1;
//    result->data[0] = '\0';
//    result->has_data = false;
//
//    // Find port (P:x)
//    char *port_start = strstr(rx_start, "P:");
//    if (port_start) {
//        port_start += 2; // Skip "P:"
//        result->port = atoi(port_start);
//    }
//
//    // Find data after the last comma and space
//    char *data_start = NULL;
//    char *current = rx_start;
//
//    // Look for the pattern ", " followed by hex data (not a parameter like "S:", "R:", etc.)
//    while ((current = strstr(current, ", ")) != NULL) {
//        current += 2; // Skip ", "
//
//        // Check if this is a parameter (contains :) or data (hex characters)
//        char *colon = strchr(current, ':');
//        char *comma = strchr(current, ',');
//        char *cr = strchr(current, '\r');
//
//        // If no colon before next comma/CR, this might be data
//        if (!colon || (comma && colon > comma) || (cr && colon > cr)) {
//            // Check if it looks like hex data (only hex chars, spaces, and end chars)
//            bool is_hex = true;
//            char *check = current;
//            while (*check && *check != '\r' && *check != '\n') {
//                if (!isxdigit(*check) && *check != ' ') {
//                    is_hex = false;
//                    break;
//                }
//                check++;
//            }
//
//            if (is_hex && check > current) {
//                data_start = current;
//                break;
//            }
//        }
//    }
//
//    // Extract data if found
//    if (data_start) {
//        int i = 0;
//        char *end = data_start;
//
//        // Find end of data (CR or LF)
//        while (*end && *end != '\r' && *end != '\n' && i < 63) {
//            if (*end != ' ') { // Skip spaces
//                result->data[i++] = *end;
//            }
//            end++;
//        }
//        result->data[i] = '\0';
//
//        if (i > 0) {
//            result->has_data = true;
//        }
//    }
//
//    return (result->port != -1) ? 1 : 0;
//}




















int lorawan_is_config_required(UART_HandleTypeDef *huart)
{
	  uint8_t rxbuf[16] = {0};
	  // Totally Flush buffer and stuff
	  HAL_UART_AbortReceive(huart);
	  __HAL_UART_FLUSH_DRREGISTER(huart);
	  __HAL_UART_CLEAR_IDLEFLAG(huart);
	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

	  HAL_UART_Transmit(&huart2, (uint8_t*)"AT%S 502=?\r\n", 12, 300);
	  HAL_UART_Receive(huart, rxbuf, 16, 200);

	  if (rxbuf[1] == '0')
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 0;
	  }
	  else
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 1;
	  }
}



int lorawan_is_connected(UART_HandleTypeDef *huart)
{
	  uint8_t rxbuf[256] = {0};
	  // Totally Flush buffer and stuff
	  HAL_UART_AbortReceive(huart);
	  __HAL_UART_FLUSH_DRREGISTER(huart);
	  __HAL_UART_CLEAR_IDLEFLAG(huart);
	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

	  HAL_UART_Transmit(huart, (uint8_t*)"ATI 3001\r\n", 10, 300);
	  HAL_UART_Receive(huart, rxbuf, 7, 100);

	  if (rxbuf[1] == '0')
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 0;
	  }
	  else
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 1;
	  }
}


int join(UART_HandleTypeDef *huart)
{
	uint16_t total_rcv = 0;
	int16_t total_expected =11;
	uint8_t rxbuf[256] = {0};
	HAL_UART_Transmit(&huart2, (uint8_t*)"AT+JOIN\r\n", 9, 300);
	HAL_UART_Receive(&huart2, rxbuf, 4, 100);
	__HAL_UART_FLUSH_DRREGISTER(&huart2);
	__HAL_UART_CLEAR_IDLEFLAG(&huart2);

	while (total_expected > 0)
	{
		  HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total_rcv, 100, &total_rcv, 10000);
		  total_expected -= total_rcv;
	}

	__HAL_UART_FLUSH_DRREGISTER(&huart2);
	__HAL_UART_CLEAR_IDLEFLAG(&huart2);

	if (rxbuf[8] == 'O')
	{
		return 1;
		__NOP(); // success
	}
	else
	{
		return 0;
		__NOP(); // fail
	}
}



int SendData(UART_HandleTypeDef *huart, char* data)
{
	uint16_t total_rcv = 0;
	int16_t total_expected = 200;
	uint8_t rxbuf[256] = {0};
	int data_size = strlen(data);
	HAL_UART_Transmit(&huart2, (uint8_t*)data, data_size, 300);
	HAL_UART_Receive(&huart2, rxbuf, 4, 100); // Get the OK back from the sync data send
	  HAL_UART_AbortReceive(huart);
	  __HAL_UART_FLUSH_DRREGISTER(huart);
	  __HAL_UART_CLEAR_IDLEFLAG(huart);
	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

//	while (total_expected > 0)
//	{
//		  HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total_rcv, 200, &total_rcv, 15000);
//		  total_expected -= total_rcv;
//	}

	  HAL_UART_AbortReceive(huart);
	  __HAL_UART_FLUSH_DRREGISTER(huart);
	  __HAL_UART_CLEAR_IDLEFLAG(huart);
	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

	  return 1;
	/*
		ACK  The message was sent and acknowledged by the LNS
		FAIL Sending of the message was failed
		SENT The message was sent successfully but not acknowledged by the LNS

		----Sample Response (no response downlink or what ever):
		00000000 0a 31 0d 0a 4f 4b 0d 0a   4f 4b 0d 0a 54 58 3a 20 	␊1␍␊OK␍␊  OK␍␊TX:
		00000016 5b 53 45 4e 54 5d 2c 20   43 3a 37 2c 20 46 3a 39 	[SENT],   C:7, F:9
		00000032 32 32 30 30 30 30 30 30   48 7a 2c 20 44 52 3a 35 	22000000  Hz, DR:5
		00000048 0d                                                	␍

		----Sample Response WITH data comin' back, the data should be: EE EE EE EE
		00000000 0a 4f 4b 0d 0a 41 44 52   58 3a 20 44 52 3a 5b 78 	␊OK␍␊ADR  X: DR:[x
		00000016 5d 20 35 2c 20 50 4f 3a   5b 76 5d 20 38 2c 20 4e 	] 5, PO:  [v] 8, N
		00000032 42 3a 5b 78 5d 20 31 2c   20 43 50 3a 5b 78 5d 20 	B:[x] 1,   CP:[x]
		00000048 66 66 3a 30 30 2c 20 50   4c 3a 5b 78 5d 20 32 34 	ff:00, P  L:[x] 24
		00000064 32 0d 0a 54 58 3a 20 5b   53 45 4e 54 5d 2c 20 43 	2␍␊TX: [  SENT], C
		00000080 3a 35 2c 20 46 3a 39 32   32 38 30 30 30 30 30 48 	:5, F:92  2800000H
		00000096 7a 2c 20 44 52 3a 35 0d   0a 52 58 3a 20 57 3a 31 	z, DR:5␍  ␊RX: W:1
		00000112 2c 20 50 3a 31 2c 20 43   3a 34 2c 20 46 3a 39 32 	, P:1, C  :4, F:92
		00000128 32 36 30 30 30 30 30 48   7a 2c 20 44 52 3a 35 2c 	2600000H  z, DR:5,
		00000144 20 52 3a 2d 39 33 64 42   6d 2c 20 53 3a 38 64 42 	 R:-93dB  m, S:8dB
		00000160 2c 20 45 45 45 45 45 45   45 45 0d                	, EEEEEE  EE␍
	*/

//	if (rxbuf[8] == 'O')
//	{
//		return 1;
//		__NOP(); // success
//	}
//	else
//	{
//		return 0;
//		__NOP(); // fail
//	}
}

int lorawan_chip_temp(UART_HandleTypeDef *huart)
{
	  uint8_t rxbuf[256] = {0};
	  // Totally Flush buffer and stuff
	  HAL_UART_AbortReceive(huart);
	  __HAL_UART_FLUSH_DRREGISTER(huart);
	  __HAL_UART_CLEAR_IDLEFLAG(huart);
	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

	  HAL_UART_Transmit(huart, (uint8_t*)"AT+TEMP\r\n", 9, 300);
	  HAL_UART_Receive(huart, rxbuf, 7, 100);

	  if (rxbuf[1] == '0')
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 0;
	  }
	  else
	  {
		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
		  return 1;
	  }
}






/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */

//  HAL_UART_Transmit(&huart2, "AT\r\n", 4, 300); // One initial AT to clear any odd commands sent before

  // Set LoRaWAN Settings
//  HAL_UART_Transmit(&huart2, "ATS 602=1\r\n", 11, 300); // Activation Mode OTAA (0 = ABP, 1 = OTAA)
//  HAL_UART_Transmit(&huart2, "ATS 603=0\r\n", 11, 300); // Set CLASS to A
//  HAL_UART_Transmit(&huart2, "ATS 604=0\r\n", 11, 300); // Conformed 0 = NO, 1 = yes
//  HAL_UART_Transmit(&huart2, "ATS 611=9\r\n", 11, 300); // Set Region to AS923-1 (JAPAN)
//  HAL_UART_Transmit(&huart2, "ATS 302=9600\r\n", 14, 300);

  // Set Dev EUI, App Key, and JOIN KEY
//  HAL_UART_Transmit(&huart2, "AT%S 500=\"0025CA00000055F70025CA00000055F7\"\r\n", 45, 300); // APP KEY
//  HAL_UART_Transmit(&huart2, "AT%S 501=\"0025CA00000055F7\"\r\n", 29, 300); // DEV EUI
//  HAL_UART_Transmit(&huart2, "AT%S 502=\"0000000000000000\"\r\n", 29, 300); // JOIN EUI
//
//  HAL_UART_Transmit(&huart2, "AT&W\r\n", 6, 300); // SAVE ALL!
//  HAL_UART_Transmit(&huart2, "ATZ\r\n", 5, 300); // Soft reboot!

//  HAL_UART_Transmit(&huart2, "AT+JOIN\r\n", 9, 300); // Test join the network
//  __NOP();
//
//  while (1)
//  {
//	  HAL_UART_Transmit(&huart2, "AT\r\n", 4, 300);
//	  HAL_UART_Transmit(&huart2, "AT+SEND \"AABBCC\"\r\n", 18, 300);
//
//	  HAL_Delay(10);
//  }



  lorawan_is_config_required(&huart2);


  is_connected = lorawan_is_connected(&huart2);
  if (!is_connected)
  {
	  is_connected = join(&huart2);
  }











































  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

	is_connected = lorawan_is_connected(&huart2);
	if (!is_connected)
	{
	  is_connected = join(&huart2);
	}
	else
	{
		// We ARE connected!
		SendData(&huart2, "AT+SEND \"AABB\"\r\n");
	}

	HAL_Delay(60000);
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
  RCC_CRSInitTypeDef RCC_CRSInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_LSE
                              |RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_5;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_MSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART1|RCC_PERIPHCLK_USART2
                              |RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the SYSCFG APB clock
  */
  __HAL_RCC_CRS_CLK_ENABLE();

  /** Configures CRS
  */
  RCC_CRSInitStruct.Prescaler = RCC_CRS_SYNC_DIV1;
  RCC_CRSInitStruct.Source = RCC_CRS_SYNC_SOURCE_LSE;
  RCC_CRSInitStruct.Polarity = RCC_CRS_SYNC_POLARITY_RISING;
  RCC_CRSInitStruct.ReloadValue = __HAL_RCC_CRS_RELOADVALUE_CALCULATE(48000000,32768);
  RCC_CRSInitStruct.ErrorLimitValue = 34;
  RCC_CRSInitStruct.HSI48CalibrationValue = 32;

  HAL_RCCEx_CRSConfig(&RCC_CRSInitStruct);
}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 255;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the WakeUp
  */
  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_DMADISABLEONERROR_INIT;
  huart1.AdvancedInit.DMADisableonRxError = UART_ADVFEATURE_DMA_DISABLEONRXERROR;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 9600;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

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
