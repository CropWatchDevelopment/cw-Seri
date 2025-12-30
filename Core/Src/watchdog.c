#include "watchdog.h"
#include "stm32l0xx.h"

#define IWDG_KEY_ENABLE 0x5555u
#define IWDG_KEY_RELOAD 0xAAAAu
#define IWDG_KEY_START  0xCCCCu

#define IWDG_PRESCALER_VALUE 6u
#define IWDG_PRESCALER_DIV   256u
#define IWDG_RELOAD_MAX      0x0FFFu

static bool g_watchdog_started = false;

static void iwdg_wait_update(uint32_t mask)
{
    uint32_t guard = 0xFFFFu;
    while ((IWDG->SR & mask) != 0u && guard-- > 0u)
    {
    }
}

static uint32_t iwdg_compute_reload(uint32_t desired_timeout_ms,
                                    uint32_t lsi_hz)
{
    if (lsi_hz == 0u)
    {
        lsi_hz = 37000u;
    }

    uint64_t ticks = (uint64_t)desired_timeout_ms * (uint64_t)lsi_hz;
    uint64_t div = (uint64_t)IWDG_PRESCALER_DIV * 1000u;
    ticks = ticks / div;

    if (ticks == 0u)
    {
        ticks = 1u;
    }
    if (ticks > ((uint64_t)IWDG_RELOAD_MAX + 1u))
    {
        ticks = (uint64_t)IWDG_RELOAD_MAX + 1u;
    }

    return (uint32_t)(ticks - 1u);
}

static uint32_t iwdg_compute_timeout_ms(uint32_t reload, uint32_t lsi_hz)
{
    if (lsi_hz == 0u)
    {
        lsi_hz = 37000u;
    }

    uint64_t ticks = (uint64_t)(reload + 1u) * (uint64_t)IWDG_PRESCALER_DIV;
    return (uint32_t)((ticks * 1000u) / lsi_hz);
}

static void iwdg_apply_reload(uint32_t reload)
{
    IWDG->KR = IWDG_KEY_ENABLE;
    IWDG->PR = IWDG_PRESCALER_VALUE;
    IWDG->RLR = (reload & IWDG_RELOAD_MAX);
    IWDG->WINR = IWDG_RELOAD_MAX;
    iwdg_wait_update(IWDG_SR_PVU | IWDG_SR_RVU | IWDG_SR_WVU);
    IWDG->KR = IWDG_KEY_RELOAD;
}

bool watchdog_init(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                   uint32_t *actual_timeout_ms)
{
    uint32_t reload = iwdg_compute_reload(desired_timeout_ms, lsi_hz);
    iwdg_apply_reload(reload);
    IWDG->KR = IWDG_KEY_START;
    g_watchdog_started = true;

    if (actual_timeout_ms != NULL)
    {
        *actual_timeout_ms = iwdg_compute_timeout_ms(reload, lsi_hz);
    }
    return true;
}

bool watchdog_update(uint32_t desired_timeout_ms, uint32_t lsi_hz,
                     uint32_t *actual_timeout_ms)
{
    if (!g_watchdog_started)
    {
        return false;
    }

    uint32_t reload = iwdg_compute_reload(desired_timeout_ms, lsi_hz);
    iwdg_apply_reload(reload);

    if (actual_timeout_ms != NULL)
    {
        *actual_timeout_ms = iwdg_compute_timeout_ms(reload, lsi_hz);
    }
    return true;
}

void watchdog_kick(void)
{
    if (!g_watchdog_started)
    {
        return;
    }
    IWDG->KR = IWDG_KEY_RELOAD;
}

bool watchdog_is_started(void)
{
    return g_watchdog_started;
}
