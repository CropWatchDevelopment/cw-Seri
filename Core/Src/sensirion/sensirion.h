#ifndef SENSIRION_H
#define SENSIRION_H

#include <stdint.h>
#include <stdbool.h>

// External variable declarations (not definitions)
extern bool has_sensor_1;
extern bool has_sensor_2;

extern uint16_t calculated_temp_1;
extern uint8_t calculated_hum_1;

extern uint16_t calculated_temp_2;
extern uint8_t  calculated_hum_2;

extern int16_t i2c_error_code;

// Function prototypes
void scan_i2c_bus(void);
int sensor_init_and_read(void);

#endif
