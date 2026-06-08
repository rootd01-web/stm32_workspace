/**
 * @file    app.c
 * @brief   Standardized LED blink application
 */
#include "app.h"
#include "stm32f4xx_hal.h"

static uint32_t s_last_tick = 0U;
static const uint32_t BLINK_PERIOD_MS = 500U; // 500 ms blink period

void app_init(void)
{
    // GPIO is already initialized in MX_GPIO_Init() from main.c
    s_last_tick = HAL_GetTick();
}

void app_process(void)
{
    uint32_t now = HAL_GetTick();

    if ((now - s_last_tick) >= BLINK_PERIOD_MS)
    {
        s_last_tick = now;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}
