/**
  ******************************************************************************
  * @file    appli_led_timer.c
  * @brief   TIM2-driven status LED blink.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */

#include "appli_led_timer.h"
#include "mx_hal_def.h"
#include "stm32g5xx_ll_gpio.h"
#include "stm32g5xx_ll_rcc.h"
#include "stm32g5xx_ll_tim.h"

#define LED_GPIO_PORT                 GPIOG
#define LED_GPIO_PIN                  LL_GPIO_PIN_3
#define LED_TIMER_TICK_HZ             10000U
#define LED_TIMER_TICKS_PER_MS        (LED_TIMER_TICK_HZ / 1000U)
#define LED_TIMER_INITIAL_PERIOD_MS  900U
#define LED_TIMER_MIN_PERIOD_MS      100U
#define LED_TIMER_PERIOD_STEP_MS     200U
#define LED_TIMER_TOGGLES_AT_START   4U
#define LED_TIMER_TOGGLES_PER_STEP   4U

static volatile uint32_t timer_period_ms = LED_TIMER_INITIAL_PERIOD_MS;
static uint32_t timer_ticks_per_ms;
static uint8_t toggles_since_period_change;

void TIM2_IRQHandler(void);

static uint32_t timer_toggle_count_limit(void)
{
  return LED_TIMER_TOGGLES_AT_START +
         (((LED_TIMER_INITIAL_PERIOD_MS - timer_period_ms) / LED_TIMER_PERIOD_STEP_MS) *
          LED_TIMER_TOGGLES_PER_STEP);
}

system_status_t appli_led_timer_init(void)
{
  uint32_t timer_clock_hz;
  uint32_t prescaler;

  HAL_RCC_GPIOG_EnableClock();
  HAL_RCC_TIM2_EnableClock();

  if ((HAL_RCC_GPIOG_IsEnabledClock() != HAL_RCC_CLK_ENABLED) ||
      (HAL_RCC_TIM2_IsEnabledClock() != HAL_RCC_CLK_ENABLED))
  {
    return SYSTEM_CLOCK_ERROR;
  }

  LL_GPIO_ResetOutputPin(LED_GPIO_PORT, LED_GPIO_PIN);
  LL_GPIO_SetPinOutputType(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_OUTPUT_PUSHPULL);
  LL_GPIO_SetPinSpeed(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_SPEED_FREQ_LOW);
  LL_GPIO_SetPinPull(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_PULL_NO);
  LL_GPIO_SetPinMode(LED_GPIO_PORT, LED_GPIO_PIN, LL_GPIO_MODE_OUTPUT);

  timer_clock_hz = HAL_RCC_GetPCLK1Freq();
  if (LL_RCC_GetAPB1Prescaler() != LL_RCC_APB1_PRESCALER_1)
  {
    timer_clock_hz *= 2U;
  }

  if ((timer_clock_hz < LED_TIMER_TICK_HZ) ||
      ((timer_clock_hz % LED_TIMER_TICK_HZ) != 0U))
  {
    return SYSTEM_CLOCK_ERROR;
  }

  prescaler = (timer_clock_hz / LED_TIMER_TICK_HZ) - 1U;
  if (prescaler > 0xFFFFU)
  {
    return SYSTEM_CLOCK_ERROR;
  }

  timer_ticks_per_ms = LED_TIMER_TICKS_PER_MS;
  timer_period_ms = LED_TIMER_INITIAL_PERIOD_MS;
  toggles_since_period_change = 0U;

  HAL_RCC_TIM2_Reset();
  LL_TIM_SetPrescaler(TIM2, prescaler);
  LL_TIM_SetAutoReload(TIM2, (timer_period_ms * timer_ticks_per_ms) - 1U);
  LL_TIM_SetCounter(TIM2, 0U);
  LL_TIM_GenerateEvent_UPDATE(TIM2);
  LL_TIM_ClearFlag_UPDATE(TIM2);

  HAL_CORTEX_NVIC_SetPriority(TIM2_IRQn,
                              HAL_CORTEX_NVIC_PREEMP_PRIORITY_15,
                              HAL_CORTEX_NVIC_SUB_PRIORITY_0);
  HAL_CORTEX_NVIC_EnableIRQ(TIM2_IRQn);
  LL_TIM_EnableIT_UPDATE(TIM2);
  LL_TIM_EnableCounter(TIM2);

  return SYSTEM_OK;
}

void TIM2_IRQHandler(void)
{
  if (LL_TIM_IsActiveFlag_UPDATE(TIM2) != 0U)
  {
    LL_TIM_ClearFlag_UPDATE(TIM2);
    LL_GPIO_TogglePin(LED_GPIO_PORT, LED_GPIO_PIN);

    toggles_since_period_change++;
    if (toggles_since_period_change >= timer_toggle_count_limit())
    {
      toggles_since_period_change = 0U;
      if (timer_period_ms <= LED_TIMER_MIN_PERIOD_MS)
      {
        timer_period_ms = LED_TIMER_INITIAL_PERIOD_MS;
      }
      else
      {
        timer_period_ms -= LED_TIMER_PERIOD_STEP_MS;
      }

      LL_TIM_SetAutoReload(TIM2, (timer_period_ms * timer_ticks_per_ms) - 1U);
    }
  }
}
