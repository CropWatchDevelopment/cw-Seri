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
#include <stdio.h>

#include "battery/vbat_lorawan.h"
#include "sensirion/sensirion.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SLEEP_TIME_MINUTES 10 // Sleep time in minutes between LoRaWAN transmissions
// Base sleep interval length (seconds) for each STOP cycle (RTC wake-up)
#define SLEEP_INTERVAL_SECONDS 30

#define DEV_EUI "0025CA0000002694"
#define JOIN_EUI "0025CA00000055F7"
#define LSE_RETRY_DELAY_MS 60000U // retry LSE recovery every 60 seconds while on LSI
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

static volatile uint16_t wakeup_counter = 0; // incremented in ISR
static uint16_t wakes_accum = 0;             // main-loop accumulator
static bool first_run = true;                // Flag to ensure first transmission happens immediately
// Number of wakeups per transmission cycle (ceil division to avoid truncation)
static uint16_t wakeups_per_cycle =
    (uint16_t)((SLEEP_TIME_MINUTES * 60u + (SLEEP_INTERVAL_SECONDS - 1u)) / SLEEP_INTERVAL_SECONDS);

static volatile bool rtc_using_lsi = false;
static volatile bool lse_fault_pending = false;
static uint32_t wake_interval_ms = SLEEP_INTERVAL_SECONDS * 1000u;
static uint32_t lse_retry_wakeup_budget = 0;

// LoRaWAN UART Baud
//  Start out at 115200 as it is the 1st time starting baud of the Ezurio LoRa module
//  then switch forever to 9600 after we switch the baud of the ezurio module.
uint32_t baudRate = 115200;
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
void configWakeupTime(void);
void RTC_RequestClockFallback(void);
static void RTC_ServiceClockHealth(uint16_t wakeups_since_last);
static bool RTC_SwitchClockToLSI(void);
static bool RTC_TryRestoreLSE(void);
static bool RTC_ReInitPreserveConfig(void);
static uint32_t RTC_GetRTCCLK_Hz(void);
static uint32_t RTC_GetCkSpreHz(void);
static void RTC_UpdatePrescalersForClock(void);
static void RTC_UpdateWakeDerivatives(uint32_t programmed_ticks, uint32_t ck_spre_hz);
static uint32_t RTC_ComputeWakeupsForDelay(uint32_t delay_ms);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// UART state verification function
static bool verify_uart_ready(UART_HandleTypeDef *huart)
{
  return (huart != NULL); // Simplified check for now
}

// Debug logging helpers over huart1 (115200 baud)
static void dbg_print(const char *s)
{
  if (!s)
    return;
  size_t n = strlen(s);
  HAL_StatusTypeDef status = HAL_UART_Transmit(&huart1, (uint8_t *)s, (uint16_t)n, 100);
  if (status != HAL_OK)
  {
    // UART failed, try to recover
    HAL_UART_DeInit(&huart1);
    HAL_Delay(10);
    MX_USART1_UART_Init();
  }
}
static void dbg_print_line(const char *s)
{
  if (!s)
    return;
  dbg_print(s);
  dbg_print("\r\n");
}
static HAL_StatusTypeDef UART2_SetBaud(uint32_t br)
{
  // Drain TX and stop RX before touching the peripheral
  while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET)
  { /* wait */
  }
  HAL_UART_AbortReceive(&huart2);
  __HAL_UART_FLUSH_DRREGISTER(&huart2);
  __HAL_UART_CLEAR_IDLEFLAG(&huart2);
  __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

  if (huart2.Init.BaudRate == br)
    return HAL_OK;
  HAL_UART_DeInit(&huart2);
  huart2.Init.BaudRate = br;
  return HAL_UART_Init(&huart2);
}

static void uart2_rx_flush(UART_HandleTypeDef *huart)
{
  HAL_UART_AbortReceive(huart);
  __HAL_UART_FLUSH_DRREGISTER(huart);
  __HAL_UART_CLEAR_IDLEFLAG(huart);
  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);
}

/* 1) Simple: for NUL-terminated strings */
static inline bool str_exists(const char *s, const char *token)
{
  if (!s || !token || *token == '\0') return false;
  return strstr(s, token) != NULL;
}

/* 2) Robust: for binary buffers that may not be NUL-terminated */
static bool span_exists(const void *buf, size_t len, const char *token)
{
  if (!buf || !token) return false;
  const size_t tlen = strlen(token);
  if (tlen == 0 || len < tlen) return false;

  const uint8_t *p = (const uint8_t *)buf;
  for (size_t off = 0; off + tlen <= len; ++off) {
    if (memcmp(p + off, token, tlen) == 0) return true;
  }
  return false;
}

static int uart2_probe_and_align(void)
{
  // Try 9600 first, then 115200
  const uint32_t bauds[2] = {9600u, 115200u};
  uint8_t buf[64];
  uint16_t got = 0;

  for (int pass = 0; pass < 2; ++pass)
  {
    uint32_t br = bauds[pass];

    // Ensure UART really is at this baud
    UART2_SetBaud(br);
    HAL_Delay(30);

    for (int attempt = 0; attempt < 3; ++attempt)
    {
      uart2_rx_flush(&huart2);
      HAL_Delay(10);

      // Some modules need a nudge when sleeping
      const uint8_t at[] = "AT\r";
      HAL_UART_Transmit(&huart2, (uint8_t *)at, sizeof(at) - 1, 50);

      memset(buf, 0, sizeof buf);
      got = 0;
      if (HAL_UARTEx_ReceiveToIdle(&huart2, buf, sizeof buf, &got, 300) == HAL_OK && got > 0)
      {
        // Look for typical tokens anywhere in the frame
        for (uint16_t i = 0; i + 1 < got; ++i)
        {
          if (buf[i] == 'O' && buf[i + 1] == 'K')
            return pass; // 0 if 9600, 1 if 115200
        }
        // Some stacks send banners first; send AT once more immediately
        uart2_rx_flush(&huart2);
        HAL_UART_Transmit(&huart2, (uint8_t *)at, sizeof(at) - 1, 50);
        memset(buf, 0, sizeof buf);
        got = 0;
        if (HAL_UARTEx_ReceiveToIdle(&huart2, buf, sizeof buf, &got, 300) == HAL_OK && got > 0)
        {
          for (uint16_t i = 0; i + 1 < got; ++i)
          {
            if (buf[i] == 'O' && buf[i + 1] == 'K')
              return pass;
          }
        }
      }
      HAL_Delay(80);
    }
  }

  return -1; // still nothing at either rate
}

char find_char_after(const char *str, const char *keyword)
{
  if (!str || !keyword)
    return '\0';

  // Simple substring search
  const char *p = str;
  const char *k;

  while (*p)
  {
    const char *s = p;
    k = keyword;
    while (*s && *k && *s == *k)
    {
      s++;
      k++;
    }
    if (*k == '\0')
    {
      // Found full keyword, return next char if available
      return *s ? *s : '\0';
    }
    p++;
  }
  return '\0'; // Not found
}

//int lorawan_is_connected(UART_HandleTypeDef *huart)
//{
//  HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
//  HAL_Delay(300); // Let the OK come back
//  uint8_t rxwakebuf[16] = {0};
//  HAL_UART_Receive(huart, rxwakebuf, 4, 300);
//  uint8_t rxbuf[256] = {0};
//  // Totally Flush buffer and stuff
//  HAL_UART_AbortReceive(huart);
//  __HAL_UART_FLUSH_DRREGISTER(huart);
//  __HAL_UART_CLEAR_IDLEFLAG(huart);
//  __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);
//
//  HAL_UART_Transmit(huart, (uint8_t *)"ATI 3001\r\n", 10, 300);
//  HAL_UART_Receive(huart, rxbuf, 7, 300);
//
//  if (rxbuf[1] == '0')
//  {
//    memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
//    return 0;
//  }
//  else
//  {
//    memset(rxbuf, 0, sizeof(rxbuf)); // Clear buffer
//    return 1;
//  }
//}

int join(UART_HandleTypeDef *huart)
{
  if (is_connected)
  {
    dbg_print_line("JOIN:skip");
    return 1;
  }
  dbg_print_line("JOIN:start");
  HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
  HAL_Delay(300); // let OK come back!
  uint8_t rxbuf[256] = {0};
  HAL_UART_Transmit(&huart2, (uint8_t *)"AT+JOIN\r\n", 9, 300);
  (void)HAL_UART_Receive(&huart2, rxbuf, 4, 100);
  __HAL_UART_FLUSH_DRREGISTER(&huart2);
  __HAL_UART_CLEAR_IDLEFLAG(&huart2);

  uint16_t offset = 0;
  uint32_t start = HAL_GetTick();
  const uint32_t overall_timeout_ms = 35000U;
  while ((HAL_GetTick() - start) < overall_timeout_ms && offset < (sizeof(rxbuf) - 1))
  {
    uint16_t chunk = 0;
    uint16_t room = (uint16_t)(sizeof(rxbuf) - 1 - offset);
    HAL_StatusTypeDef st = HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + offset, room, &chunk, 1000);
    if (st == HAL_TIMEOUT)
    {
      continue; // allow longer overall timeout budget
    }
    if (st != HAL_OK)
    {
      break; // UART failure -> give up
    }
    if (chunk == 0)
    {
      if ((HAL_GetTick() - start) > 1000U)
      {
        break; // idle long enough without more data
      }
      continue;
    }
    offset += chunk;
  }

  __HAL_UART_FLUSH_DRREGISTER(&huart2);
  __HAL_UART_CLEAR_IDLEFLAG(&huart2);

  if (offset >= sizeof(rxbuf))
  {
    offset = sizeof(rxbuf) - 1;
  }
  rxbuf[offset] = '\0';

  char result = find_char_after((const char *)rxbuf, "JOIN: [");
  char error14 = find_char_after((const char *)rxbuf, "\nERROR 1");
  if (result == 'O' || error14 == '4')
  {
    is_connected = 1;
    return 1;
  }

  if (result == 'F')
  {
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+DROP\r\n", 9, 300);
    HAL_Delay(200);
    is_connected = 0;
    return 0;
  }
  return 0;
}

int lorawan_set_battery_level(UART_HandleTypeDef *huart, uint8_t battery_level)
{
  char cmd[32]; // enough space for command
  int len = snprintf(cmd, sizeof(cmd), "AT+BAT %u\r\n", battery_level);

  if (len <= 0 || len >= sizeof(cmd))
  {
    return -1; // encoding error or buffer too small
  }

  // Flush / clear UART
  HAL_UART_AbortReceive(huart);
  __HAL_UART_FLUSH_DRREGISTER(huart);
  __HAL_UART_CLEAR_IDLEFLAG(huart);
  __HAL_UART_CLEAR_FLAG(huart,
                        UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

  // Transmit command
  if (HAL_UART_Transmit(huart, (uint8_t *)cmd, (uint16_t)len, 300) != HAL_OK)
  {
    return -2; // TX error
  }

  HAL_Delay(300);
  return 0; // success
}

static void LoRaWAN_set_fport(int fPort)
{
  char cmd[20]; // plenty big for "ATS 629=255\r\n"
  int n = snprintf(cmd, sizeof(cmd), "ATS 629=%d\r\n", fPort);
  if (n > 0 && n < (int)sizeof(cmd))
  {
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd, (uint16_t)n, 300);
  }
}

void LoRaWAN_SendHex(const uint8_t *payload, size_t length, int fPort)
{
  static const char HEX[16] = "0123456789ABCDEF";
  static const char prefix[] = "AT+SEND \"";
  static const char suffix[] = "\"\r\n";

  if (!payload || length == 0) return;

  static uint8_t txbuf[512];
  const size_t need = (sizeof(prefix)-1) + (length*2u) + (sizeof(suffix)-1);
  if (need > sizeof(txbuf)) return;

  size_t idx = 0;
  for (size_t i = 0; i < sizeof(prefix)-1; ++i) txbuf[idx++] = (uint8_t)prefix[i];
  for (size_t i = 0; i < length; ++i) { uint8_t b = payload[i]; txbuf[idx++] = (uint8_t)HEX[b>>4]; txbuf[idx++] = (uint8_t)HEX[b&0x0F]; }
  for (size_t i = 0; i < sizeof(suffix)-1; ++i) txbuf[idx++] = (uint8_t)suffix[i];

  // Wake
  (void)HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
  HAL_Delay(200);

  // Set FPort (optional: verify OK here if you want)
  LoRaWAN_set_fport(fPort);
  HAL_Delay(150);

  // Clean RX state
  HAL_UART_AbortReceive(&huart2);
  __HAL_UART_FLUSH_DRREGISTER(&huart2);
  __HAL_UART_CLEAR_IDLEFLAG(&huart2);
  __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_PEF | UART_CLEAR_NEF);

  // Send payload
  if (HAL_UART_Transmit(&huart2, txbuf, (uint16_t)idx, 1000) != HAL_OK) return;

  // ----- FIXED RECEIVE LOOP -----
  uint8_t rxbuf[256] = {0};
  size_t  total = 0;         // accumulator (write offset)
  uint16_t last = 0;         // bytes read in the last call
  uint32_t start = HAL_GetTick();
  const uint32_t overall_to_ms = 35000;   // your 35s budget

  // Try to catch immediate "OK\r\n"
  (void)HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf, sizeof(rxbuf), &last, 400);
  total += last;

  while ((HAL_GetTick() - start) < overall_to_ms) {
    // Stop if we filled the buffer
    if (total >= sizeof(rxbuf) - 1) break;

    last = 0;
    uint16_t cap = (uint16_t)(sizeof(rxbuf) - 1 - total);
    if (HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total, cap, &last, 1000) != HAL_OK) {
      HAL_Delay(20);
      continue;
    }
    if (last == 0) {
      // idle with no new data -> done
      break;
    }
    total += last;

    // Early exits if we already see decisive tokens
    rxbuf[total] = 0; // keep NUL-terminated for strstr
    if (str_exists((char*)rxbuf, "ERROR")) break;
    if (str_exists((char*)rxbuf, "TX:"))   break;
  }
  rxbuf[total < sizeof(rxbuf) ? total : sizeof(rxbuf)-1] = 0;
  // ----- END FIXED RECEIVE LOOP -----

  // If we only saw ADRX so far, give it a short second chance to get TX:
  if (!str_exists((char*)rxbuf, "TX:") && str_exists((char*)rxbuf, "ADRX:")) {
    uint16_t extra = 0;
    if (total < sizeof(rxbuf)-1 &&
        HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total, (uint16_t)(sizeof(rxbuf)-1-total), &extra, 1200) == HAL_OK &&
        extra > 0) {
      total += extra;
      rxbuf[total] = 0;
    }
  }

  // Ensure we saw at least one of the expected markers in the raw buffer
  bool has_tx = span_exists(rxbuf, total, "TX:");
  bool has_adrx = span_exists(rxbuf, total, "ADRX:");
  bool has_error = span_exists(rxbuf, total, "ERROR:");

  if ((!has_tx && !has_adrx) || has_error) {
    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
    HAL_Delay(200);
    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"ATZ\r\n", 5, 300); // just start over....
    is_connected = 0;
    return;
  }

  //
  return;

//  // this sucks if this hits.
//  if (str_exists((char*)rxbuf, "ERROR 14")) {
//    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
//    HAL_Delay(200);
//    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"ATZ\r\n", 5, 300); // just start over....
//    is_connected = 0;
//    return;
//  }
//
//  //
//  if (str_exists((char*)rxbuf, "TX:") || str_exists((char*)rxbuf, "ADRX:")) {
//    // success path (matches your expected frames)
//    return;
//  }

  // Neither ERROR nor TX: harmless log (you can decide to treat ADRX-only as success)
  // dbg_print_line("SEND:no_TX_no_ERROR");
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

  // --- Read reset reason ---
  volatile uint16_t reset_reason = Read_Reset_Reason();
  //TODO: Use this info for any (legal) purpose.

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_USART1_UART_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_ADC_Init();
  /* USER CODE BEGIN 2 */

  HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
  HAL_Delay(300);
  HAL_UART_Transmit(&huart2, (uint8_t *)"AT+DROP\r\n", 9, 300);
  HAL_Delay(300);
  int need_provision = uart2_probe_and_align();
  if (need_provision == 1)
  {
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300); // One initial AT to clear any odd commands sent before
    HAL_Delay(400);
    // Set LoRaWAN Settings
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 602=1\r\n", 11, 300); // Activation Mode OTAA (0 = ABP, 1 = OTAA)
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 603=0\r\n", 11, 300); // Set CLASS to A
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 604=1\r\n", 11, 300); // Confirmed 0 = NO, 1 = yes
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 605=3\r\n", 11, 300); // Retry if Confirm Fails, 3 Retries set (and is default)
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 611=9\r\n", 11, 300); // Set Region to AS923-1 (JAPAN)
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 302=9600\r\n", 14, 300);
    HAL_Delay(400);

    // Dynamically concatenate DEV_EUI and JOIN_EUI to form APP_KEY
    char app_key[33]; // 16 (DEV_EUI) + 16 (JOIN_EUI) + 1 (null terminator)
    sprintf(app_key, "%s%s", DEV_EUI, JOIN_EUI);

    // Build and send APP KEY command
    char cmd_app[128]; // Buffer for full command
    sprintf(cmd_app, "AT%%S 500=\"%s\"\r\n", app_key);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd_app, strlen(cmd_app), 300);
    HAL_Delay(400);

    // Dynamically build and send DEV EUI command
    char cmd_dev[64];
    sprintf(cmd_dev, "AT%%S 501=\"%s\"\r\n", DEV_EUI);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd_dev, strlen(cmd_dev), 300);
    HAL_Delay(400);

    // Dynamically build and send JOIN EUI command
    char cmd_join[64];
    sprintf(cmd_join, "AT%%S 502=\"%s\"\r\n", JOIN_EUI);
    HAL_UART_Transmit(&huart2, (uint8_t *)cmd_join, strlen(cmd_join), 300);
    HAL_Delay(400);

    HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 213=2000\r\n", 14, 300); // Set Sleep Mode to 2 seconds
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT&W\r\n", 6, 300); // SAVE ALL!
    HAL_Delay(400);
    HAL_UART_Transmit(&huart2, (uint8_t *)"ATZ\r\n", 5, 300); // Soft reboot!
    HAL_Delay(400);
    UART2_SetBaud(9600);
  }

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    uint16_t ticks;
    __disable_irq();
    ticks = wakeup_counter;
    wakeup_counter = 0;
    __enable_irq();

    RTC_ServiceClockHealth(ticks);

    wakes_accum += ticks;

    // dbg_print_u32("Loop:wakes_accum", wakes_accum);
    // dbg_print_u32("Loop:wakeups_per_cycle", wakeups_per_cycle);

    bool do_transmit = first_run || (wakes_accum >= wakeups_per_cycle);

    // Verify UART is ready after wake-up
    if (!verify_uart_ready(&huart1) || !verify_uart_ready(&huart2))
    {
      dbg_print_line("UART:reinit_failed");
      // Additional recovery could be added here if needed
    }

    if (!do_transmit)
    {
      dbg_print_line("Loop:no_tx");
    }
    if (do_transmit)
    {
      wakeup_counter = 0; // reset for next cycle
      wakes_accum = 0;
      first_run = false;
      // dbg_print_u32("Loop:wakeups_per_cycle", wakeups_per_cycle);
      if (is_connected == 0)
      {
        join(&huart2);
      }

      // Get I2C Data
      HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
      HAL_Delay(1000); // sensor power-up and stabilization
      scan_i2c_bus();
      int i2c_success = sensor_init_and_read();
      HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);

      // Format data and send
      uint8_t payload[6] = {0};
      if (i2c_success == 0)
      {
        HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin | I2C_ENABLE_Pin, GPIO_PIN_SET);
        HAL_Delay(300);
        int aproxBatteryTemp_c = ((calculated_temp_1 - 55) / 10);
        uint8_t battery = vbat_measure_and_encode(&hadc, ADC_CHANNEL_0, aproxBatteryTemp_c, /*external_power_present=*/false);
        HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin | I2C_ENABLE_Pin, GPIO_PIN_RESET);
        lorawan_set_battery_level(&huart2, battery);

        if (has_soil_sensor)
        {
          uint8_t soil_payload[8] = {0};
          soil_payload[0] = (uint8_t)(soil_e25 >> 8);
          soil_payload[1] = (uint8_t)(soil_e25 & 0xFF);
          soil_payload[2] = (uint8_t)(soil_EC >> 8);
          soil_payload[3] = (uint8_t)(soil_EC & 0xFF);
          soil_payload[4] = (uint8_t)(soil_temp >> 8);
          soil_payload[5] = (uint8_t)(soil_temp & 0xFF);
          soil_payload[6] = (uint8_t)(soil_VWC >> 8);
          soil_payload[7] = (uint8_t)(soil_VWC & 0xFF);

          LoRaWAN_SendHex(soil_payload, sizeof(soil_payload), 2);
        }
        else
        {
          payload[0] = (uint8_t)(calculated_temp_1 >> 8);
          payload[1] = (uint8_t)(calculated_temp_1 & 0xFF);
          payload[2] = (uint8_t)(calculated_hum_1 >> 8);
          payload[3] = (uint8_t)(calculated_hum_1 & 0xFF);
          LoRaWAN_SendHex(payload, 4, 1);
          // dbg_print_line("TX:done");
        }
      }
      else
      {
        // We FAILED to get a good reading for whatever reason
        // We need to specify why soon...

        // 1,2,3 all are sensor failures and will not contain data
        if (i2c_success == 1 || i2c_success == 2 || i2c_success == 3)
        {
          uint8_t code = (uint8_t)i2c_success;
          LoRaWAN_SendHex(&code, 1, 10);
        }
        // if i2c_success is 4, then the sensors returned data, but do not agree on the correct temp
        if (i2c_success == 4)
        {
          // add the dis-agreed sensor info to payload

          payload[0] = (uint8_t)(calculated_temp_1 >> 8);
          payload[1] = (uint8_t)(calculated_temp_1 & 0xFF);
          payload[2] = calculated_hum_1;

          payload[3] = (uint8_t)(calculated_temp_2 >> 8);
          payload[4] = (uint8_t)(calculated_temp_2 & 0xFF);
          payload[5] = calculated_hum_2;
          LoRaWAN_SendHex(payload, 6, 11); // send both dis-agreed values and an error
        }
      }
    }
    // Always go back to deep sleep to allow next RTC wake
    EnterDeepSleepMode();

//    HAL_Delay(60000);
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
  bool need_lsi = rtc_using_lsi || lse_fault_pending;
  bool can_drive_lse = !rtc_using_lsi && !lse_fault_pending;

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_MEDIUMLOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  uint32_t osc_mask = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_MSI;
  if (need_lsi)
  {
    osc_mask |= RCC_OSCILLATORTYPE_LSI;
  }
  if (can_drive_lse)
  {
    osc_mask |= RCC_OSCILLATORTYPE_LSE;
  }
  RCC_OscInitStruct.OscillatorType = osc_mask;
  RCC_OscInitStruct.LSEState = can_drive_lse ? RCC_LSE_ON : RCC_LSE_OFF;
  RCC_OscInitStruct.LSIState = need_lsi ? RCC_LSI_ON : RCC_LSI_OFF;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
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
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_PCLK1;
  PeriphClkInit.RTCClockSelection = rtc_using_lsi ? RCC_RTCCLKSOURCE_LSI : RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables or disables the Clock Security System based on the active RTC source
  */
  if (!rtc_using_lsi && !lse_fault_pending)
  {
    HAL_RCCEx_EnableLSECSS();
  }
  else
  {
    HAL_RCCEx_DisableLSECSS();
  }
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

  if (!RTC_ReInitPreserveConfig())
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
  huart1.Init.BaudRate = 9600;
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

static uint32_t RTC_GetRTCCLK_Hz(void)
{
  uint32_t src = __HAL_RCC_GET_RTC_SOURCE();
  switch (src)
  {
  case RCC_RTCCLKSOURCE_LSI:
#ifdef LSI_VALUE
    return LSI_VALUE;
#else
    return 37000U;
#endif
  case RCC_RTCCLKSOURCE_LSE:
#ifdef LSE_VALUE
    return LSE_VALUE;
#else
    return 32768U;
#endif
#ifdef RCC_RTCCLKSOURCE_HSE_DIV32
  case RCC_RTCCLKSOURCE_HSE_DIV32:
#ifdef HSE_VALUE
    return (HSE_VALUE / 32U);
#else
    break;
#endif
#endif
  default:
#ifdef LSI_VALUE
    return LSI_VALUE;
#else
    return 37000U;
#endif
  }
  /* Should not reach here */
  return 37000U;
}

static uint32_t RTC_GetCkSpreHz(void)
{
  uint64_t rtc_clk = (uint64_t)RTC_GetRTCCLK_Hz();
  uint64_t async = (uint64_t)hrtc.Init.AsynchPrediv + 1ULL;
  uint64_t sync = (uint64_t)hrtc.Init.SynchPrediv + 1ULL;
  uint64_t denom = async * sync;
  if (denom == 0ULL)
  {
    return 1U;
  }
  uint64_t freq = (rtc_clk + (denom / 2ULL)) / denom;
  if (freq == 0ULL)
  {
    freq = 1ULL;
  }
  return (uint32_t)freq;
}

static void RTC_UpdatePrescalersForClock(void)
{
  const uint32_t desired_async = 127U; // keeps ck_spre near 1 Hz across LSE/LSI
  const uint64_t rtc_clk = (uint64_t)RTC_GetRTCCLK_Hz();
  const uint64_t async_plus_one = (uint64_t)desired_async + 1ULL;
  uint64_t sync_plus_one = rtc_clk / async_plus_one;
  if (sync_plus_one == 0ULL)
  {
    sync_plus_one = 1ULL;
  }
  uint64_t sync = sync_plus_one - 1ULL;
  if (sync > 0x7FFFULL)
  {
    sync = 0x7FFFULL;
  }
  hrtc.Init.AsynchPrediv = desired_async;
  hrtc.Init.SynchPrediv = (uint32_t)sync;
}

static void RTC_UpdateWakeDerivatives(uint32_t programmed_ticks, uint32_t ck_spre_hz)
{
  if (ck_spre_hz == 0U)
  {
    ck_spre_hz = 1U;
  }
  uint64_t interval_ms = ((uint64_t)programmed_ticks * 1000ULL + (uint64_t)ck_spre_hz / 2ULL) /
                         (uint64_t)ck_spre_hz;
  if (interval_ms == 0ULL)
  {
    interval_ms = 1ULL;
  }
  wake_interval_ms = (uint32_t)interval_ms;

  const uint64_t cycle_ms = (uint64_t)SLEEP_TIME_MINUTES * 60ULL * 1000ULL;
  uint64_t needed = (cycle_ms + interval_ms - 1ULL) / interval_ms;
  if (needed == 0ULL)
  {
    needed = 1ULL;
  }
  if (needed > 0xFFFFULL)
  {
    needed = 0xFFFFULL;
  }
  wakeups_per_cycle = (uint16_t)needed;
}

static uint32_t RTC_ComputeWakeupsForDelay(uint32_t delay_ms)
{
  uint32_t interval = wake_interval_ms;
  if (interval == 0U)
  {
    interval = 1U;
  }
  uint64_t needed = ((uint64_t)delay_ms + (uint64_t)interval - 1ULL) / (uint64_t)interval;
  if (needed == 0ULL)
  {
    needed = 1ULL;
  }
  if (needed > 0x7FFFFFFFULL)
  {
    needed = 0x7FFFFFFFULL;
  }
  return (uint32_t)needed;
}

void configWakeupTime()
{
  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);

  const uint32_t ck_spre_hz = RTC_GetCkSpreHz();
  uint64_t desired_ticks = (uint64_t)SLEEP_INTERVAL_SECONDS * (uint64_t)ck_spre_hz;
  if (desired_ticks == 0ULL)
  {
    desired_ticks = 1ULL;
  }

  if (desired_ticks > 0x1FFFFULL)
  {
    desired_ticks = 0x1FFFFULL;
  }

  uint32_t clock_sel = RTC_WAKEUPCLOCK_CK_SPRE_16BITS;
  uint32_t reload = (uint32_t)(desired_ticks - 1ULL);
  if (reload > 0xFFFFU)
  {
    clock_sel = RTC_WAKEUPCLOCK_CK_SPRE_17BITS;
  }

  if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, reload, clock_sel) != HAL_OK)
  {
    Error_Handler();
  }

  RTC_UpdateWakeDerivatives((uint32_t)desired_ticks, ck_spre_hz);
}

static bool RTC_ReInitPreserveConfig(void)
{
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  RTC_UpdatePrescalersForClock();
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    return false;
  }

  configWakeupTime();
  return true;
}

void RTC_RequestClockFallback(void)
{
  lse_fault_pending = true;
}

static bool RTC_SwitchClockToLSI(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  __HAL_RCC_RTC_DISABLE();
  __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    HAL_PWR_DisableBkUpAccess();
    return false;
  }

  __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSI);
  __HAL_RCC_RTC_ENABLE();

  HAL_PWR_DisableBkUpAccess();

  return RTC_ReInitPreserveConfig();
}

static bool RTC_TryRestoreLSE(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
  __HAL_RCC_RTC_DISABLE();

  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    HAL_PWR_DisableBkUpAccess();
    return false;
  }

  __HAL_RCC_RTC_CONFIG(RCC_RTCCLKSOURCE_LSE);
  __HAL_RCC_RTC_ENABLE();

  HAL_PWR_DisableBkUpAccess();

  return RTC_ReInitPreserveConfig();
}

static void RTC_ServiceClockHealth(uint16_t wakeups_since_last)
{
  if (lse_fault_pending)
  {
    lse_fault_pending = false;
    if (!rtc_using_lsi)
    {
      if (!RTC_SwitchClockToLSI())
      {
        Error_Handler();
      }
      rtc_using_lsi = true;
    }
    lse_retry_wakeup_budget = RTC_ComputeWakeupsForDelay(LSE_RETRY_DELAY_MS);
    if (lse_retry_wakeup_budget == 0U)
    {
      lse_retry_wakeup_budget = 1U;
    }
  }

  if (rtc_using_lsi)
  {
    if (lse_retry_wakeup_budget > 0U)
    {
      if (wakeups_since_last >= lse_retry_wakeup_budget)
      {
        lse_retry_wakeup_budget = 0U;
      }
      else
      {
        lse_retry_wakeup_budget -= wakeups_since_last;
      }
    }

    if (lse_retry_wakeup_budget == 0U)
    {
      if (RTC_TryRestoreLSE())
      {
        rtc_using_lsi = false;
        HAL_RCCEx_EnableLSECSS();
        lse_retry_wakeup_budget = 0U;
      }
      else
      {
        lse_retry_wakeup_budget = RTC_ComputeWakeupsForDelay(LSE_RETRY_DELAY_MS);
        if (lse_retry_wakeup_budget == 0U)
        {
          lse_retry_wakeup_budget = 1U;
        }
      }
    }
  }
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
  /* Properly deinitialize UARTs before sleep */
  HAL_UART_DeInit(&huart1);
  //  HAL_UART_DeInit(&huart2);
  HAL_I2C_DeInit(&hi2c1);

  /* Configure all GPIOs for ultra-low power */
  ConfigureGPIOForLowPower();

  /* Disable unnecessary peripheral clocks */
  __HAL_RCC_I2C1_CLK_DISABLE();
  __HAL_RCC_USART1_CLK_DISABLE();
  __HAL_RCC_USART2_CLK_DISABLE();
  __HAL_RCC_GPIOB_CLK_DISABLE();
  //  __HAL_RCC_GPIOC_CLK_DISABLE(); // DO NOT DISABLE GPIO C, That is what the Crystal is connected to!!!
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_I2C1_CLK_ENABLE();
  __HAL_RCC_USART1_CLK_ENABLE();
  __HAL_RCC_USART2_CLK_ENABLE();

  /* Restore GPIO configuration for normal operation */
  MX_GPIO_Init();

  /* Re-initialize peripherals with proper sequence */
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  //  MX_USART2_UART_Init();

  /* Resume SysTick */
  HAL_ResumeTick();

  /* Add longer delay for UART stabilization */
  HAL_Delay(100);
}

void HAL_RCCEx_LSECSS_Callback(void)
{
  /*
   * A wakeup is generated in Standby mode. In any other modes, an interrupt can be sent to
   * wake-up the software (see Section 7.3.5 of the reference manual).
   * The software MUST then reset the CSSLSEON bit and stop the defective 32 kHz oscillator
   * by resetting LSEON bit. It can change the RTC clock source (LSI, HSE or no clock) through
   * the RTCSEL bit, or take any required action to secure the application.
   * The frequency of LSE oscillator must be higher than 30 kHz to avoid false positive CSS
   * detection.
   */
   /** Initialise the RCC Oscillators according to the specified parameters
    * in the RCC_OscInitTypeDef structure.
    */
    RCC_OscInitTypeDef RCC_OscInitStruct = {
        .OscillatorType = RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSI | RCC_OSCILLATORTYPE_MSI,
        .MSIState = RCC_MSI_ON,
        .LSEState = RCC_LSE_OFF,
        .LSIState = RCC_LSE_ON,
        .HSIState = RCC_HSI_ON,
        .HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT,
        .MSICalibrationValue = 0,
        .MSIClockRange = RCC_MSIRANGE_5,
        .PLL = {
            .PLLState = RCC_PLL_NONE
        }
    };
#warning: "The following might use blocking functions, e.g. when waiting for hardware to sync. Consider doing this in application-code context, rather than ISR."
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
}

// Read reset reason flags and encode into an 8-bit mask
uint8_t Read_Reset_Reason(void)
{
    volatile uint32_t csr = RCC->CSR;
    uint8_t mask = 0u;

    if (csr & RCC_CSR_PINRSTF)   mask |= (1u << 0);
    if (csr & RCC_CSR_PORRSTF)   mask |= (1u << 1);
    if (csr & RCC_CSR_SFTRSTF)   mask |= (1u << 2);
    if (csr & RCC_CSR_IWDGRSTF)  mask |= (1u << 3);
    if (csr & RCC_CSR_WWDGRSTF)  mask |= (1u << 4);
    if (csr & RCC_CSR_LPWRRSTF)  mask |= (1u << 5);

    // Clear flags after reading
    RCC->CSR |= RCC_CSR_RMVF;

    return mask;
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
