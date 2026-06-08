/**
 * @file    app.h
 * @brief   Standardized application interface for SD Card test app
 */
#ifndef APP_H
#define APP_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Called once during startup
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
