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

#include "battery/vbat_lorawan.h"
#include "sensirion/sensirion.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SLEEP_TIME_MINUTES 1  // Sleep time in minutes between LoRaWAN transmissions
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

int is_connected = 0;
static uint8_t wakeup_counter = 0;  // Counter for sleep intervals (calculated based on SLEEP_TIME_MINUTES)
// Calculate how many 32-second intervals make up SLEEP_TIME_MINUTES
static const uint8_t WAKEUPS_PER_CYCLE = (SLEEP_TIME_MINUTES * 60) / 32;
static bool first_run = true;  // Flag to ensure first transmission happens immediately

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC_Init(void);
/* USER CODE BEGIN PFP */
void EnterDeepSleepMode(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


char find_char_after(const char *str, const char *keyword) {
    if (!str || !keyword) return '\0';

    // Simple substring search
    const char *p = str;
    const char *k;

    while (*p) {
        const char *s = p;
        k = keyword;
        while (*s && *k && *s == *k) {
            s++;
            k++;
        }
        if (*k == '\0') {
            // Found full keyword, return next char if available
            return *s ? *s : '\0';
        }
        p++;
    }
    return '\0'; // Not found
}


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




















//int lorawan_is_config_required(UART_HandleTypeDef *huart)
//{
//	  HAL_UART_Transmit(huart, (uint8_t*)"AT\r\n", 4, 300);
//	uint8_t rxwakebuf[16] = {0};
//	  HAL_UART_Receive(huart, rxwakebuf, 6, 200);
//	  uint8_t rxbuf[16] = {0};
//	  // Totally Flush buffer and stuff
//	  HAL_UART_AbortReceive(huart);
//	  __HAL_UART_FLUSH_DRREGISTER(huart);
//	  __HAL_UART_CLEAR_IDLEFLAG(huart);
//	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);
//
//	  HAL_UART_Transmit(&huart2, (uint8_t*)"AT%S 502=?\r\n", 12, 300);
//	  HAL_UART_Receive(huart, rxbuf, 16, 200);
//
//	  if (rxbuf[1] == '0')
//	  {
//		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
//		  return 0;
//	  }
//	  else
//	  {
//		  memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
//		  return 1;
//	  }
//}



int lorawan_is_connected(UART_HandleTypeDef *huart)
{
	  HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
	  HAL_Delay(300); // Let the OK come back
	uint8_t rxwakebuf[16] = {0};
	  HAL_UART_Receive(huart, rxwakebuf, 4, 300);
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
	if (is_connected) return 1;
	HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
	HAL_Delay(300); // let OK come back!
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

	char result = find_char_after((const char*)rxbuf, "JOIN: [");
	char error14 = find_char_after((const char*)rxbuf, "\nERROR 1");
	if (result == 'O' || error14 == '4')
	{
		is_connected = 1;
		return 1;
		__NOP(); // success
	}
	else
	{
		is_connected = 0;
		return 0;
		__NOP(); // fail
	}
}



int SendData(UART_HandleTypeDef *huart, char* data)
{
	HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
	HAL_Delay(300);
	uint16_t total_rcv = 0;
	int16_t total_expected = 200;
	uint8_t rxbuf[256] = {0};
	int data_size = strlen(data);
	HAL_UART_Transmit(&huart2, (uint8_t*)data, data_size, 300);
//	HAL_UART_Receive(&huart2, rxbuf, 4, 100); // Get the OK back from the sync data send
//	  HAL_UART_AbortReceive(huart);
//	  __HAL_UART_FLUSH_DRREGISTER(huart);
//	  __HAL_UART_CLEAR_IDLEFLAG(huart);
//	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

//	while (total_expected > 0)
//	{
//		  HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total_rcv, 200, &total_rcv, 15000);
//		  total_expected -= total_rcv;
//	}

//	  HAL_UART_AbortReceive(huart);
//	  __HAL_UART_FLUSH_DRREGISTER(huart);
//	  __HAL_UART_CLEAR_IDLEFLAG(huart);
//	  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

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

int lorawan_set_battery_level(UART_HandleTypeDef *huart, uint8_t battery_level)
{
    char cmd[32];   // enough space for command
    int len = snprintf(cmd, sizeof(cmd), "AT+BAT %u\r\n", battery_level);

    if (len <= 0 || len >= sizeof(cmd)) {
        return -1; // encoding error or buffer too small
    }

    // Flush / clear UART
    HAL_UART_AbortReceive(huart);
    __HAL_UART_FLUSH_DRREGISTER(huart);
    __HAL_UART_CLEAR_IDLEFLAG(huart);
    __HAL_UART_CLEAR_FLAG(huart,
        UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

    // Transmit command
    if (HAL_UART_Transmit(huart, (uint8_t*)cmd, (uint16_t)len, 300) != HAL_OK) {
        return -2; // TX error
    }

    HAL_Delay(300);
    return 0; // success
}




void LoRaWAN_SendHex(const uint8_t *payload, size_t length)
{
    static const char HEX[16] = "0123456789ABCDEF";
    static const char prefix[] = "AT+SEND \"";
    static const char suffix[] = "\"\r\n";

    if (!payload || length == 0) return;

    // Max wire size = len*2 hex + 8(prefix) + 3(suffix)
    // 242B -> 484 + 11 = 495 bytes fits in 512
    static uint8_t txbuf[512];

    const size_t prefix_len = sizeof(prefix) - 1;
    const size_t suffix_len = sizeof(suffix) - 1;
    const size_t need = prefix_len + (length * 2u) + suffix_len;

    if (need > sizeof(txbuf)) {
        // Payload too large for our static buffer; don't send a truncated command
        return;
    }

    size_t idx = 0;

    // Copy prefix
    for (size_t i = 0; i < prefix_len; ++i) txbuf[idx++] = (uint8_t)prefix[i];

    // Hex-encode payload
    for (size_t i = 0; i < length; ++i) {
        uint8_t b = payload[i];
        txbuf[idx++] = (uint8_t)HEX[b >> 4];
        txbuf[idx++] = (uint8_t)HEX[b & 0x0F];
    }

    // Copy suffix
    for (size_t i = 0; i < suffix_len; ++i) txbuf[idx++] = (uint8_t)suffix[i];

    HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
    HAL_Delay(300);
    // Exactly one TX
    HAL_UART_Transmit(&huart2, txbuf, (uint16_t)idx, 300);
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
  MX_I2C1_Init();
  MX_ADC_Init();
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
//  HAL_UART_Transmit(&huart2, "ATS 213=2000\r\n", 14, 300); // Set CLASS to A
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



//  lorawan_is_config_required(&huart2);

//  float batteryVoltage = ReadBatteryVoltage();
//  char buf[32];
//  int len = snprintf(buf, sizeof(buf), "%.2f\r\n", batteryVoltage);  // format with 2 decimals









































  HAL_GPIO_WritePin(DBG_LED_GPIO_Port, DBG_LED_Pin, GPIO_PIN_SET);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	// Check if it's time for LoRaWAN transmission
	// Transmit on first run, then every WAKEUPS_PER_CYCLE wake-ups
	// With SLEEP_TIME_MINUTES=2, WAKEUPS_PER_CYCLE=8, so we transmit every 8 wake-ups
	if (first_run || wakeup_counter >= WAKEUPS_PER_CYCLE)
	{
		// Reset counter for next cycle
		wakeup_counter = 0;


//		if (first_run)
//		{
			first_run = false;
			// Check for connection again just incase on initial startup, we didn't connect
			if(is_connected == 0)
			{
				join(&huart2);
			}
			HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
			HAL_Delay(1000); // Increased delay for sensor power-up and stabilization
			scan_i2c_bus();
			bool i2c_success = sensor_init_and_read();
			HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);
			if (i2c_success)
			{
				HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin|I2C_ENABLE_Pin, GPIO_PIN_SET);
				HAL_Delay(300); // just tto let the power stabalize
				int aproxBatteryTemp_c = ((calculated_temp - 55) / 10);
				uint8_t battery = vbat_measure_and_encode(&hadc, ADC_CHANNEL_0, aproxBatteryTemp_c, /*external_power_present=*/false);
				HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin|I2C_ENABLE_Pin, GPIO_PIN_RESET);
				lorawan_set_battery_level(&huart2, battery);

				uint8_t payload[5];
				payload[0] = (uint8_t)(calculated_temp >> 8);     // high byte
				payload[1] = (uint8_t)(calculated_temp & 0xFF);   // low byte
				payload[2] = calculated_hum;
				LoRaWAN_SendHex(payload, 3);
			}

//		}
//		configWakeupTime();
		EnterDeepSleepMode();
	}



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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE|RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
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
                              |RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart1ClockSelection = RCC_USART1CLKSOURCE_PCLK2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCCEx_EnableLSECSS();
}

/**
  * @brief ADC Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC_Init(void)
{

  /* USER CODE BEGIN ADC_Init 0 */

  /* USER CODE END ADC_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC_Init 1 */

  /* USER CODE END ADC_Init 1 */

  /** Configure the global features of the ADC (Clock, Resolution, Data Alignment and number of conversion)
  */
  hadc.Instance = ADC1;
  hadc.Init.OversamplingMode = DISABLE;
  hadc.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV1;
  hadc.Init.Resolution = ADC_RESOLUTION_12B;
  hadc.Init.SamplingTime = ADC_SAMPLETIME_160CYCLES_5;
  hadc.Init.ScanConvMode = ADC_SCAN_DIRECTION_FORWARD;
  hadc.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc.Init.ContinuousConvMode = DISABLE;
  hadc.Init.DiscontinuousConvMode = DISABLE;
  hadc.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc.Init.DMAContinuousRequests = DISABLE;
  hadc.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc.Init.LowPowerAutoWait = DISABLE;
  hadc.Init.LowPowerFrequencyMode = ENABLE;
  hadc.Init.LowPowerAutoPowerOff = DISABLE;
  if (HAL_ADC_Init(&hadc) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure for the selected ADC regular channel to be converted.
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_RANK_CHANNEL_NUMBER;
  if (HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC_Init 2 */

  /* USER CODE END ADC_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.Timing = 0x00000608;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(DBG_LED_GPIO_Port, DBG_LED_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin|I2C_ENABLE_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : DBG_LED_Pin */
  GPIO_InitStruct.Pin = DBG_LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(DBG_LED_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : VBAT_MEAS_EN_Pin I2C_ENABLE_Pin */
  GPIO_InitStruct.Pin = VBAT_MEAS_EN_Pin|I2C_ENABLE_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void configWakeupTime()
{
	HAL_GPIO_WritePin(DBG_LED_GPIO_Port, DBG_LED_Pin, GPIO_PIN_SET);
	uint32_t wakeup_timer_value = 32 * 2048 - 1;  // 32 seconds
	HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeup_timer_value, RTC_WAKEUPCLOCK_RTCCLK_DIV16);
}
/**
  * @brief  Wakeup Timer callback.
  * @param  hrtc pointer to a RTC_HandleTypeDef structure that contains
  *                the configuration information for RTC.
  * @retval None
  */
void HAL_RTCEx_WakeUpTimerEventCallback(RTC_HandleTypeDef *hrtc)
{
  /* Increment counter - process LoRaWAN based on SLEEP_TIME_MINUTES setting */

  wakeup_counter++;
  
  /* Clear the wake-up timer flag to acknowledge the interrupt */
  __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(hrtc, RTC_FLAG_WUTF);
}

/**
  * @brief  Configure GPIOs for ultra-low power consumption
  * @retval None
  */
void ConfigureGPIOForLowPower(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  
  /* Enable all GPIO clocks */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  
  /* Configure all GPIO pins as analog to reduce power consumption */
  GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  
  /* Configure GPIOA pins (except UART pins PA2, PA3 and PA9, PA10) */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_4 | GPIO_PIN_5 | 
                        GPIO_PIN_6 | GPIO_PIN_7 | GPIO_PIN_8 | GPIO_PIN_11 | 
                        GPIO_PIN_12 | GPIO_PIN_13 | GPIO_PIN_14 | GPIO_PIN_15;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  
  /* Configure all GPIOB pins */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
  
  /* Configure GPIOC pins (except PC14, PC15 for LSE crystal) */
  GPIO_InitStruct.Pin = GPIO_PIN_0 | GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3 | 
                        GPIO_PIN_4 | GPIO_PIN_5 | GPIO_PIN_6 | GPIO_PIN_7 | 
                        GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10 | GPIO_PIN_11 | 
                        GPIO_PIN_12 | GPIO_PIN_13;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  
  /* Configure all GPIOD pins */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
  
  /* Configure all GPIOH pins */
  GPIO_InitStruct.Pin = GPIO_PIN_All;
  HAL_GPIO_Init(GPIOH, &GPIO_InitStruct);
}

/**
  * @brief  Restore GPIOs after wake-up
  * @retval None
  */
void RestoreGPIOAfterWakeup(void)
{
  /* Reinitialize GPIOs needed for UART operation */
  MX_GPIO_Init();
}

/**
  * @brief  Enter Deep Sleep Mode using STOP mode with RTC wake-up
  * @retval None
  */
void EnterDeepSleepMode(void)
{
  /* Configure all GPIOs for ultra-low power */
  ConfigureGPIOForLowPower();
  
  /* Disable unnecessary peripheral clocks */
  __HAL_RCC_I2C1_CLK_DISABLE();
  __HAL_RCC_USART1_CLK_DISABLE();
  __HAL_RCC_USART2_CLK_DISABLE();
//  __HAL_RCC_GPIOA_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  __HAL_RCC_GPIOC_CLK_DISABLE();
  __HAL_RCC_GPIOD_CLK_DISABLE();
  __HAL_RCC_GPIOH_CLK_DISABLE();
  
  /* Suspend SysTick to avoid wake-up from SysTick interrupt */
  HAL_SuspendTick();
  
  /* Clear any pending wake-up flags before sleeping */
  __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);
  __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
  
  /* Restart the RTC wake-up timer for next wake-up */
  configWakeupTime();
  
  /* Enter STOP Mode with Low Power Regulator */
  HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
  
  /* === DEVICE IS NOW IN DEEP SLEEP === */
  /* === WAKE UP OCCURS HERE === */
  
  /* Upon wake-up, the system clock needs to be reconfigured */
  SystemClock_Config();
  
  /* Re-enable peripheral clocks */
  __HAL_RCC_I2C1_CLK_ENABLE();
//  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();
  
  /* Restore GPIO configuration for normal operation */
//  RestoreGPIOAfterWakeup();
  
  /* Re-initialize I2C and UARTs */
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  
  /* Resume SysTick */
  HAL_ResumeTick();
  
  /* Small delay to ensure system is stable after wake-up */
  HAL_Delay(10);
}

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
//  while (1)
//  {
//  }
  HAL_NVIC_SystemReset();
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
