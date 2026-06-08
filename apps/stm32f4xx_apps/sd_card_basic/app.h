/**
 * @file    app.h
 * @brief   SD Card Basic SPI Polling — application interface
 *
 * Mục đích: gửi lệnh SPI raw (CMD0, CMD8, ACMD41, CMD58)
 * để có thể đo logic analyzer trên PA4/PA5/PA6/PA7.
 * Không dùng FatFS, không interrupt — chỉ HAL_SPI_TransmitReceive.
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Called once during startup (sau MX_SPI1_Init)
 */
void app_init(void);

/**
 * @brief  Called in the super-loop of main.c
 */
void app_process(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_H */
