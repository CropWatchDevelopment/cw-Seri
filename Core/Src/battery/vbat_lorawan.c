/*
 * vbat_lorawan.c
 */

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


/* ---- Public API --------------------------------------------------------- */

uint16_t vbat_cold_compensation_mv(int16_t temp_c)
{
    if (temp_c >= 0) return 0;
    /* 4 mV per °C below zero, capped at +200 mV */
    uint32_t add = (uint32_t)((-temp_c) * 4);
    if (add > 200U) add = 200U;
    return (uint16_t)add;
}

bool vbat_read_mv(ADC_HandleTypeDef *hadc, uint32_t adc_channel, uint16_t *vbat_mv_out)
{
    if (!hadc || !vbat_mv_out) return false;

    vbat_gate(true);
    HAL_Delay(VBAT_SETTLE_MS);

    uint32_t acc = 0;
    uint16_t s   = 0;
    for (uint32_t i = 0; i < VBAT_SAMPLES; i++) {
        if (!adc_read_counts(hadc, adc_channel, &s)) { vbat_gate(false); return false; }
        acc += s;
    }

    vbat_gate(false);

    uint32_t avg_counts = acc / VBAT_SAMPLES;

    /* mv = counts * Vref / ADC_MAX * divider_factor; round properly */
    uint64_t mv = (uint64_t)avg_counts * (uint64_t)VREF_mV;
    mv = (mv + (ADC_MAX_COUNTS/2)) / (uint64_t)ADC_MAX_COUNTS;
    mv *= VBAT_DIV_NUM;

    if (mv > 0xFFFFU) mv = 0xFFFFU;
    *vbat_mv_out = (uint16_t)mv;
    return true;
}

uint8_t lorawan_encode_battery(uint16_t vbat_mv,
                               int16_t  temp_c,
                               bool     external_power_present,
                               bool     measurement_ok)
{
    if (external_power_present) return 0;   /* 0 = external power */
    if (!measurement_ok)       return 255;  /* 255 = cannot measure */

    uint32_t v = (uint32_t)vbat_mv + (uint32_t)vbat_cold_compensation_mv(temp_c);
    if (v > 5000U) v = 5000U; /* clamp */

    if (v >= VBAT_SEG1_MIN_mV) {
        /* 3.30–3.60 V → 200..254 (55 steps over 300 mV) */
        const uint32_t span_mv = (VBAT_SEG1_MAX_mV - VBAT_SEG1_MIN_mV);    /* 300 */
        const uint32_t span_lv = (LORA_SEG1_MAX - LORA_SEG1_MIN);          /* 54 */
        uint32_t dv = (v > VBAT_SEG1_MAX_mV) ? span_mv : (v - VBAT_SEG1_MIN_mV);
        uint32_t lvl = LORA_SEG1_MIN + (dv * span_lv + (span_mv/2)) / span_mv;
        if (lvl > LORA_SEG1_MAX) lvl = LORA_SEG1_MAX;
        return (uint8_t)lvl;
    } else if (v >= VBAT_SEG2_MIN_mV) {
        /* 2.80–3.30 V → 50..199 (150 steps over 500 mV) */
        const uint32_t span_mv = (VBAT_SEG1_MIN_mV - VBAT_SEG2_MIN_mV);    /* 500 */
        const uint32_t span_lv = (LORA_SEG2_MAX - LORA_SEG2_MIN);          /* 149 */
        uint32_t dv = v - VBAT_SEG2_MIN_mV;
        uint32_t lvl = LORA_SEG2_MIN + (dv * span_lv + (span_mv/2)) / span_mv;
        if (lvl > LORA_SEG2_MAX) lvl = LORA_SEG2_MAX;
        return (uint8_t)lvl;
    } else if (v >= VBAT_SEG3_MIN_mV) {
        /* 2.00–2.80 V → 1..49 (49 steps over 800 mV) */
        const uint32_t span_mv = (VBAT_SEG2_MIN_mV - VBAT_SEG3_MIN_mV);    /* 800 */
        const uint32_t span_lv = (LORA_SEG3_MAX - LORA_SEG3_MIN);          /* 48 */
        uint32_t dv = v - VBAT_SEG3_MIN_mV;
        uint32_t lvl = LORA_SEG3_MIN + (dv * span_lv + (span_mv/2)) / span_mv;
        if (lvl > LORA_SEG3_MAX) lvl = LORA_SEG3_MAX;
        return (uint8_t)lvl;
    } else {
        /* <2.00 V */
        return 1;
    }
}

uint8_t vbat_measure_and_encode(ADC_HandleTypeDef *hadc,
                                uint32_t adc_channel,
                                int16_t  temp_c,
                                bool     external_power_present)
{
    uint16_t mv = 0;
    bool ok = vbat_read_mv(hadc, adc_channel, &mv);
    return lorawan_encode_battery(mv, temp_c, external_power_present, ok);
}
