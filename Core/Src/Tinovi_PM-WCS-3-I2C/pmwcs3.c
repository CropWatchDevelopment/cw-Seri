/*
 * pmwcs3.c
 *
 *  Created on: Oct 4, 2025
 *      Author: kevin
 */


/*
 * pmwcs3.c
 *
 *  Created on: Oct 4, 2025
 *      Author: kevin
 */


#include "pmwcs3.h"
#include <string.h>     /* memset */

/* -- private constants -------------------------------------------------- */
#define PMWCS3_I2C_TIMEOUT   100U      /* ms */

/* Command bytes – based on Tinovi examples ------------------------------ */
enum {
    CMD_READ_START    = 0x01,  /* trigger measurement */
    CMD_GET_DATA      = 0x09,  /* command to make sensor send its 8 data bytes */

    // Optional: commands for individual raw value reads, not used by get_all
    // CMD_READ_E25      = 0x02,
    // CMD_READ_EC       = 0x03,
    // CMD_READ_TEMP     = 0x04,
    // CMD_READ_VWC      = 0x05,

    CMD_CAL_AIR       = 0x06,
    CMD_CAL_WATER     = 0x07,
    CMD_CAL_EC        = 0x10,

    CMD_SET_I2C_ADDR  = 0x08
    // Other commands from Arduino lib if needed:
    // REG_CAP     0x0A
    // REG_RES     0x0B  (also for resetDefault)
    // REG_RC      0x0C
    // REG_RT      0x0D
};

/* ----------------------------------------------------------------------- */
pmwcs3_status_t pmwcs3_init(pmwcs3_t *dev,
                            I2C_HandleTypeDef *bus,
                            uint8_t i2c_addr)
{
    if (!dev || !bus) return PMWCS3_ERR_BADARG;
    dev->bus  = bus;
    dev->addr = i2c_addr & 0x7F;      /* force 7-bit */
    return PMWCS3_OK;
}

pmwcs3_status_t pmwcs3_new_address(pmwcs3_t *dev, uint8_t new_addr)
{
    uint8_t buf[2] = { CMD_SET_I2C_ADDR, (uint8_t)(new_addr & 0x7F) };
    if (HAL_I2C_Master_Transmit(dev->bus,
                                dev->addr << 1, buf, sizeof(buf),
                                PMWCS3_I2C_TIMEOUT) != HAL_OK)
        return PMWCS3_ERR_I2C;
    dev->addr = new_addr & 0x7F; // Update stored address
    return PMWCS3_OK;
}

/* generic calibration helper */
static pmwcs3_status_t calib_cmd_param(pmwcs3_t *dev,
                                       uint8_t cmd, uint16_t param)
{
    uint8_t buf[3] = { cmd, param & 0xFF, (param >> 8) & 0xFF };
    if (HAL_I2C_Master_Transmit(dev->bus, dev->addr << 1, buf, sizeof(buf), PMWCS3_I2C_TIMEOUT) != HAL_OK)
        return PMWCS3_ERR_I2C;
    return PMWCS3_OK;
}

static pmwcs3_status_t calib_cmd_no_param(pmwcs3_t *dev, uint8_t cmd)
{
    if (HAL_I2C_Master_Transmit(dev->bus,
                                dev->addr << 1, &cmd, 1, /* Send only 1 command byte */
                                PMWCS3_I2C_TIMEOUT) != HAL_OK)
        return PMWCS3_ERR_I2C;
    return PMWCS3_OK;
}

pmwcs3_status_t pmwcs3_cal_air(pmwcs3_t *dev)
{
    return calib_cmd_no_param(dev, CMD_CAL_AIR);
}

pmwcs3_status_t pmwcs3_cal_water(pmwcs3_t *dev)
{
    return calib_cmd_no_param(dev, CMD_CAL_WATER);
}

pmwcs3_status_t pmwcs3_cal_ec(pmwcs3_t *dev, uint16_t ec_uS)
{
    return calib_cmd_param(dev, CMD_CAL_EC, ec_uS);
}

pmwcs3_status_t pmwcs3_new_reading(pmwcs3_t *dev)
{
    uint8_t cmd = CMD_READ_START; /* Corrected command */
    return (HAL_I2C_Master_Transmit(dev->bus,
                                    dev->addr << 1, &cmd, 1,
                                    PMWCS3_I2C_TIMEOUT) == HAL_OK)
           ? PMWCS3_OK : PMWCS3_ERR_I2C;
}

/* core low-level fetch --------------------------------------------------- */
static pmwcs3_status_t read_raw(pmwcs3_t *dev, int16_t raw[4])
{
    uint8_t cmd_get_data = CMD_GET_DATA;
    uint8_t buf[8] = {0};

    /* First, tell the sensor to send its data */
    HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(dev->bus, dev->addr << 1, &cmd_get_data, 1, PMWCS3_I2C_TIMEOUT);
    if (status != HAL_OK)
        return PMWCS3_ERR_I2C;

    /* Then, receive the 8 bytes of data */
    if (HAL_I2C_Master_Receive(dev->bus,
                               dev->addr << 1, buf, sizeof(buf),
                               PMWCS3_I2C_TIMEOUT) != HAL_OK)
        return PMWCS3_ERR_I2C;

    /* little-endian to host */
    for (int i = 0; i < 4; ++i)
        raw[i] = (int16_t)(((uint16_t)buf[i*2+1] << 8) | buf[i*2]); // Corrected: MSB is buf[i*2+1], LSB is buf[i*2]
    return PMWCS3_OK;
}

/* public getters -------------------------------------------------------- */
pmwcs3_status_t pmwcs3_get_all(pmwcs3_t *dev, float out[4])
{
    int16_t raw[4] = {0};
    pmwcs3_status_t st = read_raw(dev, raw);
    if (st != PMWCS3_OK) return st;

    out[0] = raw[0] / 100.0f;   /* ε25  (scaled by /100.0) */
    out[1] = raw[1] / 10.0f;    /* EC   (scaled by /10.0)  */
    out[2] = raw[2] / 100.0f;   /* °C   (scaled by /100.0) */
    out[3] = raw[3] / 10.0f;    /* VWC  (scaled by /10.0)  */
    return PMWCS3_OK;
}

#define SINGLE_GET(name, idx)                                 \
pmwcs3_status_t pmwcs3_get_##name(pmwcs3_t *d, float *o)      \
{                                                             \
    float vals[4]; /* Temporary array to hold all values */   \
    pmwcs3_status_t st = pmwcs3_get_all(d, vals);             \
    if (st == PMWCS3_OK && o) {                               \
        *o = vals[idx]; /* pmwcs3_get_all already applied scaling */ \
    } else if (o) {                                           \
        *o = 0.0f; /* Default value on error */              \
    }                                                         \
    return st;                                                \
}

// Individual getters now rely on pmwcs3_get_all which has correct scaling
SINGLE_GET(e25,  0)  // Scale is 100 for e25
SINGLE_GET(ec,   1)  // Scale is 10 for ec
SINGLE_GET(temp, 2)  // Scale is 100 for temp
SINGLE_GET(vwc,  3)  // Scale is 10 for vwc
