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
#include "watchdog.h"
#include "sensirion/sensirion.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
// Sleep time in seconds between wake cycles (clamped by IWDG at runtime)
#define SLEEP_TIME_SECONDS_DEFAULT 15u
// How often to send (minutes). Wake happens more often than this.
#define SEND_INTERVAL_MINUTES 10u
#define SLEEP_INTERVAL_MARGIN_SECONDS 3u
#define BATTERY_SEND_INTERVAL_CYCLES 4500 // Should be 4400
#define SENSOR_SEND_INTERVAL_CYCLES 144u //Just over 144 day

#define DEV_EUI "0025CA00000056F7"
#define JOIN_EUI "0025CA00000055F7"

/* LSE startup timeout in milliseconds */
#ifndef LSE_STARTUP_TIMEOUT
#define LSE_STARTUP_TIMEOUT 5000u
#endif

/* RTC Backup register magic value to detect first-time init */
#define RTC_BKP_MAGIC_VALUE 0xC0DECAFE
#define RTC_BKP_MAGIC_REG   RTC_BKP_DR0
#define RTC_BKP_NEXT_ALARM_EPOCH_REG RTC_BKP_DR1
#define RTC_BKP_NEXT_SEND_EPOCH_REG  RTC_BKP_DR2

/* RTC prescalers for 32.768 kHz LSE crystal: 1 Hz tick */
#define RTC_ASYNCH_PREDIV   127u
#define RTC_SYNCH_PREDIV    255u

/* Arbitrary epoch for RTC init: 2000-01-01 00:00:00 */
#define RTC_INIT_YEAR       0u    /* 2000 */
#define RTC_INIT_MONTH      1u    /* January */
#define RTC_INIT_DAY        1u
#define RTC_INIT_HOURS      0u
#define RTC_INIT_MINUTES    0u
#define RTC_INIT_SECONDS    0u

/* Maximum LSE start retries before fail-safe */
#define LSE_START_MAX_RETRIES 3u
/* Reject restored alarms that are unreasonably far ahead (likely stale/corrupt BKP) */
#define RTC_RESTORED_ALARM_MIN_AHEAD_SECONDS 30u
#define RTC_RESTORED_ALARM_MAX_AHEAD_SECONDS 600u
/* Bound catch-up work to avoid long loops if persisted schedule is stale */
#define RTC_ALARM_CATCHUP_MAX_STEPS 4096u
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc;

I2C_HandleTypeDef hi2c1;

IWDG_HandleTypeDef hiwdg;

RTC_HandleTypeDef hrtc;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

int is_connected = 0;
static uint8_t reset_reason = 0xFF; // Store reset reason

static volatile bool g_allow_lse_fail = false;

static uint32_t transmission_count = 0;      // Total transmissions sent
// Flag to ensure first transmission happens immediately
static bool first_run = true;

static uint16_t send_battery_counter = 0;
static uint16_t send_sensor_id_counter = 0;
/* Ensure first uplink after boot prioritizes sensor payload over device-info */
static bool g_boot_sensor_payload_priority = true;

/* Next scheduled alarm time (persisted across cycles) */
static rtc_calendar_t g_next_alarm = {0};
static bool g_next_alarm_valid = false;
static uint32_t g_sleep_interval_seconds = SLEEP_TIME_SECONDS_DEFAULT;
static uint32_t g_next_send_epoch = 0u;
static bool g_next_send_valid = false;

/* Alarm fired flag - defined in stm32l0xx_it.c */
extern volatile bool g_alarm_fired;

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
static void MX_IWDG_Init(void);
/* USER CODE BEGIN PFP */
void EnterDeepSleepMode(void);
void LoRaWAN_SendHex(const uint8_t *payload, size_t length, int fPort, bool skip_response);
int lorawan_set_battery_level(UART_HandleTypeDef *huart, uint8_t battery_level);
static void usart2_recover_after_stop(void);
static void i2c1_bus_recovery(void);
static void restore_from_stop(void);
static uint32_t compute_sleep_interval_seconds(uint32_t wdg_actual_ms);
static void rtc_clear_backup_state(void);
static bool rtc_calendar_to_epoch(const rtc_calendar_t *cal, uint32_t *epoch_out);
static bool rtc_epoch_to_calendar(uint32_t epoch, rtc_calendar_t *cal);
static bool rtc_load_next_alarm_bkp(rtc_calendar_t *alarm);
static void rtc_store_next_alarm_bkp(const rtc_calendar_t *alarm);
static bool rtc_load_next_send_bkp(uint32_t *epoch_out);
static void rtc_store_next_send_bkp(uint32_t epoch);
static bool rtc_arm_alarm_a_with_retry(const rtc_calendar_t *alarm_time);
static void rtc_invalidate_next_alarm_bkp(void);
static bool rtc_restored_alarm_is_reasonable(const rtc_calendar_t *now,
                                             const rtc_calendar_t *alarm,
                                             uint32_t max_ahead_seconds);
static void rcc_apply_periph_fallback_if_lse_missing(void);
static inline void rcc_enable_guard_iopenr(uint32_t mask);
static inline void rcc_enable_guard_apb1enr(uint32_t mask);
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

// static void send_rtc_clock_indicator(void)
//{
//     if (!is_connected)
//     {
//         return;
//     }
//     uint8_t cal_payload = g_lsi_cal_last_ok ? 5u : 6u;
//     LoRaWAN_SendHex(&cal_payload, 1, 12);
//     if (!g_lsi_cal_last_ok && g_lsi_cal_status != LSI_CAL_STATUS_OK)
//     {
//         LoRaWAN_SendHex(&g_lsi_cal_status, 1, 13);
//     }
// }

// Helper to send device info (serials + reset reason)
// Returns true if a packet was actually sent.
static bool send_device_info_packet(void)
{
    bool sent = false;
    GPIO_PinState i2c_prev_state =
        HAL_GPIO_ReadPin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin);
    if (i2c_prev_state == GPIO_PIN_RESET)
    {
        HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
        HAL_Delay(1000);
    }

    scan_i2c_bus();
    // Ensure serials are fresh (though they should be stable)
    read_sensor_serials();

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

    send_sensor_id_counter++;

    if ((last_serial_1 != serial_1 && last_serial_2 != serial_2) || send_sensor_id_counter > SENSOR_SEND_INTERVAL_CYCLES)
    {
    	last_serial_1 = serial_1;
    	last_serial_2 = serial_2;
        send_sensor_id_counter = 0;
        LoRaWAN_SendHex(serial_payload, 9, 9, false);
        reset_reason = 0;
        sent = true;
    }

    if (i2c_prev_state == GPIO_PIN_RESET)
    {
        HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);
    }
    return sent;
}

static void send_device_battery(void)
{
    int aproxBatteryTemp_c = ((calculated_temp_1 - 5500) / 100);
    uint8_t battery =
        vbat_measure_and_encode(&hadc, ADC_CHANNEL_0, aproxBatteryTemp_c,
                                /*external_power_present=*/false);

    lorawan_set_battery_level(&huart2, battery);
}

static void send_sensor_reading_once(void)
{
    /*
     * Only one uplink per cycle:
     * - first post-boot cycle: send sensor payload first
     * - later cycles: send device-info when due, then return
     */
    if (!g_boot_sensor_payload_priority)
    {
        if (send_device_info_packet())
        {
            return;
        }
    }

    /* Power on I2C sensors and read data */
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_SET);
    HAL_Delay(1000);
    watchdog_kick();
    scan_i2c_bus();
    int i2c_read_result = sensor_init_and_read();
    HAL_GPIO_WritePin(I2C_ENABLE_GPIO_Port, I2C_ENABLE_Pin, GPIO_PIN_RESET);

    /* Format data and send */
    uint8_t payload[8] = {0};
    if (has_soil_sensor)
    {
        uint16_t u;
        u = (uint16_t)soil_e25;
        payload[0] = (uint8_t)(u >> 8);
        payload[1] = (uint8_t)(u & 0xFF);
        u = (uint16_t)soil_EC;
        payload[2] = (uint8_t)(u >> 8);
        payload[3] = (uint8_t)(u & 0xFF);
        u = (uint16_t)soil_temp;
        payload[4] = (uint8_t)(u >> 8);
        payload[5] = (uint8_t)(u & 0xFF);
        u = (uint16_t)soil_VWC;
        payload[6] = (uint8_t)(u >> 8);
        payload[7] = (uint8_t)(u & 0xFF);
        LoRaWAN_SendHex(payload, 8, 1, true);
    }
    else
    {
        switch (i2c_read_result)
        {
        case I2C_READ_SUCCESS:
        {
            payload[0] = (uint8_t)(calculated_temp_1 >> 8);
            payload[1] = (uint8_t)(calculated_temp_1 & 0xFF);
            payload[2] = (uint8_t)(calculated_hum_1 >> 8);
            payload[3] = (uint8_t)(calculated_hum_1 & 0xFF);
            LoRaWAN_SendHex(payload, 4, 1, true);
            break;
        }
        case I2C_SENSOR_1_MISSING:
        {
            uint8_t code = 1;
            LoRaWAN_SendHex(&code, 1, 10, true);
            break;
        }
        case I2C_SENSOR_2_MISSING:
        {
            uint8_t code = 2;
            LoRaWAN_SendHex(&code, 1, 10, true);
            break;
        }
        case I2C_SENSOR_1_READ_FAIL:
        {
            uint8_t code = 1;
            LoRaWAN_SendHex(&code, 1, 10, true);
            break;
        }
        case I2C_SENSOR_2_READ_FAIL:
        {
            uint8_t code = 2;
            LoRaWAN_SendHex(&code, 1, 10, true);
            break;
        }
        case I2C_READ_ERROR_TEMP_MISMATCH:
        {
            payload[0] = (uint8_t)(calculated_temp_1 >> 8);
            payload[1] = (uint8_t)(calculated_temp_1 & 0xFF);
            payload[2] = (uint8_t)(calculated_temp_2 >> 8);
            payload[3] = (uint8_t)(calculated_temp_2 & 0xFF);
            LoRaWAN_SendHex(payload, 4, 11, true);
            break;
        }
        default:
            break;
        }
    }

    if (g_boot_sensor_payload_priority)
    {
        g_boot_sensor_payload_priority = false;
    }
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
        watchdog_kick();
        if (total_rcv >= sizeof(rxbuf) - 1u)
            break;
        uint16_t chunk = 0;
        uint16_t cap = (uint16_t)(sizeof(rxbuf) - 1u - total_rcv);
        if (HAL_UARTEx_ReceiveToIdle(&huart2, rxbuf + total_rcv,
                                     cap, &chunk,
                                     1000) == HAL_OK)
        {
            if (chunk > 0)
            {
                total_rcv += chunk;
                if (total_rcv >= sizeof(rxbuf))
                    total_rcv = sizeof(rxbuf) - 1u;
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

void LoRaWAN_SendHex(const uint8_t *payload, size_t length, int fPort, bool skip_response)
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

    if (skip_response) return; // This is an early return, maybe remove it!!!!!

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
        watchdog_kick();
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
    return;
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
  g_allow_lse_fail = true;

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  g_allow_lse_fail = false;
  rcc_apply_periph_fallback_if_lse_missing();

  /* Keep LSE drive strength at HIGH for reliable crystal operation */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_RTC_Init();
  MX_I2C1_Init();
  MX_ADC_Init();
  MX_IWDG_Init();
  /* USER CODE BEGIN 2 */

    /* Initialize RTC with LSE (only once, guarded by BKP magic) */
    if (!rtc_init_once())
    {
        /* LSE startup failed - enter fail-safe: blink LED and reset */
        for (int i = 0; i < 10; i++)
        {
            HAL_GPIO_TogglePin(DBG_LED_GPIO_Port, DBG_LED_Pin);
            HAL_Delay(200);
        }
        HAL_NVIC_SystemReset();
    }

    /* Restore persisted alarm schedule if available */
    if (rtc_load_next_alarm_bkp(&g_next_alarm))
    {
        g_next_alarm_valid = true;
    }
    if (rtc_load_next_send_bkp(&g_next_send_epoch))
    {
        g_next_send_valid = true;
    }

    /* Initialize watchdog for hang protection (max timeout at worst-case LSI) */
    uint32_t wdg_actual_ms = 0u;
    watchdog_init_90s(&wdg_actual_ms);
    watchdog_kick();
    g_sleep_interval_seconds = compute_sleep_interval_seconds(wdg_actual_ms);

    if (g_next_alarm_valid)
    {
        rtc_calendar_t now_for_alarm_sanity = {0};
        rtc_read_now(&now_for_alarm_sanity);

        uint32_t max_ahead_seconds = g_sleep_interval_seconds * 8u;
        if (max_ahead_seconds < RTC_RESTORED_ALARM_MIN_AHEAD_SECONDS)
        {
            max_ahead_seconds = RTC_RESTORED_ALARM_MIN_AHEAD_SECONDS;
        }
        if (max_ahead_seconds > RTC_RESTORED_ALARM_MAX_AHEAD_SECONDS)
        {
            max_ahead_seconds = RTC_RESTORED_ALARM_MAX_AHEAD_SECONDS;
        }

        if (!rtc_restored_alarm_is_reasonable(&now_for_alarm_sanity,
                                              &g_next_alarm,
                                              max_ahead_seconds))
        {
            g_next_alarm_valid = false;
            rtc_invalidate_next_alarm_bkp();
        }
    }

    /* Capture reset reason early */
    reset_reason = GetResetSource();

    // Check if already joined
    int startup_join_state = lorawan_check_joined(&huart2);
    if (startup_join_state == 1)
    {
        is_connected = 1;
        // send_device_info_packet();
    }
    else if (startup_join_state == 0)
    {
        is_connected = 0;
        HAL_UART_Transmit(&huart2, (uint8_t *)"AT\r\n", 4, 300);
        HAL_Delay(300);
        int need_provision = uart2_probe_and_align();
        if (need_provision == 1)
        {
            watchdog_kick();
            HAL_UART_Transmit(
                &huart2, (uint8_t *)"AT\r\n", 4,
                300); // One initial AT to clear any odd commands sent before
            HAL_Delay(400);
            // Set LoRaWAN Settings
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 602=1\r\n", 11,
                              300); // Activation Mode OTAA (0 = ABP, 1 = OTAA)
            HAL_Delay(400);
            watchdog_kick();
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 603=0\r\n", 11,
                              300); // Set CLASS to A
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 604=1\r\n", 11,
                              300); // Confirmed 0 = NO, 1 = yes
            HAL_Delay(400);
            watchdog_kick();
            HAL_UART_Transmit(
                &huart2, (uint8_t *)"ATS 605=3\r\n", 11,
                300); // Retry if Confirm Fails, 3 Retries set (and is default)
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 611=9\r\n", 11,
                              300); // Set Region to AS923-1 (JAPAN)
            HAL_Delay(400);
            watchdog_kick();
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
            watchdog_kick();

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
            watchdog_kick();

            HAL_UART_Transmit(&huart2, (uint8_t *)"ATS 213=2000\r\n", 14,
                              300); // Set Sleep Mode to 2 seconds
            HAL_Delay(400);
            HAL_UART_Transmit(&huart2, (uint8_t *)"AT&W\r\n", 6, 300); // SAVE ALL!
            HAL_Delay(400);
            watchdog_kick();
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

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        watchdog_kick();

        /*
         * Run one cycle: either first_run or alarm fired.
         * Clear alarm flag atomically.
         */
        bool do_cycle = first_run;
        if (!do_cycle)
        {
            __disable_irq();
            do_cycle = g_alarm_fired;
            g_alarm_fired = false;
            __enable_irq();
        }

        if (do_cycle)
        {
            rtc_calendar_t now;
            rtc_read_now(&now);

            uint32_t now_epoch = 0u;
            bool now_epoch_ok = rtc_calendar_to_epoch(&now, &now_epoch);
            uint32_t send_interval_seconds = (uint32_t)SEND_INTERVAL_MINUTES * 60u;
            if (send_interval_seconds == 0u)
            {
                send_interval_seconds = 1u;
            }

            bool send_due = false;
            if (now_epoch_ok)
            {
                if (!g_next_send_valid)
                {
                    g_next_send_epoch = now_epoch + send_interval_seconds;
                    g_next_send_valid = true;
                    rtc_store_next_send_bkp(g_next_send_epoch);
                    if (first_run)
                    {
                        send_due = true;
                    }
                }
                if (g_next_send_valid && now_epoch >= g_next_send_epoch)
                {
                    send_due = true;
                }
            }
            else
            {
                send_due = true;
                g_next_send_valid = false;
            }

            if (send_due)
            {
                transmission_count++;

                /* Refresh connection flag from the module each cycle */
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
                    watchdog_kick();
                    if (is_connected)
                    {
                        send_sensor_reading_once();
                    }
                }
                else
                {
                    send_sensor_reading_once();

                    watchdog_kick();

                    /* Battery reporting */
                    send_battery_counter++;
                    if (send_battery_counter >= BATTERY_SEND_INTERVAL_CYCLES)
                    {
                        send_battery_counter = 0;
                        send_device_battery();
                    }
                }

                if (now_epoch_ok && g_next_send_valid)
                {
                    rtc_read_now(&now);
                    if (rtc_calendar_to_epoch(&now, &now_epoch))
                    {
                        while (g_next_send_epoch <= now_epoch)
                        {
                            g_next_send_epoch += send_interval_seconds;
                        }
                        rtc_store_next_send_bkp(g_next_send_epoch);
                    }
                    else
                    {
                        g_next_send_valid = false;
                    }
                }
            }

            first_run = false;
        }

        /* Compute next alarm and enter STOP mode */
        rtc_calendar_t now;
        rtc_read_now(&now);
        rtc_compute_next_alarm_fixed_grid(&now, &g_next_alarm);
        g_next_alarm_valid = true;
        rtc_store_next_alarm_bkp(&g_next_alarm);
        if (!rtc_arm_alarm_a_with_retry(&g_next_alarm))
        {
            continue;
        }
        EnterDeepSleepMode();
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI|RCC_OSCILLATORTYPE_LSI
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2|RCC_PERIPHCLK_I2C1
                              |RCC_PERIPHCLK_RTC;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
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
  * @brief IWDG Initialization Function
  * @param None
  * @retval None
  */
static void MX_IWDG_Init(void)
{

  /* USER CODE BEGIN IWDG_Init 0 */

  /* USER CODE END IWDG_Init 0 */

  /* USER CODE BEGIN IWDG_Init 1 */

  /* USER CODE END IWDG_Init 1 */
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
  hiwdg.Init.Window = 4095;
  hiwdg.Init.Reload = 4095;
  if (HAL_IWDG_Init(&hiwdg) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN IWDG_Init 2 */
  watchdog_mark_started();

  /* USER CODE END IWDG_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET)
  {
    return;
  }
  if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSS) != RESET)
  {
    return;
  }

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

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

  /* USER CODE BEGIN Check_RTC_BKUP */
  /*
   * Check if RTC was already initialized by reading backup register magic.
   * If magic is present, skip the CubeMX-generated time/date/alarm initialization
   * to preserve the running calendar and scheduled alarm across resets/wakes.
   *
   * The actual first-time RTC initialization is done in rtc_init_once().
   */
  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == RTC_BKP_MAGIC_VALUE)
  {
      /* RTC already initialized - skip time/date/alarm reset */
      return;
  }
  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm A
  */
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x0;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
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
    /* Build LLM for Ezurio Module */
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

/**
 * @brief  Recover I2C bus from stuck state (SDA held low by slave)
 * @note   Toggles SCL as GPIO to clock out any slave holding SDA low.
 *         Must be called BEFORE HAL_I2C_Init() and AFTER GPIO clocks are enabled.
 *         This prevents hard-to-debug hangs when a sensor was mid-transfer
 *         as the MCU entered STOP mode.
 */
static void i2c1_bus_recovery(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Configure SCL (PB6) as open-drain output for manual clocking */
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Configure SDA (PB7) as input to monitor release */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* Toggle SCL up to 16 times to clock out any stuck slave */
    for (uint8_t i = 0; i < 16u; i++)
    {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        /* ~5 µs low (well within I2C spec for standard/fast mode) */
        for (volatile uint32_t d = 0; d < 32u; d++) { __NOP(); }
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        for (volatile uint32_t d = 0; d < 32u; d++) { __NOP(); }

        /* If SDA is released (high), bus is free */
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_7) == GPIO_PIN_SET)
        {
            break;
        }
    }

    /* Generate a STOP condition: SDA low then high while SCL is high */
    GPIO_InitStruct.Pin = GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    for (volatile uint32_t d = 0; d < 32u; d++) { __NOP(); }
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    for (volatile uint32_t d = 0; d < 32u; d++) { __NOP(); }

    /*
     * GPIO pins will be reconfigured to I2C alternate function
     * by MX_I2C1_Init() -> HAL_I2C_Init() -> HAL_I2C_MspInit().
     */
}

static void restore_from_stop(void)
{
    /* Resume SysTick for timeouts used during clock reconfiguration */
    HAL_ResumeTick();
    watchdog_kick();

    /* Upon wake-up, the system clock needs to be reconfigured */
    g_allow_lse_fail = true;
    SystemClock_Config();
    g_allow_lse_fail = false;
    rcc_apply_periph_fallback_if_lse_missing();

    /* Keep LSE drive strength at HIGH after STOP */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);

    /* Re-enable peripheral clocks with readback/barrier guard (STM32L0 errata) */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    rcc_enable_guard_iopenr(RCC_IOPENR_GPIOAEN);
    __HAL_RCC_GPIOB_CLK_ENABLE();
    rcc_enable_guard_iopenr(RCC_IOPENR_GPIOBEN);
    __HAL_RCC_I2C1_CLK_ENABLE();
    rcc_enable_guard_apb1enr(RCC_APB1ENR_I2C1EN);
    __HAL_RCC_USART2_CLK_ENABLE();
    rcc_enable_guard_apb1enr(RCC_APB1ENR_USART2EN);

    /* Restore GPIO configuration for normal operation */
    MX_GPIO_Init();

    /*
     * I2C bus recovery: if a sensor was mid-transfer when STOP mode
     * was entered, it may hold SDA low.  Toggle SCL to free the bus
     * before re-initialising the I2C peripheral.
     */
    i2c1_bus_recovery();

    /* Re-initialize peripherals with proper sequence */
    MX_I2C1_Init();
    usart2_recover_after_stop();

    /* Add longer delay for UART stabilization */
    HAL_Delay(100);
    watchdog_kick();
}

static void rcc_apply_periph_fallback_if_lse_missing(void)
{
    if ((__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) ||
        (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSS) != RESET))
    {
        RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
        PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2 | RCC_PERIPHCLK_I2C1;
        PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_HSI;
        PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_HSI;
        if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
        {
            Error_Handler();
        }
    }
}

static inline void rcc_enable_guard_iopenr(uint32_t mask)
{
    (void)READ_BIT(RCC->IOPENR, mask);
    __DSB();
    __NOP();
}

static inline void rcc_enable_guard_apb1enr(uint32_t mask)
{
    (void)READ_BIT(RCC->APB1ENR, mask);
    __DSB();
    __NOP();
}

/*============================================================================
 * RTC ALARM A SCHEDULING FUNCTIONS
 *============================================================================*/

/**
 * @brief  Wait for LSE to become ready
 * @param  timeout_ms: Timeout in milliseconds
 * @retval true if LSE ready, false on timeout
 */
static bool lse_wait_ready(uint32_t timeout_ms)
{
    uint32_t t0 = HAL_GetTick();
    while (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET)
    {
        watchdog_kick();
        if ((HAL_GetTick() - t0) > timeout_ms)
        {
            return false;
        }
    }
    return true;
}

/**
 * @brief  Check if a given year is a leap year
 * @param  year: Year (0-99 offset from 2000)
 * @retval true if leap year
 */
static bool is_leap_year(uint8_t year)
{
    uint16_t y = 2000u + year;
    return ((y % 4 == 0) && (y % 100 != 0)) || (y % 400 == 0);
}

/**
 * @brief  Get the number of days in a given month
 * @param  month: Month (1-12)
 * @param  year: Year (0-99 offset from 2000)
 * @retval Number of days in month
 */
static uint8_t days_in_month(uint8_t month, uint8_t year)
{
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12) return 31;
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

static uint32_t compute_sleep_interval_seconds(uint32_t wdg_actual_ms)
{
    uint32_t safe_seconds = SLEEP_TIME_SECONDS_DEFAULT;

    if (wdg_actual_ms > (SLEEP_INTERVAL_MARGIN_SECONDS * 1000u))
    {
        uint32_t wdg_seconds = wdg_actual_ms / 1000u;
        if (wdg_seconds > SLEEP_INTERVAL_MARGIN_SECONDS)
        {
            uint32_t wdg_safe = wdg_seconds - SLEEP_INTERVAL_MARGIN_SECONDS;
            if (wdg_safe < safe_seconds)
            {
                safe_seconds = wdg_safe;
            }
        }
        else
        {
            safe_seconds = 1u;
        }
    }

    if (safe_seconds == 0u)
    {
        safe_seconds = 1u;
    }

    return safe_seconds;
}

static void rtc_clear_backup_state(void)
{
    if (hrtc.Instance == NULL)
    {
        hrtc.Instance = RTC;
    }
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_MAGIC_REG, 0u);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_NEXT_ALARM_EPOCH_REG, 0u);
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_NEXT_SEND_EPOCH_REG, 0u);
}

static bool rtc_calendar_to_epoch(const rtc_calendar_t *cal, uint32_t *epoch_out)
{
    if (cal == NULL || epoch_out == NULL)
    {
        return false;
    }

    if (cal->month < 1 || cal->month > 12)
    {
        return false;
    }
    if (cal->hours > 23 || cal->minutes > 59 || cal->seconds > 59)
    {
        return false;
    }

    uint8_t dim = days_in_month(cal->month, cal->year);
    if (cal->day < 1 || cal->day > dim)
    {
        return false;
    }

    uint32_t days = 0;
    for (uint8_t y = 0; y < cal->year; y++)
    {
        days += is_leap_year(y) ? 366u : 365u;
    }

    for (uint8_t m = 1; m < cal->month; m++)
    {
        days += (uint32_t)days_in_month(m, cal->year);
    }

    days += (uint32_t)(cal->day - 1u);

    uint32_t seconds = days * 86400u;
    seconds += (uint32_t)cal->hours * 3600u;
    seconds += (uint32_t)cal->minutes * 60u;
    seconds += (uint32_t)cal->seconds;

    *epoch_out = seconds;
    return true;
}

static bool rtc_epoch_to_calendar(uint32_t epoch, rtc_calendar_t *cal)
{
    if (cal == NULL)
    {
        return false;
    }

    uint32_t days = epoch / 86400u;
    uint32_t rem = epoch % 86400u;

    uint8_t year = 0u;
    while (year < 100u)
    {
        uint32_t y_days = is_leap_year(year) ? 366u : 365u;
        if (days < y_days)
        {
            break;
        }
        days -= y_days;
        year++;
    }
    if (year >= 100u)
    {
        return false;
    }

    uint8_t month = 1u;
    while (month <= 12u)
    {
        uint8_t dim = days_in_month(month, year);
        if (days < dim)
        {
            break;
        }
        days -= dim;
        month++;
    }
    if (month > 12u)
    {
        return false;
    }

    cal->year = year;
    cal->month = month;
    cal->day = (uint8_t)(days + 1u);

    cal->hours = (uint8_t)(rem / 3600u);
    rem %= 3600u;
    cal->minutes = (uint8_t)(rem / 60u);
    cal->seconds = (uint8_t)(rem % 60u);

    return true;
}

/**
 * @brief  Initialize RTC once at first boot (guarded by BKP magic)
 * @retval true on success, false on LSE failure
 */
bool rtc_init_once(void)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    /*
     * Ensure handle instance is valid before any BKP register access.
     * MX_RTC_Init() can return early when LSE is not ready.
     */
    hrtc.Instance = RTC;

    /* Enable PWR and backup access */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* Check if RTC is already initialized via backup register magic */
    uint32_t magic = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_MAGIC_REG);
    if (magic == RTC_BKP_MAGIC_VALUE)
    {
        if ((__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) ||
            (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSS) != RESET))
        {
            rtc_clear_backup_state();
            HAL_RCCEx_DisableLSECSS();
            return false;
        }

        /*
         * RTC was previously initialized. Do NOT reset backup domain.
         * Just ensure RTC handle is configured for use.
         */
        hrtc.Instance = RTC;
        hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
        hrtc.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
        hrtc.Init.SynchPrediv = RTC_SYNCH_PREDIV;
        hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
        hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
        hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
        hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;

        /* Enable RTC clock if needed */
        __HAL_RCC_RTC_ENABLE();

        /* Ensure Alarm A NVIC is enabled */
        HAL_NVIC_SetPriority(RTC_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(RTC_IRQn);

        __HAL_RCC_CLEAR_IT(RCC_IT_LSECSS);
        __HAL_RCC_LSECSS_EXTI_CLEAR_FLAG();
        HAL_RCCEx_EnableLSECSS_IT();

        return true;
    }

    /*
     * First-time initialization: Start LSE, configure RTC, set epoch time.
     */

    /* Reset backup domain to clear any previous RTC config */
    __HAL_RCC_BACKUPRESET_FORCE();
    __HAL_RCC_BACKUPRESET_RELEASE();

    /* Configure LSE with high drive strength for reliable startup */
    __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_HIGH);
    __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);

    /* Wait for LSE to stabilize with retries */
    for (uint8_t retry = 0; retry < LSE_START_MAX_RETRIES; retry++)
    {
        if (lse_wait_ready(LSE_STARTUP_TIMEOUT))
        {
            break;
        }
        /* Retry: toggle LSE off/on */
        __HAL_RCC_LSE_CONFIG(RCC_LSE_OFF);
        HAL_Delay(100);
        __HAL_RCC_LSE_CONFIG(RCC_LSE_ON);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET)
    {
        /* LSE failed to start - return failure (caller will handle fail-safe) */
        return false;
    }

    /* Select LSE as RTC clock source */
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
    {
        return false;
    }

    /* Enable RTC peripheral */
    __HAL_RCC_RTC_ENABLE();

    /* Initialize RTC with LSE prescalers for 1 Hz */
    hrtc.Instance = RTC;
    hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
    hrtc.Init.AsynchPrediv = RTC_ASYNCH_PREDIV;
    hrtc.Init.SynchPrediv = RTC_SYNCH_PREDIV;
    hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
    hrtc.Init.OutPutRemap = RTC_OUTPUT_REMAP_NONE;
    hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
    hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
    if (HAL_RTC_Init(&hrtc) != HAL_OK)
    {
        return false;
    }

    /* Set initial time to arbitrary epoch: 2000-01-01 00:00:00 */
    sTime.Hours = RTC_INIT_HOURS;
    sTime.Minutes = RTC_INIT_MINUTES;
    sTime.Seconds = RTC_INIT_SECONDS;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    sDate.Year = RTC_INIT_YEAR;
    sDate.Month = RTC_INIT_MONTH;
    sDate.Date = RTC_INIT_DAY;
    sDate.WeekDay = RTC_WEEKDAY_SATURDAY; /* Arbitrary */
    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    /* Write magic to backup register to mark RTC as initialized */
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_MAGIC_REG, RTC_BKP_MAGIC_VALUE);

    /* Enable Alarm A interrupt in NVIC */
    HAL_NVIC_SetPriority(RTC_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(RTC_IRQn);

    __HAL_RCC_CLEAR_IT(RCC_IT_LSECSS);
    __HAL_RCC_LSECSS_EXTI_CLEAR_FLAG();
    HAL_RCCEx_EnableLSECSS_IT();

    return true;
}

/**
 * @brief  Read current RTC time/date
 * @param  now: Pointer to calendar structure to fill
 * @note   Must call GetTime before GetDate to unlock shadow registers
 */
void rtc_read_now(rtc_calendar_t *now)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    /* HAL_RTC_GetTime must be called before GetDate to unlock shadow registers */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    now->year = sDate.Year;
    now->month = sDate.Month;
    now->day = sDate.Date;
    now->hours = sTime.Hours;
    now->minutes = sTime.Minutes;
    now->seconds = sTime.Seconds;
}

/**
 * @brief  Add minutes to a calendar time (handles rollover)
 * @param  cal: Pointer to calendar structure to modify
 * @param  minutes_to_add: Minutes to add
 */
static void calendar_add_minutes(rtc_calendar_t *cal, uint32_t minutes_to_add)
{
    uint32_t total_minutes = cal->minutes + minutes_to_add;
    uint32_t hours_to_add = total_minutes / 60u;
    cal->minutes = total_minutes % 60u;

    uint32_t total_hours = cal->hours + hours_to_add;
    uint32_t days_to_add = total_hours / 24u;
    cal->hours = total_hours % 24u;

    while (days_to_add > 0)
    {
        uint8_t dim = days_in_month(cal->month, cal->year);
        if (cal->day + days_to_add <= dim)
        {
            cal->day += days_to_add;
            days_to_add = 0;
        }
        else
        {
            days_to_add -= (dim - cal->day + 1);
            cal->day = 1;
            cal->month++;
            if (cal->month > 12)
            {
                cal->month = 1;
                cal->year++;
                if (cal->year > 99) cal->year = 0; /* Wrap at 2099 -> 2000 */
            }
        }
    }
}

/**
 * @brief  Add seconds to a calendar time (handles rollover)
 * @param  cal: Pointer to calendar structure to modify
 * @param  seconds_to_add: Seconds to add
 */
static void calendar_add_seconds(rtc_calendar_t *cal, uint32_t seconds_to_add)
{
    uint32_t total_seconds = cal->seconds + seconds_to_add;
    uint32_t minutes_to_add = total_seconds / 60u;
    cal->seconds = total_seconds % 60u;

    if (minutes_to_add > 0u)
    {
        calendar_add_minutes(cal, minutes_to_add);
    }
}

/**
 * @brief  Compare two calendar times
 * @retval <0 if a < b, 0 if a == b, >0 if a > b
 */
static int calendar_compare(const rtc_calendar_t *a, const rtc_calendar_t *b)
{
    if (a->year != b->year) return (int)a->year - (int)b->year;
    if (a->month != b->month) return (int)a->month - (int)b->month;
    if (a->day != b->day) return (int)a->day - (int)b->day;
    if (a->hours != b->hours) return (int)a->hours - (int)b->hours;
    if (a->minutes != b->minutes) return (int)a->minutes - (int)b->minutes;
    return (int)a->seconds - (int)b->seconds;
}

/**
 * @brief  Compute next alarm time using fixed-grid scheduling
 * @param  now: Current RTC time
 * @param  next_alarm: Pointer to next alarm (input: previous, output: new)
 * @note   Implements fixed interval grid with catch-up if late
 */
void rtc_compute_next_alarm_fixed_grid(const rtc_calendar_t *now, rtc_calendar_t *next_alarm)
{
    uint32_t interval = g_sleep_interval_seconds;
    if (interval == 0u)
    {
        interval = 1u;
    }

    /* If next_alarm is not valid/initialized, start from now + interval */
    if (!g_next_alarm_valid)
    {
        *next_alarm = *now;
        calendar_add_seconds(next_alarm, interval);
        return;
    }

    /* Advance by interval from previous schedule */
    calendar_add_seconds(next_alarm, interval);

    /* If we're late (next_alarm <= now), catch up in interval steps */
    uint32_t catchup_steps = 0u;
    while (calendar_compare(next_alarm, now) <= 0)
    {
        if (catchup_steps >= RTC_ALARM_CATCHUP_MAX_STEPS)
        {
            /*
             * Persisted schedule is too stale/corrupt for bounded catch-up.
             * Rebase to now + interval and proceed.
             */
            *next_alarm = *now;
            calendar_add_seconds(next_alarm, interval);
            return;
        }

        calendar_add_seconds(next_alarm, interval);
        catchup_steps++;

        /* Keep watchdog serviced even if we need many catch-up steps. */
        if ((catchup_steps & 0x3Fu) == 0u)
        {
            watchdog_kick();
        }
    }
}

static bool rtc_arm_alarm_a_with_retry(const rtc_calendar_t *alarm_time)
{
    for (uint8_t attempt = 0; attempt < 3u; attempt++)
    {
        if (rtc_arm_alarm_a(alarm_time))
        {
            return true;
        }
        watchdog_kick();
        HAL_Delay(100);
    }

    NVIC_SystemReset();
    return false;
}

/**
 * @brief  Arm RTC Alarm A for the specified calendar time
 * @param  alarm_time: Target alarm time
 * @retval true on success
 */
bool rtc_arm_alarm_a(const rtc_calendar_t *alarm_time)
{
    RTC_AlarmTypeDef sAlarm = {0};

    /* Deactivate any existing Alarm A */
    HAL_RTC_DeactivateAlarm(&hrtc, RTC_ALARM_A);

    /* Clear RTC Alarm A flag */
    __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);

    /* Clear EXTI line 17 (RTC Alarm) */
    __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();

    /* Configure Alarm A */
    sAlarm.AlarmTime.Hours = alarm_time->hours;
    sAlarm.AlarmTime.Minutes = alarm_time->minutes;
    sAlarm.AlarmTime.Seconds = alarm_time->seconds;
    sAlarm.AlarmTime.SubSeconds = 0;
    sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;

    /*
     * For Alarm A matching: match hours, minutes, seconds, and date.
     * RTC_ALARMMASK_NONE means all fields must match.
     */
    sAlarm.AlarmMask = RTC_ALARMMASK_NONE;
    sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
    sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
    sAlarm.AlarmDateWeekDay = alarm_time->day;
    sAlarm.Alarm = RTC_ALARM_A;

    if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BIN) != HAL_OK)
    {
        return false;
    }

    return true;
}

static void rtc_store_next_alarm_bkp(const rtc_calendar_t *alarm)
{
    uint32_t epoch = 0u;
    if (!rtc_calendar_to_epoch(alarm, &epoch))
    {
        return;
    }

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_NEXT_ALARM_EPOCH_REG, epoch);
}

static bool rtc_load_next_alarm_bkp(rtc_calendar_t *alarm)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    uint32_t epoch = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_NEXT_ALARM_EPOCH_REG);
    if (epoch == 0u)
    {
        return false;
    }

    return rtc_epoch_to_calendar(epoch, alarm);
}

static void rtc_invalidate_next_alarm_bkp(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_NEXT_ALARM_EPOCH_REG, 0u);
}

static bool rtc_restored_alarm_is_reasonable(const rtc_calendar_t *now,
                                             const rtc_calendar_t *alarm,
                                             uint32_t max_ahead_seconds)
{
    uint32_t now_epoch = 0u;
    uint32_t alarm_epoch = 0u;

    if ((now == NULL) || (alarm == NULL) || (max_ahead_seconds == 0u))
    {
        return false;
    }

    if (!rtc_calendar_to_epoch(now, &now_epoch) ||
        !rtc_calendar_to_epoch(alarm, &alarm_epoch))
    {
        return false;
    }

    if (alarm_epoch <= now_epoch)
    {
        return false;
    }

    if ((alarm_epoch - now_epoch) > max_ahead_seconds)
    {
        return false;
    }

    return true;
}

static void rtc_store_next_send_bkp(uint32_t epoch)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_NEXT_SEND_EPOCH_REG, epoch);
}

static bool rtc_load_next_send_bkp(uint32_t *epoch_out)
{
    if (epoch_out == NULL)
    {
        return false;
    }

    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    uint32_t epoch = HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_NEXT_SEND_EPOCH_REG);
    if (epoch == 0u)
    {
        return false;
    }

    *epoch_out = epoch;
    return true;
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

    /* Configure GPIOC pins for low power (leave PC14/PC15 for LSE) */
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
        rcc_enable_guard_apb1enr(RCC_APB1ENR_USART2EN);
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
 * @brief  Enter Deep Sleep Mode using STOP mode with RTC Alarm A wake-up
 * @retval None
 * @note   Alarm A must be armed before calling this function
 */
void EnterDeepSleepMode(void)
{
    HAL_I2C_DeInit(&hi2c1);

    /* Configure all GPIOs for ultra-low power */
    ConfigureGPIOForLowPower();

    /* Disable unnecessary peripheral clocks */
    __HAL_RCC_I2C1_CLK_DISABLE();
    __HAL_RCC_USART2_CLK_DISABLE();
    __HAL_RCC_GPIOB_CLK_DISABLE();
    /* DO NOT DISABLE GPIOC - LSE crystal is on PC14/PC15 */
    __HAL_RCC_GPIOD_CLK_DISABLE();
    __HAL_RCC_GPIOH_CLK_DISABLE();

    /* Suspend SysTick to avoid wake-up from SysTick interrupt */
    HAL_SuspendTick();

    /* Clear any pending wake-up flags before sleeping */
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /*
     * Race-condition guard: if the RTC alarm already fired between
     * rtc_arm_alarm_a() and here, the ALRAF flag is set and g_alarm_fired
     * is true.  Clearing the flag now would erase the only pending wake
     * source, causing WFI to sleep until the watchdog resets us.
     *
     * Instead, check atomically: if the alarm already fired, skip sleep
     * entirely and return to the main loop to process the event.
     */
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    if (g_alarm_fired ||
        __HAL_RTC_ALARM_GET_FLAG(&hrtc, RTC_FLAG_ALRAF) != 0U)
    {
        g_alarm_fired = true;
        __HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
        __HAL_RTC_ALARM_EXTI_CLEAR_FLAG();
        if (primask == 0U)
        {
            __enable_irq();
        }
        restore_from_stop();
        return;
    }

    /* Kick watchdog before entering STOP */
    watchdog_kick();

    /* Enter STOP Mode with Main Regulator for maximum wake reliability */
    HAL_PWR_EnterSTOPMode(PWR_MAINREGULATOR_ON, PWR_STOPENTRY_WFI);

    /*
     * Restore interrupt mask immediately after wake-up.
     * Keep IRQs disabled only around the WFI entry to close the
     * alarm-fired race window.
     */
    if (primask == 0U)
    {
        __enable_irq();
    }

    /* === DEVICE IS NOW IN DEEP SLEEP === */
    /* === WAKE UP OCCURS HERE (Alarm A fired) === */

    restore_from_stop();
}

void HAL_RCCEx_LSECSS_Callback(void)
{
    rtc_clear_backup_state();
    HAL_RCCEx_DisableLSECSS();
    NVIC_SystemReset();
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
    if (g_allow_lse_fail)
    {
        if ((__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) == RESET) ||
            (__HAL_RCC_GET_FLAG(RCC_FLAG_LSECSS) != RESET))
        {
            rtc_clear_backup_state();
            HAL_RCCEx_DisableLSECSS();
        }
    }
    __disable_irq();
    HAL_NVIC_SystemReset();
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
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n", file,
       line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
