/*
 * pmwcs3.h
 *
 *  Created on: Oct 4, 2025
 *      Author: kevin
 */

#ifndef PMWCS3_H_
#define PMWCS3_H_

#include "stm32l0xx_hal.h"   /* change to your device family */

#ifdef __cplusplus
extern "C" {
#endif

/* Public return codes */
typedef enum {
    PMWCS3_OK          =  0,
    PMWCS3_ERR_I2C     = -1,
    PMWCS3_ERR_TIMEOUT = -2,
    PMWCS3_ERR_BADARG  = -3
} pmwcs3_status_t;

/* Opaque device handle */
typedef struct {
    I2C_HandleTypeDef *bus;
    uint8_t            addr;      /* 7-bit address (0x63 default) */
} pmwcs3_t;

/* ---- high-level API – mirrors the Arduino library ---------------------- */
pmwcs3_status_t pmwcs3_init          (pmwcs3_t *dev,
                                      I2C_HandleTypeDef *bus,
                                      uint8_t i2c_addr);

pmwcs3_status_t pmwcs3_new_address   (pmwcs3_t *dev, uint8_t new_addr);

pmwcs3_status_t pmwcs3_cal_air       (pmwcs3_t *dev);
pmwcs3_status_t pmwcs3_cal_water     (pmwcs3_t *dev);
pmwcs3_status_t pmwcs3_cal_ec        (pmwcs3_t *dev, uint16_t ec_uS);

pmwcs3_status_t pmwcs3_new_reading   (pmwcs3_t *dev);

pmwcs3_status_t pmwcs3_get_e25       (pmwcs3_t *dev, float *out);
pmwcs3_status_t pmwcs3_get_ec        (pmwcs3_t *dev, float *out);
pmwcs3_status_t pmwcs3_get_temp      (pmwcs3_t *dev, float *out);
pmwcs3_status_t pmwcs3_get_vwc       (pmwcs3_t *dev, float *out);

/* one-shot that fills all four results at once: ret[0]=ε25, [1]=EC, [2]=T, [3]=VWC */
pmwcs3_status_t pmwcs3_get_all       (pmwcs3_t *dev, float ret[4]);

#ifdef __cplusplus
}
#endif
#endif /* PMWCS3_H_ */
