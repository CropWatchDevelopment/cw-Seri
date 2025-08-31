#include "sensirion.h"
#include "main.h"
#include "sensirion_common.h"
#include "sensirion_i2c_hal.h"
#include "sht4x_i2c.h"
#include <stdlib.h>

extern I2C_HandleTypeDef hi2c1;

// Variable definitions (declared as extern in the header)
bool has_sensor_1 = false;
bool has_sensor_2 = false;
uint16_t temp_ticks_1 = 0;
uint16_t hum_ticks_1 = 0;
uint16_t temp_ticks_2 = 0;
uint16_t hum_ticks_2 = 0;
uint16_t calculated_temp;
uint8_t  calculated_hum;
int16_t i2c_error_code = 0;

// Humidity 0..100 %RH (rounded)
static inline uint8_t rh_from_ticks(uint16_t hum_ticks) {
    // (hum_ticks * 100 + 32767) / 65535  -> round to nearest
    return (uint8_t)((((uint32_t)hum_ticks) * 100U + 32767U) / 65535U);
}

// Temperature in centi-degrees C (e.g., 2534 => 25.34 °C)
static inline int16_t tc_x100_from_ticks(uint16_t temp_ticks) {
    // t[°C] = -45 + 175 * ticks / 65535
    // Multiply by 100 to keep 0.01°C resolution, round with +32767
    return (int16_t)((((int32_t)temp_ticks) * 17500 + 32767) / 65535 - 4500);
}

void scan_i2c_bus(void)
{
	// we re-set these to false because we want to check this every time for safety
    has_sensor_1 = false;
    has_sensor_2 = false;

    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x44 << 1, 1, 10) == HAL_OK) has_sensor_1 = true;
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x46 << 1, 1, 10) == HAL_OK) has_sensor_2 = true;
}

bool sensor_init_and_read(void)
{
	// If sensor A is not initialized
	//            OR
	// sensor B is not initialized
	// Throw an error
    if (!has_sensor_1 || !has_sensor_2) {
    	i2c_error_code = NO_SENSORS_FOUND;
        return false;
    }

    i2c_error_code = NO_ERROR;
    HAL_Delay(100);

    if (has_sensor_1) {
        sht4x_init(SHT43_I2C_ADDR_44);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000);
        sht4x_init(SHT43_I2C_ADDR_44);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_1, &hum_ticks_1);
        if (i2c_error_code) return false; // If Read hit a hard fault, throw an error
    }

    if (has_sensor_2) {
        sht4x_init(SHT40_I2C_ADDR_46);
        sht4x_soft_reset();
        sensirion_i2c_hal_sleep_usec(10000);
        sht4x_init(SHT40_I2C_ADDR_46);
        i2c_error_code = sht4x_measure_high_precision_ticks(&temp_ticks_2, &hum_ticks_2);
        if (i2c_error_code) return false; // If Read hit a hard fault, throw an error
    }

    calculated_temp            = (temp_ticks_1 / 100U) + 55U;
    uint16_t calculated_temp_2 = (temp_ticks_2 / 100U) + 55U;
    calculated_hum    = rh_from_ticks(hum_ticks_1);
//    uint8_t  calculated_hum_2  = rh_from_ticks(hum_ticks_2); // Not used now, maybe later

    // compute raw differences (signed)
    int16_t temp_diff = (int16_t)calculated_temp - (int16_t)calculated_temp_2;
//    int16_t hum_diff  = (int16_t)calculated_hum - (int16_t)calculated_hum_2; // Not used now, maybe later

    // convert to absolute unsigned values
    uint8_t temp_delta = (uint8_t)abs(temp_diff);
//    uint8_t hum_delta  = (uint8_t)abs(hum_diff); // Not used now, maybe later


    // if the difference between the 2 temp sensors is greater than 5 degreese celcius,
    // Then throw an error.
    // Keeping in mind that the values coming back are multipled by 100, so 264 would be 26.4
    // and 50 is actually 5.
    if (temp_delta > 50) {
    	return false;
    }

    if (i2c_error_code) {
        return false;
    }
    return true;
}
