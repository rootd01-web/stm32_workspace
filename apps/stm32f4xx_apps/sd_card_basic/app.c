/**
 * @file    app.c
 * @brief   SD Card Basic — SPI DMA State Machine (Non-Blocking)
 *
 * Pins:
 *   PA4 = SPI1_NSS  (CS, hardware controlled)
 *   PA5 = SPI1_SCK
 *   PA6 = SPI1_MISO
 *   PA7 = SPI1_MOSI
 *
 * DMA mapping (từ CubeMX):
 *   SPI1_RX → DMA2 Stream0 Channel 3
 *   SPI1_TX → DMA2 Stream2 Channel 2
 *
 * Cơ chế State Machine:
 *   - CPU KHÔNG BAO GIỜ chờ (blocking) DMA.
 *   - `app_process()` được gọi liên tục trong main loop.
 *   - Khi DMA hoàn thành, cờ `s_dma_done` được set trong ISR.
 *   - `app_process()` kiểm tra cờ này, đọc kết quả từ buffer, 
 *     chuẩn bị buffer mới và kick DMA cho trạng thái tiếp theo, rồi return ngay.
 */

#include "app.h"
#include "spi.h"
#include "dma.h"
#include "stm32f4xx_hal.h"

/* ============================================================
 * Cấu hình
 * ============================================================ */
#define SD_SPI          (&hspi1)
#define SD_TIMEOUT_MS   1000U
#define DMA_BUF_SIZE    20U  /* 6 byte lệnh + 14 byte dummy */

/* LED PC13 active-low */
#define LED_ON()        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET)
#define LED_OFF()       HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET)
#define LED_TOGGLE()    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_13)

/* ============================================================
 * Các trạng thái của State Machine
 * ============================================================ */
typedef enum {
    ST_IDLE = 0,
    ST_START_DUMMY,
    ST_WAIT_DUMMY,
    ST_START_CMD0,
    ST_WAIT_CMD0,
    ST_START_CMD8,
    ST_WAIT_CMD8,
    ST_START_CMD55,
    ST_WAIT_CMD55,
    ST_START_CMD41,
    ST_WAIT_CMD41,
    ST_START_CMD58,
    ST_WAIT_CMD58,
    ST_SUCCESS,
    ST_ERROR
} SmState_t;

static volatile SmState_t s_state      = ST_IDLE;
static volatile uint8_t   s_dma_done   = 0U;
static uint32_t           s_state_tick = 0U; /* Timeout watchdog cho từng state */
static uint32_t           s_acmd41_timeout = 0U;

/* Buffer tĩnh cho DMA - Phải tồn tại suốt vòng đời app */
static uint8_t s_tx_buf[DMA_BUF_SIZE];
static uint8_t s_rx_buf[DMA_BUF_SIZE];

/* Biến lưu OCR và retry đếm */
static uint8_t s_ocr[4];

/* Timer cho LED chớp */
static uint32_t s_led_tick = 0U;

/* ============================================================
 * Callback DMA
 * ============================================================ */
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi->Instance == SPI1)
    {
        s_dma_done = 1U;
    }
}

/* ============================================================
 * Helper Functions
 * ============================================================ */

/**
 * @brief Chuẩn bị buffer để gửi một lệnh SD
 */
static void prepare_sd_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    s_tx_buf[0] = cmd | 0x40U;
    s_tx_buf[1] = (uint8_t)(arg >> 24U);
    s_tx_buf[2] = (uint8_t)(arg >> 16U);
    s_tx_buf[3] = (uint8_t)(arg >> 8U);
    s_tx_buf[4] = (uint8_t)(arg);
    s_tx_buf[5] = crc | 0x01U;

    /* Pad các byte còn lại bằng 0xFF để nhận R1 (và payload nếu có) */
    for (uint32_t i = 6; i < DMA_BUF_SIZE; i++)
    {
        s_tx_buf[i] = 0xFFU;
    }
}

/**
 * @brief Tìm R1 response trong rx_buf sau lệnh
 * @return Vị trí (index) của R1, hoặc -1 nếu không thấy
 */
static int32_t find_r1_response(void)
{
    /* R1 thường xuất hiện sau byte thứ 6 (chỗ dummy clock) */
    for (uint32_t i = 6; i < DMA_BUF_SIZE; i++)
    {
        /* R1 có MSB = 0 */
        if ((s_rx_buf[i] & 0x80U) == 0U)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

/**
 * @brief Bắt đầu transfer DMA và chuyển sang state chờ
 */
static void kick_dma(SmState_t next_wait_state)
{
    s_dma_done = 0U;
    s_state_tick = HAL_GetTick();
    s_state = next_wait_state;
    HAL_SPI_TransmitReceive_DMA(SD_SPI, s_tx_buf, s_rx_buf, DMA_BUF_SIZE);
}

/* ============================================================
 * app_init
 * ============================================================ */
void app_init(void)
{
    s_state = ST_START_DUMMY;
    s_led_tick = HAL_GetTick();
    LED_OFF();
}

/* ============================================================
 * app_process — Chạy liên tục trong while(1) của main
 * ============================================================ */
void app_process(void)
{
    uint32_t now = HAL_GetTick();
    int32_t r1_idx;
    uint8_t r1;

    /* Xử lý nhấp nháy LED độc lập với SPI */
    uint32_t led_period = 500U;
    if (s_state == ST_SUCCESS) led_period = 100U;
    else if (s_state == ST_ERROR) { LED_ON(); }
    
    if (s_state != ST_ERROR && (now - s_led_tick) >= led_period)
    {
        s_led_tick = now;
        LED_TOGGLE();
    }

    /* Kiểm tra timeout của các trạng thái WAIT */
    if ((s_state == ST_WAIT_DUMMY || s_state == ST_WAIT_CMD0 || 
         s_state == ST_WAIT_CMD8  || s_state == ST_WAIT_CMD55 || 
         s_state == ST_WAIT_CMD41 || s_state == ST_WAIT_CMD58))
    {
        if ((now - s_state_tick) > SD_TIMEOUT_MS)
        {
            HAL_SPI_DMAStop(SD_SPI);
            s_state = ST_ERROR;
            return;
        }
        
        /* Nếu DMA chưa xong, lập tức return để CPU làm việc khác */
        if (!s_dma_done)
        {
            return;
        }
    }

    /* --- STATE MACHINE CORE --- */
    switch (s_state)
    {
        case ST_IDLE:
        case ST_SUCCESS:
        case ST_ERROR:
            /* Không làm gì thêm */
            break;

        /* ---------- DUMMY CLOCKS ---------- */
        case ST_START_DUMMY:
            /* Gửi toàn 0xFF để power-up */
            for (uint32_t i = 0; i < DMA_BUF_SIZE; i++) s_tx_buf[i] = 0xFFU;
            kick_dma(ST_WAIT_DUMMY);
            break;

        case ST_WAIT_DUMMY:
            s_state = ST_START_CMD0;
            break;

        /* ---------- CMD0 (GO_IDLE_STATE) ---------- */
        case ST_START_CMD0:
            prepare_sd_cmd(0U, 0x00000000UL, 0x95U);
            kick_dma(ST_WAIT_CMD0);
            break;

        case ST_WAIT_CMD0:
            r1_idx = find_r1_response();
            if (r1_idx >= 0 && s_rx_buf[r1_idx] == 0x01U) {
                s_state = ST_START_CMD8;
            } else {
                s_state = ST_ERROR;
            }
            break;

        /* ---------- CMD8 (SEND_IF_COND) ---------- */
        case ST_START_CMD8:
            prepare_sd_cmd(8U, 0x000001AAUL, 0x87U);
            kick_dma(ST_WAIT_CMD8);
            break;

        case ST_WAIT_CMD8:
            r1_idx = find_r1_response();
            if (r1_idx >= 0 && s_rx_buf[r1_idx] == 0x01U) {
                /* Đọc R7 (4 bytes payload sau R1) */
                if ((uint32_t)r1_idx + 4U < DMA_BUF_SIZE) {
                    s_ocr[0] = s_rx_buf[r1_idx + 1];
                    s_ocr[1] = s_rx_buf[r1_idx + 2];
                    s_ocr[2] = s_rx_buf[r1_idx + 3];
                    s_ocr[3] = s_rx_buf[r1_idx + 4];
                }
                /* Bắt đầu vòng lặp ACMD41, set timeout 1s */
                s_acmd41_timeout = now;
                s_state = ST_START_CMD55;
            } else {
                s_state = ST_ERROR;
            }
            break;

        /* ---------- ACMD41 (APP_SEND_OP_COND) ---------- */
        case ST_START_CMD55:
            prepare_sd_cmd(55U, 0x00000000UL, 0xFFU);
            kick_dma(ST_WAIT_CMD55);
            break;

        case ST_WAIT_CMD55:
            r1_idx = find_r1_response();
            if (r1_idx >= 0) { // Có thể là 0x01
                s_state = ST_START_CMD41;
            } else {
                s_state = ST_ERROR;
            }
            break;

        case ST_START_CMD41:
            prepare_sd_cmd(41U, 0x40000000UL, 0xFFU);
            kick_dma(ST_WAIT_CMD41);
            break;

        case ST_WAIT_CMD41:
            r1_idx = find_r1_response();
            if (r1_idx >= 0) {
                r1 = s_rx_buf[r1_idx];
                if (r1 == 0x00U) {
                    /* Đã ra khỏi Idle -> Thành công bước đầu */
                    s_state = ST_START_CMD58;
                } else if (r1 == 0x01U) {
                    /* Vẫn đang Init, kiểm tra tổng thời gian timeout */
                    if ((now - s_acmd41_timeout) > 1000U) {
                        s_state = ST_ERROR; /* Quá 1 giây khởi tạo -> Lỗi */
                    } else {
                        /* Gửi lại từ CMD55 */
                        s_state = ST_START_CMD55;
                    }
                } else {
                    s_state = ST_ERROR;
                }
            } else {
                s_state = ST_ERROR;
            }
            break;

        /* ---------- CMD58 (READ_OCR) ---------- */
        case ST_START_CMD58:
            prepare_sd_cmd(58U, 0x00000000UL, 0xFFU);
            kick_dma(ST_WAIT_CMD58);
            break;

        case ST_WAIT_CMD58:
            r1_idx = find_r1_response();
            if (r1_idx >= 0 && s_rx_buf[r1_idx] == 0x00U) {
                if ((uint32_t)r1_idx + 4U < DMA_BUF_SIZE) {
                    s_ocr[0] = s_rx_buf[r1_idx + 1];
                    s_ocr[1] = s_rx_buf[r1_idx + 2];
                    s_ocr[2] = s_rx_buf[r1_idx + 3];
                    s_ocr[3] = s_rx_buf[r1_idx + 4];
                }
                /* Thành công toàn bộ! */
                s_state = ST_SUCCESS;
            } else {
                s_state = ST_ERROR;
            }
            break;
            
        default:
            s_state = ST_ERROR;
            break;
    }
}
