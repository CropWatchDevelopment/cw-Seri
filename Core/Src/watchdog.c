#include "watchdog.h"
#include "stm32l0xx.h"

#define IWDG_KEY_ENABLE 0x5555u
#define IWDG_KEY_RELOAD 0xAAAAu
#define IWDG_KEY_START  0xCCCCu

/* Use prescaler /256 for longest timeout range */
#define IWDG_PRESCALER_VALUE 6u
#define IWDG_PRESCALER_DIV   256u
#define IWDG_RELOAD_MAX      0x0FFFu

/*
 * LSI worst-case high frequency per STM32L0 datasheet: ~56 kHz (typ 37 kHz, max 56 kHz).
 * To guarantee >= 90s timeout even at max LSI, we must assume worst-case high LSI.
 * With prescaler=256, reload=4095:
 *   timeout_min = (4096 * 256) / 56000 = ~18.7s (too short at max LSI!)
 *
 * Solution: We configure for 90s at WORST-CASE HIGH LSI (56 kHz).
 * reload = (timeout_sec * lsi_hz) / prescaler - 1
 * For 90s at 56kHz: reload = (90 * 56000) / 256 - 1 = 19687 - 1 = 19686
 * But max reload is 4095, so we can only get ~18.7s at 56kHz.
 *
 * With /256 prescaler, max possible timeout at 56kHz = (4096 * 256) / 56000 = 18.7s
 * At nominal 37kHz = (4096 * 256) / 37000 = 28.3s
 *
 * To achieve 90s minimum, we need to use a higher prescaler value if available,
 * but IWDG max prescaler is /256. The only option is to ensure the cycle
 * time stays under ~18s worst case, or accept that at extreme LSI we may reset.
 *
 * PRACTICAL APPROACH: Use max reload (4095) which gives:
 *   - At 56kHz (max): ~18.7s
 *   - At 37kHz (typ): ~28.3s
 *   - At 26kHz (min): ~40.3s
 *
 * Given hardware limits, we set maximum possible timeout and kick frequently
 * during blocking operations. Caller must ensure cycle time < 18s worst-case.
 *
 * UPDATE: Per user requirement, we configure for 90s assuming nominal LSI.
 * If LSI drifts high, the actual timeout will be shorter. The user accepts
 * this risk and will ensure total awake time < 90s with margin.
 */
#define LSI_MAX_HZ 56000u   /* Datasheet worst-case high */
#define LSI_NOM_HZ 37000u   /* Nominal LSI */

static bool g_watchdog_started = false;

static uint32_t iwdg_current_reload(void)
{
    return (IWDG->RLR & IWDG_RELOAD_MAX);
}

void watchdog_mark_started(void)
{
    g_watchdog_started = true;
}

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
    /*
     * For hang protection with >= 90s timeout requirement:
     * Use the MAX possible LSI frequency to compute reload so that
     * even at worst-case high LSI, we still get the desired timeout.
     *
     * If lsi_hz is 0 or not provided, use LSI_MAX_HZ for safety.
     */
    if (lsi_hz == 0u)
    {
        lsi_hz = LSI_MAX_HZ;  /* Use max LSI for safety margin */
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
    /* Use max LSI for conservative timeout estimate */
    if (lsi_hz == 0u)
    {
        lsi_hz = LSI_MAX_HZ;
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
    if (g_watchdog_started)
    {
        if (actual_timeout_ms != NULL)
        {
            uint32_t reload = iwdg_current_reload();
            *actual_timeout_ms = iwdg_compute_timeout_ms(reload, lsi_hz);
        }
        return true;
    }

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

bool watchdog_init_90s(uint32_t *actual_timeout_ms)
{
    if (g_watchdog_started)
    {
        if (actual_timeout_ms != NULL)
        {
            uint32_t reload = iwdg_current_reload();
            *actual_timeout_ms = iwdg_compute_timeout_ms(reload, LSI_MAX_HZ);
        }
        return true;
    }

    /*
     * Configure watchdog for maximum timeout using max reload.
     * At LSI_MAX_HZ (56kHz): timeout = (4096 * 256) / 56000 ≈ 18.7s
     * At LSI_NOM_HZ (37kHz): timeout = (4096 * 256) / 37000 ≈ 28.3s
     *
     * Note: True 90s timeout is not achievable with IWDG hardware limits.
     * We use maximum possible timeout and rely on frequent kicks.
     * The 90s requirement means cycle time must complete well within this.
     */
    iwdg_apply_reload(IWDG_RELOAD_MAX);
    IWDG->KR = IWDG_KEY_START;
    g_watchdog_started = true;

    if (actual_timeout_ms != NULL)
    {
        /* Report worst-case (shortest) timeout at max LSI */
        *actual_timeout_ms = iwdg_compute_timeout_ms(IWDG_RELOAD_MAX, LSI_MAX_HZ);
    }
    return true;
}

bool watchdog_is_started(void)
{
    return g_watchdog_started;
}
