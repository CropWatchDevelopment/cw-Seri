/*
 * vbat_lorawan.c
 */

#include "main.h"
#include "vbat_lorawan.h"

/* ---- Internal helpers --------------------------------------------------- */

static inline void vbat_gate(bool enable)
{
#if VBAT_MEAS_EN_ACTIVE_HIGH
    HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(VBAT_MEAS_EN_GPIO_Port, VBAT_MEAS_EN_Pin,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

static inline void vbat_load_gate(bool enable)
{
#if VBAT_LOAD_EN_ACTIVE_HIGH
    HAL_GPIO_WritePin(VBAT_LOAD_EN_GPIO_Port, VBAT_LOAD_EN_Pin,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(VBAT_LOAD_EN_GPIO_Port, VBAT_LOAD_EN_Pin,
                      enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
#endif
}

static bool adc_read_counts(ADC_HandleTypeDef *hadc, uint32_t channel, uint16_t *out_counts)
{
    if (!hadc || !out_counts) return false;

    ADC_ChannelConfTypeDef s = {0};
    s.Channel = channel;
#if defined(ADC_RANK_CHANNEL_NUMBER)
    s.Rank    = ADC_RANK_CHANNEL_NUMBER;
#endif
    /* NOTE (STM32L0): sampling time is set globally in hadc.Init.SamplingTime */

    if (HAL_ADC_ConfigChannel(hadc, &s) != HAL_OK) return false;
    if (HAL_ADC_Start(hadc) != HAL_OK) return false;
    if (HAL_ADC_PollForConversion(hadc, 5) != HAL_OK) { (void)HAL_ADC_Stop(hadc); return false; }

    *out_counts = (uint16_t)HAL_ADC_GetValue(hadc);
    (void)HAL_ADC_Stop(hadc);
    return true;
}

static bool adc_counts_valid(uint16_t counts)
{
    return (counts >= VBAT_COUNTS_MIN_VALID) && (counts <= VBAT_COUNTS_MAX_VALID);
}

static uint16_t adc_counts_to_vbat_mv(uint16_t counts)
{
    uint64_t mv = (uint64_t)counts * (uint64_t)VREF_mV * (uint64_t)VBAT_DIV_NUM;
    mv = (mv + (ADC_MAX_COUNTS / 2)) / (uint64_t)ADC_MAX_COUNTS;
    mv = (mv + (VBAT_DIV_DEN / 2)) / (uint64_t)VBAT_DIV_DEN;
    if (mv > 0xFFFFu) mv = 0xFFFFu;
    return (uint16_t)mv;
}

static uint32_t vbat_rint_mohm(uint16_t vbat_idle_mv, uint16_t vbat_loaded_mv)
{
    if (vbat_idle_mv <= vbat_loaded_mv) return 0;
    uint32_t dv = (uint32_t)vbat_idle_mv - (uint32_t)vbat_loaded_mv;
    return (dv * 1000U + (VBAT_LOAD_mA / 2U)) / VBAT_LOAD_mA;
}

/* ---- Public API --------------------------------------------------------- */

uint16_t vbat_cold_compensation_mv(int16_t temp_c)
{
    if (temp_c >= 0) return 0;
    /* 4 mV per °C below zero, capped at +200 mV */
    uint32_t add = (uint32_t)((-temp_c) * 4);
    if (add > 200U) add = 200U;
    return (uint16_t)add;
}

bool measure_vbat_idle_mV(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_mv_out)
{
    if (!hadc || !vbat_mv_out) return false;

    bool ok = false;
    vbat_gate(true);
    HAL_Delay(VBAT_SETTLE_MS);

    // Dummy sample to charge ADC S/H cap & settle op-amp/line
    uint16_t dummy;
    (void)adc_read_counts(hadc, adc_channel, &dummy);

    uint32_t acc = 0;
    for (uint32_t i = 0; i < VBAT_SAMPLES; i++) {
        uint16_t s;
        if (!adc_read_counts(hadc, adc_channel, &s)) { vbat_gate(false); return false; }
        acc += s;
    }

    vbat_gate(false);

    uint16_t avg = (uint16_t)(acc / VBAT_SAMPLES);
    if (adc_counts_valid(avg)) {
        *vbat_mv_out = adc_counts_to_vbat_mv(avg);
        ok = true;
    }
    return ok;
}

bool measure_vbat_loaded_min_mV(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_min_mv_out)
{
    if (!hadc || !vbat_min_mv_out) return false;

    bool ok = false;
    vbat_gate(true);
    HAL_Delay(VBAT_SETTLE_MS);
    vbat_load_gate(true);
    HAL_Delay(VBAT_LOAD_SETTLE_MS);

    // Dummy sample after load to charge ADC S/H cap & settle the node
    uint16_t dummy;
    (void)adc_read_counts(hadc, adc_channel, &dummy);

    uint16_t min_counts = ADC_MAX_COUNTS;
    for (uint32_t i = 0; i < VBAT_LOAD_SAMPLES; i++) {
        uint16_t s;
        if (!adc_read_counts(hadc, adc_channel, &s)) {
            vbat_load_gate(false);
            vbat_gate(false);
            return false;
        }
        if (s < min_counts) min_counts = s;
    }

    vbat_load_gate(false);
    vbat_gate(false);

    if (adc_counts_valid(min_counts)) {
        *vbat_min_mv_out = adc_counts_to_vbat_mv(min_counts);
        ok = true;
    }
    return ok;
}

uint8_t lorawan_encode_battery(uint16_t vbat_mv,
                               int16_t  temp_c,
                               bool     external_power_present,
                               bool     measurement_ok)
{
    if (external_power_present) return 0;   /* 0 = external power */
    if (!measurement_ok)       return 255;  /* 255 = cannot measure */

    uint32_t v = (uint32_t)vbat_mv + (uint32_t)vbat_cold_compensation_mv(temp_c);
    if (v > 6000U) v = 6000U; /* clamp for math safety */

    if (VBAT_FULL_mV <= VBAT_EMPTY_mV) return 255;

    if (v <= VBAT_EMPTY_mV) return (uint8_t)VBAT_LEVEL_MIN;
    if (v >= VBAT_FULL_mV)  return (uint8_t)VBAT_LEVEL_MAX;

    const uint32_t span_mv = (VBAT_FULL_mV - VBAT_EMPTY_mV);
    const uint32_t span_lv = (VBAT_LEVEL_MAX - VBAT_LEVEL_MIN);
    uint32_t dv = v - VBAT_EMPTY_mV;
    uint32_t lvl = VBAT_LEVEL_MIN + (dv * span_lv + (span_mv / 2)) / span_mv;
    if (lvl < VBAT_LEVEL_MIN) lvl = VBAT_LEVEL_MIN;
    if (lvl > VBAT_LEVEL_MAX) lvl = VBAT_LEVEL_MAX;
    return (uint8_t)lvl;
}

uint8_t vbat_measure_and_encode(ADC_HandleTypeDef *hadc,
                                uint32_t adc_channel,
                                int16_t  temp_c,
                                bool     external_power_present)
{
    uint16_t vbat_idle_mv = 0;
    uint16_t vbat_loaded_mv = 0;
    bool idle_ok = measure_vbat_idle_mV(hadc, adc_channel, &vbat_idle_mv);
    bool loaded_ok = measure_vbat_loaded_min_mV(hadc, adc_channel, &vbat_loaded_mv);

    bool measurement_ok = (idle_ok || loaded_ok);
    if (measurement_ok && idle_ok && loaded_ok && (VBAT_RINT_LIMIT_mOHM > 0U)) {
        uint32_t rint = vbat_rint_mohm(vbat_idle_mv, vbat_loaded_mv);
        if (rint > VBAT_RINT_LIMIT_mOHM) {
            measurement_ok = false;
        }
    }

    uint16_t use_mv = loaded_ok ? vbat_loaded_mv : vbat_idle_mv;
    return lorawan_encode_battery(use_mv, temp_c, external_power_present, measurement_ok);
}
