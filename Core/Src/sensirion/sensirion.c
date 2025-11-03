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
bool sensor_1_is_present;
bool sensor_2_is_present;

void check_sensor_presence(void)
{
    sensor_1_is_present = (HAL_I2C_IsDeviceReady(&hi2c1, (SHT43_I2C_ADDR_44 << 1), 1/*trial*/, 10/*msec timeout*/) == HAL_OK);
    sensor_2_is_present = (HAL_I2C_IsDeviceReady(&hi2c1, (SHT40_I2C_ADDR_46 << 1), 1/*trial*/, 10/*msec timeout*/) == HAL_OK);
}

int sensor_init_and_read(void)
{
    int16_t  i2c_error_code;
    uint16_t temp_ticks_1 = 0u;
    uint16_t hum_ticks_1  = 0u;
    uint16_t temp_ticks_2 = 0u;
    uint16_t hum_ticks_2  = 0u;

    // If either sensor is missing => error
    if (!sensor_1_is_present || !sensor_2_is_present) {
        i2c_error_code = NO_SENSORS_FOUND;
        return NO_SENSORS_FOUND;
    }

    i2c_error_code = NO_ERROR;
    HAL_Delay(100);

    if (sensor_1_is_present) {
        sht4x_init(SHT43_I2C_ADDR_44);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000); // 10mSec
        sht4x_init(SHT43_I2C_ADDR_44);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_1, &hum_ticks_1);
        if (i2c_error_code) return HARD_FAULT_ON_SENSOR_NO_1_READ;
    }

    if (sensor_2_is_present) {
        sht4x_init(SHT40_I2C_ADDR_46);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000); // 10mSec
        sht4x_init(SHT40_I2C_ADDR_46);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_2, &hum_ticks_2);
        if (i2c_error_code) return HARD_FAULT_ON_SENSOR_NO_2_READ;
    }

    // Converted, centi-units: temperature in °C×100, humidity in %×100

    // Convert using exact integer math with rounding (centi-units)
    int16_t  calculated_temp_1 = sht4x_temp_centi_from_ticks(temp_ticks_1);  // °C×100, e.g., 2345 => 23.45 °C
    int16_t  calculated_temp_2 = sht4x_temp_centi_from_ticks(temp_ticks_2);  // °C×100
    uint16_t calculated_hum_1  = sht4x_rh_centi_from_ticks(hum_ticks_1);     // %×100, e.g., 5678 => 56.78 %RH
    uint16_t calculated_hum_2  = sht4x_rh_centi_from_ticks(hum_ticks_2);     // %×100

    // Compute absolute temperature delta in centi-degrees
    int16_t temp_diff  = (int16_t)(calculated_temp_1 - calculated_temp_2);
    uint16_t temp_delta = (temp_diff < 0) ? (uint16_t)(-temp_diff) : (uint16_t)temp_diff;

    // If the difference between the two temp sensors is greater than 5.00 °C
    if (temp_delta > 500) {
        return DIFF_BETWEEN_TEMPERATURE_READS_EXCEEDS_5C;
    }

    // Compute absolute humidity delta in centi-%RH
    uint16_t hum_diff = (calculated_hum_1 > calculated_hum_2) ? (calculated_hum_1 - calculated_hum_2) : (calculated_hum_2 - calculated_hum_1);

    // If you need +55.00 °C offset for transmission, do it here without
    // polluting the stored/calculated values:
    calculated_temp_1 = calculated_temp_1 + 5500;
    // (use tx_temp_* to build your payload)

    return (bool)(i2c_error_code);
}
