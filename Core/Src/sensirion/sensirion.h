#pragma once
#include <stdint.h>
#include <stdbool.h>

// Converted values (centi-units):
//  - Temperature: °C × 100  (e.g., 2345 => 23.45 °C)
//  - Humidity:    %RH × 100 (e.g., 5678 => 56.78 %RH)
extern int16_t  calculated_temp_1;
extern uint16_t calculated_hum_1;
extern int16_t  calculated_temp_2;
extern uint16_t calculated_hum_2;

// Raw ticks from SHT4x reads
extern uint16_t temp_ticks_1;
extern uint16_t hum_ticks_1;
extern uint16_t temp_ticks_2;
extern uint16_t hum_ticks_2;

// Sensor presence flags and I2C error code
extern bool     has_sensor_1;
extern bool     has_sensor_2;
extern int16_t  i2c_error_code;

// Public API
void scan_i2c_bus(void);
int  sensor_init_and_read(void);

// Conversion helpers (exported)
int16_t  sht4x_temp_centi_from_ticks(uint16_t t_ticks);
uint16_t sht4x_rh_centi_from_ticks(uint16_t rh_ticks);
