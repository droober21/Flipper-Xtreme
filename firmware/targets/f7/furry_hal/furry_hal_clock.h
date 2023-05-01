#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stm32wbxx_ll_rcc.h>

typedef enum {
    FurryHalClockMcoLse,
    FurryHalClockMcoSysclk,
    FurryHalClockMcoMsi100k,
    FurryHalClockMcoMsi200k,
    FurryHalClockMcoMsi400k,
    FurryHalClockMcoMsi800k,
    FurryHalClockMcoMsi1m,
    FurryHalClockMcoMsi2m,
    FurryHalClockMcoMsi4m,
    FurryHalClockMcoMsi8m,
    FurryHalClockMcoMsi16m,
    FurryHalClockMcoMsi24m,
    FurryHalClockMcoMsi32m,
    FurryHalClockMcoMsi48m,
} FurryHalClockMcoSourceId;

typedef enum {
    FurryHalClockMcoDiv1 = LL_RCC_MCO1_DIV_1,
    FurryHalClockMcoDiv2 = LL_RCC_MCO1_DIV_2,
    FurryHalClockMcoDiv4 = LL_RCC_MCO1_DIV_4,
    FurryHalClockMcoDiv8 = LL_RCC_MCO1_DIV_8,
    FurryHalClockMcoDiv16 = LL_RCC_MCO1_DIV_16,
} FurryHalClockMcoDivisorId;

/** Early initialization */
void furry_hal_clock_init_early();

/** Early deinitialization */
void furry_hal_clock_deinit_early();

/** Initialize clocks */
void furry_hal_clock_init();

/** Switch to HSI clock */
void furry_hal_clock_switch_to_hsi();

/** Switch to PLL clock */
void furry_hal_clock_switch_to_pll();

/** Stop SysTick counter without resetting */
void furry_hal_clock_suspend_tick();

/** Continue SysTick counter operation */
void furry_hal_clock_resume_tick();

/** Enable clock output on MCO pin
 * 
 * @param      source  MCO clock source
 * @param      div     MCO clock division
*/
void furry_hal_clock_mco_enable(FurryHalClockMcoSourceId source, FurryHalClockMcoDivisorId div);

/** Disable clock output on MCO pin */
void furry_hal_clock_mco_disable();

#ifdef __cplusplus
}
#endif
