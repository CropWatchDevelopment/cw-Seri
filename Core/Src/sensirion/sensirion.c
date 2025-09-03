#include "sensirion.h"
#include "main.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "sht4x_i2c.h"
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

extern I2C_HandleTypeDef hi2c1;

// was: static inline int16_t sht4x_temp_centi_from_ticks(uint16_t t_ticks)
int16_t sht4x_temp_centi_from_ticks(uint16_t t_ticks) {
    uint32_t num = 17500u * (uint32_t)t_ticks + 32767u;  // nearest rounding
    int32_t centi = (int32_t)(num / 65535u) - 4500;
    if (centi < -4500) centi = -4500;
    if (centi > 13000) centi = 13000;
    return (int16_t)centi;
}

// was: static inline uint16_t sht4x_rh_centi_from_ticks(uint16_t rh_ticks)
uint16_t sht4x_rh_centi_from_ticks(uint16_t rh_ticks) {
    uint32_t num = 12500u * (uint32_t)rh_ticks + 32767u; // nearest rounding
    int32_t centi = (int32_t)(num / 65535u) - 600;
    if (centi < 0)      centi = 0;
    if (centi > 10000)  centi = 10000;
    return (uint16_t)centi;
}

// Variable definitions (declared as extern in the header)
bool     has_sensor_1 = false;
bool     has_sensor_2 = false;
uint16_t temp_ticks_1 = 0;
uint16_t hum_ticks_1  = 0;
uint16_t temp_ticks_2 = 0;
uint16_t hum_ticks_2  = 0;

// Converted, centi-units: temperature in °C×100, humidity in %×100
int16_t  calculated_temp_1;   // e.g., 2345 => 23.45 °C
uint16_t calculated_hum_1;    // e.g.,  5678 => 56.78 %RH

int16_t  calculated_temp_2;
uint16_t calculated_hum_2;

int16_t i2c_error_code = 0;

void scan_i2c_bus(void)
{
    // re-set these to false because we want to check every time for safety
    has_sensor_1 = false;
    has_sensor_2 = false;

    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x44 << 1, 1, 10) == HAL_OK) has_sensor_1 = true;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x46 << 1, 1, 10) == HAL_OK) has_sensor_2 = true;
}

int sensor_init_and_read(void)
{
    // If either sensor is missing => error
    if (!has_sensor_1 || !has_sensor_2) {
        i2c_error_code = NO_SENSORS_FOUND;
        return 1; // sensor 1 or 2 not found
    }

    i2c_error_code = NO_ERROR;
    HAL_Delay(100);

    if (has_sensor_1) {
        sht4x_init(SHT43_I2C_ADDR_44);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000);
        sht4x_init(SHT43_I2C_ADDR_44);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_1, &hum_ticks_1);
        if (i2c_error_code) return 2; // hard fault on read
    }

    if (has_sensor_2) {
        sht4x_init(SHT40_I2C_ADDR_46);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000);
        sht4x_init(SHT40_I2C_ADDR_46);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_2, &hum_ticks_2);
        if (i2c_error_code) return 3; // hard fault on read
    }

    // Convert using exact integer math with rounding (centi-units)
    calculated_temp_1 = sht4x_temp_centi_from_ticks(temp_ticks_1);  // °C×100
    calculated_temp_2 = sht4x_temp_centi_from_ticks(temp_ticks_2);  // °C×100
    calculated_hum_1  = sht4x_rh_centi_from_ticks(hum_ticks_1);     // %×100
    calculated_hum_2  = sht4x_rh_centi_from_ticks(hum_ticks_2);     // %×100

    // Compute absolute temperature delta in centi-degrees
    int16_t temp_diff  = (int16_t)(calculated_temp_1 - calculated_temp_2);
    uint16_t temp_delta = (temp_diff < 0) ? (uint16_t)(-temp_diff) : (uint16_t)temp_diff;

    // If the difference between the two temp sensors is greater than 5.00 °C
    if (temp_delta > 500) {
        return 4;
    }

    // If you need +55.00 °C offset for transmission, do it here without
    // polluting the stored/calculated values:
    calculated_temp_1 = calculated_temp_1 + 5500;
    // (use tx_temp_* to build your payload)

    if (i2c_error_code) {
        return 1;
    }
    return 0;
}
