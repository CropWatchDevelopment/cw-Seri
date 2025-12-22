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
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "battery/vbat_lorawan.h"
#include "sensirion/sensirion.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SLEEP_TIME_MINUTES \
    10 // Sleep time in minutes between LoRaWAN transmissions
// Base sleep interval length (seconds) for each STOP cycle (RTC wake-up)
#define SLEEP_INTERVAL_SECONDS 30

#define DEV_EUI "0025CA00000056E3"
#define JOIN_EUI "0025CA00000055F7"
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

int is_connected = 0;
static uint8_t reset_reason = 0xFF; // Store reset reason

static volatile uint16_t wakeup_counter = 0; // incremented in ISR
static uint16_t wakes_accum = 0;             // main-loop accumulator
static uint32_t transmission_count = 0;      // Total transmissions sent
static bool first_run =
    true; // Flag to ensure first transmission happens immediately
static uint16_t readCount = 0;
// Number of wakeups per transmission cycle (ceil division to avoid truncation)
static const uint16_t WAKEUPS_PER_CYCLE =
    (uint16_t)((SLEEP_TIME_MINUTES * 60u + (SLEEP_INTERVAL_SECONDS - 1u)) /
               SLEEP_INTERVAL_SECONDS);

typedef enum
{
    RTC_CLOCK_LSE = 0,
    RTC_CLOCK_LSI = 1
} rtc_clock_source_t;

static volatile rtc_clock_source_t rtc_clock_source = RTC_CLOCK_LSE;
static uint32_t last_lse_retry_tick = 0;

// LoRaWAN UART Baud
//  Start out at 115200 as it is the 1st time starting baud of the Ezurio LoRa
//  module then switch forever to 9600 after we switch the baud of the ezurio
//  module.
uint32_t baudRate = 115200;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_RTC_Init(void);
static void MX_I2C1_Init(void);
static void MX_ADC_Init(void);
/* USER CODE BEGIN PFP */
void EnterDeepSleepMode(void);
void LoRaWAN_SendHex(const uint8_t *payload, size_t length, int fPort);
void configWakeupTime(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Helper to get and clear reset flags
static uint8_t GetResetSource(void)
{
    uint8_t reason = 0xFF;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LPWRRST))
    {
        reason = 0x06; // Low Power Reset
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_WWDGRST))
    {
        reason = 0x05; // Window Watchdog
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST))
    {
        reason = 0x04; // Independent Watchdog
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST))
    {
        reason = 0x03; // Software Reset
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST))
    {
        reason = 0x02; // POR/PDR
    }
    else if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST))
    {
        reason = 0x01; // PIN Reset
    }
    else
    {
        reason = 0x00; // Unknown/None
    }

    // Clear flags so next reset can be detected cleanly
    __HAL_RCC_CLEAR_RESET_FLAGS();

    return reason;
}

static HAL_StatusTypeDef UART2_SetBaud(uint32_t br)
{
    // Drain TX and stop RX before touching the peripheral
    uint32_t t0 = HAL_GetTick();
    while (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TC) == RESET)
    {
        if ((HAL_GetTick() - t0) > 50u)
        {
            break; // avoid hanging if clock/peripheral is not ready
        }
    }
    HAL_UART_AbortReceive(&huart2);
    __HAL_UART_FLUSH_DRREGISTER(&huart2);
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_FEF |
                                       UART_CLEAR_PEF | UART_CLEAR_NEF);

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
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF |
                                     UART_CLEAR_PEF | UART_CLEAR_NEF);
}

// Helper to send device info (serials + reset reason)
static void send_device_info_packet(void)
{
    // Power up sensors to read serials
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);
    scan_i2c_bus();

    // Ensure serials are fresh (though they should be stable)
    read_sensor_serials();

    // Power down sensors
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);

    uint8_t serial_payload[9] = {0};
    serial_payload[0] = (uint8_t)(serial_1 >> 24);
    serial_payload[1] = (uint8_t)(serial_1 >> 16);
    serial_payload[2] = (uint8_t)(serial_1 >> 8);
    serial_payload[3] = (uint8_t)(serial_1 & 0xFF);

    serial_payload[4] = (uint8_t)(serial_2 >> 24);
    serial_payload[5] = (uint8_t)(serial_2 >> 16);
    serial_payload[6] = (uint8_t)(serial_2 >> 8);
    serial_payload[7] = (uint8_t)(serial_2 & 0xFF);

    serial_payload[8] = reset_reason;

    // Only send if we have at least one sensor or a reset reason (always true for
    // reset reason) But user said "send sensor1 and sensor2... if they exist". If
    // both are 0, we still send reset reason? The user said "send the 'sensor id
    // and last reset reason' JUST after the JOIN request is successful." I'll
    // send it regardless of sensor presence, as reset reason is valuable. If
    // sensors are 0, they are 0.

    LoRaWAN_SendHex(serial_payload, 9, 9);
    HAL_Delay(2000); // Give it a moment
}

/* 1) Simple: for NUL-terminated strings */
static inline bool str_exists(const char *s, const char *token)
{
    if (!s || !token || *token == '\0')
        return false;
    return strstr(s, token) != NULL;
}

/* 2) Robust: for binary buffers that may not be NUL-terminated */
static bool span_exists(const void *buf, size_t len, const char *token)
{
    if (!buf || !token)
        return false;
    const size_t tlen = strlen(token);
    if (tlen == 0 || len < tlen)
        return false;

    const uint8_t *p = (const uint8_t *)buf;
    for (size_t off = 0; off + tlen <= len; ++off)
    {
        if (memcmp(p + off, token, tlen) == 0)
            return true;
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
            if (HAL_UARTEx_ReceiveToIdle(&huart2, buf, sizeof buf, &got, 300) ==
                    HAL_OK &&
                got > 0)
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
                if (HAL_UARTEx_ReceiveToIdle(&huart2, buf, sizeof buf, &got, 300) ==
                        HAL_OK &&
                    got > 0)
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

static uint32_t rtc_compute_lsi_synch_prediv(void)
{
    uint32_t lsi_hz = LSI_VALUE;
    if (lsi_hz == 0u)
    {
        lsi_hz = 37000u; // safety fallback
    }
    const uint32_t async_div = 128u; // (AsynchPrediv + 1)
    uint32_t sync = (lsi_hz / async_div);
    if (sync == 0u)
        sync = 1u;
    if (sync > 0x7FFFu)
        sync = 0x7FFFu;
    return sync - 1u;
}

static void rtc_switch_to_lsi_failover(void)
{
    if (rtc_clock_source == RTC_CLOCK_LSI)
    {
        return;
    }

    HAL_PWR_EnableBkUpAccess();

    /* Stop LSE/CSS to clear the failure condition */
    HAL_RCCEx_DisableLSECSS();
    __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);

    /* Enable LSI */
    __HAL_RCC_LSI_ENABLE();
    uint32_t t0 = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSIRDY) == RESET &&
           (HAL_GetTick() - t0) < 200u)
    {
        /* wait briefly */
    }

    /* Switch RTC clock to LSI */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* Re-init RTC with LSI dividers */
    HAL_RTC_DeInit(&hrtc);
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = rtc_compute_lsi_synch_prediv();
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    /* Re-arm wakeup timer with new clock source */
    configWakeupTime();

    last_lse_retry_tick = HAL_GetTick();
    rtc_clock_source = RTC_CLOCK_LSI;
}

static bool rtc_try_restore_lse(void)
{
    if (rtc_clock_source == RTC_CLOCK_LSE)
    {
        return true;
    }

    uint32_t now = HAL_GetTick();
    if ((now - last_lse_retry_tick) < 60000u)
    {
        return false; // throttle retry to once per minute
    }
    last_lse_retry_tick = now;

    HAL_PWR_EnableBkUpAccess();

    /* Try to restart LSE */
    __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);
    uint32_t t0 = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET &&
           (HAL_GetTick() - t0) < 2000u)
    {
        /* wait */
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET)
    {
        __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);
        return false;
    }

    /* Switch RTC clock back to LSE */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit);

    /* Re-enable CSS on LSE now that it is stable */
    HAL_RCCEx_EnableLSECSS();

    /* Re-init RTC with LSE dividers */
    HAL_RTC_DeInit(&hrtc);
    hrtc.Init.AsynchPrediv = 127;
    hrtc.Init.SynchPrediv = 255;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        Error_Handler();
    }

    configWakeupTime();

    rtc_clock_source = RTC_CLOCK_LSE;
    return true;
}

// Query connection status using ATI 3001 (per Ezurio docs)
static int lorawan_get_connection_status(UART_HandleTypeDef *huart)
{
    if (huart == NULL)
        return -1;

    const uint8_t cmd[] = "ATI 3001\r\n";
    uint8_t rxbuf[64] = {0};
    uint16_t len = 0;

    // Nudge/wake the UART and clear any stale data
    uart2_rx_flush(huart);
    (void)HAL_UART_Transmit(huart, (uint8_t *)"AT\r", 3, 300);
    HAL_Delay(50);
    // Drain any wake/OK response from the nudge
    (void)HAL_UARTEx_ReceiveToIdle(huart, rxbuf, sizeof(rxbuf), &len, 200);

    uart2_rx_flush(huart);

    if (HAL_UART_Transmit(huart, (uint8_t *)cmd, sizeof(cmd) - 1u, 300) !=
        HAL_OK)
    {
        return -1;
    }

    // Try to read the response, allow a quick retry if first read is empty
    if (HAL_UARTEx_ReceiveToIdle(huart, rxbuf, sizeof(rxbuf), &len, 800) !=
            HAL_OK ||
        len == 0)
    {
        uint16_t len2 = 0;
        if (HAL_UARTEx_ReceiveToIdle(huart, rxbuf, sizeof(rxbuf), &len2, 1000) !=
                HAL_OK ||
            len2 == 0)
        {
            return -1;
        }
        len = len2;
    }

    // Expected frames: "\n0\r\nOK\r" or "\n1\r\nOK\r"
    if (span_exists(rxbuf, len, "\n1\r\nOK\r") ||
        span_exists(rxbuf, len, "\r1\r\nOK\r"))
    {
        return 1; // Connected
    }
    if (span_exists(rxbuf, len, "\n0\r\nOK\r") ||
        span_exists(rxbuf, len, "\r0\r\nOK\r"))
    {
        return 0; // Not connected
    }

    return -1; // Unknown / parse fail
}

int lorawan_check_joined(UART_HandleTypeDef *huart)
{
    // Prefer the explicit connection status query
    int status = lorawan_get_connection_status(huart);
    if (status >= 0)
    {
        return status;
    }

    // Flush
    uart2_rx_flush(huart);

    // Send network join status command
    HAL_UART_Transmit(huart, (uint8_t *)"ATI 3001\r\n", 10, 300);

    uint8_t rxbuf[64] = {0};
    uint16_t len = 0;

    // Expecting "\r\n<status>\r\nOK\r\n" or similar.
    // Status: 0=Not Joined, 1=Joined.
    if (HAL_UARTEx_ReceiveToIdle(huart, rxbuf, sizeof(rxbuf), &len, 1000) ==
            HAL_OK &&
        len > 0)
    {
        // Look for "\n1\r" or "\r1\r"
        if (span_exists(rxbuf, len, "\n1\r") || span_exists(rxbuf, len, "\r1\r"))
        {
            return 1;
        }
        if (span_exists(rxbuf, len, "\n0\r") || span_exists(rxbuf, len, "\r0\r"))
        {
            return 0;
        }
    }
    // Unknown / parse fail
    return -1;
}

int join(UART_HandleTypeDef *huart)
{
    // Refresh connection state from the module before deciding to join
    int status = lorawan_get_connection_status(huart);
    if (status == 1)
    {
        is_connected = 1;
        return 1;
    }
    else if (status == 0)
    {
        is_connected = 0;
    }

    if (is_connected) // fallback to cached value if status was unknown
    {
        return 1;
    }

    // Ensure UART is alive
    uart2_rx_flush(&huart2);
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
    uint8_t at_buf[16] = {0};
    uint16_t at_len = 0;
    HAL_UARTEx_ReceiveToIdle(&huart2, at_buf, sizeof(at_buf), &at_len, 500);

    if (!str_exists((char *)at_buf, "OK"))
    {
        if (uart2_probe_and_align() < 0)
        {
            return 0;
        }
    }

    uint16_t total_rcv = 0;
    uint8_t rxbuf[256] = {0};

    uart2_rx_flush(&huart2);
    HAL_UART_Transmit(&huart2, (uint8_t *)"AT+JOIN\r\n", 9, 300);

    // Wait for response (up to ~35s)
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < 35000)
    {
        uint16_t chunk = 0;
        if (HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total_rcv,
                                     sizeof(rxbuf) - total_rcv - 1, &chunk,
                                     1000) == HAL_OK)
        {
            if (chunk > 0)
            {
                total_rcv += chunk;
                rxbuf[total_rcv] = 0; // Null terminate
                if (str_exists((char *)rxbuf, "JOIN: [") ||
                    str_exists((char *)rxbuf, "ERROR"))
                {
                    break;
                }
            }
        }
    }

    char result = find_char_after((const char *)rxbuf, "JOIN: [");
    char error14 = find_char_after((const char *)rxbuf, "\nERROR 1");
    if (result == 'O' || error14 == '4')
    {
        is_connected = 1;
        send_device_info_packet();
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

int lorawan_set_battery_level(UART_HandleTypeDef *huart,
                              uint8_t battery_level)
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
    __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF |
                                     UART_CLEAR_PEF | UART_CLEAR_NEF);

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

    if (!payload || length == 0)
        return;

    static uint8_t txbuf[512];
    const size_t need =
        (sizeof(prefix) - 1) + (length * 2u) + (sizeof(suffix) - 1);
    if (need > sizeof(txbuf))
        return;

    size_t idx = 0;
    for (size_t i = 0; i < sizeof(prefix) - 1; ++i)
        txbuf[idx++] = (uint8_t)prefix[i];
    for (size_t i = 0; i < length; ++i)
    {
        uint8_t b = payload[i];
        txbuf[idx++] = (uint8_t)HEX[b >> 4];
        txbuf[idx++] = (uint8_t)HEX[b & 0x0F];
    }
    for (size_t i = 0; i < sizeof(suffix) - 1; ++i)
        txbuf[idx++] = (uint8_t)suffix[i];

    // Wake
    (void)HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
    HAL_Delay(200);

    // Set FPort (optional: verify OK here if you want)
    LoRaWAN_set_fport(fPort);
    HAL_Delay(150);

    // Clean RX state
    HAL_UART_AbortReceive(&huart2);
    __HAL_UART_FLUSH_DRREGISTER(&huart2);
    __HAL_UART_CLEAR_IDLEFLAG(&huart2);
    __HAL_UART_CLEAR_FLAG(&huart2, UART_CLEAR_OREF | UART_CLEAR_FEF |
                                       UART_CLEAR_PEF | UART_CLEAR_NEF);

    // Send payload
    if (HAL_UART_Transmit(&huart2, txbuf, (uint16_t)idx, 1000) != HAL_OK)
        return;

    // ----- FIXED RECEIVE LOOP -----
    uint8_t rxbuf[256] = {0};
    size_t total = 0;  // accumulator (write offset)
    uint16_t last = 0; // bytes read in the last call
    uint32_t start = HAL_GetTick();
    const uint32_t overall_to_ms = 35000; // your 35s budget

    // Try to catch immediate "OK\r\n"
    (void)HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf, sizeof(rxbuf), &last, 400);
    total += last;

    while ((HAL_GetTick() - start) < overall_to_ms)
    {
        // Stop if we filled the buffer
        if (total >= sizeof(rxbuf) - 1)
            break;

        last = 0;
        uint16_t cap = (uint16_t)(sizeof(rxbuf) - 1 - total);
        if (HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total, cap, &last, 1000) !=
            HAL_OK)
        {
            HAL_Delay(20);
            continue;
        }
        if (last == 0)
        {
            // idle with no new data -> done
            break;
        }
        total += last;

        // Early exits if we already see decisive tokens
        rxbuf[total] = 0; // keep NUL-terminated for strstr
        if (str_exists((char *)rxbuf, "ERROR"))
            break;
        if (str_exists((char *)rxbuf, "TX:"))
            break;
    }
    rxbuf[total < sizeof(rxbuf) ? total : sizeof(rxbuf) - 1] = 0;
    // ----- END FIXED RECEIVE LOOP -----

    // If we only saw ADRX so far, give it a short second chance to get TX:
    if (!str_exists((char *)rxbuf, "TX:") && str_exists((char *)rxbuf, "ADRX:"))
    {
        uint16_t extra = 0;
        if (total < sizeof(rxbuf) - 1 &&
            HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total,
                                     (uint16_t)(sizeof(rxbuf) - 1 - total), &extra,
                                     1200) == HAL_OK &&
            extra > 0)
        {
            total += extra;
            rxbuf[total] = 0;
        }
    }

    // Ensure we saw at least one of the expected markers in the raw buffer
    bool has_tx =
        span_exists(rxbuf, total, "TX "); // datasheet uses "TX [result]"
    bool has_adrx = span_exists(rxbuf, total, "ADRX:");
    bool has_error = span_exists(rxbuf, total, "ERROR");

    if (has_error || (!has_tx && !has_adrx))
    {
        // Double-check actual link state before forcing reconnect logic
        int link = lorawan_get_connection_status(&huart2);
        if (link == 1)
        {
            is_connected = 1;
            return;
        }
        else if (link == 0)
        {
            is_connected = 0;
            // Only reset if the module explicitly reports not connected
            (void)HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
            HAL_Delay(200);
            (void)HAL_UART_Transmit(&huart2, (uint8_t *)"ATZ\r\n", 5, 300);
        }
        // If link == -1 (parse fail), skip reset and let next cycle retry
        return;
    }

    //
    return;

    //  // this sucks if this hits.
    //  if (str_exists((char*)rxbuf, "ERROR 14")) {
    //    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"AT\r\n", 4, 300);
    //    HAL_Delay(200);
    //    (void)HAL_UART_Transmit(&huart2, (uint8_t*)"ATZ\r\n", 5, 300); // just
    //    start over.... is_connected = 0; return;
    //  }
    //
    //  //
    //  if (str_exists((char*)rxbuf, "TX:") || str_exists((char*)rxbuf, "ADRX:"))
    //  {
    //    // success path (matches your expected frames)
    //    return;
    //  }
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

    /* Reset of all peripherals, Initializes the Flash interface and the Systick.
     */
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
    MX_RTC_Init();
    MX_I2C1_Init();
    MX_ADC_Init();
    /* USER CODE BEGIN 2 */

    // Capture reset reason early
    reset_reason = GetResetSource();

    // Check if already joined
    int startup_join_state = lorawan_check_joined(&huart2);
    if (startup_join_state == 1)
    {
        is_connected = 1;
        send_device_info_packet();
    }
    else if (startup_join_state == 0)
    {
        is_connected = 0;
        HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
        HAL_Delay(300);
        int need_provision = uart2_probe_and_align();
        if (need_provision == 1)
        {
            HAL_UART_Transmit(
                &huart2, (uint8_t *)"AT\r\n", 4,
                300); // One initial AT to clear any odd commands sent before
            HAL_Delay(400);
            // Set LoRaWAN Settings
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 602=1\r\n", 11,
                              300); // Activation Mode OTAA (0 = ABP, 1 = OTAA)
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 603=0\r\n", 11,
                              300); // Set CLASS to A
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 604=1\r\n", 11,
                              300); // Confirmed 0 = NO, 1 = yes
            HAL_Delay(400);
            HAL_UART_Transmit(
                &huart2, (uint8_t *)"ATS 605=3\r\n", 11,
                300); // Retry if Confirm Fails, 3 Retries set (and is default)
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 611=9\r\n", 11,
                              300); // Set Region to AS923-1 (JAPAN)
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

            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 213=2000\r\n", 14,
                              300); // Set Sleep Mode to 2 seconds
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"AT&W\r\n", 6, 300); // SAVE ALL!
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATZ\r\n", 5, 300); // Soft reboot!
            HAL_Delay(400);
            UART2_SetBaud(9600);
        }
    }
    else
    {
        // Unknown status; do not drop or re-provision, just proceed and let main
        // loop handle retries
        is_connected = 0;
    }

    // Initial Serial Number Read & Send
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);
    scan_i2c_bus();
    read_sensor_serials();
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */
        /* If we are running on LSI due to an earlier LSE failure, periodically try
         * to restore LSE. */
        if (rtc_clock_source == RTC_CLOCK_LSI)
        {
            (void)rtc_try_restore_lse();
        }

        uint16_t ticks;
        __disable_irq();
        ticks = wakeup_counter;
        wakeup_counter = 0;
        __enable_irq();

        wakes_accum += ticks;

        // dbg_print_u32("Loop:wakes_accum", wakes_accum);
        // dbg_print_u32("Loop:WAKEUPS_PER_CYCLE", WAKEUPS_PER_CYCLE);

        bool do_transmit = first_run || (wakes_accum >= WAKEUPS_PER_CYCLE);

        if (do_transmit)
        {
            wakeup_counter = 0; // reset for next cycle
            wakes_accum = 0;
            transmission_count++;
            // first_run = false; // Moved to end of block

            // dbg_print_u32("Loop:WAKEUPS_PER_CYCLE", WAKEUPS_PER_CYCLE);
            // Refresh connection flag from the module each cycle to avoid stale state
            int link_state = lorawan_get_connection_status(&huart2);
            if (link_state == 1)
            {
                is_connected = 1;
            }
            else if (link_state == 0)
            {
                is_connected = 0;
            }

            if (is_connected == 0)
            {
                join(&huart2);
            }

            // Get I2C Data
            HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
            HAL_Delay(1000); // sensor power-up and stabilization
            scan_i2c_bus();
            int i2c_success = sensor_init_and_read();

            // Read Serials while power is on
            uint32_t old_s1 = serial_1;
            uint32_t old_s2 = serial_2;
            read_sensor_serials();

            if (first_run || serial_1 != old_s1 || serial_2 != old_s2 ||
                (transmission_count % 100 == 0))
            {
                transmission_count =
                    0; // Reset counter to prevent overflow and restart interval
                if (serial_1 != 0 || serial_2 != 0)
                {
                    uint8_t serial_payload[8] = {0};
                    serial_payload[0] = (uint8_t)(serial_1 >> 24);
                    serial_payload[1] = (uint8_t)(serial_1 >> 16);
                    serial_payload[2] = (uint8_t)(serial_1 >> 8);
                    serial_payload[3] = (uint8_t)(serial_1 & 0xFF);

                    serial_payload[4] = (uint8_t)(serial_2 >> 24);
                    serial_payload[5] = (uint8_t)(serial_2 >> 16);
                    serial_payload[6] = (uint8_t)(serial_2 >> 8);
                    serial_payload[7] = (uint8_t)(serial_2 & 0xFF);

                    LoRaWAN_SendHex(serial_payload, 8, 9);
                    HAL_Delay(5000);
                }
            }

            HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);

            // Format data and send
            uint8_t payload[6] = {0};
            if (i2c_success == 0)
            {

                if (readCount > 99)
                {
                    readCount = 0;
                    // Enable Battery Measurement Pin (GPIOB Pin 0)
                    HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin,
                                      GPIO_PIN_SET);
                    HAL_Delay(300);

                    int aproxBatteryTemp_c = ((calculated_temp_1 - 5500) / 100);
                    uint8_t battery =
                        vbat_measure_and_encode(&hadc, ADC_CHANNEL_0, aproxBatteryTemp_c,
                                                /*external_power_present=*/false);

                    // Disable Battery Measurement Pin
                    HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin,
                                      GPIO_PIN_RESET);

                    lorawan_set_battery_level(&huart2, battery);
                }

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
                // if i2c_success is 4, then the sensors returned data, but do not agree
                // on the correct temp
                if (i2c_success == 4)
                {
                    // add the dis-agreed sensor info to payload

                    payload[0] = (uint8_t)(calculated_temp_1 >> 8);
                    payload[1] = (uint8_t)(calculated_temp_1 & 0xFF);
                    payload[2] = calculated_hum_1;

                    payload[3] = (uint8_t)(calculated_temp_2 >> 8);
                    payload[4] = (uint8_t)(calculated_temp_2 & 0xFF);
                    payload[5] = calculated_hum_2;
                    LoRaWAN_SendHex(payload, 6,
                                    11); // send both dis-agreed values and an error
                }
            }

            first_run = false;
        }
        // Always go back to deep sleep to allow next RTC wake
        EnterDeepSleepMode();
        //    HAL_Delay(5000);

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

    /** Configure the main internal regulator output voltage
     */
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    /** Configure LSE Drive Capability
     */
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSI | RCC_OSCILLATORTYPE_LSE;
    RCC_OscInitStruct.LSEState = RCC_LSE_ON;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
    {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection =
        RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_I2C1 | RCC_PERIPHCLK_RTC;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
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

    /** Configure the global features of the ADC (Clock, Resolution, Data
     * Alignment and number of conversion)
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
    hadc.Init.LowPowerFrequencyMode = DISABLE;
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
    hi2c1.Init.Timing = 0x00503D58;
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
    if (HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, 0, RTC_WAKEUPCLOCK_RTCCLK_DIV16) !=
        HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN RTC_Init 2 */

    /* USER CODE END RTC_Init 2 */
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
    HAL_GPIO_WritePin(GPIOB, VBAT_MEAS_EN_Pin | I2C_ENABLE_Pin, GPIO_PIN_RESET);

    /*Configure GPIO pin : DBG_LED_Pin */
    GPIO_InitStruct.Pin = DBG_LED_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DBG_LED_GPIO_Port, &GPIO_InitStruct);

    /*Configure GPIO pins : VBAT_MEAS_EN_Pin I2C_ENABLE_Pin */
    GPIO_InitStruct.Pin = VBAT_MEAS_EN_Pin | I2C_ENABLE_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* USER CODE BEGIN MX_GPIO_Init_2 */

    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

// Compute wakeup reload value according to current RTC clock source
// to keep the requested interval consistent across LSE/LSI failover.
static uint32_t rtc_compute_wakeup_reload(uint32_t seconds)
{
    uint32_t rtc_hz = (rtc_clock_source == RTC_CLOCK_LSE) ? 32768u : LSI_VALUE;
    if (rtc_hz == 0u)
    {
        rtc_hz = 37000u; // fallback if LSI_VALUE is undefined
    }

    uint32_t ticks_per_sec = rtc_hz / 16u; // wakeup clock DIV16 is used
    if (ticks_per_sec == 0u)
        ticks_per_sec = 1u;

    uint64_t raw = (uint64_t)ticks_per_sec * (uint64_t)seconds;
    if (raw == 0u)
        raw = 1u;
    if (raw > 0xFFFFu)
        raw = 0xFFFFu; // WUT is 16-bit

    // WUT reload register expects value-1
    return (uint32_t)(raw - 1u);
}

void configWakeupTime()
{
    // Optional visual indicator that we (re)armed the wake-up
    uint32_t wakeup_timer_value =
        rtc_compute_wakeup_reload((uint32_t)SLEEP_INTERVAL_SECONDS);
    HAL_StatusTypeDef st = HAL_ERROR;

    for (int attempt = 0; attempt < 3 && st != HAL_OK; ++attempt)
    {
        // Deactivate previous timer before re-arming (HAL recommendation when
        // changing value)
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
        __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
        st = HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeup_timer_value,
                                         RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    }

    if (st != HAL_OK)
    {
        // Last-resort: reinit RTC to clear stuck state, then try once more
        HAL_RTC_DeInit(&hrtc);
        MX_RTC_Init();
        HAL_RTCEx_DeactivateWakeUpTimer(&hrtc);
        __HAL_RTC_WAKEUPTIMER_CLEAR_FLAG(&hrtc, RTC_FLAG_WUTF);
        __HAL_RTC_WAKEUPTIMER_EXTI_CLEAR_FLAG();
        st = HAL_RTCEx_SetWakeUpTimer_IT(&hrtc, wakeup_timer_value,
                                         RTC_WAKEUPCLOCK_RTCCLK_DIV16);
    }
}
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

// Ensure USART2 is fully reinitialized after STOP / clock gating.
static void usart2_recover_after_stop(void)
{
    if (!__HAL_RCC_USART2_IS_CLK_ENABLED())
    {
        __HAL_RCC_USART2_CLK_ENABLE();
    }

    HAL_UART_DeInit(&huart2);
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }

    uart2_rx_flush(&huart2);
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
    //  HAL_UART_DeInit(&huart2);
    HAL_I2C_DeInit(&hi2c1);

    /* Configure all GPIOs for ultra-low power */
    ConfigureGPIOForLowPower();

    /* Disable unnecessary peripheral clocks */
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    //  __HAL_RCC_GPIOC_CLK_DISABLE(); // DO NOT DISABLE GPIO C, That is what the
    //  Crystal is connected to!!!
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
    __HAL_RCC_USART2_CLK_ENABLE();

    /* Restore GPIO configuration for normal operation */
    MX_GPIO_Init();

    /* Re-initialize peripherals with proper sequence */
    MX_I2C1_Init();
    usart2_recover_after_stop();

    /* Resume SysTick */
    HAL_ResumeTick();

    /* Add longer delay for UART stabilization */
    HAL_Delay(100);
}

void HAL_RCCEx_LSECSS_Callback(void)
{
    /* LSE failed. Immediately fall back to LSI for RTC and keep running. */
    rtc_switch_to_lsi_failover();
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
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
       line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
