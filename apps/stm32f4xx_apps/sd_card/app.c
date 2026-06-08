/**
 * @file    app.c
 * @brief   SD Card Read/Write FatFs Test Application
 */
#include "app.h"
#include "ff.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/* Application States */
typedef enum {
    STATE_TESTING,
    STATE_SUCCESS,
    STATE_ERROR
} AppState_t;

static volatile AppState_t s_app_state = STATE_TESTING;
static uint32_t s_last_tick = 0U;

/* FatFs variables */
static FATFS s_fs;
static FIL s_fil;
static const char s_test_filename[] = "test.txt";
static const char s_write_data[] = "STM32F411 SD Card SPI Test OK!\r\n";
static char s_read_buffer[64];

void app_init(void)
{
    FRESULT fr;
    UINT bytes_written = 0;
    UINT bytes_read = 0;

    s_last_tick = HAL_GetTick();

    /* 1. Mount SD card */
    fr = f_mount(&s_fs, "", 1); /* 1: Force mount now */
    if (fr != FR_OK)
    {
        s_app_state = STATE_ERROR;
        return;
    }

    /* 2. Open file to write */
    fr = f_open(&s_fil, s_test_filename, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK)
    {
        f_mount(NULL, "", 0);
        s_app_state = STATE_ERROR;
        return;
    }

    /* 3. Write data */
    fr = f_write(&s_fil, s_write_data, strlen(s_write_data), &bytes_written);
    f_close(&s_fil); /* Close file regardless of write status */
    
    if (fr != FR_OK || bytes_written < strlen(s_write_data))
    {
        f_mount(NULL, "", 0);
        s_app_state = STATE_ERROR;
        return;
    }

    /* 4. Reopen file to read */
    fr = f_open(&s_fil, s_test_filename, FA_READ);
    if (fr != FR_OK)
    {
        f_mount(NULL, "", 0);
        s_app_state = STATE_ERROR;
        return;
    }

    /* 5. Read data */
    memset(s_read_buffer, 0, sizeof(s_read_buffer));
    fr = f_read(&s_fil, s_read_buffer, sizeof(s_read_buffer) - 1, &bytes_read);
    f_close(&s_fil);

    if (fr != FR_OK || bytes_read == 0)
    {
        f_mount(NULL, "", 0);
        s_app_state = STATE_ERROR;
        return;
    }

    /* 6. Verify contents */
    if (strcmp(s_read_buffer, s_write_data) == 0)
    {
        s_app_state = STATE_SUCCESS;
    }
    else
    {
        s_app_state = STATE_ERROR;
    }

    /* 7. Clean up mount */
    f_mount(NULL, "", 0);
}

void app_process(void)
{
    uint32_t now = HAL_GetTick();
    uint32_t period = 1000U;

    switch (s_app_state)
    {
        case STATE_SUCCESS:
            period = 100U; /* Fast blinking on success (100ms) */
            break;
            
        case STATE_ERROR:
            /* Keep LED ON solid (active low: Write RESET to turn on) */
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            return;
            
        case STATE_TESTING:
        default:
            period = 500U; /* Normal blinking while testing (500ms) */
            break;
    }

    if ((now - s_last_tick) >= period)
    {
        s_last_tick = now;
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13);
    }
}
